/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2025  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "searchhandler.h"

#include <algorithm>
#include <chrono>
#include <iterator>

#include <QtLogging>
#include <QList>
#include <QMetaObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/utils/bytearray.h"
#include "base/utils/foreignapps.h"
#include "base/utils/fs.h"
#include "searchpluginmanager.h"

using namespace std::chrono_literals;

namespace
{
    // qBittorrentAccess: result names as sites give them are often hard to read with a screen reader or a
    // braille display ("Casino.1995.1080p.BluRay", "( )-Casino-1995-BD", "ÐšÐ°Ð·Ð¸Ð½Ð¾", runs of spaces).
    // Only the displayed name changes; the torrent itself keeps its own name.

    // text in UTF-8 that a plugin decoded as Windows-1252 (or Latin-1): turned back into the real text
    QString repairMisdecodedUtf8(const QString &name)
    {
        // Windows-1252 characters 0x80..0x9F that are not Latin-1
        static const char16_t cp1252High[32] = {
            0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D, 0x017D, 0x8F,
            0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D, 0x017E, 0x0178};

        bool hasHigh = false;
        QByteArray bytes;
        bytes.reserve(name.size());
        for (const QChar ch : name)
        {
            const char16_t c = ch.unicode();
            if (c < 0x80)
            {
                bytes.append(static_cast<char>(c));
                continue;
            }
            hasHigh = true;
            if (c == 0xFFFD) // a byte the plugin could not decode: keep a place for it
            {
                bytes.append('\xFF');
                continue;
            }
            const char16_t *found = std::find(std::begin(cp1252High), std::end(cp1252High), c);
            if (found != std::end(cp1252High))
                bytes.append(static_cast<char>(0x80 + (found - std::begin(cp1252High))));
            else if (c <= 0xFF)
                bytes.append(static_cast<char>(c));
            else
                return name; // a character Windows-1252 cannot hold: the text was not misdecoded
        }
        if (!hasHigh)
            return name;

        // really UTF-8: at least one character decoded, and no invalid byte except a character cut off
        // at the end of a truncated name ("...é.."); a real "Café" gives nothing but an invalid byte
        QString repaired = QString::fromUtf8(bytes);
        const bool decodedSomething = std::any_of(repaired.cbegin(), repaired.cend()
            , [](const QChar c) { return (c.unicode() >= 0x80) && (c.unicode() != 0xFFFD); });
        if (!decodedSomething)
            return name;
        qsizetype end = repaired.size();
        while ((end > 0) && ((repaired.at(end - 1) == u'.') || repaired.at(end - 1).isSpace()))
            --end;
        qsizetype cut = end;
        while ((cut > 0) && (repaired.at(cut - 1) == QChar(0xFFFD)) && ((end - cut) < 3))
            --cut;
        if (repaired.left(cut).contains(QChar(0xFFFD)))
            return name;
        repaired.remove(cut, (end - cut));
        return repaired;
    }

    QString readableResultName(QString name)
    {
        // percent-encoded names ("Casino %281995%29", "%D0%9A%D0%B0...") printed by some plugins or nova versions
        static const QRegularExpression percentCode {u"%[0-9A-Fa-f]{2}"_s};
        if (name.contains(percentCode))
        {
            const QString decoded = QUrl::fromPercentEncoding(name.toUtf8());
            if (!decoded.contains(QChar(0xFFFD)))
                name = decoded;
        }

        name = repairMisdecodedUtf8(name);

        // HTML entities left by some plugins
        name.replace(u"&amp;"_s, u"&"_s).replace(u"&quot;"_s, u"\""_s).replace(u"&#39;"_s, u"'"_s)
            .replace(u"&apos;"_s, u"'"_s).replace(u"&lt;"_s, u"<"_s).replace(u"&gt;"_s, u">"_s)
            .replace(u"&nbsp;"_s, u" "_s);

        // "_" never separates anything but words
        name.replace(u'_', u' ');

        // "." between words is a space ("Casino.1995.BluRay"), but not inside a number ("5.1", "2.0")
        // nor in "..", the mark of a truncated name
        QString out;
        out.reserve(name.size());
        for (qsizetype i = 0; i < name.size(); ++i)
        {
            const QChar ch = name.at(i);
            if (ch == u'.')
            {
                const QChar before = (i > 0) ? name.at(i - 1) : QChar();
                const QChar after = ((i + 1) < name.size()) ? name.at(i + 1) : QChar();
                const bool inNumber = before.isDigit() && after.isDigit();
                const bool inDots = (before == u'.') || (after == u'.');
                if (!inNumber && !inDots && before.isLetterOrNumber() && after.isLetterOrNumber())
                {
                    out.append(u' ');
                    continue;
                }
            }
            out.append(ch);
        }
        name = out;

        // "-" joining most words ("Casino-1995-REMASTERED-BD-1080p") is a space too; a few ("WEB-DL") stay
        static const QRegularExpression hyphenJoin {u"(?<=[\\p{L}\\p{N}\\)\\]])-(?=[\\p{L}\\p{N}\\(\\[])"_s};
        const qsizetype joins = name.count(hyphenJoin);
        if ((joins >= 3) && (joins >= name.count(u' ')))
            name.replace(hyphenJoin, u" "_s);

        // runs of spaces, empty brackets, a leading dash
        static const QRegularExpression emptyBrackets {u"\\(\\s*\\)|\\[\\s*\\]"_s};
        name.replace(emptyBrackets, QString());
        name = name.simplified();
        static const QRegularExpression leadingDash {u"^[-–—\\s]+"_s};
        name.remove(leadingDash);
        return name;
    }

    enum SearchResultColumn
    {
        PL_DL_LINK,
        PL_NAME,
        PL_SIZE,
        PL_SEEDS,
        PL_LEECHS,
        PL_ENGINE_URL,
        PL_DESC_LINK,
        PL_PUB_DATE,
        NB_PLUGIN_COLUMNS
    };

    QString toString(const QProcess::ProcessError error)
    {
        switch (error)
        {
        case QProcess::FailedToStart:
            return SearchHandler::tr("Process failed to start");
        case QProcess::Crashed:
            return SearchHandler::tr("Process crashed");
        case QProcess::Timedout:
            return SearchHandler::tr("Process timed out");
        case QProcess::WriteError:
            return SearchHandler::tr("Process write error");
        case QProcess::ReadError:
            return SearchHandler::tr("Process read error");
        case QProcess::UnknownError:
            return SearchHandler::tr("Process unknown error");
        }
        return {};
    };
}

SearchHandler::SearchHandler(const QString &pattern, const QString &category, const QStringList &usedPlugins, SearchPluginManager *manager)
    : QObject(manager)
    , m_pattern {pattern}
    , m_category {category}
    , m_usedPlugins {usedPlugins}
    , m_manager {manager}
    , m_searchProcess {new QProcess(this)}
    , m_searchTimeout {new QTimer(this)}
{
    // Load environment variables (proxy)
    m_searchProcess->setProcessEnvironment(m_manager->proxyEnvironment());
    m_searchProcess->setProgram(Utils::ForeignApps::pythonInfo().executablePath.data());
#ifdef Q_OS_UNIX
    m_searchProcess->setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);
#endif

    const QStringList params
    {
        Utils::ForeignApps::PYTHON_ISOLATE_MODE_FLAG,
        Utils::ForeignApps::PYTHON_UTF8_MODE_FLAG,
        (SearchPluginManager::engineLocation() / Path(u"nova2.py"_s)).toString(),
        m_usedPlugins.join(u','),
        m_category
    };
    m_searchProcess->setArguments(params + m_pattern.split(u' '));

    connect(m_searchProcess, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError error)
    {
        if (!m_searchCancelled)
        {
            const auto errMsg = toString(error);
            LogMsg(tr("Search process failed. Search query: \"%1\". Category: \"%2\". Engines: \"%3\". Error: \"%4\".")
                .arg(m_pattern, m_category, m_usedPlugins.join(u", "), errMsg), Log::WARNING);
            emit searchFailed(errMsg);
        }
    });
    connect(m_searchProcess, &QProcess::readyReadStandardOutput, this, &SearchHandler::readSearchOutput);
    connect(m_searchProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished)
            , this, &SearchHandler::processFinished);

    m_searchTimeout->setSingleShot(true);
    connect(m_searchTimeout, &QTimer::timeout, this, &SearchHandler::cancelSearch);
    m_searchTimeout->start(3min);

    // Launch search
    // deferred start allows clients to handle starting-related signals
    QMetaObject::invokeMethod(this, [this]() { m_searchProcess->start(QIODevice::ReadOnly); }
        , Qt::QueuedConnection);
}

bool SearchHandler::isActive() const
{
    return (m_searchProcess->state() != QProcess::NotRunning);
}

void SearchHandler::cancelSearch()
{
    if ((m_searchProcess->state() == QProcess::NotRunning) || m_searchCancelled)
        return;

#ifdef Q_OS_WIN
    m_searchProcess->kill();
#else
    m_searchProcess->terminate();
#endif
    m_searchCancelled = true;
    m_searchTimeout->stop();
}

// Slot called when QProcess is Finished
// QProcess can be finished for 3 reasons:
// Error | Stopped by user | Finished normally
void SearchHandler::processFinished(const int exitcode)
{
    m_searchTimeout->stop();

    const auto errMsg = QString::fromUtf8(m_searchProcess->readAllStandardError()).trimmed();
    if (!errMsg.isEmpty())
    {
        qWarning("%s", qUtf8Printable(errMsg));
        LogMsg(tr("Error occurred in search engine. Search query: \"%1\". Category: \"%2\". Engines: \"%3\". Error: \"%4\".")
            .arg(m_pattern, m_category, m_usedPlugins.join(u", "), errMsg), Log::WARNING);
    }

    if (m_searchCancelled)
        emit searchFinished(true);
    else if ((m_searchProcess->exitStatus() == QProcess::NormalExit) && (exitcode == 0))
        emit searchFinished(false);
    else
        emit searchFailed(errMsg);
}

// search QProcess return output as soon as it gets new
// stuff to read. We split it into lines and parse each
// line to SearchResult calling parseSearchResult().
void SearchHandler::readSearchOutput()
{
    const QByteArray output = m_searchResultLineTruncated + m_searchProcess->readAllStandardOutput();
    QList<QByteArrayView> lines = Utils::ByteArray::splitToViews(output, "\n", Qt::KeepEmptyParts);

    m_searchResultLineTruncated = lines.takeLast().trimmed().toByteArray();

    QList<SearchResult> searchResultList;
    searchResultList.reserve(lines.size());

    for (const QByteArrayView &line : asConst(lines))
    {
        if (SearchResult searchResult; parseSearchResult(line, searchResult))
            searchResultList.append(std::move(searchResult));
    }

    if (!searchResultList.isEmpty())
    {
        m_results.append(searchResultList);
        emit newSearchResults(searchResultList);
    }
}

// Parse one line of search results list
// Line is in the following form:
// file url | file name | file size | nb seeds | nb leechers | Search engine url
bool SearchHandler::parseSearchResult(const QByteArrayView line, SearchResult &searchResult)
{
    const QList<QByteArrayView> parts = Utils::ByteArray::splitToViews(line, "|");
    const qsizetype nbFields = parts.size();

    if (nbFields <= PL_ENGINE_URL)
        return false; // Anything after ENGINE_URL is optional

    searchResult = SearchResult();
    searchResult.fileUrl = QString::fromUtf8(parts.at(PL_DL_LINK).trimmed()); // download URL
    searchResult.fileName = readableResultName(QString::fromUtf8(parts.at(PL_NAME).trimmed())); // Name
    searchResult.fileSize = parts.at(PL_SIZE).trimmed().toLongLong(); // Size

    bool ok = false;

    searchResult.nbSeeders = parts.at(PL_SEEDS).trimmed().toLongLong(&ok); // Seeders
    if (!ok || (searchResult.nbSeeders < 0))
        searchResult.nbSeeders = -1;

    searchResult.nbLeechers = parts.at(PL_LEECHS).trimmed().toLongLong(&ok); // Leechers
    if (!ok || (searchResult.nbLeechers < 0))
        searchResult.nbLeechers = -1;

    searchResult.siteUrl = QString::fromUtf8(parts.at(PL_ENGINE_URL).trimmed()); // Search engine site URL
    searchResult.engineName = m_manager->pluginNameBySiteURL(searchResult.siteUrl); // Search engine name

    if (nbFields > PL_DESC_LINK)
        searchResult.descrLink = QString::fromUtf8(parts.at(PL_DESC_LINK).trimmed()); // Description Link

    if (nbFields > PL_PUB_DATE)
    {
        const qint64 secs = parts.at(PL_PUB_DATE).trimmed().toLongLong(&ok);
        if (ok && (secs > 0))
            searchResult.pubDate = QDateTime::fromSecsSinceEpoch(secs); // Date
    }

    return true;
}

SearchPluginManager *SearchHandler::manager() const
{
    return m_manager;
}

QList<SearchResult> SearchHandler::results() const
{
    return m_results;
}

QString SearchHandler::pattern() const
{
    return m_pattern;
}

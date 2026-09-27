/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Mike Tzou (Chocobo1)
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
 * qBittorrentAccess: rewritten to update from the qBittorrentAccess releases.
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

#include "programupdater.h"

#include <QtSystemDetection>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/version.h"

namespace
{
    const QString RELEASE_API_URL = u"https://api.github.com/repos/reaperaccessible/qBittorrentAccess/releases/latest"_s;
    const QString INSTALLER_NAME = u"qBittorrentAccessInstaller.exe"_s;

    QNetworkRequest makeRequest(const QString &url)
    {
        QNetworkRequest request {QUrl(url)};
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("qBittorrentAccess/" QBTACCESS_VERSION " updater"));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        return request;
    }

    // "v1.01", "1.01" -> {1, 1}; {-1, -1} if not a version
    std::pair<int, int> parseVersion(const QString &text)
    {
        static const QRegularExpression versionRegex {u"(\\d+)\\.(\\d+)"_s};
        const QRegularExpressionMatch match = versionRegex.match(text);
        if (!match.hasMatch())
            return {-1, -1};
        return {match.captured(1).toInt(), match.captured(2).toInt()};
    }
}

ProgramUpdater::ProgramUpdater(QObject *parent)
    : QObject(parent)
    , m_network {new QNetworkAccessManager(this)}
{
}

void ProgramUpdater::checkForUpdates()
{
    // QBTACCESS_UPDATE_API: for tests only, a local server standing for GitHub
    QNetworkRequest request = makeRequest(qEnvironmentVariable("QBTACCESS_UPDATE_API", RELEASE_API_URL));
    request.setRawHeader("Accept", "application/vnd.github+json");
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] { releaseInfoReceived(reply); });
}

void ProgramUpdater::releaseInfoReceived(QNetworkReply *reply)
{
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (status == 404)
    {
        // nothing released yet: this version is the latest
        emit updateCheckFinished();
        return;
    }
    if (reply->error() != QNetworkReply::NoError)
    {
        m_checkFailed = true;
        m_errorString = reply->errorString();
        LogMsg(tr("Failed to check for qBittorrentAccess updates. Error: \"%1\"").arg(m_errorString), Log::WARNING);
        emit updateCheckFinished();
        return;
    }

    const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    const auto [major, minor] = parseVersion(release.value(u"tag_name"_s).toString());
    if (major < 0)
    {
        m_checkFailed = true;
        m_errorString = tr("The latest release has no version number.");
        emit updateCheckFinished();
        return;
    }

    const bool newer = (major > QBTACCESS_VERSION_MAJOR) || ((major == QBTACCESS_VERSION_MAJOR) && (minor > QBTACCESS_VERSION_MINOR));
    if (newer)
    {
        for (const QJsonValue &assetValue : release.value(u"assets"_s).toArray())
        {
            const QJsonObject asset = assetValue.toObject();
            if (asset.value(u"name"_s).toString().compare(INSTALLER_NAME, Qt::CaseInsensitive) == 0)
            {
                m_installerUrl = asset.value(u"browser_download_url"_s).toString();
                m_installerSize = asset.value(u"size"_s).toInteger(-1);
            }
        }
        if (m_installerUrl.isEmpty())
        {
            m_checkFailed = true;
            m_errorString = tr("The latest release has no installer.");
        }
        else
        {
            m_newVersion = u"%1.%2"_s.arg(QString::number(major), QString::number(minor).rightJustified(2, u'0'));
            m_releaseBody = release.value(u"body"_s).toString();
        }
    }
    emit updateCheckFinished();
}

QString ProgramUpdater::newVersion() const
{
    return m_newVersion;
}

bool ProgramUpdater::checkFailed() const
{
    return m_checkFailed;
}

QString ProgramUpdater::errorString() const
{
    return m_errorString;
}

QString ProgramUpdater::releaseNotes() const
{
    // parts marked "[lang=fr]" / "[lang=en]" on their own line: the interface language, else English,
    // else the whole text; light markdown ("**", "#", "- ") removed for reading
    const QString wanted = Preferences::instance()->getLocale().startsWith(u"fr") ? u"fr"_s : u"en"_s;
    QHash<QString, QStringList> parts;
    QStringList unmarked;
    QString current;
    static const QRegularExpression marker {u"^\\s*\\[lang=(\\w+)\\]\\s*$"_s};
    for (const QString &line : m_releaseBody.split(u'\n'))
    {
        if (const QRegularExpressionMatch match = marker.match(line); match.hasMatch())
        {
            current = match.captured(1).toLower();
            continue;
        }
        (current.isEmpty() ? unmarked : parts[current]).append(line);
    }

    QString text = parts.value(wanted).join(u'\n').trimmed();
    if (text.isEmpty())
        text = parts.value(u"en"_s).join(u'\n').trimmed();
    if (text.isEmpty())
        text = m_releaseBody.trimmed();

    text.remove(u"**"_s).remove(u'\r');
    static const QRegularExpression heading {u"^#+\\s*"_s, QRegularExpression::MultilineOption};
    text.remove(heading);
    return text;
}

void ProgramUpdater::downloadInstaller()
{
    m_downloadReply = m_network->get(makeRequest(m_installerUrl));
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, &ProgramUpdater::downloadProgress);
    QNetworkReply *reply = m_downloadReply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] { installerReceived(reply); });
}

void ProgramUpdater::cancelDownload()
{
    if (m_downloadReply)
        m_downloadReply->abort();
}

void ProgramUpdater::installerReceived(QNetworkReply *reply)
{
    reply->deleteLater();
    m_downloadReply = nullptr;

    if (reply->error() == QNetworkReply::OperationCanceledError)
        return;
    if (reply->error() != QNetworkReply::NoError)
    {
        emit installerFailed(reply->errorString());
        return;
    }

    // a connection cut short must not give a broken installer: complete size, and a real program ("MZ")
    const QByteArray data = reply->readAll();
    if (((m_installerSize > 0) && (data.size() != m_installerSize)) || !data.startsWith("MZ"))
    {
        emit installerFailed(tr("The download is incomplete."));
        return;
    }

    m_installerPath = QDir::temp().filePath(u"qBittorrentAccessInstaller_%1.exe"_s.arg(m_newVersion));
    QFile file {m_installerPath};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || (file.write(data) != data.size()))
    {
        emit installerFailed(tr("The installer could not be saved: %1").arg(QDir::toNativeSeparators(m_installerPath)));
        return;
    }
    file.close();
    emit installerReady();
}

bool ProgramUpdater::runInstaller() const
{
#ifdef Q_OS_WIN
    // the installer asks for administrator rights (Windows prompt), closes qBittorrentAccess if it still
    // runs, installs without questions and starts qBittorrentAccess again
    const std::wstring path = QDir::toNativeSeparators(m_installerPath).toStdWString();
    const auto result = reinterpret_cast<INT_PTR>(::ShellExecuteW(nullptr, L"open", path.c_str()
        , L"/SILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS", nullptr, SW_SHOWNORMAL));
    return (result > 32);
#else
    return false;
#endif
}

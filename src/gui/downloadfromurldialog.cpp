/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "downloadfromurldialog.h"

#include <algorithm>

#include <QBoxLayout>
#include <QClipboard>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QTimer>

#include "access/accessibility.h"
#include "base/net/downloadmanager.h"
#include "ui_downloadfromurldialog.h"
#include "utils.h"

#define SETTINGS_KEY(name) u"DownloadFromURLDialog/" name

namespace
{
    bool isDownloadable(const QString &str)
    {
        return (Net::DownloadManager::hasSupportedScheme(str)
            || str.startsWith(u"magnet:", Qt::CaseInsensitive)
#ifdef QBT_USES_LIBTORRENT2
            || ((str.size() == 64) && !str.contains(QRegularExpression(u"[^0-9A-Fa-f]"_s))) // v2 hex-encoded SHA-256 info-hash
#endif
            || ((str.size() == 40) && !str.contains(QRegularExpression(u"[^0-9A-Fa-f]"_s))) // v1 hex-encoded SHA-1 info-hash
            || ((str.size() == 32) && !str.contains(QRegularExpression(u"[^2-7A-Za-z]"_s)))); // v1 Base32 encoded SHA-1 info-hash
    }
}

DownloadFromURLDialog::DownloadFromURLDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::DownloadFromURLDialog)
    , m_storeDialogSize(SETTINGS_KEY(u"Size"_s))
{
    m_ui->setupUi(this);

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Download"));
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &DownloadFromURLDialog::onSubmit);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_ui->textUrls->setWordWrapMode(QTextOption::NoWrap);

    // Paste clipboard if there is an URL in it
    const QString clipboardText = qApp->clipboard()->text();
    const QList<QStringView> clipboardList = QStringView(clipboardText).split(u'\n');

    // show urls in the original order as from the clipboard
    QStringList urls;
    urls.reserve(clipboardList.size());

    for (QStringView url : clipboardList)
    {
        url = url.trimmed();

        if (url.isEmpty())
            continue;

        if (const QString urlString = url.toString(); isDownloadable(urlString))
            urls << urlString;
    }

    if (!urls.isEmpty())
    {
        m_ui->textUrls->setText(urls.join(u'\n') + u"\n");
        m_ui->textUrls->moveCursor(QTextCursor::End);
        m_ui->buttonBox->setFocus();
        // accessibility: the focus lands on "Download"; say that the links are already there
        QTimer::singleShot(300, this, [this, count = urls.size()]
        {
            Access::announce(this, ((count == 1) ? tr("1 link pasted") : tr("%1 links pasted").arg(count)));
        });
    }

    // accessibility: the links field is named after its label, the hint below is its description
    m_ui->textUrls->setAccessibleName(Access::cleanLabel(m_ui->downloadURL_lbl->text()));
    m_ui->textUrls->setAccessibleDescription(m_ui->label_infos->text());

    // accessibility: long links cannot be reviewed in a multi-line field; a read-only "Links" list, next
    // Tab stop, shows each line (lines that are not links are marked), and a paste says how many links
    auto *linkList = new Access::ReadOnlyList(this);
    linkList->setAccessibleName(tr("Links"));
    if (auto *box = qobject_cast<QBoxLayout *>(layout()))
        box->insertWidget((box->indexOf(m_ui->label_infos) + 1), linkList);
    setTabOrder(m_ui->textUrls, linkList);
    const auto lines = [this]
    {
        QStringList result;
        for (const QString &line : m_ui->textUrls->toPlainText().split(u'\n'))
        {
            if (const QString url = line.trimmed(); !url.isEmpty())
                result.append(url);
        }
        return result;
    };
    const auto refreshLinks = [linkList, lines]
    {
        QStringList rows;
        for (const QString &url : lines())
            rows.append(isDownloadable(url) ? url : tr("Not a link: %1").arg(url));
        linkList->setRows(rows);
    };
    connect(m_ui->textUrls, &QTextEdit::textChanged, linkList, refreshLinks);
    refreshLinks();
    Access::onKeyboardPaste(m_ui->textUrls, [this, lines]
    {
        const QStringList urls = lines();
        const qsizetype count = std::count_if(urls.cbegin(), urls.cend(), [](const QString &url) { return isDownloadable(url); });
        const QString text = (count == 0) ? tr("No link") : ((count == 1) ? tr("1 link") : tr("%1 links").arg(count));
        Access::announce(m_ui->textUrls, text);
    });

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
}

DownloadFromURLDialog::~DownloadFromURLDialog()
{
    m_storeDialogSize = size();
    delete m_ui;
}

void DownloadFromURLDialog::onSubmit()
{
    const QString plainText = m_ui->textUrls->toPlainText();
    const QList<QStringView> urls = QStringView(plainText).split(u'\n');

    // process urls in the original order as from the widget
    QSet<QStringView> uniqueURLs;
    QStringList urlList;
    urlList.reserve(urls.size());

    for (QStringView url : urls)
    {
        url = url.trimmed();

        if (url.isEmpty())
            continue;

        if (!uniqueURLs.contains(url))
        {
            uniqueURLs.insert(url);
            urlList << url.toString();
        }
    }

    if (urlList.isEmpty())
    {
        QMessageBox::warning(this, tr("No URL entered"), tr("Please type at least one URL."));
        return;
    }

    emit urlsReadyToBeDownloaded(urlList);
    accept();
}

void DownloadFromURLDialog::keyPressEvent(QKeyEvent *event)
{
    if ((event->modifiers() == Qt::ControlModifier) && (event->key() == Qt::Key_Return))
    {
        onSubmit();
        return;
    }

    QDialog::keyPressEvent(event);
}

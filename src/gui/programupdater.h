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

#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// qBittorrentAccess: the program updates itself from its own GitHub releases (never from qBittorrent's
// servers). The latest release is compared with this version; the installer ("qBittorrentAccessInstaller.exe"
// asset) is downloaded, checked, then run silently: it closes the program and starts it again afterwards.
class ProgramUpdater final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ProgramUpdater)

public:
    explicit ProgramUpdater(QObject *parent = nullptr);

    void checkForUpdates();

    // after updateCheckFinished(): the newer version ("1.01"), empty when this one is the latest
    QString newVersion() const;
    bool checkFailed() const;
    QString errorString() const;
    // the release notes in the interface language (the release text has "[lang=fr]" / "[lang=en]" parts)
    QString releaseNotes() const;

    // downloads the installer: downloadProgress(), then installerReady() or installerFailed()
    void downloadInstaller();
    void cancelDownload();
    // runs the downloaded installer silently; false if it could not start
    bool runInstaller() const;

signals:
    void updateCheckFinished();
    void downloadProgress(qint64 received, qint64 total);
    void installerReady();
    void installerFailed(const QString &reason);

private:
    void releaseInfoReceived(QNetworkReply *reply);
    void installerReceived(QNetworkReply *reply);

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QString m_newVersion;
    bool m_checkFailed = false;
    QString m_errorString;
    QString m_releaseBody;
    QString m_installerUrl;
    qint64 m_installerSize = -1;
    QString m_installerPath;
};

/*
 * qBittorrentAccess, based on qBittorrent (Bittorrent Client using Qt and libtorrent).
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
 */

#include "profileimport.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include "base/global.h"

namespace
{
    const QString QBITTORRENT_NAME = u"qBittorrent"_s;

    QString withSuffix(const QString &path, const QString &configurationName)
    {
        return configurationName.isEmpty() ? path : (path + u'_' + configurationName);
    }

    // the same folder, under qBittorrent's name
    QString qbittorrentSibling(const QString &path)
    {
        const QFileInfo info {path};
        return info.dir().filePath(QBITTORRENT_NAME + info.fileName().mid(QCoreApplication::applicationName().size()));
    }

    bool copyTree(const QString &source, const QString &target)
    {
        if (!QDir().mkpath(target))
            return false;

        bool ok = true;
        QDirIterator it {source, (QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)
            , QDirIterator::Subdirectories};
        const QDir sourceDir {source};
        while (it.hasNext())
        {
            const QFileInfo info = it.nextFileInfo();
            const QString destination = QDir(target).filePath(sourceDir.relativeFilePath(info.filePath()));
            if (info.isDir())
                ok = QDir().mkpath(destination) && ok;
            else
                ok = QFile::copy(info.filePath(), destination) && ok;
        }
        return ok;
    }

    // settings files keep absolute paths into the profile (logs, resume data...): point them at the copy
    void retargetPaths(const QString &configDir, const QList<std::pair<QString, QString>> &moves)
    {
        QDirIterator it {configDir, {u"*.ini"_s}, QDir::Files, QDirIterator::Subdirectories};
        while (it.hasNext())
        {
            QFile file {it.next()};
            if (!file.open(QIODevice::ReadOnly))
                continue;
            QString text = QString::fromUtf8(file.readAll());
            file.close();

            const QString original = text;
            for (const auto &[from, to] : moves)
            {
                text.replace(QDir::fromNativeSeparators(from), QDir::fromNativeSeparators(to), Qt::CaseInsensitive);
                QString escapedFrom = QDir::toNativeSeparators(from);
                QString escapedTo = QDir::toNativeSeparators(to);
                text.replace(escapedFrom.replace(u'\\', u"\\\\"_s), escapedTo.replace(u'\\', u"\\\\"_s), Qt::CaseInsensitive);
            }
            if ((text != original) && file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(text.toUtf8());
        }
    }
}

QString ProfileImport::importFromQBittorrent(const QString &configurationName)
{
    const QString configDir = withSuffix(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation), configurationName);
    const QString dataDir = withSuffix(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation), configurationName);
    if (configDir.isEmpty() || dataDir.isEmpty() || QFileInfo::exists(configDir) || QFileInfo::exists(dataDir))
        return {}; // qBittorrentAccess already has its own profile

    const QString oldConfigDir = qbittorrentSibling(configDir);
    const QString oldDataDir = qbittorrentSibling(dataDir);
    if (!QFileInfo(oldConfigDir).isDir())
        return {}; // no qBittorrent installation to import

    if (!copyTree(oldConfigDir, configDir))
        return {};
    if (QFileInfo(oldDataDir).isDir())
        copyTree(oldDataDir, dataDir);

    retargetPaths(configDir, {{oldConfigDir, configDir}, {oldDataDir, dataDir}});
    return oldConfigDir;
}

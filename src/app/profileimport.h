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

#pragma once

#include <QString>

namespace ProfileImport
{
    // First launch of qBittorrentAccess with the default profile: copies (never moves) the
    // settings and torrents of an existing qBittorrent installation, so the user finds them
    // again. Does nothing once qBittorrentAccess has its own profile, or without qBittorrent.
    // Must run after the application name is set and before the profile is opened.
    // Returns the qBittorrent folder imported from, or an empty string.
    QString importFromQBittorrent(const QString &configurationName);
}

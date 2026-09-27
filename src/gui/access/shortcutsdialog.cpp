/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrentAccess
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

#include "shortcutsdialog.h"

#include <QDialogButtonBox>
#include <QKeySequence>
#include <QVBoxLayout>

#include "base/global.h"
#include "accessibility.h"

Access::ShortcutsDialog::ShortcutsDialog(QWidget *parent)
    : QDialog(parent)
{
    const QString title = tr("Keyboard shortcuts");
    setWindowTitle(title);
    setAttribute(Qt::WA_DeleteOnClose);

    const QList<std::pair<QString, QString>> shortcuts =
    {
        {u"Ctrl+1"_s, tr("Torrent list: all torrents")},
        {u"Ctrl+2"_s, tr("Torrent list: downloading")},
        {u"Ctrl+3"_s, tr("Torrent list: completed")},
        {u"Ctrl+4"_s, tr("Torrent list: seeding")},
        {u"Ctrl+5"_s, tr("Torrent list: errors")},
        {u"Ctrl+6"_s, tr("Search")},
        {u"Ctrl+7"_s, tr("RSS")},
        {u"Ctrl+8"_s, tr("Execution log")},
        {u"Ctrl+Shift+G"_s, tr("Global status: speeds, connection, DHT")},
        {u"Ctrl+Shift+H"_s, tr("Keyboard shortcuts")},
        {u"F6, Shift+F6"_s, tr("Next, previous zone: torrent list, properties, filters, filter field")},
        {u"Ctrl+Shift+1"_s, tr("Properties: General")},
        {u"Ctrl+Shift+2"_s, tr("Properties: Trackers")},
        {u"Ctrl+Shift+3"_s, tr("Properties: Peers")},
        {u"Ctrl+Shift+4"_s, tr("Properties: HTTP sources")},
        {u"Ctrl+Shift+5"_s, tr("Properties: Content, the files")},
        {u"Ctrl+Shift+6"_s, tr("Properties: Speed")},
        {u"Ctrl+C"_s, tr("Copy the value, in the General list")},
        {u"Ctrl+O"_s, tr("Open torrent file")},
        {u"Ctrl+Shift+O"_s, tr("Add torrent link, magnet")},
        {u"Ctrl+N"_s, tr("Create new torrent")},
        {u"Ctrl+S"_s, tr("Start selected torrents")},
        {u"Ctrl+P"_s, tr("Stop selected torrents")},
        {u"Ctrl+Shift+S"_s, tr("Resume session")},
        {u"Ctrl+Shift+P"_s, tr("Pause session")},
        {u"Ctrl+R"_s, tr("Force recheck, in the torrent list")},
        {u"Ctrl+M"_s, tr("Force start, in the torrent list")},
        {u"Enter"_s, tr("Double-click action, in the torrent list")},
        {u"Left, Right"_s, tr("Read one column of the torrent; Up and Down then stay in it, in the torrent list")},
        {u"Home, End"_s, tr("Name column, last column, in the torrent list")},
        {u"Ctrl+Home, Ctrl+End"_s, tr("First torrent, last torrent, in the torrent list")},
        {u"Space"_s, tr("Start or stop the torrent, in the torrent list")},
        {u"Ctrl+Enter"_s, tr("Open destination folder, in the torrent list")},
        {u"Ctrl+Shift+C"_s, tr("Copy magnet link, in the torrent list")},
        {u"F2"_s, tr("Rename, in the torrent list")},
        {u"Applications"_s, tr("Context menu of the selected item")},
        {u"Right, Left"_s, tr("Open, close a submenu, in menus")},
        {u"Del"_s, tr("Remove torrent")},
        {u"Shift+Del"_s, tr("Remove torrent and its files")},
        {u"Ctrl+F"_s, tr("Filter torrents")},
        {u"Ctrl++"_s, tr("Move up in queue")},
        {u"Ctrl+-"_s, tr("Move down in queue")},
        {u"Ctrl+Shift++"_s, tr("Move to top of queue")},
        {u"Ctrl+Shift+-"_s, tr("Move to bottom of queue")},
        {u"Ctrl+I"_s, tr("Statistics")},
        {u"Ctrl+,"_s, tr("Options")},
        {u"F1"_s, tr("Documentation")},
        {u"Ctrl+Q"_s, tr("Exit")}
    };

    auto *list = new ReadOnlyList(this);
    list->setAccessibleName(title); // short and fixed: the braille line keeps the current row visible
    // keys written by Qt in the interface language ("Ctrl+Maj+O", "Suppr", "Entrée"...); the table above
    // uses Qt's portable names, several sequences separated by ", "
    const auto keysText = [](const QString &portable)
    {
        QStringList parts;
        for (const QString &sequence : portable.split(u", "_s))
        {
            // a key Qt has no name for (Applications) is written as is
            const QString native = QKeySequence(sequence, QKeySequence::PortableText).toString(QKeySequence::NativeText);
            parts.append(native.isEmpty() ? sequence : native);
        }
        return parts.join(u", "_s);
    };
    for (const auto &[keys, action] : shortcuts)
        list->addItem(keysText(keys) + u" : " + action);
    list->setCurrentRow(0);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(list);
    layout->addWidget(buttons);

    resize(480, 520);
    list->setFocus(Qt::OtherFocusReason);
}

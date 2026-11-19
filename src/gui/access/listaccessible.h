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

#pragma once

class QTreeView;

namespace Access
{
    // Present a flat QTreeView (one level of rows, no folders) to screen readers as a list: role List,
    // one list item per row (not one per cell), named by the row's Qt::AccessibleTextRole (else its
    // text). Screen readers then say "1 of 12" instead of "level 1" on every row of a tree.
    // Call it in the view's constructor, before anything asks for its accessible interface.
    void presentAsList(QTreeView *view);

    // Tell the screen reader that the name of the current item of `view` changed (works for a view
    // presented as a list and for an ordinary tree)
    void notifyCurrentItemNameChanged(QTreeView *view);
}

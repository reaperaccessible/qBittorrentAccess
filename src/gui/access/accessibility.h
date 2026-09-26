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

class QAbstractItemView;
class QKeyEvent;
class QObject;
class QString;
class QWidget;

// Screen reader helpers shared by the accessibility work (qBittorrentAccess)
namespace Access
{
    // Spoken by NVDA, JAWS and Narrator through the platform accessibility API (Qt 6.8+)
    void announce(QObject *source, const QString &text);

    // Give the focus to the first widget of `page` reachable with Tab, as if the user tabbed into it
    void focusFirstChild(QWidget *page);

    // Give the focus to the child of `page` whose objectName is `objectName` (the main control of a tab),
    // or to its first Tab stop when that child is not found or cannot take the focus
    void focusNamedChild(QWidget *page, const QString &objectName);

    // Label text as a screen reader should say it: no '&' mnemonic, no HTML, no trailing ':'
    QString cleanLabel(const QString &text);

    // Name an input control; for a path editor (field + "..." button) the field gets `name`
    // and the button "Browse"
    void setControlName(QWidget *control, const QString &name);

    // Dialog pass: every input control (spin box, combo box, line edit, path editor, list, time edit)
    // without an explicit accessible name is named after the label on its left in the same layout row
    // (or the check box that enables it), and the label becomes its buddy. Without this, Qt names such
    // a control after its group box title, so 4 fields of one group all say the group title.
    // Scroll areas stop being nameless Tab stops.
    void labelControls(QWidget *root);

    // Up on the first row / Down on the last row: announce "First, <row>" / "Last, <row>"
    // instead of staying silent. Returns true when the key was handled.
    bool announceListEdge(QAbstractItemView *view, const QKeyEvent *event);
}

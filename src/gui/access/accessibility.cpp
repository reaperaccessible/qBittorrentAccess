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

#include "accessibility.h"

#include <QAbstractItemView>
#include <QAccessible>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QWidget>

#include "base/global.h"

void Access::announce(QObject *source, const QString &text)
{
    if (!source || text.isEmpty() || !QAccessible::isActive())
        return;

    QAccessibleAnnouncementEvent event {source, text};
    event.setPoliteness(QAccessible::AnnouncementPoliteness::Assertive);
    QAccessible::updateAccessibility(&event);
}

void Access::focusFirstChild(QWidget *page)
{
    if (!page)
        return;

    for (QWidget *w = page->nextInFocusChain(); w && (w != page); w = w->nextInFocusChain())
    {
        if (!page->isAncestorOf(w))
            continue;
        if ((w->focusPolicy() & Qt::TabFocus) && w->isVisible() && w->isEnabled())
        {
            w->setFocus(Qt::TabFocusReason);
            return;
        }
    }
    page->setFocus(Qt::TabFocusReason);
}

void Access::focusNamedChild(QWidget *page, const QString &objectName)
{
    if (!page)
        return;

    auto *child = page->findChild<QWidget *>(objectName);
    if (child && child->isVisible() && child->isEnabled() && (child->focusPolicy() != Qt::NoFocus))
    {
        child->setFocus(Qt::TabFocusReason);
        return;
    }
    focusFirstChild(child ? child : page);
}

bool Access::announceListEdge(QAbstractItemView *view, const QKeyEvent *event)
{
    if (!view || !view->model() || (event->modifiers() != Qt::NoModifier))
        return false;

    const bool up = (event->key() == Qt::Key_Up);
    const bool down = (event->key() == Qt::Key_Down);
    if (!up && !down)
        return false;

    const int rows = view->model()->rowCount(view->rootIndex());
    const QModelIndex current = view->currentIndex();
    if ((rows <= 0) || !current.isValid())
        return false;

    if ((up && (current.row() == 0)) || (down && (current.row() == (rows - 1))))
    {
        QString text = current.data(Qt::AccessibleTextRole).toString();
        if (text.isEmpty())
            text = current.data(Qt::DisplayRole).toString();
        //: Screen reader announcement when the first row of a list is already reached
        const QString edge = up ? QCoreApplication::translate("Access", "First") : QCoreApplication::translate("Access", "Last");
        announce(view, (edge + u", " + text));
        return true;
    }
    return false;
}

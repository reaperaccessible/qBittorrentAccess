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

#include "listaccessible.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QHash>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QPointer>
#include <QTreeView>
#include <QWidget>
#include <QWindow>

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformaccessibility.h>
#include <QtGui/qpa/qplatformintegration.h>

// Qt presents every QTreeView as a tree of cells (role TreeItem), and NVDA then says "level 1" on each
// row of a flat list. Qt's own list classes are private to Qt, so this is a small list of our own:
// the view is a List, each row one ListItem. Qt's tree view keeps sending its events numbered as
// tree cells ((row + header) * columns + column); an update handler renumbers them as rows.

namespace
{
    const char FLAT_LIST_PROPERTY[] = "qbtAccessPresentAsList";

    bool isPresentedAsList(const QObject *object)
    {
        return object && object->property(FLAT_LIST_PROPERTY).toBool();
    }

    int modelColumnCount(const QTreeView *view)
    {
        return view->model() ? view->model()->columnCount(view->rootIndex()) : 0;
    }

    int modelRowCount(const QTreeView *view)
    {
        return view->model() ? view->model()->rowCount(view->rootIndex()) : 0;
    }

    // the cell number Qt's tree view uses in its accessibility events
    int treeCellNumber(const QTreeView *view, const int visualRow, const int column)
    {
        return ((visualRow + (view->header() ? 1 : 0)) * modelColumnCount(view)) + column;
    }

    class ListRowAccessible final : public QAccessibleInterface
    {
    public:
        ListRowAccessible(QTreeView *view, const int row)
            : m_view {view}
            , m_row {row}
        {
        }

        bool isValid() const override
        {
            return m_view && m_view->model() && (m_row < modelRowCount(m_view));
        }

        QObject *object() const override
        {
            return nullptr;
        }

        QWindow *window() const override
        {
            return m_view ? m_view->window()->windowHandle() : nullptr;
        }

        QAccessibleInterface *parent() const override
        {
            return m_view ? QAccessible::queryAccessibleInterface(m_view) : nullptr;
        }

        QAccessibleInterface *child(int) const override
        {
            return nullptr;
        }

        int childCount() const override
        {
            return 0;
        }

        int indexOfChild(const QAccessibleInterface *) const override
        {
            return -1;
        }

        QAccessibleInterface *childAt(int, int) const override
        {
            return nullptr;
        }

        QAccessibleInterface *focusChild() const override
        {
            return nullptr;
        }

        QString text(const QAccessible::Text t) const override
        {
            if (!isValid())
                return {};
            const QModelIndex index = rowIndex();
            switch (t)
            {
            case QAccessible::Name:
                {
                    const QString text = index.data(Qt::AccessibleTextRole).toString();
                    return text.isEmpty() ? index.data(Qt::DisplayRole).toString() : text;
                }
            case QAccessible::Description:
                return index.data(Qt::AccessibleDescriptionRole).toString();
            default:
                return {};
            }
        }

        void setText(QAccessible::Text, const QString &) override
        {
        }

        QRect rect() const override
        {
            if (!isValid())
                return {};
            QRect rowRect = m_view->visualRect(rowIndex());
            if (rowRect.isEmpty())
                return {};
            rowRect.setLeft(0);
            rowRect.setWidth(m_view->viewport()->width());
            return {m_view->viewport()->mapToGlobal(rowRect.topLeft()), rowRect.size()};
        }

        QAccessible::Role role() const override
        {
            return QAccessible::ListItem;
        }

        QAccessible::State state() const override
        {
            QAccessible::State st;
            if (!isValid())
            {
                st.invalid = true;
                return st;
            }
            st.focusable = true;
            st.selectable = true;
            if (m_view->selectionMode() == QAbstractItemView::ExtendedSelection)
                st.extSelectable = true;
            else if (m_view->selectionMode() == QAbstractItemView::MultiSelection)
                st.multiSelectable = true;
            if (const QItemSelectionModel *selection = m_view->selectionModel()
                ; selection && selection->isRowSelected(m_row, m_view->rootIndex()))
            {
                st.selected = true;
            }
            const QModelIndex current = m_view->currentIndex();
            if (m_view->hasFocus() && current.isValid() && (current.row() == m_row))
                st.focused = true;
            const QRect rowRect = m_view->visualRect(rowIndex());
            if (rowRect.isEmpty() || !m_view->viewport()->rect().intersects(rowRect))
            {
                st.invisible = true;
                st.offscreen = true;
            }
            return st;
        }

        int row() const
        {
            return m_row;
        }

        const QTreeView *view() const
        {
            return m_view;
        }

    private:
        QModelIndex rowIndex() const
        {
            // the column of the current item when it is on this row (its text may name that column),
            // else the first column shown on screen
            const QModelIndex current = m_view->currentIndex();
            if (current.isValid() && (current.row() == m_row))
                return current;
            const QHeaderView *header = m_view->header();
            int column = 0;
            for (int visual = 0, count = header->count(); visual < count; ++visual)
            {
                if (const int logical = header->logicalIndex(visual); !m_view->isColumnHidden(logical))
                {
                    column = logical;
                    break;
                }
            }
            return m_view->model()->index(m_row, column, m_view->rootIndex());
        }

        QPointer<QTreeView> m_view;
        const int m_row;
    };

    class ListAccessible final : public QAccessibleWidget
    {
    public:
        explicit ListAccessible(QTreeView *view)
            : QAccessibleWidget(view, QAccessible::List)
        {
        }

        ~ListAccessible() override
        {
            for (const QAccessible::Id id : std::as_const(m_rows))
                QAccessible::deleteAccessibleInterface(id);
        }

        QTreeView *view() const
        {
            return static_cast<QTreeView *>(widget());
        }

        int childCount() const override
        {
            return modelRowCount(view());
        }

        QAccessibleInterface *child(const int index) const override
        {
            if ((index < 0) || (index >= childCount()))
                return nullptr;
            if (const auto it = m_rows.constFind(index); it != m_rows.constEnd())
                return QAccessible::accessibleInterface(it.value());
            auto *row = new ListRowAccessible(view(), index);
            m_rows.insert(index, QAccessible::registerAccessibleInterface(row));
            return row;
        }

        int indexOfChild(const QAccessibleInterface *child) const override
        {
            if (!child || (child->role() != QAccessible::ListItem))
                return -1;
            const auto *row = static_cast<const ListRowAccessible *>(child);
            if ((row->view() != view()) || (m_rows.value(row->row()) != QAccessible::uniqueId(const_cast<QAccessibleInterface *>(child))))
                return -1;
            return row->row();
        }

        QAccessibleInterface *focusChild() const override
        {
            const QModelIndex current = view()->currentIndex();
            if (!view()->hasFocus() || !current.isValid())
                return nullptr;
            return child(current.row());
        }

        QAccessibleInterface *childAt(const int x, const int y) const override
        {
            const QPoint position = view()->viewport()->mapFromGlobal(QPoint(x, y));
            if (const QModelIndex index = view()->indexAt(position); index.isValid())
                return child(index.row());
            return nullptr;
        }

        QAccessible::State state() const override
        {
            QAccessible::State st = QAccessibleWidget::state();
            if (view()->selectionMode() == QAbstractItemView::ExtendedSelection)
                st.extSelectable = true;
            else if (view()->selectionMode() == QAbstractItemView::MultiSelection)
                st.multiSelectable = true;
            return st;
        }

    private:
        mutable QHash<int, QAccessible::Id> m_rows;
    };

    QAccessibleInterface *listFactory(const QString &className, QObject *object)
    {
        // asked once per class of the object, most derived first: answer on the first one
        if (!isPresentedAsList(object) || (className != QLatin1String(object->metaObject()->className())))
            return nullptr;
        if (auto *view = qobject_cast<QTreeView *>(object))
            return new ListAccessible(view);
        return nullptr;
    }

    QAccessible::UpdateHandler previousUpdateHandler = nullptr;

    void listUpdateHandler(QAccessibleEvent *event)
    {
        QObject *object = event->object();
        if (isPresentedAsList(object) && (event->type() != QAccessible::TableModelChanged) && (event->child() >= 0))
        {
            // the tree view numbers cells: (row + header) * columns + column; the list numbers rows
            const auto *view = static_cast<const QTreeView *>(object);
            const int columns = modelColumnCount(view);
            if (columns <= 0)
                return;
            const int row = (event->child() / columns) - (view->header() ? 1 : 0);
            if (row < 0)
                return; // a header cell: not part of the list
            event->setChild(row);
        }

        if (previousUpdateHandler)
        {
            previousUpdateHandler(event);
            return;
        }
        if (QPlatformIntegration *integration = QGuiApplicationPrivate::platformIntegration())
        {
            if (QPlatformAccessibility *accessibility = integration->accessibility())
                accessibility->notifyAccessibilityUpdate(event);
        }
    }

    void installOnce()
    {
        static bool installed = false;
        if (installed)
            return;
        installed = true;
        QAccessible::installFactory(listFactory);
        previousUpdateHandler = QAccessible::installUpdateHandler(listUpdateHandler);
    }
}

void Access::presentAsList(QTreeView *view)
{
    installOnce();
    view->setProperty(FLAT_LIST_PROPERTY, true);

    // an interface asked for before this call is the tree one: replace it
    if (QAccessible::isActive())
    {
        if (QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(view); iface && (iface->role() != QAccessible::List))
        {
            QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(iface));
            QAccessible::queryAccessibleInterface(view);
        }
    }
}

void Access::notifyCurrentItemNameChanged(QTreeView *view)
{
    const QModelIndex current = view->currentIndex();
    if (!current.isValid() || !QAccessible::isActive())
        return;
    // numbered as Qt's tree view numbers its cells (rows shown on screen: in a tree, a file inside a
    // folder is not at its model row); a view presented as a list renumbers it as a row
    int visualRow = 0;
    for (QModelIndex above = view->indexAbove(current); above.isValid(); above = view->indexAbove(above))
        ++visualRow;
    QAccessibleEvent event {view, QAccessible::NameChanged};
    event.setChild(treeCellNumber(view, visualRow, current.column()));
    QAccessible::updateAccessibility(&event);
}

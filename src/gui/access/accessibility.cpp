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
#include <QAbstractSpinBox>
#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QClipboard>
#include <QHash>
#include <QHeaderView>
#include <QPointer>
#include <QTreeView>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QScrollArea>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

#include "base/global.h"
#include "gui/fspathedit.h"

namespace
{
    bool isInput(const QWidget *w)
    {
        if (qobject_cast<const QAbstractSpinBox *>(w) || qobject_cast<const QComboBox *>(w)
            || qobject_cast<const QAbstractItemView *>(w) || qobject_cast<const QPlainTextEdit *>(w)
            || qobject_cast<const QTextEdit *>(w) || qobject_cast<const QKeySequenceEdit *>(w)
            || qobject_cast<const FileSystemPathEdit *>(w))
        {
            return true;
        }
        if (qobject_cast<const QLineEdit *>(w))
        {
            // the inner line edit of a spin box, combo box or path editor is not a control of its own
            const QWidget *p = w->parentWidget();
            return !(qobject_cast<const QAbstractSpinBox *>(p) || qobject_cast<const QComboBox *>(p)
                || qobject_cast<const FileSystemPathEdit *>(p));
        }
        return false;
    }

    // A widget whose text can name the control next to it
    QString labelText(const QWidget *w)
    {
        if (!w || w->isHidden())
            return {};
        if (const auto *label = qobject_cast<const QLabel *>(w))
            return Access::cleanLabel(label->text());
        if (qobject_cast<const QCheckBox *>(w) || qobject_cast<const QRadioButton *>(w))
            return Access::cleanLabel(static_cast<const QAbstractButton *>(w)->text());
        return {};
    }

    // Rightmost label-like widget of a layout, for a label nested in a sub-layout of a grid cell
    QWidget *lastLabelIn(QLayout *layout)
    {
        for (int i = layout->count() - 1; i >= 0; --i)
        {
            QLayoutItem *item = layout->itemAt(i);
            if (QWidget *w = item->widget(); w && !labelText(w).isEmpty())
                return w;
        }
        return nullptr;
    }

    struct Slot
    {
        QLayout *layout = nullptr;
        int index = -1;
    };

    Slot findSlot(QLayout *root, const QObject *target)
    {
        for (int i = 0; i < root->count(); ++i)
        {
            QLayoutItem *item = root->itemAt(i);
            if ((item->widget() && (item->widget() == target)) || (item->layout() && (item->layout() == target)))
                return {root, i};
            if (QLayout *sub = item->layout())
            {
                if (const Slot slot = findSlot(sub, target); slot.layout)
                    return slot;
            }
        }
        return {};
    }

    // Label on the left of `slot` in the same row; `right` receives the first label on its right
    QWidget *labelBeside(const Slot &slot, QWidget *&right)
    {
        const auto check = [](QLayoutItem *item, QWidget *&found) -> bool // true = stop scanning
        {
            if (!item)
                return false;
            if (QWidget *w = item->widget())
            {
                if (!labelText(w).isEmpty())
                {
                    found = w;
                    return true;
                }
                return isInput(w) && !w->isHidden(); // another control in between: its label is not ours
            }
            if (QLayout *sub = item->layout())
            {
                if (QWidget *w = lastLabelIn(sub))
                {
                    found = w;
                    return true;
                }
            }
            return false;
        };

        QWidget *left = nullptr;
        if (auto *form = qobject_cast<QFormLayout *>(slot.layout))
        {
            int row = -1;
            QFormLayout::ItemRole role {};
            form->getItemPosition(slot.index, &row, &role);
            if (role == QFormLayout::FieldRole)
                check(form->itemAt(row, QFormLayout::LabelRole), left);
        }
        else if (auto *grid = qobject_cast<QGridLayout *>(slot.layout))
        {
            int row = 0, column = 0, rowSpan = 0, columnSpan = 0;
            grid->getItemPosition(slot.index, &row, &column, &rowSpan, &columnSpan);
            for (int c = column - 1; c >= 0; --c)
            {
                if (check(grid->itemAtPosition(row, c), left))
                    break;
            }
            for (int c = column + columnSpan; !right && (c < grid->columnCount()); ++c)
            {
                QWidget *found = nullptr;
                if (check(grid->itemAtPosition(row, c), found))
                {
                    right = found;
                    break;
                }
            }
        }
        else if (auto *box = qobject_cast<QBoxLayout *>(slot.layout))
        {
            const bool horizontal = ((box->direction() == QBoxLayout::LeftToRight) || (box->direction() == QBoxLayout::RightToLeft));
            if (horizontal)
            {
                for (int i = slot.index - 1; i >= 0; --i)
                {
                    if (check(box->itemAt(i), left))
                        break;
                }
                for (int i = slot.index + 1; !right && (i < box->count()); ++i)
                {
                    QWidget *found = nullptr;
                    if (check(box->itemAt(i), found))
                    {
                        right = found;
                        break;
                    }
                }
            }
            else if (slot.index > 0)
            {
                // label above the field
                if (QWidget *w = box->itemAt(slot.index - 1)->widget(); qobject_cast<QLabel *>(w) && !labelText(w).isEmpty())
                    left = w;
            }
        }
        if (left && (left->isHidden() || labelText(left).isEmpty()))
            left = nullptr;
        return (left && isInput(left)) ? nullptr : left;
    }

    QWidget *findLabel(QWidget *control)
    {
        QWidget *parent = control->parentWidget();
        QLayout *top = parent ? parent->layout() : nullptr;
        if (!top)
            return nullptr;

        QWidget *firstRight = nullptr;
        const QObject *target = control;
        for (int depth = 0; depth < 8; ++depth)
        {
            const Slot slot = findSlot(top, target);
            if (!slot.layout)
                break;
            QWidget *right = nullptr;
            if (QWidget *left = labelBeside(slot, right))
                return left;
            if (!firstRight)
                firstRight = right;
            if (slot.layout == top)
                break;
            target = slot.layout; // the row may be a sub-layout inside a grid or form cell: look around it
        }
        return firstRight;
    }

    QWidget *pathEditor(const QWidget *pathEdit)
    {
        if (auto *line = pathEdit->findChild<QLineEdit *>(Qt::FindDirectChildrenOnly))
            return line;
        return pathEdit->findChild<QComboBox *>(Qt::FindDirectChildrenOnly);
    }

    // The control right before `control` in its layout row, if that is an input control
    QWidget *inputOnTheLeft(QWidget *control)
    {
        QWidget *parent = control->parentWidget();
        QLayout *top = parent ? parent->layout() : nullptr;
        if (!top)
            return nullptr;
        const Slot slot = findSlot(top, control);
        QLayoutItem *item = nullptr;
        if (auto *grid = qobject_cast<QGridLayout *>(slot.layout))
        {
            int row = 0, column = 0, rowSpan = 0, columnSpan = 0;
            grid->getItemPosition(slot.index, &row, &column, &rowSpan, &columnSpan);
            item = (column > 0) ? grid->itemAtPosition(row, (column - 1)) : nullptr;
        }
        else if (auto *box = qobject_cast<QBoxLayout *>(slot.layout); box && (slot.index > 0)
            && ((box->direction() == QBoxLayout::LeftToRight) || (box->direction() == QBoxLayout::RightToLeft)))
        {
            item = box->itemAt(slot.index - 1);
        }
        QWidget *w = item ? item->widget() : nullptr;
        return (w && isInput(w)) ? w : nullptr;
    }

    QString nameOf(QWidget *control)
    {
        QWidget *holder = qobject_cast<FileSystemPathEdit *>(control) ? pathEditor(control) : control;
        return holder ? holder->accessibleName() : QString();
    }

    class MenuFocusFilter final : public QObject
    {
    public:
        using QObject::QObject;

        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (event->type() == QEvent::Show)
            {
                if (auto *menu = qobject_cast<QMenu *>(watched))
                {
                    // after popup() has finished positioning and possibly selecting an action itself
                    QTimer::singleShot(0, menu, [menu]
                    {
                        if (!menu->isVisible() || menu->activeAction())
                            return;
                        for (QAction *action : menu->actions())
                        {
                            if (!action->isSeparator() && action->isVisible() && action->isEnabled())
                            {
                                menu->setActiveAction(action);
                                break;
                            }
                        }
                    });
                }
            }
            return QObject::eventFilter(watched, event);
        }
    };

    class DigitKeyFilter final : public QObject
    {
    public:
        DigitKeyFilter(QObject *parent, const Qt::KeyboardModifiers modifiers, std::function<bool (int digit)> handler)
            : QObject(parent)
            , m_modifiers {modifiers}
            , m_handler {std::move(handler)}
        {
        }

        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (event->type() != QEvent::KeyPress)
                return QObject::eventFilter(watched, event);

            const auto *key = static_cast<QKeyEvent *>(event);
            const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
            // the physical key: Windows virtual-key codes '0'..'9' whatever the layout (AZERTY included)
            const quint32 vk = key->nativeVirtualKey();
            if ((modifiers != m_modifiers) || (vk < '0') || (vk > '9') || key->isAutoRepeat())
                return QObject::eventFilter(watched, event);

            // AltGr+digit typing a character ("@", "#"...) in a text field: leave the character alone
            QWidget *focus = QApplication::focusWidget();
            const bool textField = qobject_cast<QLineEdit *>(focus) || qobject_cast<QTextEdit *>(focus)
                || qobject_cast<QPlainTextEdit *>(focus) || qobject_cast<QAbstractSpinBox *>(focus);
            if (textField && !key->text().isEmpty() && key->text().at(0).isPrint())
                return QObject::eventFilter(watched, event);

            return m_handler(static_cast<int>(vk - '0'));
        }

    private:
        Qt::KeyboardModifiers m_modifiers;
        std::function<bool (int digit)> m_handler;
    };

    class DialogLabelingFilter final : public QObject
    {
    public:
        using QObject::QObject;

        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (event->type() == QEvent::Show)
            {
                if (auto *dialog = qobject_cast<QDialog *>(watched))
                    Access::labelControls(dialog);
            }
            return QObject::eventFilter(watched, event);
        }
    };

    class ListEdgeFilter final : public QObject
    {
    public:
        using QObject::QObject;

        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (event->type() == QEvent::KeyPress)
            {
                if (Access::announceListEdge(static_cast<QAbstractItemView *>(watched), static_cast<QKeyEvent *>(event)))
                    return true;
            }
            return QObject::eventFilter(watched, event);
        }
    };

    struct RowTextView
    {
        QPointer<QTreeView> view;
        int primaryColumn = 0;
    };

    QHash<const QAbstractItemModel *, RowTextView> &rowTextRegistry()
    {
        static QHash<const QAbstractItemModel *, RowTextView> registry;
        return registry;
    }

    // Icon-only buttons: their tooltip is their name, and stays so when the tooltip changes
    class TooltipNameFilter final : public QObject
    {
    public:
        using QObject::QObject;

        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (event->type() == QEvent::ToolTipChange)
            {
                auto *button = static_cast<QWidget *>(watched);
                button->setAccessibleName(Access::cleanLabel(button->toolTip()));
            }
            return QObject::eventFilter(watched, event);
        }
    };
}

QString Access::cleanLabel(const QString &text)
{
    QString result = Qt::mightBeRichText(text) ? QTextDocumentFragment::fromHtml(text).toPlainText() : text;
    result.replace(u"&&"_s, u"\x01"_s).remove(u'&').replace(u'\x01', u'&');
    result = result.simplified();
    if (result.endsWith(u"(?)"_s)) // link to the documentation, not part of the name
        result.chop(3);
    if (result.endsWith(u"..."_s)) // screen readers say "dot dot dot"
        result.chop(3);
    if (result.endsWith(u'\u2026'))
        result.chop(1);
    while (!result.isEmpty() && (result.endsWith(u':') || result.endsWith(u'：') || result.back().isSpace()))
        result.chop(1);
    return result;
}

void Access::setControlName(QWidget *control, const QString &name)
{
    if (!control || name.isEmpty())
        return;

    if (qobject_cast<FileSystemPathEdit *>(control))
    {
        QWidget *editor = pathEditor(control);
        if (editor)
            editor->setAccessibleName(name);
        if (auto *browse = control->findChild<QToolButton *>(Qt::FindDirectChildrenOnly))
        {
            browse->setAccessibleName(QCoreApplication::translate("Access", "Browse"));
            browse->setAccessibleDescription(name); // which "Browse": several per page
            if (editor)
                QWidget::setTabOrder(editor, browse); // the field first, then its button
        }
        return;
    }
    control->setAccessibleName(name);
}

void Access::labelControls(QWidget *root)
{
    if (!root)
        return;

    for (auto *scroll : root->findChildren<QScrollArea *>())
        scroll->setFocusPolicy(Qt::NoFocus);
    for (auto *buttonBox : root->findChildren<QDialogButtonBox *>())
        buttonBox->setFocusPolicy(Qt::NoFocus); // its buttons are the Tab stops, not the box

    // A group box is named after its own title (Qt names a nested one after its parent's title);
    // this matters for checkable group boxes, which are Tab stops
    for (auto *group : root->findChildren<QGroupBox *>())
    {
        if (group->accessibleName().isEmpty())
            group->setAccessibleName(cleanLabel(group->title()));
    }

    // Icon-only buttons are named after their tooltip
    auto *tooltipFilter = new TooltipNameFilter(root);
    for (auto *button : root->findChildren<QAbstractButton *>())
    {
        if (!button->text().isEmpty() || !button->accessibleName().isEmpty() || button->toolTip().isEmpty())
            continue;
        if (qobject_cast<FileSystemPathEdit *>(button->parentWidget()))
            continue; // "Browse", named with its path editor
        button->setAccessibleName(cleanLabel(button->toolTip()));
        button->installEventFilter(tooltipFilter);
    }

    QList<QWidget *> controls;
    for (QWidget *control : root->findChildren<QWidget *>())
    {
        if ((control->window() == root->window()) && isInput(control))
            controls.append(control);
    }

    // 1. the label on the left (or the check box that enables the control)
    for (QWidget *control : controls)
    {
        if (!nameOf(control).isEmpty())
            continue; // an explicit name always wins

        QWidget *label = findLabel(control);
        if (!label)
            continue;

        setControlName(control, labelText(label));
        if (auto *qlabel = qobject_cast<QLabel *>(label); qlabel && !qlabel->buddy())
            qlabel->setBuddy(qobject_cast<FileSystemPathEdit *>(control) ? pathEditor(control) : control);
    }

    // 2. a unit selector right after a named field takes the field's name ("Delete logs older than: [6] [months]")
    for (QWidget *control : controls)
    {
        if (!nameOf(control).isEmpty())
            continue;
        if (QWidget *field = inputOnTheLeft(control); field && !nameOf(field).isEmpty())
            setControlName(control, nameOf(field));
    }

    // 3. still nothing: the title of the enclosing group box, cleaned
    for (QWidget *control : controls)
    {
        if (!nameOf(control).isEmpty())
            continue;
        for (QWidget *p = control->parentWidget(); p && (p != root); p = p->parentWidget())
        {
            if (auto *group = qobject_cast<QGroupBox *>(p))
            {
                setControlName(control, cleanLabel(group->title()));
                break;
            }
        }
    }
}


void Access::announce(QObject *source, const QString &text, const bool polite)
{
    if (!source || text.isEmpty() || !QAccessible::isActive())
        return;

    QAccessibleAnnouncementEvent event {source, text};
    event.setPoliteness(polite ? QAccessible::AnnouncementPoliteness::Polite : QAccessible::AnnouncementPoliteness::Assertive);
    QAccessible::updateAccessibility(&event);
}

void Access::installDigitKeys(QObject *owner, const Qt::KeyboardModifiers modifiers, std::function<bool (int digit)> handler)
{
    qApp->installEventFilter(new DigitKeyFilter(owner, modifiers, std::move(handler)));
}

void Access::installListEdgeAnnouncer(QAbstractItemView *view)
{
    view->installEventFilter(new ListEdgeFilter(view));
}

QString Access::rowText(const QTreeView *view, const QModelIndex &index, const int primaryColumn)
{
    const QAbstractItemModel *model = index.model();
    const auto cell = [&index](const int column)
    {
        const QVariant data = index.siblingAtColumn(column).data(Qt::DisplayRole);
        return (data.typeId() == QMetaType::Double) ? QString::number(data.toDouble(), 'f', 1) : data.toString().trimmed();
    };

    QStringList parts {cell(primaryColumn)};
    const QHeaderView *header = view->header();
    for (int visual = 0, count = header->count(); visual < count; ++visual)
    {
        const int column = header->logicalIndex(visual);
        if ((column == primaryColumn) || view->isColumnHidden(column))
            continue;

        const QString text = cell(column);
        if (text.isEmpty())
            continue;

        const QString title = model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        parts.append(title.isEmpty() ? text : (title + u' ' + text));
    }
    return parts.join(u", "_s);
}

void Access::registerRowText(QTreeView *view, const int primaryColumn)
{
    const QAbstractItemModel *model = view->model();
    rowTextRegistry().insert(model, {view, primaryColumn});
    QObject::connect(view, &QObject::destroyed, qApp, [model] { rowTextRegistry().remove(model); });
}

QVariant Access::rowTextFor(const QModelIndex &index)
{
    const auto it = rowTextRegistry().constFind(index.model());
    if ((it == rowTextRegistry().cend()) || !it->view || !index.isValid())
        return {};
    return rowText(it->view, index, it->primaryColumn);
}

QStringList Access::captionValueRows(const std::initializer_list<const QGridLayout *> grids, const bool includeHidden)
{
    const auto isValue = [](const QLabel *label)
    {
        return label->objectName().endsWith(u"Val") || label->objectName().endsWith(u"Data");
    };
    const auto labelAt = [&isValue](const QLayoutItem *item) -> const QLabel *
    {
        QWidget *w = item ? item->widget() : nullptr;
        if (const auto *scroll = qobject_cast<const QScrollArea *>(w))
        {
            // a long value (comment) sits in a scroll area of its own
            for (const QLabel *inner : scroll->widget()->findChildren<QLabel *>())
            {
                if (isValue(inner))
                    return inner;
            }
            return nullptr;
        }
        return qobject_cast<const QLabel *>(w);
    };

    QStringList rows;
    for (const QGridLayout *grid : grids)
    {
        for (int row = 0; row < grid->rowCount(); ++row)
        {
            QString caption;
            for (int column = 0; column < grid->columnCount(); ++column)
            {
                const QLabel *label = labelAt(grid->itemAtPosition(row, column));
                if (!label || (label->isHidden() && !includeHidden))
                    continue;
                if (!isValue(label))
                {
                    caption = cleanLabel(label->text());
                    continue;
                }
                const QString value = Qt::mightBeRichText(label->text())
                    ? QTextDocumentFragment::fromHtml(label->text()).toPlainText().simplified()
                    : label->text().simplified();
                if (!caption.isEmpty() && !value.isEmpty())
                    rows.append(caption + u": " + value);
                caption.clear();
            }
        }
    }
    return rows;
}

Access::ReadOnlyList::ReadOnlyList(QWidget *parent)
    : QListWidget(parent)
{
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void Access::ReadOnlyList::setRows(const QStringList &rows)
{
    if (count() != rows.size())
    {
        const int current = currentRow();
        clear();
        addItems(rows);
        if (count() > 0)
            setCurrentRow(std::clamp(current, 0, (count() - 1)));
        return;
    }
    for (int i = 0; i < rows.size(); ++i)
    {
        if (item(i)->text() != rows[i])
            item(i)->setText(rows[i]);
    }
}

void Access::ReadOnlyList::keyPressEvent(QKeyEvent *event)
{
    if (announceListEdge(this, event))
        return;

    if ((event->modifiers() == Qt::ControlModifier) && (event->key() == Qt::Key_C) && currentItem())
    {
        const QString text = currentItem()->text();
        const qsizetype separator = text.indexOf(u": "_s);
        QApplication::clipboard()->setText((separator >= 0) ? text.mid(separator + 2) : text);
        announce(this, QCoreApplication::translate("Access", "Copied"));
        return;
    }
    QListWidget::keyPressEvent(event);
}

void Access::installMenuFocusFix(QObject *owner)
{
    qApp->installEventFilter(new MenuFocusFilter(owner));
}

void Access::installDialogLabeling(QObject *owner)
{
    qApp->installEventFilter(new DialogLabelingFilter(owner));
}

void Access::keepItemRowTexts(QTreeView *view, const int primaryColumn)
{
    QAbstractItemModel *model = view->model();
    const auto update = [view, model, primaryColumn]
    {
        // setData() below emits dataChanged: do not recurse
        static bool updating = false;
        if (updating)
            return;
        updating = true;
        const std::function<void (const QModelIndex &)> walk = [&](const QModelIndex &parent)
        {
            for (int row = 0; row < model->rowCount(parent); ++row)
            {
                const QModelIndex index = model->index(row, 0, parent);
                const QString text = rowText(view, index, primaryColumn);
                for (int column = 0; column < model->columnCount(parent); ++column)
                {
                    const QModelIndex cell = index.siblingAtColumn(column);
                    if (cell.data(Qt::AccessibleTextRole).toString() != text)
                        model->setData(cell, text, Qt::AccessibleTextRole);
                }
                walk(index);
            }
        };
        walk({});
        updating = false;
    };
    QObject::connect(model, &QAbstractItemModel::rowsInserted, view, update);
    QObject::connect(model, &QAbstractItemModel::modelReset, view, update);
    QObject::connect(model, &QAbstractItemModel::dataChanged, view, update);
    update();
}

void Access::focusFirstChild(QWidget *page)
{
    if (!page)
        return;

    for (QWidget *w = page->nextInFocusChain(); w && (w != page); w = w->nextInFocusChain())
    {
        if (!page->isAncestorOf(w))
            continue;
        // a focus proxy decides (a scroll area viewport forwards to its view, which may refuse Tab)
        const QWidget *target = w;
        while (target->focusProxy())
            target = target->focusProxy();
        // isVisibleTo: the page may be shown in this same event (stacked widget switch)
        if ((w->focusPolicy() & Qt::TabFocus) && (target->focusPolicy() & Qt::TabFocus) && w->isVisibleTo(page) && w->isEnabled())
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

    const QModelIndex current = view->currentIndex();
    if (!current.isValid())
        return false;

    bool atEdge = false;
    if (const auto *tree = qobject_cast<const QTreeView *>(view))
        atEdge = !(up ? tree->indexAbove(current) : tree->indexBelow(current)).isValid();
    else
        atEdge = up ? (current.row() == 0) : (current.row() == (view->model()->rowCount(current.parent()) - 1));

    if (atEdge)
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

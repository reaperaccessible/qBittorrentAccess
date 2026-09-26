/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Mike Tzou (Chocobo1)
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

#include "tristatewidget.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QString>
#include <QStyle>
#include <QStyleOptionMenuItem>
#include <QWidgetAction>

namespace
{
    // Accessibility: without this, Windows shows the widget as an unnamed "group"; it is a checkable menu
    // item with a name and a checked, unchecked or mixed state
    class TriStateWidgetAccessible final : public QAccessibleWidget
    {
    public:
        explicit TriStateWidgetAccessible(TriStateWidget *widget)
            : QAccessibleWidget(widget, QAccessible::MenuItem)
        {
        }

        QString text(const QAccessible::Text t) const override
        {
            if (t == QAccessible::Name)
                return QString(tristate()->text()).remove(u'&');
            return QAccessibleWidget::text(t);
        }

        QAccessible::State state() const override
        {
            QAccessible::State s = QAccessibleWidget::state();
            s.checkable = true;
            s.checked = (tristate()->checkState() == Qt::Checked);
            s.checkStateMixed = (tristate()->checkState() == Qt::PartiallyChecked);
            return s;
        }

        QStringList actionNames() const override
        {
            return {toggleAction()};
        }

        void doAction(const QString &actionName) override
        {
            if (actionName != toggleAction())
                return;
            QKeyEvent enter {QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier};
            QApplication::sendEvent(widget(), &enter);
        }

    private:
        TriStateWidget *tristate() const
        {
            return static_cast<TriStateWidget *>(widget());
        }
    };

    QAccessibleInterface *triStateWidgetFactory(const QString &className, QObject *object)
    {
        if ((className == u"TriStateWidget") && object && object->isWidgetType())
            return new TriStateWidgetAccessible(static_cast<TriStateWidget *>(object));
        return nullptr;
    }
}

TriStateWidget::TriStateWidget(const QString &text, QWidget *parent)
    : QWidget {parent}
    , m_text {text}
{
    setMouseTracking(true);  // for visual effects via mouse navigation
    // accessibility: the focus stays on the menu, which speaks this entry as one checkable item (a focused
    // widget made the screen reader say the entry twice); keyboard selection is drawn in paintEvent
    setFocusPolicy(Qt::NoFocus);

    static const bool factoryInstalled = []
    {
        QAccessible::installFactory(triStateWidgetFactory);
        return true;
    }();
    Q_UNUSED(factoryInstalled);
}

void TriStateWidget::setCheckState(const Qt::CheckState checkState)
{
    m_checkState = checkState;
    notifyCheckStateChanged();
}

QString TriStateWidget::text() const
{
    return m_text;
}

Qt::CheckState TriStateWidget::checkState() const
{
    return m_checkState;
}

void TriStateWidget::notifyCheckStateChanged()
{
    QAccessible::State changed;
    changed.checked = true;
    changed.checkStateMixed = true;
    QAccessibleStateChangeEvent event {this, changed};
    QAccessible::updateAccessibility(&event);
}

void TriStateWidget::setCloseOnInteraction(const bool enabled)
{
    m_closeOnInteraction = enabled;
}

QSize TriStateWidget::minimumSizeHint() const
{
    QStyleOptionMenuItem opt;
    opt.initFrom(this);
    opt.menuHasCheckableItems = true;
    const QSize contentSize = fontMetrics().size(Qt::TextSingleLine, m_text);
    return style()->sizeFromContents(QStyle::CT_MenuItem, &opt, contentSize, this);
}

void TriStateWidget::paintEvent(QPaintEvent *)
{
    QStyleOptionMenuItem opt;
    opt.initFrom(this);
    opt.menuItemType = QStyleOptionMenuItem::Normal;
    opt.checkType = QStyleOptionMenuItem::NonExclusive;
    opt.menuHasCheckableItems = true;
    opt.text = m_text;

    switch (m_checkState)
    {
    case Qt::PartiallyChecked:
        opt.state |= QStyle::State_NoChange;
        break;
    case Qt::Checked:
        opt.checked = true;
        break;
    case Qt::Unchecked:
        opt.checked = false;
        break;
    };

    // selected with the keyboard: the menu's current item is this widget's action
    const auto *menu = qobject_cast<const QMenu *>(parentWidget());
    const auto *currentAction = menu ? qobject_cast<const QWidgetAction *>(menu->activeAction()) : nullptr;
    const bool keyboardSelected = currentAction && (currentAction->defaultWidget() == this);

    if (keyboardSelected || (opt.state & QStyle::State_HasFocus)
        || rect().contains(mapFromGlobal(QCursor::pos())))
        {
        opt.state |= QStyle::State_Selected;

        if (QApplication::mouseButtons() != Qt::NoButton)
            opt.state |= QStyle::State_Sunken;
    }

    QPainter painter {this};
    style()->drawControl(QStyle::CE_MenuItem, &opt, &painter, this);
}

void TriStateWidget::mouseReleaseEvent(QMouseEvent *event)
{
    toggleCheckState();

    if (m_closeOnInteraction)
    {
        // parent `triggered` signal will be emitted
        QWidget::mouseReleaseEvent(event);
    }
    else
    {
        update();
        // need to emit `triggered` signal manually
        emit triggered(m_checkState == Qt::Checked);
    }
}

void TriStateWidget::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Return)
        || (event->key() == Qt::Key_Enter))
        {
        toggleCheckState();

        if (!m_closeOnInteraction)
        {
            update();
            // need to emit parent `triggered` signal manually
            emit triggered(m_checkState == Qt::Checked);
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void TriStateWidget::toggleCheckState()
{
    switch (m_checkState)
    {
    case Qt::Unchecked:
    case Qt::PartiallyChecked:
        m_checkState = Qt::Checked;
        break;
    case Qt::Checked:
        m_checkState = Qt::Unchecked;
        break;
    };
    notifyCheckStateChanged();
}

/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include "proptabbar.h"

#include <QButtonGroup>
#include <QPushButton>
#include <QSpacerItem>

#include "base/global.h"
#include "gui/uithememanager.h"

PropTabBar::PropTabBar(QWidget *parent)
    : QHBoxLayout(parent)
{
    setAlignment(Qt::AlignLeft | Qt::AlignCenter);
    setSpacing(3);
    m_btnGroup = new QButtonGroup(this);
    // General tab
    QPushButton *mainInfosButton = new QPushButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"help-about"_s, u"document-properties"_s),
#endif
            tr("General"), parent);
    addWidget(mainInfosButton);
    m_btnGroup->addButton(mainInfosButton, MainTab);
    // Trackers tab
    QPushButton *trackersButton = new QPushButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"trackers"_s, u"network-server"_s),
#endif
            tr("Trackers"), parent);
    addWidget(trackersButton);
    m_btnGroup->addButton(trackersButton, TrackersTab);
    // Peers tab
    QPushButton *peersButton = new QPushButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"peers"_s),
#endif
            tr("Peers"), parent);
    addWidget(peersButton);
    m_btnGroup->addButton(peersButton, PeersTab);
    // URL seeds tab
    QPushButton *URLSeedsButton = new QPushButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"network-server"_s),
#endif
            tr("HTTP Sources"), parent);
    addWidget(URLSeedsButton);
    m_btnGroup->addButton(URLSeedsButton, URLSeedsTab);
    // Files tab
    QPushButton *filesButton = new QPushButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"directory"_s),
#endif
            tr("Content"), parent);
    addWidget(filesButton);
    m_btnGroup->addButton(filesButton, FilesTab);
    // Spacer
    addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed));
    // Speed tab
    QPushButton *speedButton = new QPushButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"chart-line"_s),
#endif
            tr("Speed"), parent);
    addWidget(speedButton);
    m_btnGroup->addButton(speedButton, SpeedTab);
    // Accessibility: no Alt+letter shortcuts on these buttons, duplicates of Ctrl+Shift+1..6 (main window)
    // Accessibility: the current tab button is "checked", so screen readers say which tab is shown
    m_btnGroup->setExclusive(false);
    for (QAbstractButton *button : m_btnGroup->buttons())
        button->setCheckable(true);

    // SIGNAL/SLOT
    connect(m_btnGroup, &QButtonGroup::idClicked
            , this, &PropTabBar::setCurrentIndex);
}

QString PropTabBar::tabTitle(const int index) const
{
    const QAbstractButton *button = m_btnGroup->button(index);
    return button ? button->text() : QString();
}

int PropTabBar::currentIndex() const
{
    return m_currentIndex;
}

void PropTabBar::setCurrentIndex(int index)
{
    if (index >= m_btnGroup->buttons().size())
        index = 0;
    // If asked to hide or if the currently selected tab is clicked
    if ((index < 0) || (m_currentIndex == index))
    {
        if (m_currentIndex >= 0)
        {
          m_btnGroup->button(m_currentIndex)->setDown(false);
          m_btnGroup->button(m_currentIndex)->setChecked(false);
          m_currentIndex = -1;
          emit visibilityToggled(false);
        }
        return;
    }
    // Unselect previous tab
    if (m_currentIndex >= 0)
    {
        m_btnGroup->button(m_currentIndex)->setDown(false);
        m_btnGroup->button(m_currentIndex)->setChecked(false);
    }
    else
    {
        // Nothing was selected, show!
        emit visibilityToggled(true);
    }
    // Select the new button
    m_btnGroup->button(index)->setDown(true);
    m_btnGroup->button(index)->setChecked(true);
    m_currentIndex = index;
    // Emit the signal
    emit tabChanged(index);
}

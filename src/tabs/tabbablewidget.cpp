/*
 * tabbable.cpp
 * Copyright (C) 2007  Kevin Smith
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "tabbablewidget.h"

#include "jidutil.h"
#include "psioptions.h"
#include "tabdlg.h"
#include "tabmanager.h"

#include <QTimer>
#ifdef Q_OS_WIN
#    include <windows.h>
#endif

//----------------------------------------------------------------------------
// TabbableWidget
//----------------------------------------------------------------------------

TabbableWidget::TabbableWidget(const Jid &jid, PsiAccount *pa, TabManager *tabManager)
    : AdvancedWidget<QWidget>(nullptr)
    , state_(ActivationState::Deactivated)
    , jid_(jid)
    , pa_(pa)
    , tabManager_(tabManager)
{
    stateCommitTimer_.setInterval(100);
    stateCommitTimer_.setSingleShot(true);
    connect(&stateCommitTimer_, &QTimer::timeout, this, [this](){
        // the idea is to not call activated/deactivated virtual methods immediatelly
        // what makes trackbar working better in some cases
        if(state_ == ActivationState::Activated) {
            activated();
        }
        else {
            deactivated();
        }
    });
    //QTimer::singleShot(0, this, SLOT(ensureTabbedCorrectly()));
}

void TabbableWidget::ensureTabbedCorrectly()
{
    if (tabManager_->shouldBeTabbed(this)) {
        if (!isTabbed()) {
            tabManager_->getTabs(this)->addTab(this);
        }
        else {
            QTimer::singleShot(0, tabManager_->getTabs(this), SLOT(showTabWithoutActivation()));
        }
    }
    else {
        if (PsiOptions::instance()->getOption("options.ui.tabs.tab-singles").toString().contains(tabManager_->tabKind(this))) {
            if (isTabbed()) {
                if (getManagingTabDlg()->tabCount() > 1) {
                    getManagingTabDlg()->closeTab(this, false);
                    tabManager_->newTabs(this)->addTab(this);
                }
            }
            else {
                tabManager_->newTabs(this)->addTab(this);
            }
        }
        else {
            if (isTabbed()) {
                getManagingTabDlg()->closeTab(this, false);
            }

            // FIXME: showWithoutActivation() works on all
            //   platforms, but bringToFront() (which might be
            //   called immediately after) does not work on all
            //   platforms if it follows a call to
            //   showWithoutActivation().  As a temporary fix, we
            //   will only call showWithoutActivation() on
            //   platforms where both calls can be made in
            //   succession.
#ifdef Q_OS_WIN
            showWithoutActivation();
#else
            show();
#endif
        }
    }
}

void TabbableWidget::bringToFront(bool raiseWindow)
{
    if (isTabbed()) {
        getManagingTabDlg()->selectTab(this);
    }
    if (raiseWindow) {
        ::bringToFront(this);
    }
}

TabbableWidget::~TabbableWidget()
{
    if (isTabbed()) {
        getManagingTabDlg()->removeTabWithNoChecks(this);
    }
}

/**
 * Checks if the dialog is in a tabset
 */
bool TabbableWidget::isTabbed()
{
    return tabManager_->isChatTabbed(this);
}

TabDlg* TabbableWidget::getManagingTabDlg()
{
    return tabManager_->getManagingTabs(this);
}

/**
 * Runs any gumph necessary before hiding a tab.
 * (checking new messages, setting the autodelete, cancelling composing etc)
 * \return TabbableWidget is ready to be hidden.
 */
bool TabbableWidget::readyToHide()
{
    return true;
}

Jid TabbableWidget::jid() const
{
    return jid_;
}

void TabbableWidget::setJid(const Jid& j)
{
    jid_ = j;
}

const QString& TabbableWidget::getDisplayName() const
{
    return jid_.node();
}

void TabbableWidget::deactivated()
{
}

void TabbableWidget::activated()
{
}

/**
 * Returns true if this tab is active in the active window.
 */
bool TabbableWidget::isActiveTab()
{
    if (isHidden()) {
        return false;
    }
    if (!isTabbed()) {
        return isActiveWindow() && !isMinimized();
    }
    return getManagingTabDlg()->isActiveWindow() &&
           getManagingTabDlg()->tabOnTop(this) &&
          !getManagingTabDlg()->isMinimized();
}

void TabbableWidget::doFlash(bool on)
{
    AdvancedWidget<QWidget>::doFlash(on);
    emit updateFlashState();
}

TabbableWidget::State TabbableWidget::state() const
{
    return TabbableWidget::State::None;
}

int TabbableWidget::unreadMessageCount() const
{
    return 0;
}

/**
 * Use this to invalidate tab state.
 */
void TabbableWidget::invalidateTab()
{
    setWindowTitle(desiredCaption());
    emit invalidateTabInfo();
}

PsiAccount* TabbableWidget::account() const
{
    return pa_;
}

void TabbableWidget::changeEvent(QEvent* event)
{
    AdvancedWidget<QWidget>::changeEvent(event);

    if (event->type() == QEvent::ActivationChange ||
        event->type() == QEvent::WindowStateChange)
    {
        if (isActiveTab()) {
            state_ = ActivationState::Activated;
        }
        else {
            state_ = ActivationState::Deactivated;
        }
        stateCommitTimer_.start();
    }
}

/**
 * Set the icon of the tab.
 */
void TabbableWidget::setTabIcon(const QIcon &icon)
{
    icon_ = icon;
    TabDlg* tabDlg = tabManager_->getManagingTabs(this);
    if (tabDlg) {
        tabDlg->setTabIcon(this, icon);
    }
}

const QIcon &TabbableWidget::icon() const
{
    return icon_;
}

void TabbableWidget::hideTab()
{
    if(isTabbed())
        getManagingTabDlg()->hideTab(this);
    else
        hide();
}

void TabbableWidget::pinTab()
{
    getManagingTabDlg()->pinTab(this);
}

bool TabbableWidget::isGroupChat()
{
    return objectName() == "GroupChatDlg";
}

/*
 * tabbable.h
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

#ifndef TABBABLEWIDGET_H
#define TABBABLEWIDGET_H

#include "advwidget.h"
#include "im.h" // ChatState
#include "sendbuttonmenu.h"

#include <QIcon>
#include <QTimer>

class PsiAccount;
class TabDlg;
class TabManager;

namespace XMPP {
class Jid;
class Message;
}
using namespace XMPP;

class TabbableWidget : public AdvancedWidget<QWidget> {
    Q_OBJECT
public:
    TabbableWidget(const Jid &, PsiAccount *, TabManager *tabManager);
    ~TabbableWidget();

    PsiAccount * account() const;
    void         setTabIcon(const QIcon &);
    const QIcon &icon() const;

    virtual Jid            jid() const;
    virtual const QString &getDisplayName() const;

    virtual bool readyToHide();
    TabDlg *     getManagingTabDlg();

    bool isTabbed();
    bool isActiveTab();
    bool isGroupChat();

    // reimplemented
    virtual void doFlash(bool on);

    virtual void invalidateTab();

    enum class State : char { None = 0, Composing, Inactive, Highlighted };
    virtual State   state() const              = 0;
    virtual int     unreadMessageCount() const = 0;
    virtual QString desiredCaption() const     = 0;
    virtual void    setVSplitterPosition(int, int) {} // default implementation do nothing

    // Templates
    SendButtonTemplatesMenu *getTemplateMenu();
    void                     showTemplateEditor();
    // ---

signals:
    void invalidateTabInfo();
    void updateFlashState();
    void eventsRead(const Jid &);
    void vSplitterMoved(int, int);

public slots:
    void         bringToFront(bool raiseWindow = true);
    virtual void ensureTabbedCorrectly();
    void         hideTab();
    void         pinTab();

protected:
    virtual void setJid(const Jid &);
    virtual void deactivated();
    virtual void activated();

    // reimplemented
    void changeEvent(QEvent *e);

private:
    enum class ActivationState : char { Activated, Deactivated };
    ActivationState state_;
    QTimer          stateCommitTimer_;

    Jid         jid_;
    PsiAccount *pa_;
    TabManager *tabManager_;
    QIcon       icon_;
    // Templates
    static int                                 chatsCount;
    static SendButtonTemplatesMenu *           templateMenu;
    static QPointer<SendButtonTemplatesEditor> templateEditDlg;
    // ---
};

#endif // TABBABLEWIDGET_H

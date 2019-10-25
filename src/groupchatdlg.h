/*
 * groupchatdlg.h - dialogs for handling groupchat
 * Copyright (C) 2001-2002  Justin Karneges
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

#ifndef GROUPCHATDLG_H
#define GROUPCHATDLG_H

#include "advwidget.h"
#include "languagemanager.h"
#include "mucmanager.h"
#include "psievent.h"
#include "tabbablewidget.h"
#include "ui_groupchatdlg.h"

#include <QDialog>
#include <QWidget>

class GCMainDlg;
class MessageView;
class PsiAccount;
class PsiCon;
class PsiOptions;
class QColorGroup;
class QRect;

namespace XMPP {
    class Message;
}
using namespace XMPP;

class GCMainDlg : public TabbableWidget
{
    Q_OBJECT
public:
    GCMainDlg(PsiAccount *, const Jid &, TabManager *tabManager);
    ~GCMainDlg();

    PsiAccount* account() const;

    void error(int, const QString &);
    void gcSelfPresence(const Status &s);
    void presence(const QString &, const Status &);
    void message(const Message &, const PsiEvent::Ptr &e = PsiEvent::Ptr());
    void joined();
    void setPassword(const QString&);
    const QString& nick() const;
    const QDateTime& lastMsgTime() const;
    bool isLastMessageAlert() const;

    bool isInactive() const;
    void reactivate();
    void setJid(const Jid &j);
    void appendSysMsg(const QString &, bool alert=false);
    void dispatchMessage(const MessageView &mv);
    QStringList mucRosterContent() const;

    // reimplemented
    virtual TabbableWidget::State state() const;
    virtual int unreadMessageCount() const;
    const QString & getDisplayName() const;
    virtual QString desiredCaption() const;

protected:
    void setShortcuts();

    // reimplemented
    void resizeEvent(QResizeEvent *e);
    void dragEnterEvent(QDragEnterEvent *);
    void dropEvent(QDropEvent *);
    void closeEvent(QCloseEvent *);
    void mucInfoDialog(const QString& title, const QString& message, const Jid& actor, const QString& reason);
    void setStatusTabIcon(int status);

signals:
    void aSend(Message &);
    void messagesRead(const Jid &);
    void messageAppended(const QString &, QWidget*);

public slots:
    // reimplemented
    virtual void deactivated();
    virtual void activated();
    virtual void ensureTabbedCorrectly();

    void optionsUpdate();
    void doBookmark();

private slots:
    void openURL(const QString&);
    void onNickInsertClick(const QString &nick);
    void scrollUp();
    void scrollDown();
    void mle_returnPressed();
    void openTopic();
    void sendNewTopic(const QMap<LanguageManager::LangId, QString> &topics);
    //void openFind();
    void configureRoom();
    //void doFind(const QString &);
    void pa_updatedActivity();
    void goDisc();
    void goConn();
    void goForcedLeave();
    void lv_action(const QString &, const Status &, int);
    void doClear();
    void doClearButton();
    void copyMucJid();
    void doRemoveBookmark();
    void buildMenu();
    void logSelectionChanged();
    void setConnecting();
    void unsetConnecting();
    void action_error(MUCManager::Action, int, const QString&);
    void updateMucName();
    void updateGCVCard();
    void discoInfoFinished();
    void updateIdentityVisibility();
    void updateBookmarkIcon();
#ifdef WHITEBOARDING
    void openWhiteboard();
#endif
    void chatEditCreated();
    void sendButtonMenu();
    void editTemplates();
    void doPasteAndSend();
    void sendTemp(const QString &);
    void psButtonEnabled();
    void horizSplitterMoved();
    void doMinimize();
    void avatarUpdated(const Jid& jid);
    void doContactContextMenu(const QString &nick);

public:
    class Private;
    friend class Private;
private:
    Private *d;
    Ui::GroupChatDlg ui_;

    void doAlert();
    void appendMessage(const Message &, bool);
    void setLooks();
    void setToolbuttons();

    void mucKickMsgHelper(const QString &nick, const Status &s, const QString &nickJid, const QString &title,
            const QString &youSimple, const QString &youBy, const QString &someoneSimple,
            const QString &someoneBy);

    void contextMenuEvent(QContextMenuEvent *);

    inline XMPP::Jid jidForNick(const QString &nick) const;

    void setMucSelfAvatar();
};

#endif // GROUPCHATDLG_H

/*
 * chatdlg.h - dialog for handling chats
 * Copyright (C) 2001-2007  Justin Karneges, Michail Pishchagin
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

#ifndef CHATDLG_H
#define CHATDLG_H

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QShowEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QDropEvent>
#include <QCloseEvent>
#include <QTextEdit>

#include "advwidget.h"

#include "tabbablewidget.h"
#include "messageview.h"


namespace XMPP
{
class Jid;
class Message;
}
using namespace XMPP;

class PsiAccount;
class UserListItem;
class QDropEvent;
class QDragEnterEvent;
class ChatView;
class ChatEdit;
class FileSharingItem;

struct UserStatus {
    UserStatus()
            : userListItem(0)
            , statusType(XMPP::Status::Offline) {}
    UserListItem* userListItem;
    XMPP::Status::Type statusType;
    QString status;
    int priority = 0;
    QString publicKeyID;
};

class ChatDlg : public TabbableWidget
{
    Q_OBJECT
protected:
    ChatDlg(const Jid& jid, PsiAccount* account, TabManager* tabManager);
    virtual void init();

public:
    static ChatDlg* create(const Jid& jid, PsiAccount* account, TabManager* tabManager);
    ~ChatDlg();

    // reimplemented
    void setJid(const Jid &) override;
    const QString & getDisplayName() const override;

    // reimplemented
    bool readyToHide() override;
    TabbableWidget::State state() const override;
    int unreadMessageCount() const override;
    QString desiredCaption() const override;
    void ensureTabbedCorrectly() override;

public:
    PsiAccount* account() const;
    void setInputText(const QString &text);
    Jid realJid() const;
    bool autoSelectContact() const {return autoSelectContact_;}
    static UserStatus userStatusFor(const Jid& jid, QList<UserListItem*> ul, bool forceEmptyResource);
    void preloadHistory();
    void dispatchMessage(const MessageView &mv);
    virtual void appendSysMsg(const QString& txt) = 0;
    void appendMessage(const Message &, bool local = false);

signals:
    void aInfo(const Jid &);
    void aHistory(const Jid &);
    void aVoice(const Jid &);
    void messagesRead(const Jid &);
    void aSend(Message &);
    void aFile(const Jid &);
    void messageAppended(const QString &, QWidget*);

    /**
     * Signals if user (re)started/stopped composing
     */
    void composing(bool);

protected:
    virtual void setShortcuts();

    // reimplemented
    void closeEvent(QCloseEvent *) override;
    void hideEvent(QHideEvent *) override;
    void showEvent(QShowEvent *) override;
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    bool autoSelectContact_;

public slots:
    // reimplemented
    virtual void deactivated() override;
    virtual void activated() override;

    virtual void optionsUpdate();
    void updateContact(const Jid &, bool);
    void incomingMessage(const Message &);
    virtual void updateAvatar() = 0;
    void updateAvatar(const Jid&);

protected slots:
    void doInfo();
    virtual void doHistory();
    virtual void doClear();
    virtual void doFileShare(const QList<Reference> &references, const QString &desc);
    virtual void doSend();
    void doVoice();
    void doFile();

private slots:
    virtual void updatePGP();
    virtual void setPGPEnabled(bool enabled);
    void encryptedMessageSent(int, bool, int, const QString &);
    void setChatState(XMPP::ChatState s);
    void updateIsComposing(bool);
    void setContactChatState(ChatState s);
    void logSelectionChanged();
    void capsChanged(const Jid&);
    void addEmoticon(QString text);
    void initComposing();
    void setComposing();
    void getHistory();

protected slots:
    void checkComposing();

protected:
    // reimplemented
    void invalidateTab() override;

    void updateRealJid();
    void resetComposing();
    void doneSend();
    void holdMessages(bool hold);
    void displayMessage(const MessageView &mv);
    virtual void setLooks();
    void setSelfDestruct(int);
    virtual void chatEditCreated();
    void initHighlighters();

    virtual void initUi() = 0;
    virtual void capsChanged();
    virtual void updateJidWidget(const QList<UserListItem*> &ul, int status, bool fromPresence);
    virtual void contactUpdated(UserListItem* u, int status, const QString& statusString);

    virtual bool isEncryptionEnabled() const;

protected:
    virtual void nicksChanged();

    QString whoNick(bool local) const;

    virtual ChatView* chatView() const = 0;
    virtual ChatEdit* chatEdit() const = 0;

private:
    bool highlightersInstalled_;
    QString dispNick_;
    int status_, priority_;
    QString statusString_;

    void initActions();
    QAction* act_send_;
    QAction* act_scrollup_;
    QAction* act_scrolldown_;
    QAction* act_close_;
    QAction* act_hide_;

    int pending_;
    bool keepOpen_;
    bool warnSend_;

    bool trackBar_;
    void doTrackBar();

    QTimer* selfDestruct_;

    QString key_;
    int transid_;
    Message m_;
    bool lastWasEncrypted_;
    Jid realJid_;

    // Message Events & Chat States
    QTimer* composingTimer_;
    bool isComposing_;
    bool sendComposingEvents_;
    bool historyState;
    QString eventId_;
    ChatState contactChatState_;
    ChatState lastChatState_;
    QList<MessageView> *delayedMessages;

    QList<Reference> fileShareReferences_;
    QString fileShareDesc_;
};

#endif

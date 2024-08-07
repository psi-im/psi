/*
 * chatview_webkit.h - Webkit based chatview
 * Copyright (C) 2010-2017  Sergey Ilinykh
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

#ifndef CHATVIEW_WEBKIT_H
#define CHATVIEW_WEBKIT_H

#include "chatviewcommon.h"
#include "webview.h"

#include <QDateTime>
#include <QFrame>
#include <QPointer>
#include <QWidget>

class ChatEdit;
class ChatView;
class ChatViewPrivate;
class ChatViewTheme;
class MessageView;
class PsiAccount;

namespace XMPP {
class Jid;
}

class ChatView : public QFrame, public ChatViewCommon {
    Q_OBJECT
public:
    ChatView(QWidget *parent);
    ~ChatView();

    void markReceived(QString id);

    // reimplemented
    QSize sizeHint() const;

    void setDialog(QWidget *dialog);
    void setSessionData(bool isMuc, bool isMucPrivate, const XMPP::Jid &jid, const QString name);
    void setAccount(PsiAccount *acc);
    void setLocalNickname(const QString &nickname);

    void contextMenuEvent(QContextMenuEvent *event);
    bool handleCopyEvent(QObject *object, QEvent *event, ChatEdit *chatEdit);

    void dispatchMessage(const MessageView &m);
    void sendJsCode(const QString &js);

    void     clear();
    void     doTrackBar();
    WebView *textWidget();
    QWidget *realTextWidget();
    QObject *jsBridge();

public slots:
    void scrollUp();
    void scrollDown();
    void updateAvatar(const XMPP::Jid &jid, ChatViewCommon::UserType utype);

    void setEncryptionEnabled(bool enabled);

protected:
    // override the tab/esc behavior
    bool focusNextPrevChild(bool next);
    void changeEvent(QEvent *event);
    // void keyPressEvent(QKeyEvent *);

private:
    void outgoingReaction(const QString &messageId, const QString &reaction);

protected slots:
    void psiOptionChanged(const QString &);
    // void autoCopy();

public slots:
    void init();

private slots:
    void sessionInited();

signals:
    void showNickMenu(const QString &);
    void nickInsertClick(const QString &nick);
    void quote(const QString &text);
    void outgoingReactions(const QString &messageId, const QSet<QString> &reactions);
    void outgoingMessageRetraction(const QString &messageId);
    void editMessageRequested(const QString &messageId, const QString &text);
    void forwardMessageRequested(const QString &messageId, const QString &nick, const QString &text);
    void openInfoRequested(const QString &nickname);
    void openChatRequested(const QString &nickname);

private:
    friend class ChatViewPrivate;
    friend class ChatViewJSObject;
    QScopedPointer<ChatViewPrivate> d;
};

#endif // CHATVIEW_WEBKIT_H

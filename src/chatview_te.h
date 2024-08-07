/*
 * chatview_te.h - subclass of PsiTextView to handle various hotkeys
 * Copyright (C) 2001-2010  Justin Karneges, Michail Pishchagin, Sergey Ilinykh
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

#ifndef CHATVIEW_TE_H
#define CHATVIEW_TE_H

#include "chatviewcommon.h"
#include "psitextview.h"
#include "xmpp/jid/jid.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QPointer>
#include <QWidget>

class ChatEdit;
class ChatViewBase;
class ITEAudioController;
class ITEMediaOpener;
class MessageView;

namespace XMPP {
class Jid;
}

class ChatView : public PsiTextView, public ChatViewCommon {
    Q_OBJECT
public:
    ChatView(QWidget *parent);
    ~ChatView();

    void addLogIconsResources();
    void markReceived(QString id);

    // reimplemented
    QSize  sizeHint() const;
    void   clear();
    void   contextMenuEvent(QContextMenuEvent *e);
    QMenu *createStandardContextMenu(const QPoint &position);

    void init();
    void setDialog(QWidget *dialog);
    void setSessionData(bool isMuc, bool isMucPrivate, const XMPP::Jid &jid, const QString name);
    void setLocalNickname(const QString &nickname);

    void insertText(const QString &text, QTextCursor &insertCursor);
    void appendText(const QString &text);
    void dispatchMessage(const MessageView &);
    bool handleCopyEvent(QObject *object, QEvent *event, ChatEdit *chatEdit);

    void      deferredScroll();
    void      doTrackBar();
    ChatView *textWidget();
    QWidget  *realTextWidget();

    void updateAvatar(const XMPP::Jid &jid, ChatViewCommon::UserType utype);
public slots:
    void scrollUp();
    void scrollDown();

    void setEncryptionEnabled(bool enabled);

protected:
    // override the tab/esc behavior
    bool focusNextPrevChild(bool next);
    void keyPressEvent(QKeyEvent *);

    QString formatTimeStamp(const QDateTime &time);
    QString colorString(bool local, bool spooled) const;

    void renderMucMessage(const MessageView &, QTextCursor &insertCursor);
    void renderMessage(const MessageView &, QTextCursor &insertCursor);
    void renderSysMessage(const MessageView &);
    void renderSubject(const MessageView &);
    void renderMucSubject(const MessageView &);
    void renderUrls(const MessageView &);

protected slots:
    void autoCopy();

private slots:
    void slotScroll();

signals:
    void showNickMenu(const QString &);
    void quote(const QString &text);
    void nickInsertClick(const QString &nick);
    void outgoingReactions(const QString &messageId, const QSet<QString> &reactions);
    void outgoingMessageRetraction(const QString &messageId);
    void editMessageRequested(const QString &messageId, const QString &text);
    void forwardMessageRequested(const QString &messageId, const QString &nick, const QString &text);
    void openInfoRequested(const QString &nickname);
    void openChatRequested(const QString &nickname);

private:
    bool              isMuc_;
    bool              isMucPrivate_;
    bool              isEncryptionEnabled_;
    bool              useMessageIcons_;
    int               oldTrackBarPosition;
    XMPP::Jid         jid_;
    QString           name_;
    QString           localNickname_;
    QPointer<QWidget> dialog_;
    QAction          *actQuote_;
};

#endif // CHATVIEW_TE_H

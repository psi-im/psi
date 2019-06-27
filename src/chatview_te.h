/*
 * chatview_te.h - subclass of PsiTextView to handle various hotkeys
 * Copyright (C) 2001-2010  Justin Karneges, Michail Pishchagin, Rion
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

#include <QWidget>
#include <QDateTime>
#include <QPointer>
#include <QContextMenuEvent>

#include "psitextview.h"
#include "chatviewcommon.h"
#include "xmpp/jid/jid.h"

class ChatEdit;
class ChatViewBase;
class MessageView;
class ITEAudioController;
namespace XMPP {
    class Jid;
}

class ChatView : public PsiTextView, public ChatViewCommon
{
    Q_OBJECT
public:
    ChatView(QWidget* parent);
    ~ChatView();

    void addLogIconsResources();
    void markReceived(QString id);

    // reimplemented
    QSize sizeHint() const;
    void clear();
    void contextMenuEvent(QContextMenuEvent *e);
    QMenu* createStandardContextMenu(const QPoint &position);

    void init();
    void setDialog(QWidget* dialog);
    void setSessionData(bool isMuc, bool isMucPrivate, const XMPP::Jid &jid, const QString name);

    void insertText(const QString &text, QTextCursor &insertCursor);
    void appendText(const QString &text);
    void dispatchMessage(const MessageView &);
    bool handleCopyEvent(QObject *object, QEvent *event, ChatEdit *chatEdit);

    void deferredScroll();
    void doTrackBar();
    bool internalFind(QString str, bool startFromBeginning = false);
    ChatView *textWidget();
    QWidget *realTextWidget();

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

    QString replaceMarker(const MessageView &mv) const;
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
    void showNM(const QString&);
    void quote(const QString &text);
    void nickInsertClick(const QString &nick);

private:
    bool isMuc_;
    bool isMucPrivate_;
    bool isEncryptionEnabled_;
    bool useMessageIcons_;
    int  oldTrackBarPosition;
    XMPP::Jid jid_;
    QString name_;
    QPointer<QWidget> dialog_;
    ITEAudioController *voiceMsgCtrl;

    QPixmap logIconSend;
    QPixmap logIconReceive;
    QPixmap logIconDelivered;
    QPixmap logIconSendPgp;
    QPixmap logIconReceivePgp;
    QPixmap logIconDeliveredPgp;
    QPixmap logIconTime;
    QPixmap logIconInfo;
    QPixmap logIconCorrected;
    QPixmap logIconHistory;
    QAction *actQuote_;
};

#endif

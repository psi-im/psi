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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef CHATVIEW_H
#define CHATVIEW_H

#include <QWidget>
#include <QDateTime>
#include <QFrame>
#include <QPointer>
#include "webview.h"
#include "chatviewcommon.h"

class ChatEdit;
class ChatView;
class MessageView;
class PsiAccount;
class ChatViewTheme;
class ChatViewPrivate;
namespace XMPP {
	class Jid;
}

class ChatView : public QFrame, public ChatViewCommon
{
	Q_OBJECT
public:
	ChatView(QWidget* parent);
	~ChatView();

	void markReceived(QString id);

	// reimplemented
	QSize sizeHint() const;

	void setDialog(QWidget* dialog);
	void setSessionData(bool isMuc, const XMPP::Jid &jid, const QString name);
	void setAccount(PsiAccount *acc);

	void contextMenuEvent(QContextMenuEvent* event);
	void sendJsObject(const QVariantMap &);
	bool handleCopyEvent(QObject *object, QEvent *event, ChatEdit *chatEdit);

	void dispatchMessage(const MessageView &m);

	void clear();
	void doTrackBar();
	bool internalFind(QString str, bool startFromBeginning = false);
	WebView * textWidget();
	QWidget * realTextWidget();
	QObject * jsBridge();

public slots:
	void scrollUp();
	void scrollDown();
	void updateAvatar(const XMPP::Jid &jid, UserType utype);

	void setEncryptionEnabled(bool enabled);

protected:
	// override the tab/esc behavior
	bool focusNextPrevChild(bool next);
	void changeEvent(QEvent * event);
	//void keyPressEvent(QKeyEvent *);

protected slots:
	void psiOptionChanged(const QString &);
	//void autoCopy();

public slots:
	void init();

private slots:
	void checkJsBuffer();
	void sessionInited();

signals:
	void showNM(const QString&);
	void nickInsertClick(const QString &nick);

private:
	friend class ChatViewPrivate;
	friend class ChatViewJSObject;
	QScopedPointer<ChatViewPrivate> d;
};

#endif

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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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

#include "advwidget.h"

#include "tabbablewidget.h"


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
	void setJid(const Jid &);
	const QString & getDisplayName();

	// reimplemented
	virtual bool readyToHide();
	virtual TabbableWidget::State state() const;
	virtual int unreadMessageCount() const;
	virtual QString desiredCaption() const;
	virtual void ensureTabbedCorrectly();

public:
	PsiAccount* account() const;

signals:
	void aInfo(const Jid &);
	void aHistory(const Jid &);
	void aVoice(const Jid &);
	void messagesRead(const Jid &);
	void aSend(const Message &);
	void aFile(const Jid &);

	/**
	 * Signals if user (re)started/stopped composing
	 */
	void composing(bool);

protected:
	virtual void setShortcuts();

	// reimplemented
	void closeEvent(QCloseEvent *);
	void hideEvent(QHideEvent *);
	void showEvent(QShowEvent *);
	void dropEvent(QDropEvent* event);
	void dragEnterEvent(QDragEnterEvent* event);
	bool eventFilter(QObject *obj, QEvent *event);

public slots:
	// reimplemented
	virtual void deactivated();
	virtual void activated();

	virtual void optionsUpdate();
	void updateContact(const Jid &, bool);
	void incomingMessage(const Message &);
	virtual void updateAvatar() = 0;
	void updateAvatar(const Jid&);

protected slots:
	void doInfo();
	virtual void doHistory();
	virtual void doClear();
	virtual void doSend();
	void doVoice();
	void doFile();

private slots:
	void setKeepOpenFalse();
	void setWarnSendFalse();
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

protected slots:
	void checkComposing();

protected:
	// reimplemented
	virtual void invalidateTab();

	void resetComposing();
	void doneSend();
	virtual void setLooks();
	void setSelfDestruct(int);
	virtual void chatEditCreated();

	virtual void initUi() = 0;
	virtual void capsChanged();
	virtual void contactUpdated(UserListItem* u, int status, const QString& statusString);

	void appendMessage(const Message &, bool local = false);
	virtual bool isEncryptionEnabled() const;
	virtual void appendSysMsg(const QString& txt) = 0;
	virtual void nicksChanged();

	QString whoNick(bool local) const;

	virtual ChatView* chatView() const = 0;
	virtual ChatEdit* chatEdit() const = 0;

private:
	bool highlightersInstalled_;
	QString dispNick_;
	int status_;
	QString statusString_;

	void initActions();
	QAction* act_send_;
	QAction* act_scrollup_;
	QAction* act_scrolldown_;
	QAction* act_close_;

	int pending_;
	bool keepOpen_;
	bool warnSend_;

	QTimer* selfDestruct_;

	QString key_;
	int transid_;
	Message m_;
	bool lastWasEncrypted_;

	// Message Events & Chat States
	QTimer* composingTimer_;
	bool isComposing_;
	bool sendComposingEvents_;
	QString eventId_;
	ChatState contactChatState_;
	ChatState lastChatState_;
};

#endif

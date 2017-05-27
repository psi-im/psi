/*
 * psicontactmenu.cpp - a PsiContact context menu
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#include "psicontactmenu.h"

#include "psiaccount.h"
#include "resourcemenu.h"
#include "groupmenu.h"

class InviteToGroupChatMenu;

class PsiContactMenu::Private : public QObject
{
	Q_OBJECT

public:
	Private(PsiContactMenu *menu, PsiContact *_contact);

public slots:
	void updateActions();
	void mucHide();
	void mucShow();
	void mucLeave();
	void rename();
	void addContact();
	void removeContact();
	void inviteToGroupchat(PsiAccount *account, QString groupchat);
	void setContactGroup(QString group);
	void block(bool );
	void setAlwaysVisible(bool visible);
	void addAuth();
	void receiveIncomingEvent();
	void sendMessage();
	void openChat();
#ifdef WHITEBOARDING
	void openWhiteboard();
	void openWhiteboardTo(PsiContact*, const XMPP::Jid &jid);
#endif
	void voiceCall();
	void sendFile();
	void transportLogon();
	void transportLogoff();
	void authResend();
	void authRerequest();
	void authRemove();
	void customStatus();
	void pictureAssign();
	void pictureClear();
	void gpgAssignKey();
	void gpgUnassignKey();
	void vcard();
	void history();
	void sendMessageTo(PsiContact*, const XMPP::Jid &jid);
	void openChatTo(PsiContact*, const XMPP::Jid &jid);
	void executeCommand(PsiContact*, const XMPP::Jid &jid);
	void openActiveChat(PsiContact*, const XMPP::Jid &jid);
	void copyJid();
	void updateBlockActionState();
	
public:
	QPointer<PsiContact> contact_;
	PsiContactMenu *menu_;

	QAction *renameAction_;
	QAction *removeAction_;

	QAction *addAuthAction_;
	QAction *transportLogonAction_;
	QAction *transportLogoffAction_;
	QAction *receiveIncomingEventAction_;
	QMenu *msgMenu_;
	QAction *sendMessageAction_;
	QMenu *sendMessageToMenu_;
	QAction *openChatAction_;
	QMenu *openChatToMenu_;
#ifdef WHITEBOARDING
	QAction *openWhiteboardAction_;
	QMenu *openWhiteboardToMenu_;
#endif
	ResourceMenu *executeCommandMenu_;
	ResourceMenu *activeChatsMenu_;
	QAction *voiceCallAction_;
	QAction *sendFileAction_;
	QAction *customStatusAction_;
	InviteToGroupChatMenu *inviteToGroupchatMenu_;
	GroupMenu *groupMenu_;
	QMenu *authMenu_;
	QAction *authResendAction_;
	QAction *authRerequestAction_;
	QAction *authRemoveAction_;
	QMenu *pictureMenu_;
	QAction *pictureAssignAction_;
	QAction *pictureClearAction_;
	QAction *gpgAssignKeyAction_;
	QAction *gpgUnassignKeyAction_;
	QAction *vcardAction_;
	QAction *historyAction_;
	QAction *mucHideAction_;
	QAction *mucShowAction_;
	QAction *mucLeaveAction_;
	QAction *blockAction_;
	QAction *visibleAction_;
	QAction *_copyUserJid;
	QAction *_copyMucJid;
	QAction *_separator1;
	QAction *_separator2;
	QMenu *_advancedMenu;
};

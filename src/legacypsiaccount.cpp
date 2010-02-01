/*
 * legacypsiaccount.h - PsiAccount with ContactProfile compatibility
 * Copyright (C) 2009-2010  Michail Pishchagin
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "legacypsiaccount.h"

#include "psicon.h"
#include "contactview.h"

LegacyPsiAccount::LegacyPsiAccount(const UserAccount &acc, PsiContactList *parent, CapsRegistry* capsRegistry, TabManager *tabManager)
	: PsiAccount(acc, parent, capsRegistry, tabManager)
	, cp_(0)
{
}

LegacyPsiAccount::~LegacyPsiAccount()
{
	delete cp_;
}

void LegacyPsiAccount::init()
{
	cp_ = new ContactProfile(this, name(), psi()->contactView());
	connect(cp_, SIGNAL(actionDefault(const Jid &)),SLOT(actionDefault(const Jid &)));
	connect(cp_, SIGNAL(actionRecvEvent(const Jid &)),SLOT(actionRecvEvent(const Jid &)));
	connect(cp_, SIGNAL(actionSendMessage(const Jid &)),SLOT(actionSendMessage(const Jid &)));
	connect(cp_, SIGNAL(actionSendMessage(const QList<XMPP::Jid> &)),SLOT(actionSendMessage(const QList<XMPP::Jid> &)));
	connect(cp_, SIGNAL(actionSendUrl(const Jid &)),SLOT(actionSendUrl(const Jid &)));
	connect(cp_, SIGNAL(actionRemove(const Jid &)),SLOT(actionRemove(const Jid &)));
	connect(cp_, SIGNAL(actionRename(const Jid &, const QString &)),SLOT(actionRename(const Jid &, const QString &)));
	connect(cp_, SIGNAL(actionGroupRename(const QString &, const QString &)),SLOT(actionGroupRename(const QString &, const QString &)));
	connect(cp_, SIGNAL(actionHistory(const Jid &)),SLOT(actionHistory(const Jid &)));
	connect(cp_, SIGNAL(actionOpenChat(const Jid &)),SLOT(actionOpenChat(const Jid &)));
	connect(cp_, SIGNAL(actionOpenChatSpecific(const Jid &)),SLOT(actionOpenChatSpecific(const Jid &)));
#ifdef WHITEBOARDING
	connect(cp_, SIGNAL(actionOpenWhiteboard(const Jid &)),SLOT(actionOpenWhiteboard(const Jid &)));
	connect(cp_, SIGNAL(actionOpenWhiteboardSpecific(const Jid &)),SLOT(actionOpenWhiteboardSpecific(const Jid &)));
#endif
	connect(cp_, SIGNAL(actionAgentSetStatus(const Jid &, Status &)),SLOT(actionAgentSetStatus(const Jid &, Status &)));
	connect(cp_, SIGNAL(actionInfo(const Jid &)),SLOT(actionInfo(const Jid &)));
	connect(cp_, SIGNAL(actionAuth(const Jid &)),SLOT(actionAuth(const Jid &)));
	connect(cp_, SIGNAL(actionAuthRequest(const Jid &)),SLOT(actionAuthRequest(const Jid &)));
	connect(cp_, SIGNAL(actionAuthRemove(const Jid &)),SLOT(actionAuthRemove(const Jid &)));
	connect(cp_, SIGNAL(actionAdd(const Jid &)),SLOT(actionAdd(const Jid &)));
	connect(cp_, SIGNAL(actionGroupAdd(const Jid &, const QString &)),SLOT(actionGroupAdd(const Jid &, const QString &)));
	connect(cp_, SIGNAL(actionGroupRemove(const Jid &, const QString &)),SLOT(actionGroupRemove(const Jid &, const QString &)));
	connect(cp_, SIGNAL(actionVoice(const Jid &)),SLOT(actionVoice(const Jid &)));
	connect(cp_, SIGNAL(actionSendFile(const Jid &)),SLOT(actionSendFile(const Jid &)));
	connect(cp_, SIGNAL(actionSendFiles(const Jid &, const QStringList&)),SLOT(actionSendFiles(const Jid &, const QStringList&)));
	connect(cp_, SIGNAL(actionExecuteCommand(const Jid &, const QString&)),SLOT(actionExecuteCommand(const Jid &, const QString&)));
	connect(cp_, SIGNAL(actionExecuteCommandSpecific(const Jid &, const QString&)),SLOT(actionExecuteCommandSpecific(const Jid &, const QString&)));
	connect(cp_, SIGNAL(actionSetMood()),SLOT(actionSetMood()));
	connect(cp_, SIGNAL(actionSetAvatar()),SLOT(actionSetAvatar()));
	connect(cp_, SIGNAL(actionUnsetAvatar()),SLOT(actionUnsetAvatar()));
	connect(cp_, SIGNAL(actionDisco(const Jid &, const QString &)),SLOT(actionDisco(const Jid &, const QString &)));
	connect(cp_, SIGNAL(actionInvite(const Jid &, const QString &)),SLOT(actionInvite(const Jid &, const QString &)));
	connect(cp_, SIGNAL(actionAssignKey(const Jid &)),SLOT(actionAssignKey(const Jid &)));
	connect(cp_, SIGNAL(actionUnassignKey(const Jid &)),SLOT(actionUnassignKey(const Jid &)));
}

ContactProfile* LegacyPsiAccount::contactProfile() const
{
	return cp_;
}

void LegacyPsiAccount::setUserAccount(const UserAccount &acc)
{
	PsiAccount::setUserAccount(acc);
	cp_->setName(name());
}

void LegacyPsiAccount::stateChanged()
{
	PsiAccount::stateChanged();

	if(loggedIn()) {
		cp_->setState(makeSTATUS(status()));
	}
	else {
		if(isActive()) {
			cp_->setState(-1);
			if(usingSSL())
				cp_->setUsingSSL(true);
			else
				cp_->setUsingSSL(false);
		}
		else {
			cp_->setState(STATUS_OFFLINE);
			cp_->setUsingSSL(false);
		}
	}
}

void LegacyPsiAccount::profileUpdateEntry(const UserListItem& u)
{
	PsiAccount::profileUpdateEntry(u);
	cp_->updateEntry(u);
}

void LegacyPsiAccount::profileRemoveEntry(const Jid& jid)
{
	PsiAccount::profileRemoveEntry(jid);
	cp_->removeEntry(jid);
}

void LegacyPsiAccount::profileAnimateNick(const Jid& jid)
{
	PsiAccount::profileAnimateNick(jid);
	cp_->animateNick(jid);
}

void LegacyPsiAccount::profileSetAlert(const Jid& jid, const PsiIcon* icon)
{
	PsiAccount::profileSetAlert(jid, icon);
	cp_->setAlert(jid, icon);
}

void LegacyPsiAccount::profileClearAlert(const Jid& jid)
{
	PsiAccount::profileClearAlert(jid);
	cp_->clearAlert(jid);
}

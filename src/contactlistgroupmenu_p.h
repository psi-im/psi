/*
 * contactlistgroupmenu_p.h - context menu for contact list groups
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
 * Copyright (C) 2017  Ivan Romanov <drizt@land.ru>
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

#pragma once

#include "contactlistgroupmenu.h"

#include "contactlistitem.h"
#include "xmpp_tasks.h"
#include "contactlistitem.h"
#include "contactlistmodel.h"
#include "groupchatdlg.h"
#include "iconaction.h"
#include "iconaction.h"
#include "iconset.h"
#include "psiaccount.h"
#include "psicontact.h"
#include "psicontactlist.h"
#include "psioptions.h"
#include "shortcutmanager.h"
#include "statusdlg.h"

#include <QObject>

class ContactListGroupMenu::Private : public QObject
{
	Q_OBJECT

public:
	Private(ContactListGroupMenu *menu, ContactListItem *item);

public slots:
	void updateActions();
	void mucHide();
	void mucShow();
	void mucLeave();
	void rename();
	void removeGroupAndContacts();
	void authResend();
	void authRequest();
	void authRemove();
	void customStatus();
	void setStatusFromDialog(const QList<XMPP::Jid> &j, const Status &s);
	void removeGroupWithoutContacts();
	void sendMessage();
	void actHide(bool hide);

public:
	ContactListGroupMenu *q;
	ContactListItem *group;
	QAction *renameAction_;
	QAction *removeGroupAndContactsAction_;
	QAction *sendMessageAction_;
	QAction *removeGroupWithoutContactsAction_;
	QMenu *authMenu_;
	QAction *actionAuth_;
	QAction *actionAuthRequest_;
	QAction *actionAuthRemove_;
	QAction *actionCustomStatus_;
	QAction *actionMucHide_;
	QAction *actionMucShow_;
	QAction *actionMucLeave_;
	QAction *actionHide_;
};

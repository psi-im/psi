/*
 * resourcemenu.cpp - helper class for displaying contact's resources
 * Copyright (C) 2006-2010  Michail Pishchagin
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

#include "resourcemenu.h"

#include "psiiconset.h"
#include "userlist.h"
#include "xmpp_status.h"
#include "psicontact.h"
#include "psiaccount.h"

/**
 * \class ResourceMenu
 * Helper class that displays available resources using QMenu.
 */

ResourceMenu::ResourceMenu(QWidget *parent)
	: QMenu(parent)
	, activeChatsMode_(false)
{
}

ResourceMenu::ResourceMenu(const QString& title, PsiContact* contact, QWidget* parent)
	: QMenu(parent)
	, contact_(contact)
	, activeChatsMode_(false)
{
	setTitle(title);

	Q_ASSERT(contact);
	connect(contact_, SIGNAL(updated()), SLOT(contactUpdated()));
	contactUpdated();
}

/**
 * Helper function to add resource to the menu.
 */
void ResourceMenu::addResource(const UserResource &r)
{
	addResource(r.status().type(), r.name());
}

/**
 * Helper function to add resource to the menu.
 */
void ResourceMenu::addResource(int status, QString name)
{
	QString rname = name;
	if(rname.isEmpty())
		rname = tr("[blank]");

	//rname += " (" + status2txt(status) + ")";

	QAction* action = new QAction(PsiIconset::instance()->status(status).icon(), rname, this);
	addAction(action);
	action->setProperty("resource", QVariant(name));
	connect(action, SIGNAL(activated()), SLOT(actionActivated()));
}

void ResourceMenu::actionActivated()
{
	QAction* action = static_cast<QAction*>(sender());
	emit resourceActivated(action->property("resource").toString());

	if (contact_) {
		XMPP::Jid jid(contact_->jid());
		jid = jid.withResource(action->property("resource").toString());
		emit resourceActivated(contact_, jid);
	}
}

#ifndef NEWCONTACTLIST
// FIXME: Deprecate this function and move to signal-based calls
void ResourceMenu::addResource(const UserResource &r, int id)
{
	addResource(r.status().type(), r.name(), id);
}

// FIXME: Deprecate this function and move to signal-based calls
void ResourceMenu::addResource(int status, QString name, int id)
{
	QString rname = name;
	if(rname.isEmpty())
		rname = tr("[blank]");

	//rname += " (" + status2txt(status) + ")";

	insertItem(PsiIconset::instance()->status(status).icon(), rname, id);
}
#endif

void ResourceMenu::contactUpdated()
{
	if (!contact_)
		return;
	if (isVisible())
		return;
	clear();

	if (!activeChatsMode_) {
		foreach(const UserResource& resource, contact_->userResourceList())
			addResource(resource);
	}
	else {
		foreach(QString resourceName, contact_->account()->hiddenChats(contact_->jid())) {
			XMPP::Status::Type status;
			const UserResourceList &rl = contact_->userResourceList();
			UserResourceList::ConstIterator uit = rl.find(resourceName);
			if (uit != rl.end() || (uit = rl.priority()) != rl.end())
				status = makeSTATUS((*uit).status());
			else
				status = XMPP::Status::Offline;
			addResource(status, resourceName);
		}
	}
}

bool ResourceMenu::activeChatsMode() const
{
	return activeChatsMode_;
}

void ResourceMenu::setActiveChatsMode(bool activeChatsMode)
{
	activeChatsMode_ = activeChatsMode;
	contactUpdated();
}

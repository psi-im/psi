/*
 * statusmenu.cpp - helper class that displays available statuses using QMenu
 * Copyright (C) 2008-2010  Michail Pishchagin
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

#include "statusmenu.h"

#include "psioptions.h"
#include "psiiconset.h"
#include "psiaccount.h"
#include "xmpp_status.h"
#include "common.h"

/**
 * \class StatusMenu
 * Helper class that displays available statuses using QMenu.
 */

StatusMenu::StatusMenu(QWidget* parent)
	: QMenu(parent), currentStatus_(XMPP::Status::Offline)
{
	addStatus(XMPP::Status::Online);
	if (PsiOptions::instance()->getOption("options.ui.menu.status.chat").toBool())
		addStatus(XMPP::Status::FFC);
	addSeparator();
	addStatus(XMPP::Status::Away);
	if (PsiOptions::instance()->getOption("options.ui.menu.status.xa").toBool())
		addStatus(XMPP::Status::XA);
	addStatus(XMPP::Status::DND);
	if (PsiOptions::instance()->getOption("options.ui.menu.status.invisible").toBool()) {
		addSeparator();
		addStatus(XMPP::Status::Invisible);
	}
#ifndef YAPSI_ACTIVEX_SERVER
	addSeparator();
	addStatus(XMPP::Status::Offline);
#endif
}

void StatusMenu::setStatus(XMPP::Status::Type status)
{
	if (currentStatus_ == status)
		return;

	currentStatus_ = status;
	foreach(QAction* action, actions())
		action->setChecked(actionStatus(action) == status);
}

XMPP::Status::Type StatusMenu::actionStatus(const QAction* action) const
{
	Q_ASSERT(action);
	return static_cast<XMPP::Status::Type>(action->property("type").toInt());
}

void StatusMenu::addStatus(XMPP::Status::Type type)
{
	QAction* action = new QAction(status2txt(type), this);
	action->setCheckable(true);
	action->setChecked(currentStatus_ == type);
	action->setIcon(PsiIconset::instance()->status(type).icon());
	action->setProperty("type", QVariant(type));
	connect(action, SIGNAL(activated()), SLOT(actionActivated()));
	addAction(action);
}

void StatusMenu::actionActivated()
{
	QAction* action = static_cast<QAction*>(sender());
	setStatus(actionStatus(action));
	emit statusChanged(currentStatus_);
}

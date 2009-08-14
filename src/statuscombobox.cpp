/*
 * statuscombobox.cpp - helper class that displays available statuses using QComboBox
 * Copyright (C) 2008  Maciej Niedzielski
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

#include "statuscombobox.h"

#include "psioptions.h"
#include "common.h"

/**
 * \brief Constructs new StatusComboBox
 * \param parent, widget's parent
 * \param type, selected status type just after creation
 */
StatusComboBox::StatusComboBox(QWidget* parent, XMPP::Status::Type type)
	: QComboBox(parent)
{
	if (PsiOptions::instance()->getOption("options.ui.menu.status.chat").toBool()) {
		addStatus(XMPP::Status::FFC);
	}
	addStatus(XMPP::Status::Online);
	addStatus(XMPP::Status::Away);
	if (PsiOptions::instance()->getOption("options.ui.menu.status.xa").toBool()) {
		addStatus(XMPP::Status::XA);
	}
	addStatus(XMPP::Status::DND);
	if (PsiOptions::instance()->getOption("options.ui.menu.status.invisible").toBool()) {
		addStatus(XMPP::Status::Invisible);
	}
	addStatus(XMPP::Status::Offline);
	
	connect(this, SIGNAL(currentIndexChanged(int)), SLOT(onCurrentIndexChanged(int)));

	setStatus(type);
}

/**
 * \brief Set currently selected status
 */
void StatusComboBox::setStatus(XMPP::Status::Type type)
{
	if (type == XMPP::Status::FFC && !PsiOptions::instance()->getOption("options.ui.menu.status.chat").toBool())
		type = XMPP::Status::Online;
	else if (type == XMPP::Status::XA && !PsiOptions::instance()->getOption("options.ui.menu.status.xa").toBool())
		type = XMPP::Status::Away;
	else if (type == XMPP::Status::Invisible && !PsiOptions::instance()->getOption("options.ui.menu.status.invisible").toBool())
		type = XMPP::Status::Offline;

	for (int i = 0; i < count(); ++i) {
		if (static_cast<XMPP::Status::Type>(itemData(i).toInt()) == type) {
			setCurrentIndex(i);
			break;
		}
	}
}

/**
 * \brief Read currently selected status
 */
XMPP::Status::Type StatusComboBox::status() const
{
	return static_cast<XMPP::Status::Type>(itemData(currentIndex()).toInt());
}

// private

void StatusComboBox::addStatus(XMPP::Status::Type status){
	addItem(status2txt(status), status);
}

void StatusComboBox::onCurrentIndexChanged(int index)
{
	emit statusChanged(static_cast<XMPP::Status::Type>(itemData(index).toInt()));
}

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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "statuscombobox.h"

#include "psioptions.h"
#include "psiiconset.h"
#include "common.h"

/**
 * \brief Constructs new StatusComboBox
 * \param parent, widget's parent
 * \param type, selected status type just after creation
 */
StatusComboBox::StatusComboBox(QWidget* parent, XMPP::Status::Type type)
	: QComboBox(parent)
{
	addStatus(XMPP::Status::Online);
	if (PsiOptions::instance()->getOption("options.ui.menu.status.chat").toBool()) {
		addStatus(XMPP::Status::FFC);
	}
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
#ifdef Q_OS_MAC
	addItem(status2txt(status), status);
#else
	addItem(PsiIconset::instance()->status(status).icon(), status2txt(status), status);
#endif
}

void StatusComboBox::onCurrentIndexChanged(int index)
{
	emit statusChanged(static_cast<XMPP::Status::Type>(itemData(index).toInt()));
}

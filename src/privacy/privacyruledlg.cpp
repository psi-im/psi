/*
 * privacyruledlg.cpp
 * Copyright (C) 2006  Remko Troncon
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
 
#include "privacyruledlg.h"
#include "privacylistitem.h"

PrivacyRuleDlg::PrivacyRuleDlg()
{
	ui_.setupUi(this);

	ui_.cb_action->addItem(tr("Deny"), PrivacyListItem::Deny);
	ui_.cb_action->addItem(tr("Allow"), PrivacyListItem::Allow);

	ui_.cb_type->addItem(tr("JID"), PrivacyListItem::JidType);
	ui_.cb_type->addItem(tr("Group"), PrivacyListItem::GroupType);
	ui_.cb_type->addItem(tr("Subscription"), PrivacyListItem::SubscriptionType);
	ui_.cb_type->addItem(tr("*"), PrivacyListItem::FallthroughType);

	connect(ui_.cb_type,SIGNAL(currentIndexChanged(const QString&)),SLOT(type_selected(const QString&)));
	connect(ui_.pb_cancel,SIGNAL(clicked()),SLOT(reject()));
	connect(ui_.pb_ok,SIGNAL(clicked()),SLOT(accept()));
}

void PrivacyRuleDlg::setRule(const PrivacyListItem& item)
{
	// Type
	if (item.type() == PrivacyListItem::SubscriptionType) {
		ui_.cb_type->setCurrentIndex(ui_.cb_type->findData(item.type()));
		ui_.cb_value->setCurrentIndex(ui_.cb_value->findData(item.value()));
	} else {
		ui_.cb_type->setCurrentIndex(ui_.cb_type->findData(item.type())); // clears combo as well
		ui_.cb_value->insertItem(0, item.value());
	}

	// Action
	ui_.cb_action->setCurrentIndex(ui_.cb_action->findData(item.action()));

	// Selection
	ui_.ck_messages->setChecked(item.message());
	ui_.ck_queries->setChecked(item.iq());
	ui_.ck_presenceIn->setChecked(item.presenceIn());
	ui_.ck_presenceOut->setChecked(item.presenceOut());
}

PrivacyListItem PrivacyRuleDlg::rule() const
{
	PrivacyListItem item;

	// Type & value
	PrivacyListItem::Type t = (PrivacyListItem::Type)ui_.cb_type->itemData(ui_.cb_type->currentIndex()).toInt();
	if(t == PrivacyListItem::SubscriptionType) {
		item.setType(t);
		item.setValue(ui_.cb_value->itemData(ui_.cb_value->currentIndex()).toString());
	}
	else {
		item.setType(t);
		item.setValue(ui_.cb_value->currentText());
	}

	// Action
	item.setAction((PrivacyListItem::Action)ui_.cb_action->itemData(ui_.cb_action->currentIndex()).toInt());

	// Selection
	item.setMessage(ui_.ck_messages->isChecked());
	item.setIQ(ui_.ck_queries->isChecked());
	item.setPresenceIn(ui_.ck_presenceIn->isChecked());
	item.setPresenceOut(ui_.ck_presenceOut->isChecked());

	return item;
}

void PrivacyRuleDlg::type_selected(const QString& type)
{
	ui_.cb_value->clear();
	ui_.cb_value->setItemText(ui_.cb_value->currentIndex(), "");
	PrivacyListItem::Type t = (PrivacyListItem::Type)ui_.cb_type->itemData(ui_.cb_type->currentIndex()).toInt();
	if (t == PrivacyListItem::SubscriptionType) {
		ui_.cb_value->addItem(tr("None"), "none");
		ui_.cb_value->addItem(tr("Both"), "both");
		ui_.cb_value->addItem(tr("From"), "from");
		ui_.cb_value->addItem(tr("To"), "to");
		ui_.cb_value->setEditable(false);
	}
	else {
		ui_.cb_value->setEditable(true);
	}

	if (type == tr("*")) {
		ui_.cb_value->setEnabled(false);
	}
	else {
		ui_.cb_value->setEnabled(true);
	}
}

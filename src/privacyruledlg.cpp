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
	
	connect(ui_.cb_type,SIGNAL(currentIndexChanged(const QString&)),SLOT(type_selected(const QString&)));
	connect(ui_.pb_cancel,SIGNAL(clicked()),SLOT(reject()));
	connect(ui_.pb_ok,SIGNAL(clicked()),SLOT(accept()));
}

void PrivacyRuleDlg::setRule(const PrivacyListItem& item)
{
	// Type
	if (item.type() == PrivacyListItem::SubscriptionType) {
		ui_.cb_type->setCurrentIndex(ui_.cb_type->findText(tr("Subscription")));
		if (item.value() == "both") {
			ui_.cb_type->setCurrentIndex(ui_.cb_type->findText(tr("Both")));
		}
		else if (item.value() == "none") {
			ui_.cb_type->setCurrentIndex(ui_.cb_type->findText(tr("None")));
		}
		else if (item.value() == "from") {
			ui_.cb_type->setCurrentIndex(ui_.cb_type->findText(tr("From")));
		}
		else if (item.value() == "to") {
			ui_.cb_type->setCurrentIndex(ui_.cb_type->findText(tr("To")));
		}
	}
	else {
		if (item.type() == PrivacyListItem::JidType) {
			ui_.cb_type->setCurrentIndex(ui_.cb_type->findText(tr("JID")));
		}
		else if (item.type() == PrivacyListItem::GroupType) {
			ui_.cb_type->setCurrentIndex(ui_.cb_type->findText(tr("Group")));
		}
		else {
			ui_.cb_type->setCurrentIndex(ui_.cb_type->findText(tr("*")));
		}
		ui_.cb_value->setCurrentText(item.value());
	}

	// Action
	if (item.action() == PrivacyListItem::Allow) {
		ui_.cb_action->setCurrentIndex(ui_.cb_action->findText(tr("Allow")));
	}
	else {
		ui_.cb_action->setCurrentIndex(ui_.cb_action->findText(tr("Deny")));
	}

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
	if(ui_.cb_type->currentText() == tr("Subscription")) {
		item.setType(PrivacyListItem::SubscriptionType);
		if (ui_.cb_value->currentText() == tr("To")) 
			item.setValue("to");
		else if (ui_.cb_value->currentText() == tr("From")) 
			item.setValue("from");
		else if (ui_.cb_value->currentText() == tr("Both")) 
			item.setValue("both");
		else if (ui_.cb_value->currentText() == tr("None")) 
			item.setValue("none");
	}
	else {
		if (ui_.cb_type->currentText() == tr("JID")) {
			item.setType(PrivacyListItem::JidType);
		}
		else if (ui_.cb_type->currentText() == tr("Group")) {
			item.setType(PrivacyListItem::GroupType);
		}
		else {
			item.setType(PrivacyListItem::FallthroughType);
		}
		item.setValue(ui_.cb_value->currentText());
	}
	
	// Action
	if(ui_.cb_action->currentText() == tr("Deny")) {
		item.setAction(PrivacyListItem::Deny);
	}
	else {
		item.setAction(PrivacyListItem::Allow);
	}
	
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
	ui_.cb_value->setCurrentText("");
	if (type == tr("Subscription")) {
		ui_.cb_value->insertItem(tr("None"));
		ui_.cb_value->insertItem(tr("Both"));
		ui_.cb_value->insertItem(tr("From"));
		ui_.cb_value->insertItem(tr("To"));
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

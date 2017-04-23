/*
 * mucconfigdlg.cpp
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QVariant>
#include <QMessageBox>
#include <QInputDialog>
#include <QMap>
#include <QScrollArea>

#include "mucmanager.h"
#include "mucaffiliationsmodel.h"
#include "mucaffiliationsproxymodel.h"
#include "mucconfigdlg.h"
#include "xdata_widget.h"
#include "infodlg.h"
#include "vcardfactory.h"
#include "xmpp_vcard.h"
#include "psiaccount.h"

using namespace XMPP;

MUCConfigDlg::MUCConfigDlg(MUCManager* manager, QWidget* parent)
	: QDialog(parent), manager_(manager)
{
	setAttribute(Qt::WA_DeleteOnClose);
	ui_.setupUi(this);
	setModal(false);

	QVBoxLayout *data_layout = new QVBoxLayout(ui_.pg_general_data);
	data_layout->setMargin(0);

	data_container_ = new QScrollArea(ui_.pg_general_data);
	data_layout->addWidget(data_container_);
	data_container_->setWidgetResizable(true);

	connect(ui_.tabs,SIGNAL(currentChanged(int)),SLOT(currentTabChanged(int)));
	connect(ui_.pb_apply,SIGNAL(clicked()),SLOT(apply()));
	ui_.pb_close->setDefault(true);

	// General tab
	data_ = NULL;
	connect(manager_, SIGNAL(getConfiguration_success(const XData&)), SLOT(getConfiguration_success(const XData&)));
	connect(manager_, SIGNAL(getConfiguration_error(int, const QString&)), SLOT(getConfiguration_error(int, const QString&)));
	connect(manager_, SIGNAL(setConfiguration_success()), SLOT(setConfiguration_success()));
	connect(manager_, SIGNAL(setConfiguration_error(int, const QString&)), SLOT(setConfiguration_error(int, const QString&)));
	connect(manager_, SIGNAL(getItemsByAffiliation_success(MUCItem::Affiliation, const QList<MUCItem>&)), SLOT(getItemsByAffiliation_success(MUCItem::Affiliation, const QList<MUCItem>&)));
	connect(manager_, SIGNAL(setItems_success()), SLOT(setItems_success()));
	connect(manager_, SIGNAL(setItems_error(int, const QString&)), SLOT(setItems_error(int, const QString&)));
	connect(manager_, SIGNAL(getItemsByAffiliation_error(MUCItem::Affiliation, int, const QString&)), SLOT(getItemsByAffiliation_error(MUCItem::Affiliation, int, const QString&)));
	connect(manager_, SIGNAL(destroy_success()), SLOT(destroy_success()));
	connect(manager_, SIGNAL(destroy_error(int, const QString&)), SLOT(destroy_error(int, const QString&)));
	connect(ui_.pb_destroy, SIGNAL(clicked()), SLOT(destroy()));

	// Affiliations tab
	ui_.pb_add->setEnabled(false);
	ui_.pb_remove->setEnabled(false);
	connect(ui_.tv_affiliations,SIGNAL(addEnabled(bool)),ui_.pb_add,SLOT(setEnabled(bool)));
	connect(ui_.tv_affiliations,SIGNAL(removeEnabled(bool)),ui_.pb_remove,SLOT(setEnabled(bool)));
	connect(ui_.pb_add,SIGNAL(clicked()),SLOT(add()));
	connect(ui_.pb_remove,SIGNAL(clicked()),ui_.tv_affiliations,SLOT(removeCurrent()));
	connect(ui_.le_filter, SIGNAL(textChanged(const QString&)), SLOT(applyFilter(const QString&)));

	affiliations_model_ = new MUCAffiliationsModel();
	affiliations_proxy_model_ = new MUCAffiliationsProxyModel(affiliations_model_);
	affiliations_proxy_model_->setSourceModel(affiliations_model_);
	ui_.tv_affiliations->setModel(affiliations_proxy_model_);

	for (int i = 0; i < affiliations_proxy_model_->rowCount(); i++)
		ui_.tv_affiliations->setExpanded(affiliations_proxy_model_->index(i, 0), true);

	// Roles & affiliations
	setRole(MUCItem::NoRole);
	setAffiliation(MUCItem::NoAffiliation);
}

MUCConfigDlg::~MUCConfigDlg()
{
	delete affiliations_model_;
}

void MUCConfigDlg::setRoleAffiliation(MUCItem::Role role, MUCItem::Affiliation affiliation)
{
	if (role_ != role) {
		setRole(role);
	}
	if (affiliation_ != affiliation) {
		setAffiliation(affiliation);
	}
}

void MUCConfigDlg::setRole(MUCItem::Role role)
{
	role_ = role;
}

void MUCConfigDlg::setAffiliation(MUCItem::Affiliation affiliation)
{
	affiliation_ = affiliation;
	ui_.tabs->setTabEnabled(ui_.tabs->indexOf(ui_.tab_general),affiliation == MUCItem::Owner);
	if (ui_.tabs->currentWidget() == ui_.tab_general) {
		refreshGeneral();
	}
	else if (ui_.tabs->currentWidget() == ui_.tab_affiliations) {
		refreshAffiliations();
	}
}

void MUCConfigDlg::refreshGeneral()
{
	if (affiliation_ == MUCItem::Owner) {
		delete data_;
		data_ = NULL;
		ui_.lb_general_message->setText(tr("Requesting room configuration ..."));
		ui_.sw_general->setCurrentWidget(ui_.pg_general_message);
		ui_.busy->start();
		manager_->getConfiguration();
	}
	else {
		ui_.lb_general_message->setText(tr("You are not an owner of this room"));
		ui_.sw_general->setCurrentWidget(ui_.pg_general_message);
		if (ui_.tabs->currentWidget() == ui_.tab_general)
			ui_.tabs->setCurrentWidget(ui_.tab_affiliations);
	}
}

void MUCConfigDlg::refreshAffiliations()
{
	affiliations_model_->resetAffiliationLists();
	if (affiliation_ >= MUCItem::Member) {
		ui_.busy->start();
		pending_requests_.clear();
		pending_requests_ += MUCItem::Outcast;
		manager_->getItemsByAffiliation(MUCItem::Outcast);
		pending_requests_ += MUCItem::Member;
		manager_->getItemsByAffiliation(MUCItem::Member);
		pending_requests_ += MUCItem::Admin;
		manager_->getItemsByAffiliation(MUCItem::Admin);
		pending_requests_ += MUCItem::Owner;
		manager_->getItemsByAffiliation(MUCItem::Owner);
	}
	ui_.tv_affiliations->clearSelection();
}

void MUCConfigDlg::refreshVcard()
{
	if (!ui_.tab_vcard->layout()) {
		QVBoxLayout *layout = new QVBoxLayout;

		const VCard vcard = VCardFactory::instance()->vcard(manager_->room());
		vcard_ = new InfoWidget(InfoWidget::MucAdm, manager_->room(), vcard, manager_->account());
		layout->addWidget(vcard_);
		ui_.tab_vcard->setLayout(layout);
		connect(vcard_, SIGNAL(busy()), ui_.busy, SLOT(start()));
		connect(vcard_, SIGNAL(released()), ui_.busy, SLOT(stop()));
	}
	vcard_->doRefresh();
}

void MUCConfigDlg::add()
{
	bool ok;
	QString text = QInputDialog::getText(this, tr("Add affiliation"), tr("Enter the JID of the user:"), QLineEdit::Normal, "", &ok);
	if (ok && ui_.tv_affiliations->currentIndex().isValid()) {
		if (!text.isEmpty()) {

			QModelIndex index = affiliations_proxy_model_->mapToSource(ui_.tv_affiliations->currentIndex());

			if (index.parent().isValid())
				index = index.parent();

			if (!index.parent().isValid()) {
				XMPP::Jid jid(text);
				if (jid.isValid()) {

					// TODO: Check if the user is already in the list

					int row = affiliations_model_->rowCount(index);
					affiliations_model_->insertRows(row,1,index);
					QModelIndex newIndex = affiliations_model_->index(row,0,index);
					affiliations_model_->setData(newIndex, QVariant(jid.bare()));
					ui_.tv_affiliations->setCurrentIndex(affiliations_proxy_model_->mapFromSource(newIndex));
					return;
				}
			}
		}

		QMessageBox::critical(this, tr("Error"), tr("You have entered an invalid JID."));
	}
}

void MUCConfigDlg::apply()
{
	if (ui_.tabs->currentWidget() == ui_.tab_general) {
	   	if (affiliation_ == MUCItem::Owner && data_) {
			XData data;
			data.setFields(data_->fields());
			ui_.busy->start();
			manager_->setConfiguration(data);
		}
	}
	else if (ui_.tabs->currentWidget() == ui_.tab_affiliations) {
		QList<MUCItem> changes = affiliations_model_->changes();
		if (!changes.isEmpty()) {
			ui_.busy->start();
			manager_->setItems(changes);
		}
	}
	else if (ui_.tabs->currentWidget() == ui_.tab_vcard) {
		vcard_->publish();
	}
}

void MUCConfigDlg::destroy()
{
	int i = QMessageBox::warning(this, tr("Destroy room"), tr("Are you absolutely certain you want to destroy this room?"),tr("Yes"),tr("No"));
	if (i == 0) {
		manager_->destroy();
	}
}

void MUCConfigDlg::currentTabChanged(int)
{
	ui_.busy->stop();
	if (ui_.tabs->currentWidget() == ui_.tab_affiliations) {
		refreshAffiliations();
	} else if (ui_.tabs->currentWidget() == ui_.tab_general) {
		refreshGeneral();
	} else {
		refreshVcard();
	}

}

void MUCConfigDlg::applyFilter(const QString& s)
{
	affiliations_proxy_model_->setFilterFixedString(s);
}

void MUCConfigDlg::getConfiguration_success(const XData& d)
{
	if (affiliation_ == MUCItem::Owner) {
		ui_.busy->stop();
		delete data_;
		data_ = new XDataWidget(manager_->account()->psi(), ui_.pg_general_data, manager_->client(), manager_->room());
		data_->setForm(d, false);
		data_container_->setWidget(data_);
		data_container_->updateGeometry();
		ui_.sw_general->setCurrentWidget(ui_.pg_general_data);
	}
}

void MUCConfigDlg::getConfiguration_error(int, const QString& e)
{
	ui_.busy->stop();
	QString text = tr("There was an error retrieving the room configuration");
	if (!e.isEmpty())
		text += QString(":\n") + e;
	ui_.lb_general_message->setText(text);
	ui_.sw_general->setCurrentWidget(ui_.pg_general_message);
}

void MUCConfigDlg::setConfiguration_success()
{
	if (affiliation_ == MUCItem::Owner && ui_.tabs->currentWidget() == ui_.tab_general) {
		ui_.busy->stop();
	}
}

void MUCConfigDlg::setConfiguration_error(int, const QString& e)
{
	if (ui_.tabs->currentWidget() == ui_.tab_general) {
		ui_.busy->stop();
		QString text = tr("There was an error changing the room configuration");
		if (!e.isEmpty())
			text += QString(":\n") + e;
		ui_.lb_general_message->setText(text);
		ui_.sw_general->setCurrentWidget(ui_.pg_general_message);
	}
}

void MUCConfigDlg::getItemsByAffiliation_success(MUCItem::Affiliation a, const QList<MUCItem>& items)
{
	if (pending_requests_.contains(a) && ui_.tabs->currentWidget() == ui_.tab_affiliations) {
		ui_.tv_affiliations->setUpdatesEnabled(false);
		bool dynamicSortFilter = affiliations_proxy_model_->dynamicSortFilter();
		affiliations_proxy_model_->setDynamicSortFilter(false);

		affiliations_model_->setAffiliationListEnabled(a);
		affiliations_model_->addItems(items);
		removePendingRequest(a);

		affiliations_proxy_model_->setDynamicSortFilter(dynamicSortFilter);
		ui_.tv_affiliations->setUpdatesEnabled(true);
	}
}

void MUCConfigDlg::getItemsByAffiliation_error(MUCItem::Affiliation a, int, const QString&)
{
	if (pending_requests_.contains(a) && ui_.tabs->currentWidget() == ui_.tab_affiliations) {
		affiliations_model_->setAffiliationListEnabled(a,false);
		removePendingRequest(a);
	}
}

void MUCConfigDlg::setItems_success()
{
	if (ui_.tabs->currentWidget() == ui_.tab_affiliations) {
		ui_.busy->stop();
		refreshAffiliations();
	}
}

void MUCConfigDlg::setItems_error(int, const QString&)
{
	if (ui_.tabs->currentWidget() == ui_.tab_affiliations) {
		ui_.busy->stop();
		QMessageBox::critical(this, tr("Error"), tr("There was an error setting modifying the affiliations."));
		refreshAffiliations();
	}
}


void MUCConfigDlg::removePendingRequest(MUCItem::Affiliation a)
{
	pending_requests_.removeAll(a);
	if (pending_requests_.count() == 0)
		ui_.busy->stop();
}

void MUCConfigDlg::destroy_success()
{
	if (ui_.tabs->currentWidget() == ui_.tab_general) {
		ui_.busy->stop();
	}
}

void MUCConfigDlg::destroy_error(int, const QString&)
{
	if (ui_.tabs->currentWidget() == ui_.tab_general) {
		ui_.busy->stop();
		QMessageBox::critical(this, tr("Error"), tr("There was an error destroying the room."));
	}
}

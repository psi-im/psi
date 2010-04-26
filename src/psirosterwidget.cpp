/*
 * psirosterwidget.cpp
 * Copyright (C) 2010  Yandex LLC (Michail Pishchagin)
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

#include "psirosterwidget.h"

#include <QVBoxLayout>
#include <QStackedWidget>
#include <QMessageBox>

#include "psicontactlistmodel.h"
#include "psicontactlistview.h"
#include "contactlistproxymodel.h"
#include "psicontact.h"
#include "contactlistitemproxy.h"
#include "contactlistgroup.h"
#ifdef MODELTEST
#include "modeltest.h"
#endif
#include "psioptions.h"
#include "contactlistutil.h"
#include "psiaccount.h"

static const QString showOfflineOptionPath = "options.ui.contactlist.show.offline-contacts";
static const QString showHiddenOptionPath = "options.ui.contactlist.show.hidden-contacts-group";
static const QString showAgentsOptionPath = "options.ui.contactlist.show.agent-contacts";
static const QString showSelfOptionPath = "options.ui.contactlist.show.self-contact";
static const QString showStatusMessagesOptionPath = "options.ui.contactlist.status-messages.show";

PsiRosterWidget::PsiRosterWidget(QWidget* parent)
	: QWidget(parent)
	, stackedWidget_(0)
	, contactListPage_(0)
	, filterPage_(0)
	, contactListPageView_(0)
	, filterPageView_(0)
	, contactListModel_(0)
	, filterModel_(0)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setMargin(0);
	layout->setSpacing(0);

	stackedWidget_ = new QStackedWidget(this);
	stackedWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	layout->addWidget(stackedWidget_);

	// contactListPage_
	contactListPage_ = new QWidget(0);
	contactListPage_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	stackedWidget_->addWidget(contactListPage_);

	QVBoxLayout* contactListPageLayout = new QVBoxLayout(contactListPage_);
	contactListPageLayout->setMargin(0);
	contactListPageView_ = new PsiContactListView(contactListPage_);
	contactListPageView_->setObjectName("contactListView");
	contactListPageLayout->addWidget(contactListPageView_);

	// filterPage_
	filterPage_ = new QWidget(0);
	filterPage_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	stackedWidget_->addWidget(filterPage_);

	QVBoxLayout* filterPageLayout = new QVBoxLayout(filterPage_);
	filterPageLayout->setMargin(0);
	filterPageView_ = new PsiContactListView(filterPage_);
	filterPageLayout->addWidget(filterPageView_);
}

PsiRosterWidget::~PsiRosterWidget()
{
}

void PsiRosterWidget::setContactList(PsiContactList* contactList)
{
	Q_ASSERT(contactList);
	Q_ASSERT(!contactList_);
	contactList_ = contactList;
	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));

	connect(contactList_, SIGNAL(showAgentsChanged(bool)), SLOT(showAgentsChanged(bool)));
	connect(contactList_, SIGNAL(showHiddenChanged(bool)), SLOT(showHiddenChanged(bool)));
	connect(contactList_, SIGNAL(showSelfChanged(bool)), SLOT(showSelfChanged(bool)));
	connect(contactList_, SIGNAL(showOfflineChanged(bool)), SLOT(showOfflineChanged(bool)));
	optionChanged(showAgentsOptionPath);
	optionChanged(showHiddenOptionPath);
	optionChanged(showSelfOptionPath);
	optionChanged(showOfflineOptionPath);

	contactListModel_ = new PsiContactListModel(contactList_);
	contactListModel_->invalidateLayout();
	contactListModel_->setGroupsEnabled(true);
	contactListModel_->setAccountsEnabled(true);
	contactListModel_->storeGroupState("contacts");
#ifdef MODELTEST
	new ModelTest(contactListModel_, this);
#endif

	ContactListProxyModel* contactListProxyModel = new ContactListProxyModel(this);
	contactListProxyModel->setSourceModel(contactListModel_);
#ifdef MODELTEST
	new ModelTest(contactListProxyModel, this);
#endif

	contactListPageView_->setModel(contactListProxyModel);
	connect(contactListPageView_, SIGNAL(removeSelection(QMimeData*)), SLOT(removeSelection(QMimeData*)));
	connect(contactListPageView_, SIGNAL(removeGroupWithoutContacts(QMimeData*)), SLOT(removeGroupWithoutContacts(QMimeData*)));
}

void PsiRosterWidget::optionChanged(const QString& option)
{
	if (!contactList_)
		return;

	if (option == showAgentsOptionPath) {
		contactList_->setShowAgents(PsiOptions::instance()->getOption(showAgentsOptionPath).toBool());
	}
	else if (option == showHiddenOptionPath) {
		contactList_->setShowHidden(PsiOptions::instance()->getOption(showHiddenOptionPath).toBool());
	}
	else if (option == showSelfOptionPath) {
		contactList_->setShowSelf(PsiOptions::instance()->getOption(showSelfOptionPath).toBool());
	}
	else if (option == showOfflineOptionPath) {
		contactList_->setShowOffline(PsiOptions::instance()->getOption(showOfflineOptionPath).toBool());
	}
}

void PsiRosterWidget::removeSelection(QMimeData* selection)
{
	ContactListUtil::removeContact(0, selection, contactListModel_, this, this);
}

void PsiRosterWidget::removeContactConfirmation(const QString& id, bool confirmed)
{
	ContactListUtil::removeContactConfirmation(id, confirmed, contactListModel_, contactListPageView_);
}

void PsiRosterWidget::removeGroupWithoutContacts(QMimeData* selection)
{
	int n = QMessageBox::information(contactListPageView_, tr("Remove Group"),
	                                 tr("This will cause all contacts in this group to be disassociated with it.\n"
	                                    "\n"
	                                    "Proceed?"),
	                                 tr("&Yes"), tr("&No"));

	if (n == 0) {
		QModelIndexList indexes = contactListModel_->indexesFor(0, selection);
		Q_ASSERT(indexes.count() == 1);
		Q_ASSERT(contactListModel_->indexType(indexes.first()) == ContactListModel::GroupType);
		if (indexes.count() != 1)
			return;
		ContactListItemProxy* proxy = contactListModel_->modelIndexToItemProxy(indexes.first());
		ContactListGroup* group = proxy ? dynamic_cast<ContactListGroup*>(proxy->item()) : 0;
		if (!group)
			return;

		QList<PsiContact*> contacts = group->contacts();
		foreach(PsiContact* c, group->contacts()) {
			c->account()->actionGroupRemove(c->jid(), group->name());
		}
	}
}

void PsiRosterWidget::showAgentsChanged(bool enabled)
{
	PsiOptions::instance()->setOption(showAgentsOptionPath, enabled);
}

void PsiRosterWidget::showHiddenChanged(bool enabled)
{
	PsiOptions::instance()->setOption(showHiddenOptionPath, enabled);
}

void PsiRosterWidget::showSelfChanged(bool enabled)
{
	PsiOptions::instance()->setOption(showSelfOptionPath, enabled);
}

void PsiRosterWidget::showOfflineChanged(bool enabled)
{
	PsiOptions::instance()->setOption(showOfflineOptionPath, enabled);
}

void PsiRosterWidget::setShowStatusMsg(bool enabled)
{
	PsiOptions::instance()->setOption(showStatusMessagesOptionPath, enabled);
}

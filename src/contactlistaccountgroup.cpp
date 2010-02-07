/*
 * contactlistaccountgroup.h
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

#include "contactlistaccountgroup.h"

#include <QStringList>

#include "psicontact.h"
#include "psiaccount.h"
#include "contactlistitemproxy.h"
#include "psiselfcontact.h"
#include "contactlistaccountmenu.h"
#include "contactlistgroupcache.h"

ContactListAccountGroup::ContactListAccountGroup(ContactListModel* model, ContactListGroup* parent, PsiAccount* account)
	: ContactListNestedGroup(model, parent, QString())
	, isRoot_(!account)
	, account_(account)
{
	if (account_) {
		model->groupCache()->addGroup(this);
		connect(account_, SIGNAL(destroyed(QObject*)), SLOT(accountUpdated()));
		connect(account_, SIGNAL(updatedAccount()), SLOT(accountUpdated()));
	}
}

ContactListAccountGroup::~ContactListAccountGroup()
{
	clearGroup();
}

void ContactListAccountGroup::clearGroup()
{
	foreach(ContactListAccountGroup* account, accounts_) {
		removeItem(findGroup(account));
	}
	qDeleteAll(accounts_);
	accounts_.clear();

	ContactListNestedGroup::clearGroup();
}

PsiAccount* ContactListAccountGroup::account() const
{
	return account_;
}

ContactListModel::Type ContactListAccountGroup::type() const
{
	return ContactListModel::AccountType;
}

void ContactListAccountGroup::addContact(PsiContact* contact, QStringList contactGroups)
{
	Q_ASSERT(contact);
	if (isRoot()) {
		ContactListAccountGroup* accountGroup = findAccount(contact->account());
		if (!accountGroup) {
			accountGroup = new ContactListAccountGroup(model(), this, contact->account());
			accounts_.append(accountGroup);
			addItem(new ContactListItemProxy(this, accountGroup));
			accountGroup->addContact(accountGroup->account()->selfContact(), QStringList() << QString());
		}

		accountGroup->addContact(contact, contactGroups);
	}
	else {
		if (model()->groupsEnabled() && !contact->isSelf()) {
			ContactListNestedGroup::addContact(contact, contactGroups);
		}
		else {
			ContactListGroup::addContact(contact, contactGroups);
		}
	}
}

void ContactListAccountGroup::contactGroupsChanged(PsiContact* contact, QStringList contactGroups)
{
	if (isRoot()) {
		ContactListAccountGroup* accountGroup = findAccount(contact->account());
		if (accountGroup) {
			accountGroup->contactGroupsChanged(contact, contactGroups);
		}
	}
	else {
		if (model()->groupsEnabled() && !contact->isSelf()) {
			ContactListNestedGroup::contactGroupsChanged(contact, contactGroups);
		}
		else {
			ContactListGroup::contactGroupsChanged(contact, contactGroups);
		}
	}
}

bool ContactListAccountGroup::isRoot() const
{
	return isRoot_;
}

ContactListAccountGroup* ContactListAccountGroup::findAccount(PsiAccount* account) const
{
	Q_ASSERT(isRoot());
	foreach(ContactListAccountGroup* accountGroup, accounts_)
		if (accountGroup->account() == account)
			return accountGroup;

	return 0;
}

void ContactListAccountGroup::removeAccount(ContactListAccountGroup* accountGroup)
{
	Q_ASSERT(accountGroup);
	accounts_.removeAll(accountGroup);
	removeItem(ContactListGroup::findGroup(accountGroup));
	accountGroup->deleteLater();
}

void ContactListAccountGroup::accountUpdated()
{
	Q_ASSERT(!isRoot());
	ContactListAccountGroup* root = dynamic_cast<ContactListAccountGroup*>(parent());
	Q_ASSERT(root);

	model()->updatedItem(root->findGroup(this));

	if (account_.isNull() || !account_->enabled()) {
		clearGroup();
		root->removeAccount(this);
	}
}

const QString& ContactListAccountGroup::displayName() const
{
	if (account_) {
		return account_->name();
	}

	static QString emptyName;
	return emptyName;
}

QString ContactListAccountGroup::comparisonName() const
{
	return displayName();
}

QString ContactListAccountGroup::internalGroupName() const
{
	if (account_) {
		return account_->id();
	}

	return QString();
}

ContactListItemProxy* ContactListAccountGroup::findGroup(ContactListGroup* group) const
{
	return ContactListGroup::findGroup(group);
}

ContactListItemMenu* ContactListAccountGroup::contextMenu(ContactListModel* model)
{
	return new ContactListAccountMenu(this, model);
}

bool ContactListAccountGroup::isEditable() const
{
	return true;
}

bool ContactListAccountGroup::canContainSpecialGroups() const
{
	return !isRoot() && model()->groupsEnabled();
}

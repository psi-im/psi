#include "contactlist.h"
#include "contactlistgroupitem.h"
#include "contactlistrootitem.h"
#include "contactlistalphacomparator.h"


ContactList::ContactList(QObject* parent)
	: QObject(parent), showOffline_(false), showGroups_(true)
{
	rootItem_ = new ContactListRootItem(this);
	invisibleGroup_ = new ContactListRootItem(this);
	altInvisibleGroup_ = new ContactListRootItem(this);
	itemComparator_ = new ContactListAlphaComparator();
}

const ContactListItemComparator* ContactList::itemComparator() const
{
	return itemComparator_;
}

const QString& ContactList::search() const
{
	return search_;
}

void ContactList::setShowOffline(bool showOffline)
{
	if (showOffline_ != showOffline) {
		showOffline_ = showOffline;
		updateParents();
	}
}

void ContactList::setShowGroups(bool showGroups)
{
	if (showGroups_ != showGroups) {
		showGroups_ = showGroups;
		updateParents();
	}
}

void ContactList::setSearch(const QString& search)
{
	QString oldSearch = search_;
	search_ = search;

	if ((oldSearch.isEmpty() && !search.isEmpty()) || search.isEmpty()) {
		updateParents();
	}
	if (search.isEmpty()) {
		updateParents();
	}
	else if (search.startsWith(oldSearch)) {
		updateVisibleParents();
	}
	else if (oldSearch.startsWith(search)) {
		updateInvisibleParents();
	}
	else {
		updateParents();
	}
}

void ContactList::emitDataChanged() 
{
	emit dataChanged();
}

/*void ContactList::setShowGroups(bool showGroups)
{
	showGroups_ = showGroups;
}

void ContactList::setShowAccounts(bool showAccounts)
{
	showAccounts_ = showAccounts;
}*/

ContactListRootItem* ContactList::rootItem()
{
	return rootItem_;
}

ContactListRootItem* ContactList::invisibleGroup()
{
	return invisibleGroup_;
}

/*ContactListGroupItem* ContactList::hiddenGroup()
{
	return hiddenGroup_;
}

ContactListGroupItem* ContactList::agentsGroup()
{
	return agentsGroup_;
}

ContactListGroupItem* ContactList::conferenceGroup()
{
	return conferenceGroup_;
}*/

void ContactList::updateVisibleParents()
{
	rootItem()->updateParents();
	emit dataChanged();
}

void ContactList::updateInvisibleParents()
{
	invisibleGroup()->updateParents();
	emit dataChanged();
}

void ContactList::updateParents()
{
	// Switch invisible groups
	ContactListRootItem* tmpInvisibleGroup = invisibleGroup_;
	invisibleGroup_ = altInvisibleGroup_;
	altInvisibleGroup_ = tmpInvisibleGroup;

	// Move items around
	rootItem()->updateParents();
	altInvisibleGroup_->updateParents();

	emit dataChanged();
}

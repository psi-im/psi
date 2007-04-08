#ifndef CONTACTLISTROOTITEM_H
#define CONTACTLISTROOTITEM_H

#include "contactlistgroupitem.h"

class ContactList;

class ContactListRootItem : public ContactListGroupItem
{
public:
	ContactListRootItem(ContactList* contactList) : ContactListGroupItem(0), contactList_(contactList) { }

	virtual ContactList* contactList() const { return contactList_; }
	virtual void updateParent() { }

private:
	ContactList* contactList_;
};

#endif

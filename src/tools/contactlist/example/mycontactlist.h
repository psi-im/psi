#ifndef MYCONTACTLIST_H
#define MYCONTACTLIST_H

#include "contactlist.h"
#include "contactlistgroupitem.h"
#include "contactlistrootitem.h"
#include "status.h"
#include "mycontact.h"
#include "mygroup.h"

class ContactViewItem;

class MyContactList : public ContactList
{
public:
	MyContactList() : ContactList() { };

	void addContact(const QString& name, const QString& jid, const QString& pic, const Status& status, const QString& group = QString()) {
		ContactListGroupItem* groupItem = rootItem();
		if (!group.isEmpty()) {
			MyGroup g(group);
			groupItem = static_cast<MyGroup*>(rootItem()->findFirstItem(&g));;
			if (!groupItem) {
				groupItem = static_cast<MyGroup*>(invisibleGroup()->findFirstItem(&g));
			}

			if (!groupItem) {
				groupItem = new MyGroup(group, rootItem());
			}
		}
		new MyContact(name, jid, pic, status, groupItem);
	}
};

#endif

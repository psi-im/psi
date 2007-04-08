#ifndef CONTACTLISTGROUPITEM_H
#define CONTACTLISTGROUPITEM_H

#include <QString>
#include <QList>

#include "contactlistitem.h"

class ContactListGroupItem : public ContactListItem
{
public:
	ContactListGroupItem(ContactListGroupItem* parent = 0);
	virtual ~ContactListGroupItem();

	int indexOf(ContactListItem*) const;
	ContactListItem* atIndex(int index) const;
	int items() const;
	void addItem(ContactListItem*);
	void removeItem(ContactListItem*);
	virtual bool expanded() const;
	virtual ContactListItem* findFirstItem(ContactListItem*);
	virtual int count() const;
	virtual int countOnline() const;
	virtual void updateParent();
	virtual void updateParents();
	//virtual QList<ContactListItem*> visibleItems();
	//virtual QList<ContactListItem*> invisibleItems();

private:
	QList<ContactListItem*> items_;
};

#endif

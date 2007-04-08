#ifndef CONTACTLISTGROUP_H
#define CONTACTLISTGROUP_H

#include <QString>

#include "contactlistgroupitem.h"

class ContactListGroup : public ContactListGroupItem
{
public:
	ContactListGroup(ContactListGroupItem* parent);
	virtual ~ContactListGroup();

	virtual const QString& name() const = 0;
};

#endif

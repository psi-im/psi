#ifndef MYGROUP_H
#define MYGROUP_H

#include <QString>

#include "contactlistgroup.h"

class MyGroup : public ContactListGroup
{
public:
	MyGroup(const QString& name, ContactListGroupItem* parent = 0) : ContactListGroup(parent), name_(name) { }
	virtual const QString& name() const { return name_; }

	bool expanded() const {
		return (name_ != "Others");
	}

	bool equals(ContactListItem* other) const {
		MyGroup* group = dynamic_cast<MyGroup*>(other);
		return (group && group->name_ == name_); 
	}

private:
	QString name_;
};

#endif

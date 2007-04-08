#ifndef CONTACTLISTITEMCOMPARATOR_H
#define CONTACTLISTITEMCOMPARATOR_H

class ContactListItem;

class ContactListItemComparator
{
public:
	ContactListItemComparator() { }
	virtual ~ContactListItemComparator() { }
	virtual int compare(ContactListItem *it1, ContactListItem *it2) const = 0;
};

#endif

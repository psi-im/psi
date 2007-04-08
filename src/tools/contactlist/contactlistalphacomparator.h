#ifndef CONTACTLISTALPHACOMPARATOR_H
#define CONTACTLISTALPHACOMPARATOR_H

#include "contactlistitemcomparator.h"

class ContactListItem;

class ContactListAlphaComparator : public ContactListItemComparator
{
public:
	ContactListAlphaComparator();
	virtual int compare(ContactListItem *it1, ContactListItem *it2) const;
};

#endif

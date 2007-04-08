#include "contactlistalphacomparator.h"
#include "contactlistitem.h"
#include "contactlistcontact.h"
#include "contactlistgroup.h"


ContactListAlphaComparator::ContactListAlphaComparator() 
{ 
}


int ContactListAlphaComparator::compare(ContactListItem *it1, ContactListItem *it2) const 
{
	// Contacts
	ContactListContact* it1_contact = dynamic_cast<ContactListContact*>(it1);
	ContactListContact* it2_contact = dynamic_cast<ContactListContact*>(it2);
	if (it1_contact && it2_contact) {
		if (it1_contact->name() < it2_contact->name()) {
			return -1;
		}
		else if (it1_contact->name() > it2_contact->name()) {
			return 1;
		}
		else {
			return 0;
		}
	}
	else if (it1_contact) {
		return -1;
	}
	else if (it2_contact) {
		return 1;
	}

	// Groups
	ContactListGroup* it1_group = dynamic_cast<ContactListGroup*>(it1);
	ContactListGroup* it2_group = dynamic_cast<ContactListGroup*>(it2);
	if (it1_group && it2_group) {
		if (it1_group->name() < it2_group->name()) {
			return -1;
		}
		else if (it1_group->name() > it2_group->name()) {
			return 1;
		}
		else {
			return 0;
		}
	}
	else if (it1_group) {
		return -1;
	}
	else if (it2_group) {
		return 1;
	}

	return 0;
}

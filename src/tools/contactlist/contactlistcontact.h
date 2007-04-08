#ifndef CONTACTLISTCONTACT_H
#define CONTACTLISTCONTACT_H

#include <QString>
#include <QIcon>

#include "status.h"
#include "contactlistitem.h"

class ContactListContact : public ContactListItem
{
public:
	ContactListContact(ContactListGroupItem* defaultParent);
	virtual ~ContactListContact();

	virtual const QString& name() const = 0;
	// FIXME: Change this into Jid
	virtual QString jid() const = 0;
	virtual Status status() const = 0;
	virtual QIcon statusIcon() const;
	virtual QIcon picture() const;
	virtual void updateParent();

	virtual QString toolTip() const;

	virtual int countOnline() const;
};

#endif

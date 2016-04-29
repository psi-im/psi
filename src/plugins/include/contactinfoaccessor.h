#ifndef CONTACTINFOACCESSOR_H
#define CONTACTINFOACCESSOR_H

class ContactInfoAccessingHost;

class ContactInfoAccessor
{
public:
	virtual ~ContactInfoAccessor() {}

	virtual void setContactInfoAccessingHost(ContactInfoAccessingHost* host) = 0;

};

Q_DECLARE_INTERFACE(ContactInfoAccessor, "org.psi-im.ContactInfoAccessor/0.1");


#endif // CONTACTINFOACCESSOR_H

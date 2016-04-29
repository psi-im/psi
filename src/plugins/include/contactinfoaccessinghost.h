#ifndef CONTACTINFOACCESSINGHOST_H
#define CONTACTINFOACCESSINGHOST_H

class QString;
class QStringList;

class ContactInfoAccessingHost
{
public:
	virtual ~ContactInfoAccessingHost() {}

	// Note that all this methods are checking full jid (with resource)
	// So for normal contacts is preferable to use bare jid in queries
	virtual bool isSelf(int account, const QString& jid) = 0;
	virtual bool isAgent(int account, const QString& jid) = 0;
	virtual bool inList(int account, const QString& jid) = 0;
	virtual bool isPrivate(int account, const QString& jid) = 0;
	virtual bool isConference(int account, const QString& jid) = 0;
	virtual QString name(int account, const QString& jid) = 0;
	virtual QString status(int account, const QString& jid) = 0;
	virtual QString statusMessage(int account, const QString& jid) = 0;
	virtual QStringList resources(int account, const QString& jid) = 0;
};

Q_DECLARE_INTERFACE(ContactInfoAccessingHost, "org.psi-im.ContactInfoAccessingHost/0.1");

#endif // CONTACTINFOACCESSINGHOST_H

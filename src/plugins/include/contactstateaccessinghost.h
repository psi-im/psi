#ifndef CONTACTSTATEACCESSINGHOST_H
#define CONTACTSTATEACCESSINGHOST_H

class QDomElement;
class QString;

class ContactStateAccessingHost
{
public:
	virtual ~ContactStateAccessingHost() {}

	virtual bool setActivity(int account, const QString& Jid, QDomElement xml) = 0;
	virtual bool setMood(int account, const QString& Jid, QDomElement xml) = 0;
	virtual bool setTune(int account, const QString& Jid, QString tune) = 0;
};

Q_DECLARE_INTERFACE(ContactStateAccessingHost, "org.psi-im.ContactStateAccessingHost/0.2");

#endif // CONTACTSTATEACCESSINGHOST_H

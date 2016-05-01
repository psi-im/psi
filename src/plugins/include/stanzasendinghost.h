#ifndef STANZASENDINGHOST_H
#define STANZASENDINGHOST_H

class QDomElement;
class QString;

class StanzaSendingHost
{
public:
	virtual ~StanzaSendingHost() {}

	virtual void sendStanza(int account, const QDomElement& xml) = 0;
	virtual void sendStanza(int account, const QString& xml) = 0;

	virtual void sendMessage(int account, const QString& to, const QString& body, const QString& subject, const QString& type) = 0;

	virtual QString uniqueId(int account) = 0;
	virtual QString escape(const QString& str) = 0;
};

Q_DECLARE_INTERFACE(StanzaSendingHost, "org.psi-im.StanzaSendingHost/0.1");

#endif

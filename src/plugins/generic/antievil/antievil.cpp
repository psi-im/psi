/*
 * (c) 2007-2008 Maciej Niedzielski
 */

#include <QObject>
#include <QTextStream>
#include <QDebug>

#include "psiplugin.h"
#include "stanzafilter.h"
#include "stanzasender.h"
#include "stanzasendinghost.h"

class AntiEvilPlugin: public QObject, public PsiPlugin, public StanzaFilter, public StanzaSender
{
	Q_OBJECT
	Q_INTERFACES(PsiPlugin StanzaFilter StanzaSender);

public:
	AntiEvilPlugin()
		: stanzaSender(0)
	{
	}


	//-- PsiPlugin -------------------------------------------

	virtual QString name() const
	{
		// this will be displayed
		return "Machekku's Evil Blocker Plugin";
	}

	virtual QString shortName() const
	{
		// internal name, no spaces please!
		return "antievil";
	}

	virtual QString version() const
	{
		return "0.1";
	}

	virtual QWidget* options() const
	{
		return 0;
	}

	virtual bool enable()
	{
		if (stanzaSender) {
			enabled = true;
		}
		return enabled;
	}

	virtual bool disable()
	{
		enabled = false;
		return true;
	}


	//-- StanzaFilter ----------------------------------------

	virtual bool incomingStanza(int account, const QDomElement& stanza)
	{
		bool blocked = false;

		if (enabled) {
			for (QDomNode n = stanza.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement i = n.toElement();
				if (!i.isNull() && i.tagName() == "evil" && i.attribute("xmlns") == "http://jabber.org/protocol/evil") {
					qDebug("evil blocked! ;)");

					if (stanza.tagName() == "iq") {
						qDebug("sending 'forbidden' error");
						QString sender = stanza.attribute("from");
						QString reply = QString("<iq type='error' %1><error type='modify'><bad-request xmlns='urn:ietf:params:xml:xmpp-stanzas'/></error></iq>")
							.arg(sender.isEmpty() ? "" : QString("to='%1'").arg(sender));

						stanzaSender->sendStanza(account, reply);
					}

					blocked = true;	// stop processing this stanza
					break;
				}
			}
		}

		return blocked;
	}


	//-- StanzaSender ----------------------------------------

	virtual void setStanzaSendingHost(StanzaSendingHost *host)
	{
		stanzaSender = host;
	}


private:
	StanzaSendingHost* stanzaSender;
	bool enabled;
};

Q_EXPORT_PLUGIN2(antievil, AntiEvilPlugin)

#include "antievil.moc"

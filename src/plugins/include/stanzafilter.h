#ifndef STANZAFILTER_H
#define STANZAFILTER_H

#include <QDomElement>

class StanzaFilter
{
public:
	virtual ~StanzaFilter() {}

	// true = handled, don't pass to next handler

    virtual bool incomingStanza(int account, const QDomElement& xml) = 0;
};

Q_DECLARE_INTERFACE(StanzaFilter, "org.psi-im.StanzaFilter/0.1");

#endif

#ifndef EVENTFILTER_H
#define EVENTFILTER_H

#include <QDomElement>

class EventFilter
{
public:
	virtual ~EventFilter() {}

	// true = handled, don't pass to next handler

    virtual bool processEvent(int account, const QDomElement& e) = 0;
	virtual bool processMessage(int account, const QString& fromJid, const QString& body, const QString& subject) = 0;
};

Q_DECLARE_INTERFACE(EventFilter, "org.psi-im.EventFilter/0.1");

#endif

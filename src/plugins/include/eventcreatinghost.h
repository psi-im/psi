#ifndef EVENTCREATINGHOST_H
#define EVENTCREATINGHOST_H

class QObject;
class QString;

class EventCreatingHost
{
public:
	virtual ~EventCreatingHost() {}

	virtual void createNewEvent(int account, const QString& jid, const QString& descr, QObject *receiver, const char* slot) = 0;
};

Q_DECLARE_INTERFACE(EventCreatingHost, "org.psi-im.EventCreatingHost/0.1");
#endif

#ifndef EVENTCREATOR_H
#define EVENTCREATOR_H

class EventCreatingHost;

class EventCreator
{
public:
	virtual ~EventCreator() {}

	virtual void setEventCreatingHost(EventCreatingHost* host) = 0;

};

Q_DECLARE_INTERFACE(EventCreator, "org.psi-im.EventCreator/0.1");

#endif

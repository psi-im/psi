#ifndef STANZASENDER_H
#define STANZASENDER_H

class StanzaSendingHost;

class StanzaSender
{
public:
	virtual ~StanzaSender() {}

	virtual void setStanzaSendingHost(StanzaSendingHost *host) = 0;
};

Q_DECLARE_INTERFACE(StanzaSender, "org.psi-im.StanzaSender/0.1");

#endif
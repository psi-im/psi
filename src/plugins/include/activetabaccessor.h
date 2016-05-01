#ifndef ACTIVETABACCESSOR_H
#define ACTIVETABACCESSOR_H

class ActiveTabAccessingHost;

class ActiveTabAccessor
{
public:
	virtual ~ActiveTabAccessor() {}

	virtual void setActiveTabAccessingHost(ActiveTabAccessingHost* host) = 0;

};

Q_DECLARE_INTERFACE(ActiveTabAccessor, "org.psi-im.ActiveTabAccessor/0.1");

#endif // ACTIVETABACCESSOR_H

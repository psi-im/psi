#ifndef APPLICATIONINFOACCESSOR_H
#define APPLICATIONINFOACCESSOR_H

class ApplicationInfoAccessingHost;

class ApplicationInfoAccessor
{
public:
	virtual ~ApplicationInfoAccessor() {}

	virtual void setApplicationInfoAccessingHost(ApplicationInfoAccessingHost* host) = 0;

};

Q_DECLARE_INTERFACE(ApplicationInfoAccessor, "org.psi-im.ApplicationInfoAccessor/0.1");

#endif // APPLICATIONINFOACCESSOR_H

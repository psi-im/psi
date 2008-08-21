#ifndef IQFILTER_H
#define IQFILTER_H

class IqFilteringHost;

class IqFilter
{
public:
	virtual ~IqFilter() {}

	virtual void setIqFilteringHost(IqFilteringHost *host) = 0;
};

Q_DECLARE_INTERFACE(IqFilter, "org.psi-im.IqFilter/0.1");

#endif
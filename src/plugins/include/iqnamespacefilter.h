#ifndef IQNAMESPACEFILTER_H
#define IQNAMESPACEFILTER_H

#include <QDomElement>

class IqNamespaceFilter
{
public:
	virtual ~IqNamespaceFilter() {}

	virtual bool iqGet(int account, const QDomElement& xml) = 0;
	virtual bool iqSet(int account, const QDomElement& xml) = 0;
	virtual bool iqResult(int account, const QDomElement& xml) = 0;
	virtual bool iqError(int account, const QDomElement& xml) = 0;
};

Q_DECLARE_INTERFACE(IqNamespaceFilter, "org.psi-im.IqNamespaceFilter/0.1");

#endif

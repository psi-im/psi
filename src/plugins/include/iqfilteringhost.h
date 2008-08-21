#ifndef IQFILTERINGHOST_H
#define IQFILTERINGHOST_H

#include <QString>
#include <QRegExp>

class IqNamespaceFilter;

class IqFilteringHost
{
public:
	virtual ~IqFilteringHost() {}

	virtual void addIqNamespaceFilter(const QString& ns, IqNamespaceFilter* filter) = 0;
	virtual void addIqNamespaceFilter(const QRegExp& ns, IqNamespaceFilter* filter) = 0;

	virtual void removeIqNamespaceFilter(const QString& ns, IqNamespaceFilter* filter) = 0;
	virtual void removeIqNamespaceFilter(const QRegExp& ns, IqNamespaceFilter* filter) = 0;
};

// TODO(mck): iq results/error may contain no namespaced element - think about this!!

Q_DECLARE_INTERFACE(IqFilteringHost, "org.psi-im.IqFilteringHost/0.1");

#endif

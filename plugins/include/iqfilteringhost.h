#ifndef IQFILTERINGHOST_H
#define IQFILTERINGHOST_H

#include <QtPlugin>

class IqNamespaceFilter;
class QRegularExpression;
class QString;

class IqFilteringHost {
public:
    virtual ~IqFilteringHost() { }

    virtual void addIqNamespaceFilter(const QString &ns, IqNamespaceFilter *filter) = 0;
    virtual void addIqNamespaceFilter(const QRegularExpression &ns, IqNamespaceFilter *filter) = 0;

    virtual void removeIqNamespaceFilter(const QString &ns, IqNamespaceFilter *filter) = 0;
    virtual void removeIqNamespaceFilter(const QRegularExpression &ns, IqNamespaceFilter *filter) = 0;
};

// TODO(mck): iq results/error may contain no namespaced element - think about this!!

Q_DECLARE_INTERFACE(IqFilteringHost, "org.psi-im.IqFilteringHost/0.1");

#endif // IQFILTERINGHOST_H

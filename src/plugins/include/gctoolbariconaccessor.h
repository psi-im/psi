#ifndef GCTOOLBARICONACCESSOR_H
#define GCTOOLBARICONACCESSOR_H

#include <QList>
#include <QVariantHash>

class QObject;
class QAction;
class QString;

class GCToolbarIconAccessor
{
public:
	virtual ~GCToolbarIconAccessor() {}

	virtual QList < QVariantHash > getGCButtonParam() = 0;
	virtual QAction* getGCAction(QObject* parent, int account, const QString& contact) = 0;
};

Q_DECLARE_INTERFACE(GCToolbarIconAccessor, "org.psi-im.GCToolbarIconAccessor/0.1");

#endif // GCTOOLBARICONACCESSOR_H

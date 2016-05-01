#ifndef TOOLBARICONACCESSOR_H
#define TOOLBARICONACCESSOR_H

#include <QList>
#include <QVariantHash>

class QObject;
class QAction;
class QString;

class ToolbarIconAccessor
{
public:
	virtual ~ToolbarIconAccessor() {}

	virtual QList < QVariantHash > getButtonParam() = 0;
	virtual QAction* getAction(QObject* parent, int account, const QString& contact) = 0;
};

Q_DECLARE_INTERFACE(ToolbarIconAccessor, "org.psi-im.ToolbarIconAccessor/0.1");

#endif // TOOLBARICONACCESSOR_H

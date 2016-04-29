#ifndef MENUACCESSOR_H
#define MENUACCESSOR_H

#include <QList>
#include <QVariantHash>

class QObject;
class QAction;
class QString;

class MenuAccessor
{
public:
	virtual ~MenuAccessor() {}

	virtual QList < QVariantHash > getAccountMenuParam() = 0;
	virtual QList < QVariantHash > getContactMenuParam() = 0;
	virtual QAction* getContactAction(QObject* parent, int account, const QString& contact) = 0;
	virtual QAction* getAccountAction(QObject* parent, int account) = 0;

};

Q_DECLARE_INTERFACE(MenuAccessor, "org.psi-im.MenuAccessor/0.1");

#endif

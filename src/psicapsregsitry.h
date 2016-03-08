#ifndef PSICAPSREGSITRY_H
#define PSICAPSREGSITRY_H

#include "xmpp_caps.h"

class PsiCapsRegistry : public XMPP::CapsRegistry
{
	Q_OBJECT

public:
	PsiCapsRegistry(QObject *parent = 0);

	void saveData(const QByteArray &data);
	QByteArray loadData();
};

#endif // PSICAPSREGSITRY_H

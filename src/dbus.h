#ifndef PSI_DBUS_H
#define PSI_DBUS_H

#include "psicon.h"

#define PSIDBUSNAME "org.psi-im.Psi"
#define PSIDBUSMAINIF "org.psi_im.Psi.Main"
bool dbusInit(const QString profile);

void addPsiConAdapter(PsiCon *psicon);

#endif

// SPDX-License-Identifier: GPL-2.0-or-later
#include "psimediaprovider.h"
#include <QPointer>
// Isolated provider lookup for the standalone SDK/UI check. Full Psi links its
// normal plugin manager; both paths exercise the same capture facade below it.
namespace PsiMedia {
static Provider         *current = nullptr;
static QPointer<QObject> object;
Provider                *provider() { return object ? current : nullptr; }
void                     setProvider(Provider *value)
{
    current = value;
    object  = value->qobject();
}
}

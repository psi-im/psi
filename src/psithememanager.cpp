/*
 * psithememanager.h - manages all themes in psi
 * Copyright (C) 2010  Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "psithememanager.h"

#include "applicationinfo.h"
#include "theme.h"

#include <QCoreApplication>

class PsiThemeManager::Private {
public:
    QMap<QString, PsiThemeProvider *> providers;
    QSet<QString>                     required;
};

//---------------------------------------------------------
// PsiThemeManager
//---------------------------------------------------------
PsiThemeManager::PsiThemeManager(QObject *parent) : QObject(parent)
{
    d = new Private; //(this);
}

PsiThemeManager::~PsiThemeManager() { delete d; }

void PsiThemeManager::registerProvider(PsiThemeProvider *provider, bool required)
{
    d->providers[provider->type()] = provider;
    if (required) {
        d->required.insert(provider->type());
    }
}

PsiThemeProvider *PsiThemeManager::unregisterProvider(const QString &type)
{
    auto p = d->providers.take(type);
    if (p) {
        p->unloadCurrent();
    }
    return p;
}

PsiThemeProvider *PsiThemeManager::provider(const QString &type) { return d->providers.value(type); }

QList<PsiThemeProvider *> PsiThemeManager::registeredProviders() const { return d->providers.values(); }

bool PsiThemeManager::loadAll()
{
    foreach (const QString &type, d->providers.keys()) {
        if (!d->providers[type]->loadCurrent() && d->required.contains(type)) {
            return false;
        }
    }
    return true;
}

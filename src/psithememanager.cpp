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
    QString                           failedId;
    QString                           errorString;
    PsiThemeProvider::LoadRestult     loadResult = PsiThemeProvider::LoadNotStarted;
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

PsiThemeProvider::LoadRestult PsiThemeManager::loadAll()
{
    d->loadResult    = PsiThemeProvider::LoadInProgress;
    QObject *ctx     = nullptr;
    auto     pending = std::shared_ptr<QList<PsiThemeProvider *>> { new QList<PsiThemeProvider *>() };
    auto     cleanup = [this, pending](QObject *ctx, PsiThemeProvider *provider, bool failure) {
        if (failure) {
            d->failedId = QLatin1String(provider->type());
            for (auto p : *pending) {
                p->cancelCurrentLoading();
                p->disconnect(ctx);
            }
            ctx->deleteLater();
            d->loadResult = PsiThemeProvider::LoadFailure;
            emit currentLoadFailed();
            return;
        }
        pending->removeOne(provider);
        provider->disconnect(ctx);
        if (pending->isEmpty()) {
            ctx->deleteLater();
            d->loadResult = PsiThemeProvider::LoadSuccess;
            emit currentLoadSuccess();
        }
    };
    for (auto it = d->providers.begin(); it != d->providers.end(); ++it) {
        auto provider = it.value();
        auto required = d->required.contains(it.key());
        auto status   = provider->loadCurrent();
        if (status == PsiThemeProvider::LoadFailure && required) {
            d->failedId   = QLatin1String(provider->type());
            d->loadResult = PsiThemeProvider::LoadFailure;
            return PsiThemeProvider::LoadFailure;
        }
        if (status == PsiThemeProvider::LoadSuccess) {
            continue;
        }
        // in progress
        pending->append(provider);

        if (!ctx) {
            ctx = new QObject(this);
        }
        connect(provider, &PsiThemeProvider::themeChanged, ctx,
                [ctx, provider, cleanup]() { cleanup(ctx, provider, false); });
        connect(provider, &PsiThemeProvider::currentLoadFailed, ctx,
                [ctx, provider, required, cleanup]() { cleanup(ctx, provider, required); });
    }
    d->loadResult = pending->isEmpty() ? PsiThemeProvider::LoadSuccess : PsiThemeProvider::LoadInProgress;
    return d->loadResult;
}

QString PsiThemeManager::failedId() const { return d->failedId; }

/*
 * turencontrollermanager.cpp
 * Copyright (C) 2006  Remko Troncon
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

#ifndef QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include "tunecontrollermanager.h"

#include "tunecontroller.h"
#include "tunecontrollerplugin.h"

#include <QPluginLoader>
#include <QtCore>

/**
 * \class TuneControllerManager
 * \brief A manager for all tune controller plugins.
 */

TuneControllerManager::TuneControllerManager()
{
    foreach (QObject *plugin, QPluginLoader::staticInstances()) {
        loadPlugin(plugin);
    }
}

TuneControllerManager::~TuneControllerManager() {}

void TuneControllerManager::updateControllers(const QStringList &blacklist)
{
    bool isInBlacklist;
    foreach (const QString &name, plugins_.keys()) {
        TuneControllerPtr c = controllers_.value(name);
        isInBlacklist       = blacklist.contains(name);
        if (!c && !isInBlacklist) {
            c = TuneControllerPtr(plugins_[name]->createController());
            connect(c.data(), &TuneController::stopped, this, &TuneControllerManager::stopped);
            connect(c.data(), &TuneController::playing, this, [this](const Tune &tune) {
                if (checkTune(tune)) {
                    emit playing(tune);
                }
            });
            controllers_.insert(name, c);
        } else if (c && isInBlacklist) {
            emit stopped();
            controllers_.remove(name);
        }
    }
}

Tune TuneControllerManager::currentTune() const
{
    foreach (const TuneControllerPtr &c, controllers_.values()) {
        Tune t = c->currentTune();
        if (!t.isNull() && checkTune(t)) {
            return t;
        }
    }
    return Tune();
}

/**
 * \brief Returns a list of all the supported controllers.
 */
QList<QString> TuneControllerManager::controllerNames() const { return plugins_.keys(); }

/**
 * \brief Creates a new controller.
 */
TuneController *TuneControllerManager::createController(const QString &name) const
{
    return plugins_[name]->createController();
}

/**
 * \brief Loads a TuneControllerPlugin from a file.
 */
bool TuneControllerManager::loadPlugin(const QString &file)
{
    // return QPluginLoader(file).load();
    return loadPlugin(QPluginLoader(file).instance());
}

bool TuneControllerManager::loadPlugin(QObject *plugin)
{
    if (!plugin)
        return false;

    TuneControllerPluginPtr tcplugin = TuneControllerPluginPtr(qobject_cast<TuneControllerPlugin *>(plugin));
    if (tcplugin) {
        if (!plugins_.contains(tcplugin->name())) {
            // qDebug() << "Registering plugin " << tcplugin->name();
            plugins_[tcplugin->name()] = tcplugin;
        }
        return true;
    }
    return false;
}

void TuneControllerManager::setTuneFilters(const QStringList &filters, const QString &pattern)
{
    tuneUrlFilters_         = filters;
    tuneTitleFilterPattern_ = pattern;
}

bool TuneControllerManager::checkTune(const Tune &tune) const
{
    if (!tuneTitleFilterPattern_.isEmpty() && !tune.name().isEmpty()) {
        QRegExp tuneTitleFilter(tuneTitleFilterPattern_);
        if (tuneTitleFilter.isValid() && tuneTitleFilter.exactMatch(tune.name())) {
            return false;
        }
    }
    if (!tune.url().isEmpty()) {
        int index = tune.url().lastIndexOf(".");
        if (index != -1) {
            QString extension = tune.url().right(tune.url().length() - (index + 1)).toLower();
            if (!extension.isEmpty() && tuneUrlFilters_.contains(extension)) {
                return false;
            }
        }
    }
    return true;
}

// ----------------------------------------------------------------------------

#ifdef TC_ITUNES
Q_IMPORT_PLUGIN(ITunesPlugin)
#endif

#ifdef TC_WINAMP
Q_IMPORT_PLUGIN(WinAmpPlugin)
#endif

#ifdef TC_AIMP
Q_IMPORT_PLUGIN(AIMPPlugin)
#endif

#ifdef TC_PSIFILE
Q_IMPORT_PLUGIN(PsiFilePlugin)
#endif

#if defined(TC_MPRIS) && defined(USE_DBUS)
Q_IMPORT_PLUGIN(MPRISPlugin)
#endif

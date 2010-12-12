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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include <QtCore>
#include <QPluginLoader>
#include <QCoreApplication>
#include <QDebug>

#include "tunecontrollermanager.h"
#include "tunecontrollerplugin.h"

/**
 * \class TuneControllerManager
 * \brief A manager for all tune controller plugins.
 */


TuneControllerManager::TuneControllerManager() : QObject(QCoreApplication::instance())
{
	foreach(QObject* plugin,QPluginLoader::staticInstances()) {
		loadPlugin(plugin);
	}
}


/**
 * \brief Returns a list of all the supported controllers.
 */
QList<QString> TuneControllerManager::controllerNames() const
{
	return plugins_.keys();
}


/**
 * \brief Creates a new controller.
 */
TuneController* TuneControllerManager::createController(const QString& name) const
{
	return plugins_[name]->createController();
}


/**
 * \brief Loads a TuneControllerPlugin from a file.
 */
bool TuneControllerManager::loadPlugin(const QString& file)
{
	//return QPluginLoader(file).load();
	return loadPlugin(QPluginLoader(file).instance());
}


bool TuneControllerManager::loadPlugin(QObject* plugin)
{
	if (!plugin)
		return false;

	TuneControllerPlugin* tcplugin = qobject_cast<TuneControllerPlugin*>(plugin);
	if (tcplugin) {
		if(!plugins_.contains(tcplugin->name())) {
			//qDebug() << "Registering plugin " << tcplugin->name();
			plugins_[tcplugin->name()] = tcplugin;
		}
		return true;
	}

	return false;
}
	
/**
 * \brief Retrieves the global plugin manager.
 */
TuneControllerManager* TuneControllerManager::instance()
{
	if (!instance_) 
		instance_ = new TuneControllerManager();
	return instance_;
}
	
TuneControllerManager* TuneControllerManager::instance_ = 0;


// ---------------------------------------------------------------------------- 

#ifdef TC_ITUNES
Q_IMPORT_PLUGIN(itunesplugin)
#endif

#ifdef TC_XMMS
Q_IMPORT_PLUGIN(xmmsplugin);
#endif

#ifdef TC_WINAMP
Q_IMPORT_PLUGIN(winampplugin);
#endif

#ifdef TC_PSIFILE
Q_IMPORT_PLUGIN(psifileplugin);
#endif

#if defined(TC_MPRIS) && defined(USE_DBUS)
Q_IMPORT_PLUGIN(mprisplugin);
#endif

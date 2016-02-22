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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include <QtCore>
#include <QPluginLoader>

#include "tunecontroller.h"
#include "tunecontrollermanager.h"
#include "tunecontrollerplugin.h"

/**
 * \class TuneControllerManager
 * \brief A manager for all tune controller plugins.
 */


TuneControllerManager::TuneControllerManager()
{
	foreach(QObject* plugin,QPluginLoader::staticInstances()) {
		loadPlugin(plugin);
	}
}

TuneControllerManager::~TuneControllerManager()
{
}

void TuneControllerManager::updateControllers(const QStringList &blacklist)
{
	bool isInBlacklist;
	foreach(const QString &name, plugins_.keys()) {
		TuneControllerPtr c = controllers_.value(name);
		isInBlacklist = blacklist.contains(name);
		if (!c && !isInBlacklist) {
			c = TuneControllerPtr(plugins_[name]->createController());
			connect(c.data(),SIGNAL(stopped()),SIGNAL(stopped()));
			connect(c.data(),SIGNAL(playing(const Tune&)),SLOT(sendTune(const Tune&)));
			controllers_.insert(name, c);
		}
		else if (c && isInBlacklist) {
			emit stopped();
			controllers_.remove(name);
		}
	}
}

Tune TuneControllerManager::currentTune() const
{
	foreach(const TuneControllerPtr &c, controllers_.values()) {
		Tune t = c->currentTune();
		if (!t.isNull() && checkTune(t)) {
			return t;
		}
	}
	return Tune();
}

void TuneControllerManager::sendTune(const Tune &tune)
{
	if (checkTune(tune)) {
		emit playing(tune);
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

	TuneControllerPluginPtr tcplugin = TuneControllerPluginPtr(qobject_cast<TuneControllerPlugin*>(plugin));
	if (tcplugin) {
		if(!plugins_.contains(tcplugin->name())) {
			//qDebug() << "Registering plugin " << tcplugin->name();
			plugins_[tcplugin->name()] = tcplugin;
		}
		return true;
	}
	return false;
}

void TuneControllerManager::setTuneFilters(const QStringList &filters, const QString &pattern)
{
	tuneUrlFilters_ = filters;
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
			QString extension = tune.url().right(tune.url().length() - (index+1)).toLower();
			if (!extension.isEmpty() && tuneUrlFilters_.contains(extension)) {
				return false;
			}
		}
	}
	return true;
}

// ----------------------------------------------------------------------------

#ifdef TC_ITUNES
#ifdef HAVE_QT5
Q_IMPORT_PLUGIN(ITunesPlugin)
#else
Q_IMPORT_PLUGIN(itunesplugin)
#endif
#endif

#ifdef TC_WINAMP
#ifdef HAVE_QT5
Q_IMPORT_PLUGIN(WinAmpPlugin)
#else
Q_IMPORT_PLUGIN(winampplugin)
#endif
#endif

#ifdef TC_AIMP
#ifdef HAVE_QT5
Q_IMPORT_PLUGIN(AIMPPlugin)
#else
Q_IMPORT_PLUGIN(aimpplugin)
#endif
#endif

#ifdef TC_PSIFILE
#ifdef HAVE_QT5
Q_IMPORT_PLUGIN(PsiFilePlugin)
#else
Q_IMPORT_PLUGIN(psifileplugin)
#endif
#endif

#if defined(TC_MPRIS) && defined(USE_DBUS)
#ifdef HAVE_QT5
Q_IMPORT_PLUGIN(MPRISPlugin)
#else
Q_IMPORT_PLUGIN(mprisplugin)
#endif
#endif

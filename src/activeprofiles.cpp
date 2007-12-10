/*
 * activeprofiles.cpp - Class for interacting with other psi instances
 * Copyright (C) 2006  Maciej Niedzielski
 * Copyright (C) 2006-2007  Martin Hostettler
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

#include "activeprofiles.h"
#include <QMessageBox>
#include "profiles.h"


ActiveProfiles* ActiveProfiles::instance_ = 0;

/**
 * \fn virtual ActiveProfiles::~ActiveProfiles();
 * \brief Destroys the object.
 */

/**
 * \fn bool ActiveProfiles::isActive(const QString &profile) const;
 * \brief Returns true is \a profile is running.
 */

/**
 * \fn bool ActiveProfiles::setThisProfile(const QString &profile);
 * \brief Registeres this application instance as \a profile.
 * Note: you can call this function multiple times with the same value of \a profile.
 */

/**
 * \fn void ActiveProfiles::unsetThisProfile();
 * \brief Unregistered this application profile.
 */

/**
 * \fn QString ActiveProfiles::thisProfile() const;
 * \brief Returns the profile name registered for this application instance.
 * Returns empty string if no profile is registered.
 */

/**
 * \fn ActiveProfiles::ActiveProfiles(const QString &name);
 * \brief Creates new object and registers this application with its \a name.
 */

/**
 * \brief Returns the instance of ActiveProfiles.
 */
ActiveProfiles* ActiveProfiles::instance()
{
	if (!instance_) {
		instance_ = new ActiveProfiles();
	}

	return instance_;
}

/**
 * \fn ActiveProfiles::ActiveProfiles()
 * \brief Creates new object.
 */


/**
 * \fn bool ActiveProfiles::raiseOther(QString profile, bool withUI) const
 * \brief raises the main windows of another psi instance.
 */

/**
 * \brief Requests other Psi instance to open \a uri.
 * If \a profile is not running, other active instance is selected.
 * If the request cannot be sent, function returns false.
 */
bool ActiveProfiles::sendOpenUri(const QUrl &uri, const QString &profile) const
{
	return sendOpenUri(uri.toString(), profile);
}

/**
 * \fn bool ActiveProfiles::sendOpenUri(const QString &uri, const QString &profile) const
 * \brief Requests other Psi instance to open \a uri.
 * If \a profile is not running, other active instance is selected.
 * If the request cannot be sent, function returns false.
 */


/**
 * \fn void ActiveProfiles::openUri(const QUrl &uri);
 * \brief Signal emitted when other Psi instance requested to open \a uri.
 */

/**
 * \brief Picks one of running Psi instances and returns its profile name.
 */
QString ActiveProfiles::pickProfile() const
{
	QStringList profiles = getProfilesList();
	foreach (QString p, profiles) {
		if (isActive(p)) {
			return p;
		}
	}
	return "";
}

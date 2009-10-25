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
 * \brief Returns true if \a profile is running.
 */

/**
 * \fn bool ActiveProfiles::isAnyActive() const;
 * \brief Returns true if there is at least one running Psi instance.
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
 * \fn bool ActiveProfiles::setStatus(const QString &profile, const QString &status, const QString &message) const
 * \brief Requests Psi instance running \a profile to change status.
 * If \a profile is empty, other running instance is selected.
 * If the request cannot be sent, function returns false.
 */

/**
 * \fn bool ActiveProfiles::openUri(const QString &profile, const QString &uri) const
 * \brief Requests Psi instance running \a profile to open \a uri.
 * If \a profile is empty, other running instance is selected.
 * If the request cannot be sent, function returns false.
 */

/**
 * \fn bool ActiveProfiles::raise(QString profile, bool withUI) const
 * \brief Raises the main windows of Psi instance running \a profile.
 * If \a profile is empty, other running instance is selected.
 */

/**
 * \fn void setStatusRequested(const QString &status, const QString &message)
 * \brief Signal emitted when other Psi instance requested to change status.
 */

/**
 * \fn void ActiveProfiles::openUriRequested(const QUrl &uri)
 * \brief Signal emitted when other Psi instance requested to open \a uri.
 */

/**
 * \fn void ActiveProfiles::raiseRequested()
 * \brief Signal emitted when other Psi instance requested to raise main window.
 */

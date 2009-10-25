/*
 * activeprofiles_stub.cpp - Class for interacting with other psi instances
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

#include <QCoreApplication>
#include <QString>

bool ActiveProfiles::isActive(const QString &profile) const
{
	Q_UNUSED(profile);
	return false;
}

bool ActiveProfiles::isAnyActive() const
{
	return false;
}

bool ActiveProfiles::setThisProfile(const QString &profile)
{
	Q_UNUSED(profile);
	return true;
}

void ActiveProfiles::unsetThisProfile()
{
}

QString ActiveProfiles::thisProfile() const
{
	return QString();
}

ActiveProfiles::ActiveProfiles()
	: QObject(QCoreApplication::instance())
{
}

ActiveProfiles::~ActiveProfiles()
{
}

bool ActiveProfiles::setStatus(const QString &profile, const QString &status, const QString &message) const
{
	Q_UNUSED(profile);
	Q_UNUSED(status);
	Q_UNUSED(message);
	return true;
}

bool ActiveProfiles::openUri(const QString &profile, const QString &uri) const
{
	Q_UNUSED(uri);
	Q_UNUSED(profile);
	return true;
}

bool ActiveProfiles::raise(const QString &profile, bool withUI) const
{
	Q_UNUSED(profile);
	Q_UNUSED(withUI);
	return true;
}

/*
 * Copyright (C) 2006  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include "irisnetplugin.h"

namespace XMPP {

//----------------------------------------------------------------------------
// IrisNetProvider
//----------------------------------------------------------------------------
NetInterfaceProvider *IrisNetProvider::createNetInterfaceProvider()
{
	return 0;
}

NetAvailabilityProvider *IrisNetProvider::createNetAvailabilityProvider()
{
	return 0;
}

NameProvider *IrisNetProvider::createNameProviderInternet()
{
	return 0;
}

NameProvider *IrisNetProvider::createNameProviderLocal()
{
	return 0;
}

ServiceProvider *IrisNetProvider::createServiceProvider()
{
	return 0;
}

//----------------------------------------------------------------------------
// NameProvider
//----------------------------------------------------------------------------
bool NameProvider::supportsSingle() const
{
	return false;
}

bool NameProvider::supportsLongLived() const
{
	return false;
}

bool NameProvider::supportsRecordType(int type) const
{
	Q_UNUSED(type);
	return false;
}

void NameProvider::resolve_localResultsReady(int id, const QList<XMPP::NameRecord> &results)
{
	Q_UNUSED(id);
	Q_UNUSED(results);
}

void NameProvider::resolve_localError(int id, XMPP::NameResolver::Error e)
{
	Q_UNUSED(id);
	Q_UNUSED(e);
}

}

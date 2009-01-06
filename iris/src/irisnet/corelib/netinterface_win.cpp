/*
 * Copyright (C) 2008  Barracuda Networks
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

class WinNet : public NetInterfaceProvider
{
	Q_OBJECT
	Q_INTERFACES(XMPP::NetInterfaceProvider)

public:
	WinNet()
	{
	}

	void start()
	{
	}

	QList<Info> interfaces() const
	{
		return QList<Info>();
	}
};

class WinNetProvider : public IrisNetProvider
{
	Q_OBJECT
	Q_INTERFACES(XMPP::IrisNetProvider)

public:
	virtual NetInterfaceProvider *createNetInterfaceProvider()
	{
		return new WinNet;
	}
};

IrisNetProvider *irisnet_createWinNetProvider()
{
        return new WinNetProvider;
}

}

#include "netinterface_win.moc"

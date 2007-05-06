/*
 * ahcommandserver.h - Server implementation of JEP-50 (Ad-Hoc Commands)
 * Copyright (C) 2005  Remko Troncon
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
 
#ifndef AHCOMMANDSERVER_H
#define AHCOMMANDSERVER_H
 
class AHCServerManager;
class QString;
class AHCommand;
namespace XMPP {
	class Jid;
}

class AHCommandServer
{
public:
	AHCommandServer(AHCServerManager*);
	virtual ~AHCommandServer();

	virtual QString name() const = 0; 
	virtual QString node() const = 0; 
	virtual bool isAllowed(const XMPP::Jid&) const { return true; }
	virtual AHCommand execute(const AHCommand&, const XMPP::Jid& requester) = 0;
	virtual void cancel(const AHCommand&) { }

protected:
	AHCServerManager* manager() const { return manager_; }

private:
	AHCServerManager* manager_;
};

#endif

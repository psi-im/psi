/*
 * rc.h - Implementation of JEP-146 (Remote Controlling Clients)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef RC_H
#define RC_H

#include "ahcommandserver.h"

class PsiCon;

class RCCommandServer : public AHCommandServer
{
public:
	RCCommandServer(AHCServerManager* m) : AHCommandServer(m) { }
	virtual QString node() const
		{ return QString("http://jabber.org/protocol/rc#") + rcNode(); }
	virtual QString rcNode() const = 0;
	virtual bool isAllowed(const Jid&) const;

};

class RCSetStatusServer : public RCCommandServer
{
public:
	RCSetStatusServer(AHCServerManager* m) : RCCommandServer(m) { }
	virtual QString name() const { return "Set Status"; }
	virtual QString rcNode() const { return "set-status"; }
	virtual AHCommand execute(const AHCommand&, const Jid&);
};

class RCForwardServer : public RCCommandServer
{
public:
	RCForwardServer(AHCServerManager* m) : RCCommandServer(m) { }
	virtual QString name() const { return "Forward Messages"; }
	virtual QString rcNode() const { return "forward"; }
	virtual AHCommand execute(const AHCommand& c, const Jid&);
};

class RCLeaveMucServer : public RCCommandServer
{
public:
	RCLeaveMucServer(AHCServerManager* m) : RCCommandServer(m) { }
	virtual QString name() const { return "Leave All Conferences"; }
	virtual QString rcNode() const { return "leave-muc"; }
	virtual AHCommand execute(const AHCommand& c, const Jid&);
};

class RCSetOptionsServer : public RCCommandServer
{
public:
	RCSetOptionsServer(AHCServerManager* m, PsiCon* c) : RCCommandServer(m), psiCon_(c) { }
	virtual QString name() const { return "Set Options"; }
	virtual QString rcNode() const { return "set-options"; }
	virtual AHCommand execute(const AHCommand& c, const Jid&);

private:
	PsiCon* psiCon_;
};

#endif

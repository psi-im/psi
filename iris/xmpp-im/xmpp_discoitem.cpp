/*
 * xmpp_discoitem.cpp
 * Copyright (C) 2003  Justin Karneges
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "xmpp_discoitem.h"

using namespace XMPP;

class DiscoItem::Private
{
public:
	Private()
	{
		action = None;
	}

	Jid jid;
	QString name;
	QString node;
	Action action;

	Features features;
	Identities identities;
};

DiscoItem::DiscoItem()
{
	d = new Private;
}

DiscoItem::DiscoItem(const DiscoItem &from)
{
	d = new Private;
	*this = from;
}

DiscoItem & DiscoItem::operator= (const DiscoItem &from)
{
	d->jid = from.d->jid;
	d->name = from.d->name;
	d->node = from.d->node;
	d->action = from.d->action;
	d->features = from.d->features;
	d->identities = from.d->identities;

	return *this;
}

DiscoItem::~DiscoItem()
{
	delete d;
}

AgentItem DiscoItem::toAgentItem() const
{
	AgentItem ai;

	ai.setJid( jid() );
	ai.setName( name() );

	Identity id;
	if ( !identities().isEmpty() )
		id = identities().first();

	ai.setCategory( id.category );
	ai.setType( id.type );

	ai.setFeatures( d->features );

	return ai;
}

void DiscoItem::fromAgentItem(const AgentItem &ai)
{
	setJid( ai.jid() );
	setName( ai.name() );

	Identity id;
	id.category = ai.category();
	id.type = ai.type();
	id.name = ai.name();

	Identities idList;
	idList << id;

	setIdentities( idList );

	setFeatures( ai.features() );
}

const Jid &DiscoItem::jid() const
{
	return d->jid;
}

void DiscoItem::setJid(const Jid &j)
{
	d->jid = j;
}

const QString &DiscoItem::name() const
{
	return d->name;
}

void DiscoItem::setName(const QString &n)
{
	d->name = n;
}

const QString &DiscoItem::node() const
{
	return d->node;
}

void DiscoItem::setNode(const QString &n)
{
	d->node = n;
}

DiscoItem::Action DiscoItem::action() const
{
	return d->action;
}

void DiscoItem::setAction(Action a)
{
	d->action = a;
}

const Features &DiscoItem::features() const
{
	return d->features;
}

void DiscoItem::setFeatures(const Features &f)
{
	d->features = f;
}

const DiscoItem::Identities &DiscoItem::identities() const
{
	return d->identities;
}

void DiscoItem::setIdentities(const Identities &i)
{
	d->identities = i;

	if ( name().isEmpty() && i.count() )
		setName( i.first().name );
}


DiscoItem::Action DiscoItem::string2action(QString s)
{
	Action a;

	if ( s == "update" )
		a = Update;
	else if ( s == "remove" )
		a = Remove;
	else
		a = None;

	return a;
}

QString DiscoItem::action2string(Action a)
{
	QString s;

	if ( a == Update )
		s = "update";
	else if ( a == Remove )
		s = "remove";
	else
		s = QString::null;

	return s;
}



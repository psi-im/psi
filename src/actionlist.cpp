/*
 * actionlist.cpp - the customizeable action list
 * Copyright (C) 2004  Michail Pishchagin
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

#include <QList>

#include "actionlist.h"

#include <QHash>

#include "iconaction.h"

//----------------------------------------------------------------------------
// ActionList
//----------------------------------------------------------------------------

class ActionList::Private : public QObject
{
	Q_OBJECT
public:
	Private() { }
	Private( const Private & );

	QString name;
	int id;
	QStringList sortedActions;
	QHash<QString, IconAction*> actions;
	bool autoDeleteActions;

public slots:
	void actionDestroyed(QObject *);
};

ActionList::ActionList( QString name, int id, bool autoDelete )
	: QObject()
{
	d = new Private();
	d->autoDeleteActions = autoDelete;

	d->name = name;
	d->id   = id;
}

ActionList::ActionList( const ActionList &from )
	: QObject()
{
	d = new Private( *from.d );
}

ActionList::~ActionList()
{
	delete d;
}

QString ActionList::name() const
{
	return d->name;
}

int ActionList::id() const
{
	return d->id;
}

IconAction *ActionList::action( QString name ) const
{
	return d->actions[name];
}

QStringList ActionList::actions() const
{
	return d->sortedActions;
}

void ActionList::addAction( QString name, IconAction *action )
{
	d->sortedActions << name;

	if ( action ) {
		action->setObjectName( name );
		d->actions.insert( name, action );
		d->connect( action, SIGNAL( destroyed(QObject *) ), d, SLOT( actionDestroyed(QObject *) ) );
	}
}

void ActionList::clear()
{
	if (d->autoDeleteActions)
		qDeleteAll(d->actions);
	d->actions.clear();
}

ActionList::Private::Private( const Private &from )
	: QObject()
{
	name = from.name;
	id   = from.id;

	actions = from.actions;
	autoDeleteActions = from.autoDeleteActions;

	sortedActions = from.sortedActions;
}

void ActionList::Private::actionDestroyed(QObject *obj)
{
	actions.remove( obj->objectName() );
}

//----------------------------------------------------------------------------
// MetaActionList
//----------------------------------------------------------------------------

class MetaActionList::Private
{
public:
	Private() { }

	QList<ActionList*> lists;
};

MetaActionList::MetaActionList()
	: QObject()
{
	d = new Private();
}

MetaActionList::~MetaActionList()
{
	while (!d->lists.isEmpty())
		delete d->lists.takeFirst();
	delete d;
}

ActionList *MetaActionList::actionList( QString name ) const
{
	foreach(ActionList* a, d->lists) {
		if (a->name() == name)
			return a;
	}

	return 0;
}

QList<ActionList*> MetaActionList::actionLists( int id ) const
{
	QList<ActionList*> list;

	for ( int i = 0; i < 32; i++ ) {
		if ( !(id & ( 1 << i )) )
			continue;

		foreach(ActionList* a, d->lists) {
			if ( a->id() & ( 1 << i ) )
				list.append(a);
		}
	}

	return list;
}

ActionList MetaActionList::suitableActions( int id ) const
{
	QList<ActionList*> lists = actionLists( id );
	ActionList actions("", 0, false);

	foreach(ActionList* list, lists) {
		QStringList actionList = list->actions();
		QStringList::Iterator it2 = actionList.begin();
		for ( ; it2 != actionList.end(); ++it2 )
			actions.addAction( *it2, list->action( *it2 ) );
	}

	return actions;
}

QStringList MetaActionList::actionLists() const
{
	QStringList names;

	foreach(ActionList* l, d->lists)
		names << l->name();

	return names;
}

void MetaActionList::addList( ActionList *list )
{
	if ( list )
		d->lists.append( list );
}

void MetaActionList::clear()
{
	
	foreach(ActionList* l, d->lists) {
		l->clear();
	}
}

#include "actionlist.moc"

/*
 * Copyright (C) 2008  Justin Karneges
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

#ifndef OBJECTSESSION_H
#define OBJECTSESSION_H

#include <QObject>

namespace XMPP {

class ObjectSessionPrivate;
class ObjectSessionWatcherPrivate;

class ObjectSession : public QObject
{
	Q_OBJECT

public:
	ObjectSession(QObject *parent = 0);
	~ObjectSession();

	// clear all deferred requests, invalidate watchers
	void reset();

	bool isDeferred(QObject *obj, const char *method);
	void defer(QObject *obj, const char *method,
		QGenericArgument val0 = QGenericArgument(),
		QGenericArgument val1 = QGenericArgument(),
		QGenericArgument val2 = QGenericArgument(),
		QGenericArgument val3 = QGenericArgument(),
		QGenericArgument val4 = QGenericArgument(),
		QGenericArgument val5 = QGenericArgument(),
		QGenericArgument val6 = QGenericArgument(),
		QGenericArgument val7 = QGenericArgument(),
		QGenericArgument val8 = QGenericArgument(),
		QGenericArgument val9 = QGenericArgument());
	void deferExclusive(QObject *obj, const char *method,
		QGenericArgument val0 = QGenericArgument(),
		QGenericArgument val1 = QGenericArgument(),
		QGenericArgument val2 = QGenericArgument(),
		QGenericArgument val3 = QGenericArgument(),
		QGenericArgument val4 = QGenericArgument(),
		QGenericArgument val5 = QGenericArgument(),
		QGenericArgument val6 = QGenericArgument(),
		QGenericArgument val7 = QGenericArgument(),
		QGenericArgument val8 = QGenericArgument(),
		QGenericArgument val9 = QGenericArgument());

	void pause();
	void resume();

private:
	friend class ObjectSessionWatcher;
	ObjectSessionPrivate *d;
};

class ObjectSessionWatcher
{
public:
	ObjectSessionWatcher(ObjectSession *sess);
	~ObjectSessionWatcher();

	bool isValid() const;

private:
	friend class ObjectSessionPrivate;
	ObjectSessionWatcherPrivate *d;
};

}

#endif

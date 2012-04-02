/****************************************************************************
** tasklist.h - A small, but useful Task List
** Copyright (C) 2003  Michail Pishchagin
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
**
****************************************************************************/

#ifndef TASKLIST_H
#define TASKLIST_H

#include <QObject>

#include "xmpp_task.h"

using namespace XMPP;

//----------------------------------------------------------------------------
// TaskList -- read some comments inline
//----------------------------------------------------------------------------

class TaskList : public QObject, public QList<Task*>
{
	Q_OBJECT

public:
	TaskList()
	{
	}

	~TaskList()
	{
		for(QList<Task*>::Iterator i = begin(); i != end(); i++) {
			(*i)->safeDelete();
		}
	}

	void append(Task *d)
	{
		if ( isEmpty() )
			emit started();

		connect(d, SIGNAL(destroyed(QObject *)), SLOT(taskDestroyed(QObject *)));
		QList<Task*>::append(d);
	}

signals:
	// started() is emitted, when TaskList doesn't have any tasks in it,
	// and append() is called, indicating, that TaskList contains at least one
	// running Task
	void started();

	// finished() is emitted when TaskList contains one Task, and it suddenly
	// terminates, indicating, that TaskList is empty now
	void finished();

private slots:
	void taskDestroyed(QObject *p)
	{
		removeAll(static_cast<Task*>(p));

		if ( isEmpty() )
			emit finished();
	}
};

#endif

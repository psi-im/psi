/*
 * globaleventqueue.h - a list of all queued events from enabled accounts
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#ifndef GLOBALEVENTQUEUE_H
#define GLOBALEVENTQUEUE_H

#include <QObject>

#include "psievent.h"

class GlobalEventQueue : public QObject
{
	Q_OBJECT

public:
	static GlobalEventQueue* instance();

	int count() const;

	const QList<int>& ids() const;
	PsiEvent *peek(int id) const;

protected:
	void enqueue(EventItem* item);
	void dequeue(EventItem* item);

signals:
	void queueChanged();

private:
	GlobalEventQueue();

	static GlobalEventQueue* instance_;
	QList<int> ids_;
	QList<EventItem*> items_;
	friend class EventQueue;
};

#endif

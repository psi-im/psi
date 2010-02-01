/*
 * globaleventqueue.cpp - a list of all queued events from enabled accounts
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

#include "globaleventqueue.h"

#include <QCoreApplication>

GlobalEventQueue* GlobalEventQueue::instance()
{
	if (!instance_)
		instance_ = new GlobalEventQueue();
	return instance_;
}

int GlobalEventQueue::count() const
{
	return items_.count();
}

const QList<int>& GlobalEventQueue::ids() const
{
	return ids_;
}

PsiEvent *GlobalEventQueue::peek(int id) const
{
	Q_ASSERT(ids_.contains(id));
	foreach(EventItem* item, items_)
		if (item->id() == id)
			return item->event();
	return 0;
}

void GlobalEventQueue::enqueue(EventItem* item)
{
	Q_ASSERT(item);
	Q_ASSERT(!ids_.contains(item->id()));
	if (!item || ids_.contains(item->id()))
		return;

	ids_.append(item->id());
	items_.append(item);
	Q_ASSERT(ids_.contains(item->id()));

	emit queueChanged();
}

void GlobalEventQueue::dequeue(EventItem* item)
{
	Q_ASSERT(item);
	Q_ASSERT(ids_.contains(item->id()));
	if (!item || !ids_.contains(item->id()))
		return;

	ids_.removeAll(item->id());
	items_.removeAll(item);
	Q_ASSERT(!ids_.contains(item->id()));

	emit queueChanged();
}

GlobalEventQueue::GlobalEventQueue()
	: QObject(QCoreApplication::instance())
{}

GlobalEventQueue* GlobalEventQueue::instance_ = NULL;

/*
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

#include <QTimer>
#include <qplatformdefs.h>

#include "safedelete.h"
#include "xmpp_task.h"
#include "xmpp_client.h"
#include "xmpp_xmlcommon.h"

using namespace XMPP;

class Task::TaskPrivate
{
public:
	TaskPrivate() {}

	QString id;
	bool success;
	int statusCode;
	QString statusString;
	Client *client;
	bool insig, deleteme, autoDelete;
	bool done;
};

Task::Task(Task *parent)
:QObject(parent)
{
	init();

	d->client = parent->client();
	d->id = client()->genUniqueId();
	connect(d->client, SIGNAL(disconnected()), SLOT(clientDisconnected()));
}

Task::Task(Client *parent, bool)
:QObject(0)
{
	init();

	d->client = parent;
	connect(d->client, SIGNAL(disconnected()), SLOT(clientDisconnected()));
}

Task::~Task()
{
	delete d;
}

void Task::init()
{
	d = new TaskPrivate;
	d->success = false;
	d->insig = false;
	d->deleteme = false;
	d->autoDelete = false;
	d->done = false;
}

Task *Task::parent() const
{
	return (Task *)QObject::parent();
}

Client *Task::client() const
{
	return d->client;
}

QDomDocument *Task::doc() const
{
	return client()->doc();
}

QString Task::id() const
{
	return d->id;
}

bool Task::success() const
{
	return d->success;
}

int Task::statusCode() const
{
	return d->statusCode;
}

const QString & Task::statusString() const
{
	return d->statusString;
}

void Task::go(bool autoDelete)
{
	d->autoDelete = autoDelete;

	onGo();
}

bool Task::take(const QDomElement &x)
{
	const QObjectList p = children();

	// pass along the xml
	Task *t;
	for(QObjectList::ConstIterator it = p.begin(); it != p.end(); ++it) {
		QObject *obj = *it;
		if(!obj->inherits("XMPP::Task"))
			continue;

		t = static_cast<Task*>(obj);
		if(t->take(x))
			return true;
	}

	return false;
}

void Task::safeDelete()
{
	if(d->deleteme)
		return;

	d->deleteme = true;
	if(!d->insig)
		SafeDelete::deleteSingle(this);
}

void Task::onGo()
{
}

void Task::onDisconnect()
{
	if(!d->done) {
		d->success = false;
		d->statusCode = ErrDisc;
		d->statusString = tr("Disconnected");

		// delay this so that tasks that react don't block the shutdown
		QTimer::singleShot(0, this, SLOT(done()));
	}
}

void Task::send(const QDomElement &x)
{
	client()->send(x);
}

void Task::setSuccess(int code, const QString &str)
{
	if(!d->done) {
		d->success = true;
		d->statusCode = code;
		d->statusString = str;
		done();
	}
}

void Task::setError(const QDomElement &e)
{
	if(!d->done) {
		d->success = false;
		getErrorFromElement(e, d->client->streamBaseNS(), &d->statusCode, &d->statusString);
		done();
	}
}

void Task::setError(int code, const QString &str)
{
	if(!d->done) {
		d->success = false;
		d->statusCode = code;
		d->statusString = str;
		done();
	}
}

void Task::done()
{
	if(d->done || d->insig)
		return;
	d->done = true;

	if(d->deleteme || d->autoDelete)
		d->deleteme = true;

	d->insig = true;
	finished();
	d->insig = false;

	if(d->deleteme)
		SafeDelete::deleteSingle(this);
}

void Task::clientDisconnected()
{
	onDisconnect();
}

void Task::debug(const char *fmt, ...)
{
	char *buf;
	QString str;
	int size = 1024;
	int r;

	do {
		buf = new char[size];
		va_list ap;
		va_start(ap, fmt);
		r = QT_VSNPRINTF(buf, size, fmt, ap);
		va_end(ap);

		if(r != -1)
			str = QString(buf);

		delete [] buf;

		size *= 2;
	} while(r == -1);

	debug(str);
}

void Task::debug(const QString &str)
{
	client()->debug(QString("%1: ").arg(metaObject()->className()) + str);
}

bool Task::iqVerify(const QDomElement &x, const Jid &to, const QString &id, const QString &xmlns)
{
	if(x.tagName() != "iq")
		return false;

	Jid from(x.attribute("from"));
	Jid local = client()->jid();
	Jid server = client()->host();

	// empty 'from' ?
	if(from.isEmpty()) {
		// allowed if we are querying the server
		if(!to.isEmpty() && !to.compare(server))
			return false;
	}
	// from ourself?
	else if(from.compare(local, false) || from.compare(local.domain(),false)) {
		// allowed if we are querying ourself or the server
		if(!to.isEmpty() && !to.compare(local, false) && !to.compare(server))
			return false;
	}
	// from anywhere else?
	else {
		if(!from.compare(to))
			return false;
	}

	if(!id.isEmpty()) {
		if(x.attribute("id") != id)
			return false;
	}

	if(!xmlns.isEmpty()) {
		if(queryNS(x) != xmlns)
			return false;
	}

	return true;
}


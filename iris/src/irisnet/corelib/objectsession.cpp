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

#include "objectsession.h"

#include <QList>
#include <QByteArray>
#include <QMetaObject>
#include <QMetaType>
#include <QTimer>

namespace XMPP {

class ObjectSessionWatcherPrivate
{
public:
	ObjectSession *sess;
};

class ObjectSessionPrivate : public QObject
{
	Q_OBJECT

public:
	ObjectSession *q;

	class MethodCall
	{
	public:
		QObject *obj;
		QByteArray method;
		class Argument
		{
		public:
			int type;
			void *data;
		};
		QList<Argument> args;

		MethodCall(QObject *_obj, const char *_method) :
			obj(_obj),
			method(_method)
		{
		}

		~MethodCall()
		{
			clearArgs();
		}

		void clearArgs()
		{
			for(int n = 0; n < args.count(); ++n)
				QMetaType::destroy(args[n].type, args[n].data);
			args.clear();
		}

		bool setArgs(QGenericArgument val0 = QGenericArgument(),
			QGenericArgument val1 = QGenericArgument(),
			QGenericArgument val2 = QGenericArgument(),
			QGenericArgument val3 = QGenericArgument(),
			QGenericArgument val4 = QGenericArgument(),
			QGenericArgument val5 = QGenericArgument(),
			QGenericArgument val6 = QGenericArgument(),
			QGenericArgument val7 = QGenericArgument(),
			QGenericArgument val8 = QGenericArgument(),
			QGenericArgument val9 = QGenericArgument())
		{
			const char *arg_name[] =
			{
				val0.name(), val1.name(), val2.name(),
				val3.name(), val4.name(), val5.name(),
				val6.name(), val7.name(), val8.name(),
				val9.name()
			};

			void *arg_data[] =
			{
				val0.data(), val1.data(), val2.data(),
				val3.data(), val4.data(), val5.data(),
				val6.data(), val7.data(), val8.data(),
				val9.data()
			};

			clearArgs();

			for(int n = 0; n < 10; ++n)
			{
				if(arg_name[n] == 0)
					break;

				Argument arg;
				arg.type = QMetaType::type(arg_name[n]);
				if(!arg.type)
				{
					clearArgs();
					return false;
				}

				arg.data = QMetaType::construct(arg.type, arg_data[n]);
				args += arg;
			}

			return true;
		}
	};

	QList<MethodCall*> pendingCalls;
	QTimer *callTrigger;
	bool paused;
	QList<ObjectSessionWatcherPrivate*> watchers;

	ObjectSessionPrivate(ObjectSession *_q) :
		QObject(_q),
		q(_q),
		paused(false)
	{
		callTrigger = new QTimer(this);
		connect(callTrigger, SIGNAL(timeout()), SLOT(doCall()));
		callTrigger->setSingleShot(true);
	}

	~ObjectSessionPrivate()
	{
		invalidateWatchers();

		callTrigger->disconnect(this);
		callTrigger->setParent(0);
		callTrigger->deleteLater();
	}

	void addPendingCall(MethodCall *call)
	{
		pendingCalls += call;
		if(!paused && !callTrigger->isActive())
			callTrigger->start();
	}

	bool havePendingCall(QObject *obj, const char *method) const
	{
		foreach(const MethodCall *call, pendingCalls)
		{
			if(call->obj == obj && qstrcmp(call->method.data(), method) == 0)
				return true;
		}
		return false;
	}

	void invalidateWatchers()
	{
		for(int n = 0; n < watchers.count(); ++n)
			watchers[n]->sess = 0;
		watchers.clear();
	}

private slots:
	void doCall()
	{
		MethodCall *call = pendingCalls.takeFirst();
		if(!pendingCalls.isEmpty())
			callTrigger->start();

		Q_ASSERT(call->args.count() <= 10);

		QGenericArgument arg[10];
		for(int n = 0; n < call->args.count(); ++n)
                	arg[n] = QGenericArgument(QMetaType::typeName(call->args[n].type), call->args[n].data);

		bool ok;
		ok = QMetaObject::invokeMethod(call->obj, call->method.data(),
			Qt::DirectConnection,
			arg[0], arg[1], arg[2], arg[3], arg[4],
			arg[5], arg[6], arg[7], arg[8], arg[9]);
		Q_ASSERT(ok);

		delete call;
	}
};

ObjectSessionWatcher::ObjectSessionWatcher(ObjectSession *sess)
{
	d = new ObjectSessionWatcherPrivate;
	d->sess = sess;
	if(d->sess)
		d->sess->d->watchers += d;
}

ObjectSessionWatcher::~ObjectSessionWatcher()
{
	delete d;
}

bool ObjectSessionWatcher::isValid() const
{
	if(d->sess)
		return true;
	else
		return false;
}


ObjectSession::ObjectSession(QObject *parent) :
	QObject(parent)
{
	d = new ObjectSessionPrivate(this);
}

ObjectSession::~ObjectSession()
{
	delete d;
}

void ObjectSession::reset()
{
	d->invalidateWatchers();
	if(d->callTrigger->isActive())
		d->callTrigger->stop();
	d->pendingCalls.clear();
}

bool ObjectSession::isDeferred(QObject *obj, const char *method)
{
	return d->havePendingCall(obj, method);
}

void ObjectSession::defer(QObject *obj, const char *method,
	QGenericArgument val0,
	QGenericArgument val1,
	QGenericArgument val2,
	QGenericArgument val3,
	QGenericArgument val4,
	QGenericArgument val5,
	QGenericArgument val6,
	QGenericArgument val7,
	QGenericArgument val8,
	QGenericArgument val9)
{
	ObjectSessionPrivate::MethodCall *call = new ObjectSessionPrivate::MethodCall(obj, method);
	call->setArgs(val0, val1, val2, val3, val4, val5, val6, val7, val8, val9);
	d->addPendingCall(call);
}

void ObjectSession::deferExclusive(QObject *obj, const char *method,
	QGenericArgument val0,
	QGenericArgument val1,
	QGenericArgument val2,
	QGenericArgument val3,
	QGenericArgument val4,
	QGenericArgument val5,
	QGenericArgument val6,
	QGenericArgument val7,
	QGenericArgument val8,
	QGenericArgument val9)
{
	if(d->havePendingCall(obj, method))
		return;

	ObjectSessionPrivate::MethodCall *call = new ObjectSessionPrivate::MethodCall(obj, method);
	call->setArgs(val0, val1, val2, val3, val4, val5, val6, val7, val8, val9);
	d->addPendingCall(call);
}

void ObjectSession::pause()
{
	Q_ASSERT(!d->paused);

	if(d->callTrigger->isActive())
		d->callTrigger->stop();
	d->paused = true;
}

void ObjectSession::resume()
{
	Q_ASSERT(d->paused);

	d->paused = false;
	if(!d->pendingCalls.isEmpty())
		d->callTrigger->start();
}

}

#include "objectsession.moc"

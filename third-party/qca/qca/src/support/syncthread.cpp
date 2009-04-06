/*
 * Copyright (C) 2006  Justin Karneges <justin@affinix.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include "qca_support.h"

#include <QEventLoop>
#include <QMetaMethod>
#include <QMutexLocker>
#include <QWaitCondition>

namespace QCA {

QByteArray methodReturnType(const QMetaObject *obj, const QByteArray &method, const QList<QByteArray> argTypes)
{
	for(int n = 0; n < obj->methodCount(); ++n)
	{
		QMetaMethod m = obj->method(n);
		QByteArray sig = m.signature();
		int offset = sig.indexOf('(');
		if(offset == -1)
			continue;
		QByteArray name = sig.mid(0, offset);
		if(name != method)
			continue;
		if(m.parameterTypes() != argTypes)
			continue;

		return m.typeName();
	}
	return QByteArray();
}

bool invokeMethodWithVariants(QObject *obj, const QByteArray &method, const QVariantList &args, QVariant *ret, Qt::ConnectionType type)
{
	// QMetaObject::invokeMethod() has a 10 argument maximum
	if(args.count() > 10)
		return false;

	QList<QByteArray> argTypes;
	for(int n = 0; n < args.count(); ++n)
		argTypes += args[n].typeName();

	// get return type
	int metatype = 0;
	QByteArray retTypeName = methodReturnType(obj->metaObject(), method, argTypes);
	if(!retTypeName.isEmpty())
	{
		metatype = QMetaType::type(retTypeName.data());
		if(metatype == 0) // lookup failed
			return false;
	}

	QGenericArgument arg[10];
	for(int n = 0; n < args.count(); ++n)
		arg[n] = QGenericArgument(args[n].typeName(), args[n].constData());

	QGenericReturnArgument retarg;
	QVariant retval;
	if(metatype != 0)
	{
		retval = QVariant(metatype, (const void *)0);
		retarg = QGenericReturnArgument(retval.typeName(), retval.data());
	}

	if(!QMetaObject::invokeMethod(obj, method.data(), type, retarg, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7], arg[8], arg[9]))
		return false;

	if(retval.isValid() && ret)
		*ret = retval;
	return true;
}

//----------------------------------------------------------------------------
// SyncThread
//----------------------------------------------------------------------------
class SyncThreadAgent;

class SyncThread::Private : public QObject
{
	Q_OBJECT
public:
	SyncThread *q;
	QMutex m;
	QWaitCondition w;
	QEventLoop *loop;
	SyncThreadAgent *agent;
	bool last_success;
	QVariant last_ret;

	Private(SyncThread *_q) : QObject(_q), q(_q)
	{
		loop = 0;
		agent = 0;
	}

private slots:
	void agent_started();
	void agent_call_ret(bool success, const QVariant &ret);
};

class SyncThreadAgent : public QObject
{
	Q_OBJECT
public:
	SyncThreadAgent(QObject *parent = 0) : QObject(parent)
	{
		QMetaObject::invokeMethod(this, "started", Qt::QueuedConnection);
	}

signals:
	void started();
	void call_ret(bool success, const QVariant &ret);

public slots:
	void call_do(QObject *obj, const QByteArray &method, const QVariantList &args)
	{
		QVariant ret;
		bool ok = invokeMethodWithVariants(obj, method, args, &ret, Qt::DirectConnection);
		emit call_ret(ok, ret);
	}
};

SyncThread::SyncThread(QObject *parent)
:QThread(parent)
{
	d = new Private(this);
	qRegisterMetaType<QVariant>("QVariant");
	qRegisterMetaType<QVariantList>("QVariantList");
}

SyncThread::~SyncThread()
{
	stop();
	delete d;
}

void SyncThread::start()
{
	QMutexLocker locker(&d->m);
	Q_ASSERT(!d->loop);
	QThread::start();
	d->w.wait(&d->m);
}

void SyncThread::stop()
{
	QMutexLocker locker(&d->m);
	if(!d->loop)
		return;
	QMetaObject::invokeMethod(d->loop, "quit");
	d->w.wait(&d->m);
	wait();
}

QVariant SyncThread::call(QObject *obj, const QByteArray &method, const QVariantList &args, bool *ok)
{
	QMutexLocker locker(&d->m);
	bool ret;
	ret = QMetaObject::invokeMethod(d->agent, "call_do",
		Qt::QueuedConnection, Q_ARG(QObject*, obj),
		Q_ARG(QByteArray, method), Q_ARG(QVariantList, args));
	Q_ASSERT(ret);
	d->w.wait(&d->m);
	if(ok)
		*ok = d->last_success;
	QVariant v = d->last_ret;
	d->last_ret = QVariant();
	return v;
}

void SyncThread::run()
{
	d->m.lock();
	d->loop = new QEventLoop;
	d->agent = new SyncThreadAgent;
	connect(d->agent, SIGNAL(started()), d, SLOT(agent_started()), Qt::DirectConnection);
	connect(d->agent, SIGNAL(call_ret(bool, const QVariant &)), d, SLOT(agent_call_ret(bool, const QVariant &)), Qt::DirectConnection);
	d->loop->exec();
	d->m.lock();
	atEnd();
	delete d->agent;
	delete d->loop;
	d->agent = 0;
	d->loop = 0;
	d->w.wakeOne();
	d->m.unlock();
}

void SyncThread::Private::agent_started()
{
	q->atStart();
	w.wakeOne();
	m.unlock();
}

void SyncThread::Private::agent_call_ret(bool success, const QVariant &ret)
{
	QMutexLocker locker(&m);
	last_success = success;
	last_ret = ret;
	w.wakeOne();
}

}

#include "syncthread.moc"

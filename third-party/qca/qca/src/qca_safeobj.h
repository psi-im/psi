/*
 * qca_safeobj.h - Qt Cryptographic Architecture
 * Copyright (C) 2008  Justin Karneges <justin@affinix.com>
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

#ifndef QCA_SAFEOBJ_H
#define QCA_SAFEOBJ_H

// NOTE: this API is private to QCA

#include <QTimer>
#include <QSocketNotifier>

namespace QCA {

// This function performs the following steps:
//   obj->disconnect(owner); // to prevent future signals to owner
//   obj->setParent(0);      // to prevent delete if parent is deleted
//   obj->deleteLater();     // now we can forget about the object
void releaseAndDeleteLater(QObject *owner, QObject *obj);

class SafeTimer : public QObject
{
	Q_OBJECT
public:
	SafeTimer(QObject *parent = 0) :
		QObject(parent)
	{
		t = new QTimer(this);
		connect(t, SIGNAL(timeout()), SIGNAL(timeout()));
	}

	~SafeTimer()
	{
		releaseAndDeleteLater(this, t);
	}

	int interval() const                { return t->interval(); }
	bool isActive() const               { return t->isActive(); }
	bool isSingleShot() const           { return t->isSingleShot(); }
	void setInterval(int msec)          { t->setInterval(msec); }
	void setSingleShot(bool singleShot) { t->setSingleShot(singleShot); }
	int timerId() const                 { return t->timerId(); }

public slots:
	void start(int msec)                { t->start(msec); }
	void start()                        { t->start(); }
	void stop()                         { t->stop(); }

signals:
	void timeout();

private:
	QTimer *t;
};

class SafeSocketNotifier : public QObject
{
	Q_OBJECT
public:
	SafeSocketNotifier(int socket, QSocketNotifier::Type type,
		QObject *parent = 0) :
		QObject(parent)
	{
		sn = new QSocketNotifier(socket, type, this);
		connect(sn, SIGNAL(activated(int)), SIGNAL(activated(int)));
	}

	~SafeSocketNotifier()
	{
		sn->setEnabled(false);
		releaseAndDeleteLater(this, sn);
	}

	bool isEnabled() const             { return sn->isEnabled(); }
	int socket() const                 { return sn->socket(); }
	QSocketNotifier::Type type() const { return sn->type(); }

public slots:
	void setEnabled(bool enable)       { sn->setEnabled(enable); }

signals:
	void activated(int socket);

private:
	QSocketNotifier *sn;
};

}

#endif

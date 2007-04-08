/*
 * xmpp_task.h
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

#ifndef XMPP_TASK_H
#define XMPP_TASK_H

#include <QObject>
#include <QString>

class QDomDocument;
class QDomElement;

namespace XMPP {
	class Client;
	class Jid;

	class Task : public QObject
	{
		Q_OBJECT
	public:
		enum { ErrDisc };
		Task(Task *parent);
		Task(Client *, bool isRoot);
		virtual ~Task();

		Task *parent() const;
		Client *client() const;
		QDomDocument *doc() const;
		QString id() const;

		bool success() const;
		int statusCode() const;
		const QString & statusString() const;

		void go(bool autoDelete=false);
		virtual bool take(const QDomElement &);
		void safeDelete();

	signals:
		void finished();

	protected:
		virtual void onGo();
		virtual void onDisconnect();
		void send(const QDomElement &);
		void setSuccess(int code=0, const QString &str="");
		void setError(const QDomElement &);
		void setError(int code=0, const QString &str="");
		void debug(const char *, ...);
		void debug(const QString &);
		bool iqVerify(const QDomElement &x, const Jid &to, const QString &id, const QString &xmlns="");

	private slots:
		void clientDisconnected();
		void done();

	private:
		void init();

		class TaskPrivate;
		TaskPrivate *d;
	};
}

#endif

/*
 * tasks.h - basic tasks
 * Copyright (C) 2001, 2002  Justin Karneges
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

#ifndef JABBER_TASKS_H
#define JABBER_TASKS_H

#include <qstring.h>
#include <qdom.h>
//Added by qt3to4:
#include <QList>

#include "im.h"
#include "xmpp_vcard.h"
#include "xmpp_discoinfotask.h"

namespace XMPP
{
	class Roster;
	class Status;

	class JT_Register : public Task
	{
		Q_OBJECT
	public:
		JT_Register(Task *parent);
		~JT_Register();

		// OLd style registration
		void reg(const QString &user, const QString &pass);

		void changepw(const QString &pass);
		void unreg(const Jid &j="");

		const Form & form() const;
		bool hasXData() const;
		const XData& xdata() const;
		void getForm(const Jid &);
		void setForm(const Form &);
		void setForm(const Jid&, const XData &);

		void onGo();
		bool take(const QDomElement &);

	private:
		QDomElement iq;
		Jid to;

		class Private;
		Private *d;
	};

	class JT_UnRegister : public Task
	{
		Q_OBJECT
	public:
		JT_UnRegister(Task *parent);
		~JT_UnRegister();

		void unreg(const Jid &);

		void onGo();

	private slots:
		void getFormFinished();
		void unregFinished();

	private:
		class Private;
		Private *d;
	};

	class JT_Roster : public Task
	{
		Q_OBJECT
	public:
		JT_Roster(Task *parent);
		~JT_Roster();

		void get();
		void set(const Jid &, const QString &name, const QStringList &groups);
		void remove(const Jid &);

		const Roster & roster() const;

		QString toString() const;
		bool fromString(const QString &);

		void onGo();
		bool take(const QDomElement &x);

	private:
		int type;
		QDomElement iq;
		Jid to;

		class Private;
		Private *d;
	};

	class JT_PushRoster : public Task
	{
		Q_OBJECT
	public:
		JT_PushRoster(Task *parent);
		~JT_PushRoster();

		bool take(const QDomElement &);

	signals:
		void roster(const Roster &);

	private:
		class Private;
		Private *d;
	};

	class JT_Presence : public Task
	{
		Q_OBJECT
	public:
		JT_Presence(Task *parent);
		~JT_Presence();

		void pres(const Status &);
		void pres(const Jid &, const Status &);
		void sub(const Jid &, const QString &subType, const QString& nick = QString());

		void onGo();

	private:
		QDomElement tag;
		int type;

		class Private;
		Private *d;
	};

	class JT_PushPresence : public Task
	{
		Q_OBJECT
	public:
		JT_PushPresence(Task *parent);
		~JT_PushPresence();

		bool take(const QDomElement &);

	signals:
		void presence(const Jid &, const Status &);
		void subscription(const Jid &, const QString &, const QString&);

	private:
		class Private;
		Private *d;
	};
	
	class JT_Session : public Task
	{
	public:
		JT_Session(Task *parent);
		void onGo();
		bool take(const QDomElement&);
	};

	class JT_Message : public Task
	{
		Q_OBJECT
	public:
		JT_Message(Task *parent, const Message &);
		~JT_Message();

		void onGo();

	private:
		Message m;

		class Private;
		Private *d;
	};

	class JT_PushMessage : public Task
	{
		Q_OBJECT
	public:
		JT_PushMessage(Task *parent);
		~JT_PushMessage();

		bool take(const QDomElement &);

	signals:
		void message(const Message &);

	private:
		class Private;
		Private *d;
	};

	class JT_GetServices : public Task
	{
		Q_OBJECT
	public:
		JT_GetServices(Task *);

		void get(const Jid &);

		const AgentList & agents() const;

		void onGo();
		bool take(const QDomElement &x);

	private:
		class Private;
		Private *d;

		QDomElement iq;
		Jid jid;
		AgentList agentList;
	};

	class JT_VCard : public Task
	{
		Q_OBJECT
	public:
		JT_VCard(Task *parent);
		~JT_VCard();

		void get(const Jid &);
		void set(const VCard &);

		const Jid & jid() const;
		const VCard  & vcard() const;
		
		void onGo();
		bool take(const QDomElement &x);

	private:
		int type;

		class Private;
		Private *d;
	};

	class JT_Search : public Task
	{
		Q_OBJECT
	public:
		JT_Search(Task *parent);
		~JT_Search();

		const Form & form() const;
		const QList<SearchResult> & results() const;

		void get(const Jid &);
		void set(const Form &);

		void onGo();
		bool take(const QDomElement &x);

	private:
		QDomElement iq;
		int type;

		class Private;
		Private *d;
	};

	class JT_ClientVersion : public Task
	{
		Q_OBJECT
	public:
		JT_ClientVersion(Task *);

		void get(const Jid &);
		void onGo();
		bool take(const QDomElement &);

		const Jid & jid() const;
		const QString & name() const;
		const QString & version() const;
		const QString & os() const;

	private:
		QDomElement iq;

		Jid j;
		QString v_name, v_ver, v_os;
	};
/*
	class JT_ClientTime : public Task
	{
		Q_OBJECT
	public:
		JT_ClientTime(Task *, const Jid &);

		void go();
		bool take(const QDomElement &);

		Jid j;
		QDateTime utc;
		QString timezone, display;

	private:
		QDomElement iq;
	};
*/
	class JT_ServInfo : public Task
	{
		Q_OBJECT
	public:
		JT_ServInfo(Task *);
		~JT_ServInfo();

		bool take(const QDomElement &);
	};

	class JT_Gateway : public Task
	{
		Q_OBJECT
	public:
		JT_Gateway(Task *);

		void get(const Jid &);
		void set(const Jid &, const QString &prompt);
		void onGo();
		bool take(const QDomElement &);

		Jid jid() const;
		QString desc() const;
		QString prompt() const;

	private:
		QDomElement iq;

		int type;
		Jid v_jid;
		QString v_prompt, v_desc;
	};

	class JT_Browse : public Task
	{
		Q_OBJECT
	public:
		JT_Browse(Task *);
		~JT_Browse();

		void get(const Jid &);

		const AgentList & agents() const;
		const AgentItem & root() const;

		void onGo();
		bool take(const QDomElement &);

	private:
		class Private;
		Private *d;

		AgentItem browseHelper (const QDomElement &i);
	};

	class JT_DiscoItems : public Task
	{
		Q_OBJECT
	public:
		JT_DiscoItems(Task *);
		~JT_DiscoItems();
	
		void get(const Jid &, const QString &node = QString::null);
		void get(const DiscoItem &);
	
		const DiscoList &items() const;
	
		void onGo();
		bool take(const QDomElement &);
	
	private:
		class Private;
		Private *d;
	};

	class JT_DiscoPublish : public Task
	{
		Q_OBJECT
	public:
		JT_DiscoPublish(Task *);
		~JT_DiscoPublish();
	
		void set(const Jid &, const DiscoList &);
	
		void onGo();
		bool take(const QDomElement &);
	
	private:
		class Private;
		Private *d;
	};
}

#endif

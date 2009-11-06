/*
 * Copyright (C) 2009  Barracuda Networks, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "groupchatbrowsecontroller.h"

#include "groupchatbrowsewindow.h"
#include "psiaccount_b.h"
#include "xmpp_tasks.h"
#include "xmpp_discoinfotask.h"
#include "xmpp_xmlcommon.h"
#include "psiiconset.h"

Q_DECLARE_METATYPE(GroupChatBrowseWindow::RoomOptions)

// FIXME: this code is thrown together and doesn't support multiple
//   simlutaneous operations or canceling

// FIXME: dialogRegister might be used improperly

class JT_DestroyRoom : public XMPP::Task
{
	Q_OBJECT

public:
	JT_DestroyRoom(XMPP::Task *parent) :
		XMPP::Task(parent)
	{
	}

	void destroy(const XMPP::Jid &roomJid)
	{
		to = roomJid;
		iq = createIQ(doc(), "set", to.full(), id());
		QDomElement query = doc()->createElement("query");
		query.setAttribute("xmlns", "http://jabber.org/protocol/muc#owner");
		QDomElement edestroy = doc()->createElement("destroy");
		query.appendChild(edestroy);
		iq.appendChild(query);
	}

	virtual void onGo()
	{
		send(iq);
	}

	virtual bool take(const QDomElement &x)
	{
		if(!iqVerify(x, to, id()))
			return false;

		if(x.attribute("type") == "result")
			setSuccess();
		else
			setError(x);

		return true;
	}

private:
	QDomElement iq;
	XMPP::Jid to;
};

class GroupChatBrowseController::Private : public QObject
{
	Q_OBJECT

public:
	GroupChatBrowseController *q;
	PsiAccount *account;
	GroupChatBrowseWindow *bw;

	QList<GroupChatBrowseWindow::RoomInfo> rooms;
	XMPP::Jid roomBeingCreated;

	Private(GroupChatBrowseController *_q, PsiAccount *_account, QWidget *parent) :
		QObject(_q),
		q(_q),
		account(_account)
	{
		qRegisterMetaType<GroupChatBrowseWindow::RoomOptions>();

		bw = new PsiGroupChatBrowseWindow(parent);
		bw->setController(q);
		bw->setAttribute(Qt::WA_DeleteOnClose);
		//bw->setGroupChatIcon(IconsetFactory::icon("psi/start-chat").pixmap());
		connect(bw, SIGNAL(onBrowse(const XMPP::Jid &)), SLOT(bw_onBrowse(const XMPP::Jid &)));
		connect(bw, SIGNAL(onJoin(const XMPP::Jid &)), SLOT(bw_onJoin(const XMPP::Jid &)));
		connect(bw, SIGNAL(onCreate(const XMPP::Jid &)), SLOT(bw_onCreate(const XMPP::Jid &)));
		connect(bw, SIGNAL(onCreateConfirm(const GroupChatBrowseWindow::RoomOptions &)), SLOT(bw_onCreateConfirm(const GroupChatBrowseWindow::RoomOptions &)));
		connect(bw, SIGNAL(onCreateCancel(const XMPP::Jid &)), SLOT(bw_onCreateCancel(const XMPP::Jid &)));
		connect(bw, SIGNAL(onCreateFinalize(const XMPP::Jid &, bool)), SLOT(bw_onCreateFinalize(const XMPP::Jid &, bool)));
		connect(bw, SIGNAL(onDestroy(const XMPP::Jid &)), SLOT(bw_onDestroy(const XMPP::Jid &)));
		connect(bw, SIGNAL(onSetAutoJoin(const QList<XMPP::Jid> &, bool)), SLOT(bw_onSetAutoJoin(const QList<XMPP::Jid> &, bool)));
		connect(bw, SIGNAL(destroyed(QObject *)), SLOT(bw_destroyed()));

		account->dialogRegister(bw);

		bw->setServerVisible(false);
		bw->setNicknameVisible(false);
		bw->show();
		bw->setServer("conference");
	}

	~Private()
	{
		account->dialogUnregister(bw);
	}

	void disco_next_room()
	{
		int at = -1;
		for(int n = 0; n < rooms.count(); ++n)
		{
			if(rooms[n].participants == -2)
			{
				at = n;
				break;
			}
		}

		// no more undisco'd rooms?
		if(at == -1)
			return;

		rooms[at].participants = -1;

		XMPP::JT_DiscoInfo *jt_info = new XMPP::JT_DiscoInfo(account->client()->rootTask());
		connect(jt_info, SIGNAL(finished()), SLOT(jt_info_finished()));
		jt_info->get(rooms[at].jid);
		jt_info->go(true);
	}

	void joinSuccess()
	{
		//controller_->recentGCAdd(jid_.full());
		//ui_.busy->stop();

		//closeDialogs(this);
		//deleteLater();

		if(!roomBeingCreated.isEmpty())
			bw->handleCreateConfirmed();
		else
			bw->handleJoinSuccess();
	}

	void joinError(const QString &reason)
	{
		//ui_.busy->stop();
		//setWidgetsEnabled(true);

		//account_->dialogUnregister(this);
		//controller_->dialogRegister(this);

		if(!roomBeingCreated.isEmpty())
			bw->handleCreateError(reason);
		else
			bw->handleJoinError(reason);
	}

public slots:
	void bw_destroyed()
	{
		bw = 0;
		q->deleteLater();
	}

	void bw_onBrowse(const XMPP::Jid &roomServer)
	{
		XMPP::JT_DiscoItems *jt_browse = new XMPP::JT_DiscoItems(account->client()->rootTask());
		connect(jt_browse, SIGNAL(finished()), SLOT(jt_browse_finished()));
		jt_browse->get(roomServer);
		jt_browse->go(true);
	}

	void bw_onJoin(const XMPP::Jid &room)
	{
		roomBeingCreated = XMPP::Jid();

		//printf("attempt to join [%s]\n", qPrintable(room.full()));

		QString host = room.domain();
		QString node = room.node();
		QString nick = account->jid().node();
		QString pass; // TODO

		if(!account->groupChatJoin(host, node, nick, pass, false))
		{
			QMetaObject::invokeMethod(bw, "handleJoinError", Qt::QueuedConnection,
				Q_ARG(QString, tr("You are in or joining this groupchat already!")));
			return;
		}

		// FIXME: this used to be an unregister against psicon
		account->dialogUnregister(bw);
		XMPP::Jid jid = node + '@' + host + '/' + nick;
		account->dialogRegister(bw, jid);
	}

	void bw_onCreate(const XMPP::Jid &room)
	{
		roomBeingCreated = room;

		GroupChatBrowseWindow::RoomOptions defaultOpts;

		QMetaObject::invokeMethod(bw, "handleCreateSuccess", Qt::QueuedConnection,
			Q_ARG(GroupChatBrowseWindow::RoomOptions, defaultOpts));
	}

	void bw_onCreateConfirm(const GroupChatBrowseWindow::RoomOptions &options)
	{
		Q_UNUSED(options);

		XMPP::Jid room = roomBeingCreated;

		//printf("attempt to create [%s]\n", qPrintable(room.full()));

		QString host = room.domain();
		QString node = room.node();
		QString nick = account->jid().node();
		QString pass; // TODO

		if(!account->groupChatJoin(host, node, nick, pass, false))
		{
			QMetaObject::invokeMethod(bw, "handleCreateError", Qt::QueuedConnection,
				Q_ARG(QString, tr("You are in or creating/joining this groupchat already!")));
			return;
		}

		// FIXME: this used to be an unregister against psicon
		account->dialogUnregister(bw);
		XMPP::Jid jid = node + '@' + host + '/' + nick;
		account->dialogRegister(bw, jid);
	}

	void bw_onCreateCancel(const XMPP::Jid &room)
	{
		Q_UNUSED(room);
	}

	void bw_onCreateFinalize(const XMPP::Jid &room, bool join)
	{
		Q_UNUSED(room);
		Q_UNUSED(join);
	}

	void bw_onDestroy(const XMPP::Jid &room)
	{
		printf("destroying [%s]\n", qPrintable(room.full()));
		JT_DestroyRoom *jt_destroy = new JT_DestroyRoom(account->client()->rootTask());
		connect(jt_destroy, SIGNAL(finished()), SLOT(jt_destroy_finished()));
		jt_destroy->destroy(room);
		jt_destroy->go(true);
	}

	void bw_onSetAutoJoin(const QList<XMPP::Jid> &rooms, bool enabled)
	{
		foreach(const XMPP::Jid &room, rooms)
			account->setAutoJoined(room, enabled);
	}

	void jt_browse_finished()
	{
		XMPP::JT_DiscoItems *jt_browse = (XMPP::JT_DiscoItems *)sender();
		XMPP::DiscoList list = jt_browse->items();

		rooms.clear();
		foreach(const XMPP::DiscoItem &i, list)
		{
			GroupChatBrowseWindow::RoomInfo ri;
			ri.jid = i.jid();
			ri.roomName = i.name();
			ri.participants = -2; // means undisco'd
			// FIXME: make sure bookmarks have been fetched
			ri.autoJoin = account->isAutoJoined(ri.jid);
			rooms += ri;
		}

		bw->handleBrowseResultsReady(rooms);

		//disco_next_room();
	}

	void jt_info_finished()
	{
		XMPP::JT_DiscoInfo *jt_info = (XMPP::JT_DiscoInfo *)sender();

		// which room is this for?
		XMPP::Jid jid = jt_info->jid();
		int at = -1;
		for(int n = 0; n < rooms.count(); ++n)
		{
			if(rooms[n].jid == jid)
			{
				at = n;
				break;
			}
		}
		if(at == -1)
			return;

		XMPP::DiscoItem item = jt_info->item();
		rooms[at].roomName = item.name();
		rooms[at].participants = 0; // FIXME: get actual value

		disco_next_room();
	}

	void jt_destroy_finished()
	{
		JT_DestroyRoom *jt_destroy = (JT_DestroyRoom *)sender();

		if(jt_destroy->success())
			bw->handleDestroySuccess();
		else
			bw->handleDestroyError(jt_destroy->statusString());
	}
};

GroupChatBrowseController::GroupChatBrowseController(PsiAccount *pa, QWidget *parentWindow) :
	QObject(pa)
{
	d = new Private(this, pa, parentWindow);
}

GroupChatBrowseController::~GroupChatBrowseController()
{
	if(d->bw)
		d->account->dialogUnregister(d->bw);
	delete d;
}

void GroupChatBrowseController::joined()
{
	d->joinSuccess();
}

void GroupChatBrowseController::error(int, const QString &str)
{
	d->joinError(str);
}

#include "groupchatbrowsecontroller.moc"

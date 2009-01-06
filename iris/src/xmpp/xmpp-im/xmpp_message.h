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

#ifndef XMPP_MESSAGE_H
#define XMPP_MESSAGE_H

#include "xmpp_stanza.h"
#include "xmpp_url.h"
#include "xmpp_chatstate.h"
#include "xmpp_receipts.h"
#include "xmpp_address.h"
#include "xmpp_rosterx.h"
#include "xmpp_muc.h"

class QString;
class QDateTime;

namespace XMPP {
	class Jid;
	class PubSubItem;
	class PubSubRetraction;
	class HTMLElement;
	class HttpAuthRequest;
	class XData;

	typedef enum { OfflineEvent, DeliveredEvent, DisplayedEvent,
			ComposingEvent, CancelEvent } MsgEvent;

	class Message
	{
	public:
		Message(const Jid &to="");
		Message(const Message &from);
		Message & operator=(const Message &from);
		~Message();

		Jid to() const;
		Jid from() const;
		QString id() const;
		QString type() const;
		QString lang() const;
		QString subject(const QString &lang="") const;
		QString body(const QString &lang="") const;
		QString thread() const;
		Stanza::Error error() const;

		void setTo(const Jid &j);
		void setFrom(const Jid &j);
		void setId(const QString &s);
		void setType(const QString &s);
		void setLang(const QString &s);
		void setSubject(const QString &s, const QString &lang="");
		void setBody(const QString &s, const QString &lang="");
		void setThread(const QString &s, bool send = false);
		void setError(const Stanza::Error &err);

		// JEP-0060
		const QString& pubsubNode() const;
		const QList<PubSubItem>& pubsubItems() const;
		const QList<PubSubRetraction>& pubsubRetractions() const;

		// JEP-0091
		QDateTime timeStamp() const;
		void setTimeStamp(const QDateTime &ts, bool send = false);

		// JEP-0071
		HTMLElement html(const QString &lang="") const;
		void setHTML(const HTMLElement &s, const QString &lang="");
		bool containsHTML() const;

		// JEP-0066
		UrlList urlList() const;
		void urlAdd(const Url &u);
		void urlsClear();
		void setUrlList(const UrlList &list);

		// JEP-0022
		QString eventId() const;
		void setEventId(const QString& id);
		bool containsEvents() const;
		bool containsEvent(MsgEvent e) const;
		void addEvent(MsgEvent e);

		// JEP-0085
		ChatState chatState() const;
		void setChatState(ChatState);
 
		// XEP-0184
		MessageReceipt messageReceipt() const;
		void setMessageReceipt(MessageReceipt);

		// JEP-0027
		QString xencrypted() const;
		void setXEncrypted(const QString &s);
		
		// JEP-0033
		AddressList addresses() const;
		AddressList findAddresses(Address::Type t) const;
		void addAddress(const Address &a);
		void clearAddresses();
		void setAddresses(const AddressList &list);

		// JEP-144
		const RosterExchangeItems& rosterExchangeItems() const;
		void setRosterExchangeItems(const RosterExchangeItems&);

		// JEP-172
		void setNick(const QString&);
		const QString& nick() const;

		// JEP-0070
		void setHttpAuthRequest(const HttpAuthRequest&);
		HttpAuthRequest httpAuthRequest() const;

		// JEP-0004
		void setForm(const XData&);
		const XData& getForm() const;

		// JEP-xxxx SXE
		void setSxe(const QDomElement&);
		const QDomElement& sxe() const;

		// MUC
		void addMUCStatus(int);
		const QList<int>& getMUCStatuses() const;
		void addMUCInvite(const MUCInvite&);
		const QList<MUCInvite>& mucInvites() const;
		void setMUCDecline(const MUCDecline&);
		const MUCDecline& mucDecline() const;
		const QString& mucPassword() const;
		void setMUCPassword(const QString&);

		// Obsolete invitation
		QString invite() const;
		void setInvite(const QString &s);

		// for compatibility.  delete me later
		bool spooled() const;
		void setSpooled(bool);
		bool wasEncrypted() const;
		void setWasEncrypted(bool);

		Stanza toStanza(Stream *stream) const;
		bool fromStanza(const Stanza &s, int tzoffset);

	private:
		class Private;
		Private *d;
	};
}

#endif

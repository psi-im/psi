/*
 * httpauthmanager.cpp - Classes managing incoming HTTP-Auth requests
 * Copyright (C) 2006  Maciej Niedzielski
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

#include "httpauthmanager.h"
#include "psihttpauthrequest.h"
#include "xmpp_xmlcommon.h"
#include "xmpp_task.h"
#include "xmpp_client.h"
#include "xmpp_message.h"
#include "xmpp_stream.h"

class HttpAuthListener : public XMPP::Task
{
	Q_OBJECT

public:
	HttpAuthListener(XMPP::Task *);
	bool take(const QDomElement &);
	void reply(bool confirm, const PsiHttpAuthRequest &);

signals:
	void request(const PsiHttpAuthRequest&);
};

HttpAuthListener::HttpAuthListener(XMPP::Task *parent) : XMPP::Task(parent)
{
}

bool HttpAuthListener::take(const QDomElement &e)
{
	QDomElement c = e.elementsByTagName("confirm").item(0).toElement();
		// yes, of course, we need to check namespace, too
		// but it is impossible before calling addCorrectNS(),
		// which may be time/resources consuming
		// while this protocol is not so commonly used,
		// so it probably makes sense to do a "first guess" check like this

	if (c.isNull())
		return false;

	XMPP::Stanza s = client()->stream().createStanza(addCorrectNS(e));
	if(s.isNull())
		return false;

	if (s.kind() == XMPP::Stanza::IQ && s.type() != "get")
		return false;

	// now really checking (with namespace)
	c = s.element().elementsByTagNameNS("http://jabber.org/protocol/http-auth", "confirm").item(0).toElement();
	if (c.isNull())
		return false;

	emit request(PsiHttpAuthRequest(XMPP::HttpAuthRequest(c), s));

	return true;
}

void HttpAuthListener::reply(bool confirm, const PsiHttpAuthRequest &req)
{
	const XMPP::Stanza &s = req.stanza();
	if (s.kind() == XMPP::Stanza::Message) {
		XMPP::Message m(s.from());
		if (!confirm) {
			m.setType("error");
			m.setError(XMPP::HttpAuthRequest::denyError);
		}
		m.setHttpAuthRequest(req);

		QDomElement t = s.element().elementsByTagNameNS(s.baseNS(), "thread").item(0).toElement();
		if (!t.isNull()) {
			m.setThread(t.text(), true);
		}	//FIX-ME: it doesn't make sense to create Message object just to read the thread, does it?

		client()->sendMessage(m);
 	}
	else {
		QDomElement e;
		e = createIQ(client()->doc(), confirm ? "result" : "error", s.from().full(), s.id());
		e.appendChild(req.toXml(*client()->doc()));
		if (!confirm) {
			e.appendChild(XMPP::HttpAuthRequest::denyError.toXml(*client()->doc(), client()->stream().baseNS()));
		}
		send(e);
	}
}

/*!
	\class HttpAuthManager
	\brief Manages incoming JEP-0070 requests.

	HttpAuthManager instance sits on your client's root task
	and intercepts all incoming JEP-0070 confirmation requests.

	When new requests arrives, 'incoming' signal is raised.

	Important: Because this manager needs to intercept
	some message stanzas, it needs to be registered before
	standard message handler (JT_PushMessage),
	which is registered by Client::start().
*/

/*!
	Creates new manager attached to \a root task.
*/
HttpAuthManager::HttpAuthManager(XMPP::Task *root)
{
	listener_ = new HttpAuthListener(root);
	connect(listener_,SIGNAL(request(const PsiHttpAuthRequest&)),SIGNAL(confirmationRequest(const PsiHttpAuthRequest&)));
}

/*!
	Destroys the manager.
*/
HttpAuthManager::~HttpAuthManager()
{
	// listener_ is deleted by root task, don't delete it here again!
}

/*!
	Confirm the \a request.
*/
void HttpAuthManager::confirm(const PsiHttpAuthRequest &request)
{
	listener_->reply(true, request);
}

/*!
	Deny the \a request.
*/
void HttpAuthManager::deny(const PsiHttpAuthRequest &request)
{
	listener_->reply(false, request);
}

#include "httpauthmanager.moc"

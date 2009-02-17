#include "cudatasks.h"

#include "xmpp_xmlcommon.h"

namespace XMPP {

//----------------------------------------------------------------------------
// JT_CudaLogin
//----------------------------------------------------------------------------
JT_CudaLogin::JT_CudaLogin(Task *parent)
:Task(parent)
{
}

void JT_CudaLogin::get(const Jid &jid, const QString &target)
{
	j = jid;
	_supportsTarget = false;
	iq = createIQ(doc(), "get", "loginbot", id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "http://barracudanetworks.com/protocol/cudalogin");
	if(!target.isEmpty())
		query.appendChild(textTag(doc(), "target", target));
	iq.appendChild(query);
}

void JT_CudaLogin::onGo()
{
	send(iq);
}

bool JT_CudaLogin::take(const QDomElement &x)
{
	if(!iqVerify(x, "loginbot", id()))
		return false;

	// TODO: confirm namespace
	if(x.attribute("type") == "result") {
		for(QDomNode n = x.firstChild(); !n.isNull(); n = n.nextSibling()) {
			QDomElement i = n.toElement();
			if(i.isNull())
				continue;

			if(i.tagName() == "result") {
				v_url = i.attribute("url");

				// check for 'target' tag in reply
				for(QDomNode n = i.firstChild(); !n.isNull(); n = n.nextSibling()) {
					QDomElement i = n.toElement();
					if(i.isNull())
						continue;

					if(i.tagName() == "target") {
						_supportsTarget = true;
						break;
					}
				}
			}
		}

		setSuccess();
	}
	else {
		setError(x);
	}

	return true;
}

const Jid & JT_CudaLogin::jid() const
{
	return j;
}

const QString & JT_CudaLogin::url() const
{
	return v_url;
}

bool JT_CudaLogin::supportsTarget() const
{
	return _supportsTarget;
}

//----------------------------------------------------------------------------
// JT_GetDisclaimer
//----------------------------------------------------------------------------
JT_GetDisclaimer::JT_GetDisclaimer(Task *parent)
:Task(parent)
{
}

void JT_GetDisclaimer::get(const Jid &jid)
{
	j = jid;
	iq = createIQ(doc(), "query", j.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "http://barracudanetworks.com/protocol/disclaimer");
	iq.appendChild(query);
	//query.appendChild(textTag(doc(), "name", client()->clientName()));
}

void JT_GetDisclaimer::onGo()
{
	send(iq);
}

bool JT_GetDisclaimer::take(const QDomElement &x)
{
	Jid jid_clientupgrader = Jid( "disclaimer" );
	if(!iqVerify(x, jid_clientupgrader, ""))
		return false;

	if(x.attribute("type") == "result") {
		for(QDomNode n = x.firstChild(); !n.isNull(); n = n.nextSibling()) {
			QDomElement i = n.toElement();
			if(i.isNull())
				continue;

			if(i.tagName() == "message") {
				v_msg = tagContent(i);
			}
		}
		setSuccess();
	}
	else {
		//setError(x);
		return false;
	}

	return true;
}

const Jid & JT_GetDisclaimer::jid() const
{
	return j;
}

const QString & JT_GetDisclaimer::message() const
{
	return v_msg;
}

//----------------------------------------------------------------------------
// JT_GetClientVersion
//----------------------------------------------------------------------------
JT_GetClientVersion::JT_GetClientVersion(Task *parent)
:Task(parent)
{
}

void JT_GetClientVersion::get(const Jid &jid)
{
	j = jid;
	iq = createIQ(doc(), "result", j.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:version");
	iq.appendChild(query);
	query.appendChild(textTag(doc(), "name", client()->clientName()));
	query.appendChild(textTag(doc(), "version", client()->clientVersion()));
	query.appendChild(textTag(doc(), "os", client()->OSName()));
}

void JT_GetClientVersion::onGo()
{
	send(iq);
}

bool JT_GetClientVersion::take(const QDomElement &x)
{
	Jid jid_clientupgrader = Jid( "clientupgrader" );
	if(!iqVerify(x, jid_clientupgrader, ""))
		return false;

	if(x.attribute("type") == "result") {
		for(QDomNode n = x.firstChild(); !n.isNull(); n = n.nextSibling()) {
			QDomElement i = n.toElement();
			if(i.isNull())
				continue;

			if(i.tagName() == "version") {
				v_ver = tagContent(i);
			}
			if(i.tagName() == "updater") {
				v_updater = tagContent(i);
			}
			if(i.tagName() == "port") {
				v_port = tagContent(i);
			}
			if(i.tagName() == "ssl") {
				v_ssl = tagContent(i);
			}
			if(i.tagName() == "message") {
				v_message = tagContent(i);
			}
			if(i.tagName() == "message_title") {
				v_message_title = tagContent(i);
			}
		}

		setSuccess();
	}
	else {
		//setError(x);
		return false;
	}

	return true;
}

const Jid & JT_GetClientVersion::jid() const
{
	return j;
}

const QString & JT_GetClientVersion::name() const
{
	return v_name;
}

const QString & JT_GetClientVersion::version() const
{
	return v_ver;
}

const QString & JT_GetClientVersion::updater() const
{
	return v_updater;
}

const QString & JT_GetClientVersion::ssl() const
{
	return v_ssl;
}

const QString & JT_GetClientVersion::message() const
{
	return v_message;
}

const QString & JT_GetClientVersion::message_title() const
{
	return v_message_title;
}

const QString & JT_GetClientVersion::port() const
{
	return v_port;
}

const QString & JT_GetClientVersion::os() const
{
	return v_os;
}

//----------------------------------------------------------------------------
// JT_PushGetClientVersion
//----------------------------------------------------------------------------
JT_PushGetClientVersion::JT_PushGetClientVersion(Task *parent)
:Task(parent)
{
}

bool JT_PushGetClientVersion::take(const QDomElement &e)
{
	if(e.attribute("type") == "set") {
		QDomElement q = queryTag(e);
		if(!q.isNull() && q.attribute("xmlns") == "http://barracuda.com/xmppextensions/upgrade") {
			QString ver;
			QList<Url> urls;
			for(QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement i = n.toElement();
				if(i.isNull())
					continue;

				if(i.tagName() == "version") {
					ver = tagContent(i);
				}
				else if(i.tagName() == "url") {
					Url url;
					url.type = i.attribute("type");
					url.url = tagContent(i);
					urls += url;
				}
			}

			emit newVersion(ver, urls);

			send(createIQ(doc(), "result", e.attribute("from"), e.attribute("id")));
			return true;
		}
	}

	return false;
}

}

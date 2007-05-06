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

#include "xmpp_jid.h"
#include "xmpp_stanza.h"
#include "xmpp_stream.h"

using namespace XMPP;

#define NS_STANZAS  "urn:ietf:params:xml:ns:xmpp-stanzas"
#define NS_XML "http://www.w3.org/XML/1998/namespace"

//----------------------------------------------------------------------------
// Stanza::Error
//----------------------------------------------------------------------------

/**
	\class Stanza::Error
	\brief Represents stanza error

	Stanza error consists of error type and condition.
	In addition, it may contain a human readable description,
	and application specific element.

	One of the usages of this class is to easily generate error XML:

	\code
	QDomElement e = createIQ(client()->doc(), "error", jid, id);
	Error error(Stanza::Error::Auth, Stanza::Error::NotAuthorized);
	e.appendChild(error.toXml(*client()->doc(), client()->stream().baseNS()));
	\endcode

	This class implements JEP-0086, which means that it can read both
	old and new style error elements. Also, generated XML will contain
	both type/condition and code.
	Error text in output XML is always presented in XMPP-style only.

	All functions will always try to guess missing information based on mappings defined in the JEP.
*/

/**

	\enum Stanza::Error::ErrorType
	\brief Represents error type
*/

/**
	\enum Stanza::Error::ErrorCond
	\brief Represents error condition
*/

/**
	\brief Constructs new error
*/
Stanza::Error::Error(int _type, int _condition, const QString &_text, const QDomElement &_appSpec)
{
	type = _type;
	condition = _condition;
	text = _text;
	appSpec = _appSpec;
}


class Stanza::Error::Private
{
public:
	struct ErrorTypeEntry
	{
		const char *str;
		int type;
	};
	static ErrorTypeEntry errorTypeTable[];

	struct ErrorCondEntry
	{
		const char *str;
		int cond;
	};
	static ErrorCondEntry errorCondTable[];

	struct ErrorCodeEntry
	{
		int cond;
		int type;
		int code;
	};
	static ErrorCodeEntry errorCodeTable[];

	struct ErrorDescEntry
	{
		int cond;
		char *name;
		char *str;
	};
	static ErrorDescEntry errorDescriptions[];


	static int stringToErrorType(const QString &s)
	{
		for(int n = 0; errorTypeTable[n].str; ++n) {
			if(s == errorTypeTable[n].str)
				return errorTypeTable[n].type;
		}
		return -1;
	}

	static QString errorTypeToString(int x)
	{
		for(int n = 0; errorTypeTable[n].str; ++n) {
			if(x == errorTypeTable[n].type)
				return errorTypeTable[n].str;
		}
		return QString();
	}

	static int stringToErrorCond(const QString &s)
	{
		for(int n = 0; errorCondTable[n].str; ++n) {
			if(s == errorCondTable[n].str)
				return errorCondTable[n].cond;
		}
		return -1;
	}

	static QString errorCondToString(int x)
	{
		for(int n = 0; errorCondTable[n].str; ++n) {
			if(x == errorCondTable[n].cond)
				return errorCondTable[n].str;
		}
		return QString();
	}

	static int errorTypeCondToCode(int t, int c)
	{
		Q_UNUSED(t);
		for(int n = 0; errorCodeTable[n].cond; ++n) {
			if(c == errorCodeTable[n].cond)
				return errorCodeTable[n].code;
		}
		return 0;
	}

	static QPair<int, int> errorCodeToTypeCond(int x)
	{
		for(int n = 0; errorCodeTable[n].cond; ++n) {
			if(x == errorCodeTable[n].code)
				return QPair<int, int>(errorCodeTable[n].type, errorCodeTable[n].cond);
		}
		return QPair<int, int>(-1, -1);
	}

	static QPair<QString,QString> errorCondToDesc(int x)
	{
		for(int n = 0; errorDescriptions[n].str; ++n) {
			if(x == errorDescriptions[n].cond)
				return QPair<QString,QString>(QObject::tr(errorDescriptions[n].name), QObject::tr(errorDescriptions[n].str));
		}
		return QPair<QString,QString>();
	}
};

Stanza::Error::Private::ErrorTypeEntry Stanza::Error::Private::errorTypeTable[] =
{
	{ "cancel",   Cancel },
	{ "continue", Continue },
	{ "modify",   Modify },
	{ "auth",     Auth },
	{ "wait",     Wait },
	{ 0, 0 },
};

Stanza::Error::Private::ErrorCondEntry Stanza::Error::Private::errorCondTable[] =
{
	{ "bad-request",             BadRequest },
	{ "conflict",                Conflict },
	{ "feature-not-implemented", FeatureNotImplemented },
	{ "forbidden",               Forbidden },
	{ "gone",                    Gone },
	{ "internal-server-error",   InternalServerError },
	{ "item-not-found",          ItemNotFound },
	{ "jid-malformed",           JidMalformed },
	{ "not-acceptable",          NotAcceptable },
	{ "not-allowed",             NotAllowed },
	{ "not-authorized",          NotAuthorized },
	{ "payment-required",        PaymentRequired },
	{ "recipient-unavailable",   RecipientUnavailable },
	{ "redirect",                Redirect },
	{ "registration-required",   RegistrationRequired },
	{ "remote-server-not-found", RemoteServerNotFound },
	{ "remote-server-timeout",   RemoteServerTimeout },
	{ "resource-constraint",     ResourceConstraint },
	{ "service-unavailable",     ServiceUnavailable },
	{ "subscription-required",   SubscriptionRequired },
	{ "undefined-condition",     UndefinedCondition },
	{ "unexpected-request",      UnexpectedRequest },
	{ 0, 0 },
};

Stanza::Error::Private::ErrorCodeEntry Stanza::Error::Private::errorCodeTable[] =
{
	{ BadRequest,            Modify, 400 },
	{ Conflict,              Cancel, 409 },
	{ FeatureNotImplemented, Cancel, 501 },
	{ Forbidden,             Auth,   403 },
	{ Gone,                  Modify, 302 },	// permanent
	{ InternalServerError,   Wait,   500 },
	{ ItemNotFound,          Cancel, 404 },
	{ JidMalformed,          Modify, 400 },
	{ NotAcceptable,         Modify, 406 },
	{ NotAllowed,            Cancel, 405 },
	{ NotAuthorized,         Auth,   401 },
	{ PaymentRequired,       Auth,   402 },
	{ RecipientUnavailable,  Wait,   404 },
	{ Redirect,              Modify, 302 },	// temporary
	{ RegistrationRequired,  Auth,   407 },
	{ RemoteServerNotFound,  Cancel, 404 },
	{ RemoteServerTimeout,   Wait,   504 },
	{ ResourceConstraint,    Wait,   500 },
	{ ServiceUnavailable,    Cancel, 503 },
	{ SubscriptionRequired,  Auth,   407 },
	{ UndefinedCondition,    Wait,   500 },	// Note: any type matches really
	{ UnexpectedRequest,     Wait,   400 },
	{ 0, 0, 0 },
};

Stanza::Error::Private::ErrorDescEntry Stanza::Error::Private::errorDescriptions[] =
{
	{ BadRequest,            QT_TR_NOOP("Bad request"),             QT_TR_NOOP("The sender has sent XML that is malformed or that cannot be processed.") },
	{ Conflict,              QT_TR_NOOP("Conflict"),                QT_TR_NOOP("Access cannot be granted because an existing resource or session exists with the same name or address.") },
	{ FeatureNotImplemented, QT_TR_NOOP("Feature not implemented"), QT_TR_NOOP("The feature requested is not implemented by the recipient or server and therefore cannot be processed.") },
	{ Forbidden,             QT_TR_NOOP("Forbidden"),               QT_TR_NOOP("The requesting entity does not possess the required permissions to perform the action.") },
	{ Gone,                  QT_TR_NOOP("Gone"),                    QT_TR_NOOP("The recipient or server can no longer be contacted at this address.") },
	{ InternalServerError,   QT_TR_NOOP("Internal server error"),   QT_TR_NOOP("The server could not process the stanza because of a misconfiguration or an otherwise-undefined internal server error.") },
	{ ItemNotFound,          QT_TR_NOOP("Item not found"),          QT_TR_NOOP("The addressed JID or item requested cannot be found.") },
	{ JidMalformed,          QT_TR_NOOP("JID malformed"),           QT_TR_NOOP("The sending entity has provided or communicated an XMPP address (e.g., a value of the 'to' attribute) or aspect thereof (e.g., a resource identifier) that does not adhere to the syntax defined in Addressing Scheme.") },
	{ NotAcceptable,         QT_TR_NOOP("Not acceptable"),          QT_TR_NOOP("The recipient or server understands the request but is refusing to process it because it does not meet criteria defined by the recipient or server (e.g., a local policy regarding acceptable words in messages).") },
	{ NotAllowed,            QT_TR_NOOP("Not allowed"),             QT_TR_NOOP("The recipient or server does not allow any entity to perform the action.") },
	{ NotAuthorized,         QT_TR_NOOP("Not authorized"),          QT_TR_NOOP("The sender must provide proper credentials before being allowed to perform the action, or has provided improper credentials.") },
	{ PaymentRequired,       QT_TR_NOOP("Payment required"),        QT_TR_NOOP("The requesting entity is not authorized to access the requested service because payment is required.") },
	{ RecipientUnavailable,  QT_TR_NOOP("Recipient unavailable"),   QT_TR_NOOP("The intended recipient is temporarily unavailable.") },
	{ Redirect,              QT_TR_NOOP("Redirect"),                QT_TR_NOOP("The recipient or server is redirecting requests for this information to another entity, usually temporarily.") },
	{ RegistrationRequired,  QT_TR_NOOP("Registration required"),   QT_TR_NOOP("The requesting entity is not authorized to access the requested service because registration is required.") },
	{ RemoteServerNotFound,  QT_TR_NOOP("Remote server not found"), QT_TR_NOOP("A remote server or service specified as part or all of the JID of the intended recipient does not exist.") },
	{ RemoteServerTimeout,   QT_TR_NOOP("Remote server timeout"),   QT_TR_NOOP("A remote server or service specified as part or all of the JID of the intended recipient (or required to fulfill a request) could not be contacted within a reasonable amount of time.") },
	{ ResourceConstraint,    QT_TR_NOOP("Resource constraint"),     QT_TR_NOOP("The server or recipient lacks the system resources necessary to service the request.") },
	{ ServiceUnavailable,    QT_TR_NOOP("Service unavailable"),     QT_TR_NOOP("The server or recipient does not currently provide the requested service.") },
	{ SubscriptionRequired,  QT_TR_NOOP("Subscription required"),   QT_TR_NOOP("The requesting entity is not authorized to access the requested service because a subscription is required.") },
	{ UndefinedCondition,    QT_TR_NOOP("Undefined condition"),     QT_TR_NOOP("The error condition is not one of those defined by the other conditions in this list.") },
	{ UnexpectedRequest,     QT_TR_NOOP("Unexpected request"),      QT_TR_NOOP("The recipient or server understood the request but was not expecting it at this time (e.g., the request was out of order).") },
};

/**
	\brief Returns the error code

	If the error object was constructed with a code, this code will be returned.
	Otherwise, the code will be guessed.

	0 means unknown code.
*/
int Stanza::Error::code() const
{
	return originalCode ? originalCode : Private::errorTypeCondToCode(type, condition);
}

/**
	\brief Creates a StanzaError from \a code.

	The error's type and condition are guessed from the give \a code.
	The application-specific error element is preserved.
*/
bool Stanza::Error::fromCode(int code)
{
	QPair<int, int> guess = Private::errorCodeToTypeCond(code);
	if(guess.first == -1 || guess.second == -1)
		return false;

	type = guess.first;
	condition = guess.second;
	originalCode = code;

	return true;
}

/**
	\brief Reads the error from XML

	This function finds and reads the error element \a e.

	You need to provide the base namespace of the stream which this stanza belongs to
	(probably by using stream.baseNS() function).
*/
bool Stanza::Error::fromXml(const QDomElement &e, const QString &baseNS)
{
	if(e.tagName() != "error" && e.namespaceURI() != baseNS)
		return false;

	// type
	type = Private::stringToErrorType(e.attribute("type"));

	// condition
	QDomNodeList nl = e.childNodes();
	QDomElement t;
	condition = -1;
	int n;
	for(n = 0; n < nl.count(); ++n) {
		QDomNode i = nl.item(n);
		t = i.toElement();
		if(!t.isNull()) {
			// FIX-ME: this shouldn't be needed
			if(t.namespaceURI() == NS_STANZAS || t.attribute("xmlns") == NS_STANZAS) {
				condition = Private::stringToErrorCond(t.tagName());
				if (condition != -1)
					break;
			}
		}
	}

	// code
	originalCode = e.attribute("code").toInt();

	// try to guess type/condition
	if(type == -1 || condition == -1) {
		QPair<int, int> guess(-1, -1);
		if (originalCode)
			guess = Private::errorCodeToTypeCond(originalCode);

		if (type == -1)
			type = guess.first != -1 ? guess.first : Cancel;
		if (condition == -1)
			condition = guess.second != -1 ? guess.second : UndefinedCondition;
	}

	// text
	t = e.elementsByTagNameNS(NS_STANZAS, "text").item(0).toElement();
	if(!t.isNull())
		text = t.text().trimmed();
	else
		text = e.text().trimmed();

	// appspec: find first non-standard namespaced element
	appSpec = QDomElement();
	nl = e.childNodes();
	for(n = 0; n < nl.count(); ++n) {
		QDomNode i = nl.item(n);
		if(i.isElement() && i.namespaceURI() != NS_STANZAS) {
			appSpec = i.toElement();
			break;
		}
	}

	return true;
}

/**
	\brief Writes the error to XML

	This function creates an error element representing the error object.

	You need to provide the base namespace of the stream to which this stanza belongs to
	(probably by using stream.baseNS() function).
*/
QDomElement Stanza::Error::toXml(QDomDocument &doc, const QString &baseNS) const
{
	QDomElement errElem = doc.createElementNS(baseNS, "error");
	QDomElement t;

	// XMPP error
	QString stype = Private::errorTypeToString(type);
	if(stype.isEmpty())
		return errElem;
	QString scond = Private::errorCondToString(condition);
	if(scond.isEmpty())
		return errElem;

	errElem.setAttribute("type", stype);
	errElem.appendChild(t = doc.createElementNS(NS_STANZAS, scond));
	t.setAttribute("xmlns", NS_STANZAS);	// FIX-ME: this shouldn't be needed

	// old code
	int scode = code();
	if(scode)
		errElem.setAttribute("code", scode);

	// text
	if(!text.isEmpty()) {
		t = doc.createElementNS(NS_STANZAS, "text");
		t.setAttribute("xmlns", NS_STANZAS);	// FIX-ME: this shouldn't be needed
		t.appendChild(doc.createTextNode(text));
		errElem.appendChild(t);
	}

	// application specific
	errElem.appendChild(appSpec);

	return errElem;
}

/**
	\brief Returns the error name and description

	Returns the error name (e.g. "Not Allowed") and generic description.
*/
QPair<QString,QString> Stanza::Error::description() const
{
	return Private::errorCondToDesc(condition);
}

//----------------------------------------------------------------------------
// Stanza
//----------------------------------------------------------------------------
class Stanza::Private
{
public:
	static int stringToKind(const QString &s)
	{
		if(s == "message")
			return Message;
		else if(s == "presence")
			return Presence;
		else if(s == "iq")
			return IQ;
		else
			return -1;
	}

	static QString kindToString(Kind k)
	{
		if(k == Message)
			return "message";
		else if(k == Presence)
			return "presence";
		else
			return "iq";
	}

	Stream *s;
	QDomElement e;
};

Stanza::Stanza()
{
	d = 0;
}

Stanza::Stanza(Stream *s, Kind k, const Jid &to, const QString &type, const QString &id)
{
	d = new Private;

	Kind kind;
	if(k == Message || k == Presence || k == IQ)
		kind = k;
	else
		kind = Message;

	d->s = s;
	d->e = d->s->doc().createElementNS(s->baseNS(), Private::kindToString(kind));
	if(to.isValid())
		setTo(to);
	if(!type.isEmpty())
		setType(type);
	if(!id.isEmpty())
		setId(id);
}

Stanza::Stanza(Stream *s, const QDomElement &e)
{
	d = 0;
	if(e.namespaceURI() != s->baseNS())
		return;
	int x = Private::stringToKind(e.tagName());
	if(x == -1)
		return;
	d = new Private;
	d->s = s;
	d->e = e;
}

Stanza::Stanza(const Stanza &from)
{
	d = 0;
	*this = from;
}

Stanza & Stanza::operator=(const Stanza &from)
{
	delete d;
	d = 0;
	if(from.d)
		d = new Private(*from.d);
	return *this;
}

Stanza::~Stanza()
{
	delete d;
}

bool Stanza::isNull() const
{
	return (d ? false: true);
}

QDomElement Stanza::element() const
{
	return d->e;
}

QString Stanza::toString() const
{
	return Stream::xmlToString(d->e);
}

QDomDocument & Stanza::doc() const
{
	return d->s->doc();
}

QString Stanza::baseNS() const
{
	return d->s->baseNS();
}

QDomElement Stanza::createElement(const QString &ns, const QString &tagName)
{
	return d->s->doc().createElementNS(ns, tagName);
}

QDomElement Stanza::createTextElement(const QString &ns, const QString &tagName, const QString &text)
{
	QDomElement e = d->s->doc().createElementNS(ns, tagName);
	e.appendChild(d->s->doc().createTextNode(text));
	return e;
}

void Stanza::appendChild(const QDomElement &e)
{
	d->e.appendChild(e);
}

Stanza::Kind Stanza::kind() const
{
	return (Kind)Private::stringToKind(d->e.tagName());
}

void Stanza::setKind(Kind k)
{
	d->e.setTagName(Private::kindToString(k));
}

Jid Stanza::to() const
{
	return Jid(d->e.attribute("to"));
}

Jid Stanza::from() const
{
	return Jid(d->e.attribute("from"));
}

QString Stanza::id() const
{
	return d->e.attribute("id");
}

QString Stanza::type() const
{
	return d->e.attribute("type");
}

QString Stanza::lang() const
{
	return d->e.attributeNS(NS_XML, "lang", QString());
}

void Stanza::setTo(const Jid &j)
{
	d->e.setAttribute("to", j.full());
}

void Stanza::setFrom(const Jid &j)
{
	d->e.setAttribute("from", j.full());
}

void Stanza::setId(const QString &id)
{
	d->e.setAttribute("id", id);
}

void Stanza::setType(const QString &type)
{
	d->e.setAttribute("type", type);
}

void Stanza::setLang(const QString &lang)
{
	d->e.setAttribute("xml:lang", lang);
}

Stanza::Error Stanza::error() const
{
	Error err;
	QDomElement e = d->e.elementsByTagNameNS(d->s->baseNS(), "error").item(0).toElement();
	if(!e.isNull())
		err.fromXml(e, d->s->baseNS());

	return err;
}

void Stanza::setError(const Error &err)
{
	QDomDocument doc = d->e.ownerDocument();
	QDomElement errElem = err.toXml(doc, d->s->baseNS());

	QDomElement oldElem = d->e.elementsByTagNameNS(d->s->baseNS(), "error").item(0).toElement();
	if(oldElem.isNull()) {
		d->e.appendChild(errElem);
	}
	else {
		d->e.replaceChild(errElem, oldElem);
	}
}

void Stanza::clearError()
{
	QDomElement errElem = d->e.elementsByTagNameNS(d->s->baseNS(), "error").item(0).toElement();
	if(!errElem.isNull())
		d->e.removeChild(errElem);
}


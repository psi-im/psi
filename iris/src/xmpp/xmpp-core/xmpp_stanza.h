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

#ifndef XMPP_STANZA_H
#define XMPP_STANZA_H

#include <QPair>
#include <QString>
#include <QDomElement>

class QDomDocument;

namespace XMPP 
{
	class Jid;
	class Stream;

	class Stanza
	{
	public:
		enum Kind { Message, Presence, IQ };

		Stanza();
		Stanza(const Stanza &from);
		Stanza & operator=(const Stanza &from);
		virtual ~Stanza();

		class Error
		{
		public:
			enum ErrorType { Cancel = 1, Continue, Modify, Auth, Wait };
			enum ErrorCond
			{
				BadRequest = 1,
				Conflict,
				FeatureNotImplemented,
				Forbidden,
				Gone,
				InternalServerError,
				ItemNotFound,
				JidMalformed,
				NotAcceptable,
				NotAllowed,
				NotAuthorized,
				PaymentRequired,
				RecipientUnavailable,
				Redirect,
				RegistrationRequired,
				RemoteServerNotFound,
				RemoteServerTimeout,
				ResourceConstraint,
				ServiceUnavailable,
				SubscriptionRequired,
				UndefinedCondition,
				UnexpectedRequest
			};

			Error(int type=Cancel, int condition=UndefinedCondition, const QString &text="", const QDomElement &appSpec=QDomElement());

			int type;
			int condition;
			QString text;
			QDomElement appSpec;

			int code() const;
			bool fromCode(int code);

			QPair<QString, QString> description() const;

			QDomElement toXml(QDomDocument &doc, const QString &baseNS) const;
			bool fromXml(const QDomElement &e, const QString &baseNS);
		private:
			class Private;
			int originalCode;

		};

		bool isNull() const;

		QDomElement element() const;
		QString toString() const;

		QDomDocument & doc() const;
		QString baseNS() const;
		QDomElement createElement(const QString &ns, const QString &tagName);
		QDomElement createTextElement(const QString &ns, const QString &tagName, const QString &text);
		void appendChild(const QDomElement &e);

		Kind kind() const;
		void setKind(Kind k);

		Jid to() const;
		Jid from() const;
		QString id() const;
		QString type() const;
		QString lang() const;

		void setTo(const Jid &j);
		void setFrom(const Jid &j);
		void setId(const QString &id);
		void setType(const QString &type);
		void setLang(const QString &lang);

		Error error() const;
		void setError(const Error &err);
		void clearError();

	private:
		friend class Stream;
		Stanza(Stream *s, Kind k, const Jid &to, const QString &type, const QString &id);
		Stanza(Stream *s, const QDomElement &e);

		class Private;
		Private *d;
	};
}

#endif

#ifndef XMPP_BASE64_H
#define XMPP_BASE64_H

#include <QByteArray>
#include <QString>

namespace XMPP {
	class Base64
	{
		public:
			static QString encode(const QByteArray&);
			static QByteArray decode(const QString &s);
	};
}

#endif

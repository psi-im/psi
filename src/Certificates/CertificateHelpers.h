/*
 * Copyright (C) 2008  Remko Troncon
 * Licensed under the GNU GPL license.
 * See COPYING for details.
 */

#ifndef CERTUTIL_H
#define CERTUTIL_H

#include <QtCrypto>

class QString;
namespace QCA {
	class CertificateCollection;
}
namespace XMPP {
	class QCATLSHandler;
}

class CertificateHelpers 
{
	public:
		static QCA::CertificateCollection allCertificates(const QStringList& dirs);
		static QString resultToString(int result, QCA::Validity);
		static bool checkCertificate(QCA::TLS* tls, XMPP::QCATLSHandler *tlsHandler, QString &tlsOverrideDomain, QByteArray &tlsOverrideCert, QObject * canceler, const QString &title, const QString &host);

	protected:
		static QString validityToString(QCA::Validity);
};

#endif

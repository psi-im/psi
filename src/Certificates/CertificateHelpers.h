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

class CertificateHelpers 
{
	public:
		static QCA::CertificateCollection allCertificates(const QStringList& dirs);
		static QString resultToString(int result, QCA::Validity);

	protected:
		static QString validityToString(QCA::Validity);
};

#endif

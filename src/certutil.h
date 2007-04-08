#ifndef CERTUTIL_H
#define CERTUTIL_H

#include <QtCrypto>
#include <QString>

class CertUtil 
{
public:
	static QCA::CertificateCollection allCertificates();
	static QString resultToString(int result, QCA::Validity);

protected:
	static QString validityToString(QCA::Validity);
};

#endif

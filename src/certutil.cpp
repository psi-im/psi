#include <QtCrypto>
#include <QStringList>
#include <QDomDocument>
#include <QDebug>

#include "applicationinfo.h"
#include "certutil.h"

using namespace QCA;

/**
 * \class CertUtil
 * \brief A class providing utility functions for Certificates.
 */

/**
 * \brief Returns the list of directories with certificates.
 */
static QStringList certificateStores()
{
	QStringList l;
	l += ApplicationInfo::resourcesDir() + "/certs";
	l += ApplicationInfo::homeDir() + "/certs";
	return l;
}

/**
 * \brief Returns the collection of all available certificates.
 * This collection includes the system-wide certificates, as well as any
 * custom certificate in the Psi-specific cert dirs.
 */
CertificateCollection CertUtil::allCertificates()
{
	CertificateCollection certs(systemStore());
	QStringList stores = certificateStores();
	for (QStringList::ConstIterator s = stores.begin(); s != stores.end(); ++s) {
		QDir store(*s);

		// Read in PEM certificates
		store.setNameFilters(QStringList("*.crt") + QStringList("*.pem"));
		QStringList cert_files = store.entryList();
		for (QStringList::ConstIterator c = cert_files.begin(); c != cert_files.end(); ++c) {
			//qDebug() << "certutil.cpp: Reading " << store.filePath(*c);
			ConvertResult result;
			Certificate cert = Certificate::fromPEMFile(store.filePath(*c),&result);
			if (result == ConvertGood) {
				certs.addCertificate(cert);
			}
			else {
				qWarning(QString("certutil.cpp: Invalid PEM certificate: %1").arg(store.filePath(*c)));
			}
		}

		// Read in old XML format certificates (DEPRECATED)
		store.setNameFilter("*.xml");
		cert_files = store.entryList();
		for(QStringList::ConstIterator it = cert_files.begin(); it != cert_files.end(); ++it) {
			qWarning(QString("Loading certificate in obsolete XML format: %1").arg(store.filePath(*it)));
			QFile f(store.filePath(*it));
			if(!f.open(QIODevice::ReadOnly))
				continue;
			QDomDocument doc;
			bool ok = doc.setContent(&f);
			f.close();
			if(!ok)
				continue;

			QDomElement base = doc.documentElement();
			if(base.tagName() != "store")
				continue;
			QDomNodeList cl = base.elementsByTagName("certificate");

			for(int n = 0; n < (int)cl.count(); ++n) {
				QDomElement data = cl.item(n).toElement().elementsByTagName("data").item(0).toElement();
				if(!data.isNull()) {
					ConvertResult result;
					Certificate cert = Certificate::fromDER(Base64().stringToArray(data.text()),&result);
					if (result == ConvertGood) {
						certs.addCertificate(cert);
					}
					else {
						qWarning(QString("certutil.cpp: Invalid XML certificate: %1").arg(store.filePath(*it)));
					}
				}
			}
		}
	}
	return certs;
}

QString CertUtil::validityToString(QCA::Validity v)
{
	QString s;
	switch(v)
	{
		case QCA::ValidityGood:
			s = "Validated";
			break;
		case QCA::ErrorRejected:
			s = "Root CA is marked to reject the specified purpose";
			break;
		case QCA::ErrorUntrusted:
			s = "Certificate not trusted for the required purpose";
			break;
		case QCA::ErrorSignatureFailed:
			s = "Invalid signature";
			break;
		case QCA::ErrorInvalidCA:
			s = "Invalid CA certificate";
			break;
		case QCA::ErrorInvalidPurpose:
			s = "Invalid certificate purpose";
			break;
		case QCA::ErrorSelfSigned:
			s = "Certificate is self-signed";
			break;
		case QCA::ErrorRevoked:
			s = "Certificate has been revoked";
			break;
		case QCA::ErrorPathLengthExceeded:
			s = "Maximum certificate chain length exceeded";
			break;
		case QCA::ErrorExpired:
			s = "Certificate has expired";
			break;
		case QCA::ErrorExpiredCA:
			s = "CA has expired";
			break;
		case QCA::ErrorValidityUnknown:
		default:
			s = "General certificate validation error";
			break;
	}
	return s;
}

QString CertUtil::resultToString(int result, QCA::Validity validity)
{
	QString s;
	switch(result) {
		case QCA::TLS::NoCertificate:
			s = QObject::tr("The server did not present a certificate.");
			break;
		case QCA::TLS::Valid:
			s = QObject::tr("Certificate is valid.");
			break;
		case QCA::TLS::HostMismatch:
			s = QObject::tr("The hostname does not match the one the certificate was issued to.");
			break;
		case QCA::TLS::InvalidCertificate:
			s = validityToString(validity);
			break;

		default:
			s = QObject::tr("General certificate validation error.");
			break;
	}
	return s;
}

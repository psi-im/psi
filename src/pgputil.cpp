#include <QStringList>

#include "pgputil.h"

bool PGPUtil::pgpAvailable()
{
	return (QCA::isSupported("openpgp") && keystores.count() > 0);
}

QString PGPUtil::stripHeaderFooter(const QString &str)
{
	QString s;
	if (str.isEmpty()) {
		qWarning("pgputil.cpp: Empty PGP message");
		return "";
	}
	if(str.at(0) != '-')
		return str;
	QStringList lines = QStringList::split('\n', str, true);
	QStringList::ConstIterator it = lines.begin();
	// skip the first line
	++it;
	if(it == lines.end())
		return str;

	// skip the header
	for(; it != lines.end(); ++it) {
		if((*it).isEmpty())
			break;
	}
	if(it == lines.end())
		return str;
	++it;
	if(it == lines.end())
		return str;

	bool first = true;
	for(; it != lines.end(); ++it) {
		if((*it).at(0) == '-')
			break;
		if(!first)
			s += '\n';
		s += (*it);
		first = false;
	}

	return s;
}


QString PGPUtil::addHeaderFooter(const QString &str, int type)
{
	QString stype;
	if(type == 0)
		stype = "MESSAGE";
	else
		stype = "SIGNATURE";

	QString s;
	s += QString("-----BEGIN PGP %1-----\n").arg(stype);
	s += "Version: PGP\n";
	s += "\n";
	s += str + '\n';
	s += QString("-----END PGP %1-----\n").arg(stype);
	return s;
}


QCA::KeyStoreEntry PGPUtil::getSecretKeyStoreEntry(const QString& keyID)
{
	foreach(QString k, QCA::keyStoreManager()->keyStores()) {
		QCA::KeyStore ks(k);
		if (ks.type() == QCA::KeyStore::PGPKeyring && ks.holdsIdentities()) {
			foreach(QCA::KeyStoreEntry ke, ks.entryList()) {
				if (ke.type() == QCA::KeyStoreEntry::TypePGPSecretKey && ke.pgpSecretKey().keyId() == keyID) {
					return ke;
				}
			}
		}
	}
	return QCA::KeyStoreEntry();
}

QCA::KeyStoreEntry PGPUtil::getPublicKeyStoreEntry(const QString& keyID)
{
	foreach(QString k, QCA::keyStoreManager()->keyStores()) {
		QCA::KeyStore ks(k);
		if (ks.type() == QCA::KeyStore::PGPKeyring && ks.holdsIdentities()) {
			foreach(QCA::KeyStoreEntry ke, ks.entryList()) {
				if (ke.type() == QCA::KeyStoreEntry::TypePGPPublicKey && ke.pgpPublicKey().keyId() == keyID) {
					return ke;
				}
			}
		}
	}
	return QCA::KeyStoreEntry();
}

QString PGPUtil::messageErrorString(enum QCA::SecureMessage::Error e)
{
	QString msg;
	switch(e) {
		case QCA::SecureMessage::ErrorPassphrase:
			msg = QObject::tr("Invalid passphrase");
			break;
		case QCA::SecureMessage::ErrorFormat:
			msg = QObject::tr("Invalid input format");
			break;
		case QCA::SecureMessage::ErrorSignerExpired:
			msg = QObject::tr("Signing key expired");
			break;
		case QCA::SecureMessage::ErrorSignerInvalid:
			msg = QObject::tr("Invalid key");
			break;
		case QCA::SecureMessage::ErrorEncryptExpired:
			msg = QObject::tr("Encrypting key expired");
			break;
		case QCA::SecureMessage::ErrorEncryptUntrusted:
			msg = QObject::tr("Encrypting key is untrusted");
			break;
		case QCA::SecureMessage::ErrorEncryptInvalid:
			msg = QObject::tr("Encrypting key is invalid");
			break;
		case QCA::SecureMessage::ErrorNeedCard:
			msg = QObject::tr("PGP card is missing");
			break;
		default:
			msg = QObject::tr("Unknown error");
	}
	return msg;
}

bool PGPUtil::equals(QCA::PGPKey k1, QCA::PGPKey k2)
{
	if (k1.isNull()) {
		return k2.isNull();
	}
	else if (k2.isNull()) {
		return false;
	}
	else {
		return k1.keyId() == k2.keyId();
	}
}

QList<QCA::KeyStore*> PGPUtil::keystores;

QMap<QString,QString> PGPUtil::passphrases;

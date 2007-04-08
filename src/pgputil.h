#ifndef PGPUTIL_H
#define PGPUTIL_H

#include <QString>
#include <QList>
#include <QMap>
#include <QtCrypto>

class PGPUtil
{
public:
	static bool pgpAvailable();
	
	static QCA::KeyStoreEntry getSecretKeyStoreEntry(const QString& key);
	static QCA::KeyStoreEntry getPublicKeyStoreEntry(const QString& key);
	
	static QString stripHeaderFooter(const QString &);
	static QString addHeaderFooter(const QString &, int);
			
	static QString messageErrorString(enum QCA::SecureMessage::Error);

	static bool equals(QCA::PGPKey, QCA::PGPKey);

	static QMap<QString,QString> passphrases;
	static QSet<QCA::KeyStore*> keystores;
};

#endif

#ifndef PGPUTIL_H
#define PGPUTIL_H

#include <QSet>
#include <QList>
#include <QMap>
#include <QtCrypto>

class QString;
class PassphraseDlg;
namespace QCA {
	class KeyStore;
	class PGPKey;
}

class PGPUtil : public QObject
{
	Q_OBJECT 

public:
	static PGPUtil& instance();

	bool pgpAvailable();

	void setEventHandler(QCA::EventHandler* e);

	void handleEvent(int id, const QCA::Event& event);
	
	QCA::KeyStoreEntry getSecretKeyStoreEntry(const QString& key);
	QCA::KeyStoreEntry getPublicKeyStoreEntry(const QString& key);
	
	QString stripHeaderFooter(const QString &);
	QString addHeaderFooter(const QString &, int);
			
	QString messageErrorString(enum QCA::SecureMessage::Error);

	bool equals(QCA::PGPKey, QCA::PGPKey);

	QSet<QCA::KeyStore*> keystores;

	void removePassphrase(const QString& id);

protected:
	PGPUtil();

	void promptPassphrase(int id, const QCA::Event& event);

protected slots:
	void passphraseDone(int);

private:
	static PGPUtil* instance_;

	struct EventItem {
		int id;
		QCA::Event event;
	};
	QList<EventItem> pendingEvents_;

	QMap<QString,QString> passphrases_;
	QCA::EventHandler* qcaEventHandler_;
	PassphraseDlg* passphraseDlg_;
	int currentEventId_;
	QString currentEntryId_;
};

#endif

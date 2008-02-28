#ifndef PGPUTIL_H
#define PGPUTIL_H

// FIXME: instead of a singleton, make it a member of PsiCon.


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
	void clearPGPAvailableCache();

	static void showDiagnosticText(const QString& event, const QString& diagnostic);
	static void showDiagnosticText(QCA::SecureMessage::Error error, const QString& diagnostic);

	QCA::KeyStoreEntry getSecretKeyStoreEntry(const QString& key);
	QCA::KeyStoreEntry getPublicKeyStoreEntry(const QString& key);

	QString stripHeaderFooter(const QString &);
	QString addHeaderFooter(const QString &, int);

	QString messageErrorString(enum QCA::SecureMessage::Error);

	bool equals(QCA::PGPKey, QCA::PGPKey);

	void removePassphrase(const QString& id);

signals:
	void pgpKeysUpdated();

protected:
	PGPUtil();
	~PGPUtil();

	void promptPassphrase(int id, const QCA::Event& event);

protected slots:
	void handleEvent(int id, const QCA::Event& event);
	void passphraseDone(int);
	void keyStoreAvailable(const QString&);

private:
	static PGPUtil* instance_;

	struct EventItem {
		int id;
		QCA::Event event;
	};
	QList<EventItem> pendingEvents_;

	QSet<QCA::KeyStore*> keystores_;
	QMap<QString,QString> passphrases_;
	QCA::EventHandler* qcaEventHandler_;
	QCA::KeyStoreManager qcaKeyStoreManager_;
	PassphraseDlg* passphraseDlg_;
	int currentEventId_;
	QString currentEntryId_;
	bool cache_no_pgp_;

	// FIXME
	friend class PGPKeyDlg;
};

#endif

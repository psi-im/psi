#ifndef PGPUTIL_H
#define PGPUTIL_H

// FIXME: instead of a singleton, make it a member of PsiCon.

#include "pgpkeydlg.h"
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QtCrypto>

namespace QCA {
class KeyStore;
class PGPKey;
}

class PGPUtil : public QObject {
    Q_OBJECT

public:
    static PGPUtil &instance();

    void reloadKeyStores();
    bool pgpAvailable();
    void clearPGPAvailableCache();

    static void showDiagnosticText(const QString &event, const QString &diagnostic);
    static void showDiagnosticText(QCA::SecureMessage::Error error, const QString &diagnostic);

    QCA::KeyStoreEntry getSecretKeyStoreEntry(const QString &key);
    QCA::KeyStoreEntry getPublicKeyStoreEntry(const QString &key);

    QString stripHeaderFooter(const QString &);
    QString addHeaderFooter(const QString &, int);

    QString messageErrorString(QCA::SecureMessage::Error);

    bool equals(QCA::PGPKey, QCA::PGPKey);

    static QString getKeyOwnerName(const QString &key);
    static QString getPublicKeyData(const QString &key);
    static QString getFingerprint(const QString &key);
    static QString chooseKey(PGPKeyDlg::Type type, const QString &key, const QString &title);

signals:
    void pgpKeysUpdated();

protected:
    PGPUtil();
    ~PGPUtil();

    void clearKeyStores();

protected slots:
    void keyStoreAvailable(const QString &);

private:
    static PGPUtil *instance_;

    struct EventItem {
        int        id;
        QCA::Event event;
    };
    QList<EventItem> pendingEvents_;

    QSet<QCA::KeyStore *>  keystores_;
    QCA::EventHandler *    qcaEventHandler_;
    QCA::KeyStoreManager * qcaKeyStoreManager_;
    int                    currentEventId_;
    QString                currentEntryId_;
    bool                   cache_no_pgp_;
};

#endif // PGPUTIL_H

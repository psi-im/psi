/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "applicationinfo.h"
#include "bobfilecache.h"
#include "filecache.h"
#include "filedatabase.h"
#include "fileutil.h"
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QtCrypto>
#include <iris/xmpp_bitsofbinary.h>
#include <iris/xmpp_client.h>
#include <memory>

// FileCache needs only this much of two classes that bring in the whole application.
static QString bobDirectory;
QString        ApplicationInfo::bobDir() { return bobDirectory; }
QString        ApplicationInfo::currentProfileDir(HomedirType) { return bobDirectory; }
QString        ApplicationInfo::version() { return QStringLiteral("test"); }
QString        ApplicationInfo::fileCacheNS() { return QStringLiteral("https://psi-im.org/test/filecache"); }
QString        FileUtil::mimeToFileExt(const QString &) { return {}; }

class BoBCacheTest : public QObject {
    Q_OBJECT
    QTemporaryDir                     dir;
    std::unique_ptr<QCA::Initializer> qca;

    static BoBData sample(const QByteArray &bytes)
    {
        BoBData data;
        data.setHash(Hash::from(Hash::Sha1, bytes));
        data.setData(bytes);
        data.setType(QStringLiteral("image/png"));
        data.setMaxAge(3600);
        return data;
    }

private slots:
    void initTestCase()
    {
        qca = std::make_unique<QCA::Initializer>();
        QVERIFY(dir.isValid());
        bobDirectory = dir.path();
    }
    void dataIsVisibleOnlyThroughTheAccountItCameThrough()
    {
        BoBFileCache corporate(QStringLiteral("corporate")), personal(QStringLiteral("personal"));
        const auto   data = sample("corporate thumbnail");
        corporate.put(data);
        QCOMPARE(corporate.get(data.hash()).data(), data.data());
        QCOMPARE(corporate.get(data.hash()).type(), data.type());
        QVERIFY(personal.get(data.hash()).isNull());
    }
    void bothAccountsSeeDataTheyBothReceived()
    {
        BoBFileCache corporate(QStringLiteral("corporate")), personal(QStringLiteral("personal"));
        const auto   data = sample("shared thumbnail");
        corporate.put(data);
        personal.put(data);
        QCOMPARE(corporate.get(data.hash()).data(), data.data());
        QCOMPARE(personal.get(data.hash()).data(), data.data());
    }
    void forgedDataIsRefused()
    {
        BoBFileCache corporate(QStringLiteral("corporate")), personal(QStringLiteral("personal"));
        const auto   data = sample("genuine thumbnail");
        corporate.put(data);
        auto forged = data;
        forged.setData("something else");
        personal.put(forged);
        QVERIFY2(personal.get(data.hash()).isNull(), "a forged cid must not grant access");
        QCOMPARE(corporate.get(data.hash()).data(), data.data());
    }
    void clientAnswersOnlyForItsOwnAccount()
    {
        // This is what JT_BoBServer answers a remote request with.
        XMPP::Client corporateClient, personalClient;
        BoBFileCache corporate(QStringLiteral("corporate")), personal(QStringLiteral("personal"));
        corporateClient.bobManager()->setCache(&corporate);
        personalClient.bobManager()->setCache(&personal);
        const QByteArray bytes("published thumbnail");
        const auto       published = corporateClient.bobManager()->append(bytes, QStringLiteral("image/png"), 3600);
        QCOMPARE(corporateClient.bobManager()->bobData(published.cid()).data(), bytes);
        QVERIFY(personalClient.bobManager()->bobData(published.cid()).isNull());
    }
    void accountsSurviveRestart()
    {
        BoBFileCache corporate(QStringLiteral("corporate"));
        const auto   data = sample("persistent thumbnail");
        corporate.put(data);
        // Written by the shared cache's sync timer.
        const auto registered = [&data] {
            QSqlQuery query(FileDatabase::connection());
            query.prepare(QStringLiteral("SELECT 1 FROM file_hashes WHERE digest=:digest"));
            query.bindValue(QStringLiteral(":digest"), data.hash().data());
            return query.exec() && query.next();
        };
        QTRY_VERIFY_WITH_TIMEOUT(registered(), 5000);
        FileCache reopened(QStringLiteral("bob"), dir.path());
        auto     *item = reopened.get(data.hash());
        QVERIFY(item);
        QCOMPARE(item->metadata().value(QStringLiteral("accounts")).toStringList(),
                 QStringList { QStringLiteral("corporate") });
    }
};
QTEST_GUILESS_MAIN(BoBCacheTest)
#include "bobcache.moc"

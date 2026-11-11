/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historysqlreadworker.h"

#include <QCoreApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimer>

namespace {
// Real schema and archive links, so failures can be injected into nested
// projection queries as well as the initial page query.
struct HistoryDatabase {
    QTemporaryDir        directory;
    QSqlDatabase         db;
    HistorySqlReadWorker worker { directory.filePath(QStringLiteral("history.db")) };
    HistoryArchive::Key  key { "account", "alice@example.test", "bob@example.test", false };

    HistoryDatabase()
    {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("history-read-errors"));
        db.setDatabaseName(directory.filePath(QStringLiteral("history.db")));
        if (!directory.isValid() || !db.open())
            qFatal("Cannot open history fixture");
        QSqlQuery q(db);
        for (const auto *sql :
             { "CREATE TABLE system(key TEXT,value TEXT)", "INSERT INTO system VALUES('version','0.1')",
               "CREATE TABLE contacts(id INTEGER PRIMARY KEY,acc_id TEXT,type INTEGER,jid TEXT,lifetime INTEGER)",
               "CREATE TABLE events(id INTEGER PRIMARY KEY,contact_id INTEGER REFERENCES contacts(id) "
               "ON DELETE CASCADE,resource TEXT,date TEXT,type INTEGER,direction INTEGER,subject TEXT,"
               "m_text TEXT,lang TEXT,extra_data TEXT)" }) {
            if (!q.exec(QLatin1String(sql)))
                qFatal("Cannot initialize history fixture");
        }
        if (!HistoryArchive::migrate(db))
            qFatal("Cannot migrate history fixture");
        if (!q.exec(QStringLiteral("CREATE TABLE message_media_refs(position INTEGER PRIMARY KEY, "
                                   "event_id INTEGER,ordinal INTEGER,publication_id BLOB,fingerprint BLOB)")))
            qFatal("Cannot initialize media-link fixture");
        for (const auto *sql :
             { "CREATE TABLE media_assets(id INTEGER PRIMARY KEY,account TEXT)",
               "CREATE TABLE media_asset_aliases(account TEXT,alias TEXT,asset_id INTEGER)",
               "CREATE TABLE message_media_resources(publication_id BLOB PRIMARY KEY,asset_id INTEGER,"
               "size INTEGER,url_key TEXT,kind INTEGER,name TEXT,mime TEXT)",
               "CREATE TABLE message_media_hashes(publication_id BLOB,algorithm TEXT,digest BLOB,range_key TEXT)" })
            if (!q.exec(QLatin1String(sql)))
                qFatal("Cannot initialize resource fixture");
        {
            auto files = QSqlDatabase::addDatabase("QSQLITE", "history-read-files");
            files.setDatabaseName(directory.filePath("files.db"));
            if (!files.open())
                qFatal("Cannot open registry fixture");
            QSqlQuery f(files);
            for (const auto *sql :
                 { "CREATE TABLE files(id BLOB PRIMARY KEY,partition TEXT,resource_key TEXT,scope_account TEXT,"
                   "complete INTEGER,error TEXT,size INTEGER,media_type TEXT,created INTEGER,max_age INTEGER)",
                   "CREATE TABLE file_hashes(file_id BLOB,algorithm TEXT,digest BLOB)",
                   "CREATE TABLE file_checksums(file_id BLOB,algorithm TEXT,digest BLOB,offset INTEGER,length INTEGER)",
                   "CREATE TABLE file_ranges(file_id BLOB,offset INTEGER,length INTEGER)",
                   "CREATE TABLE url_content(account TEXT,url_key TEXT,content_hash TEXT)" })
                if (!f.exec(QLatin1String(sql)))
                    qFatal("Cannot initialize registry fixture");
            files.close();
        }
        QSqlDatabase::removeDatabase("history-read-files");
        worker.open();
    }
    ~HistoryDatabase()
    {
        worker.close();
        db.close();
        db = {};
        QSqlDatabase::removeDatabase(QStringLiteral("history-read-errors"));
    }
    void seed()
    {
        HistoryArchive::Page page;
        page.stable = page.complete = true;
        page.requestIndex = page.firstIndex = 0;
        page.resultCount                    = 2;
        for (int i = 1; i <= 2; ++i) {
            HistoryArchive::Entry entry;
            entry.uid       = QString::number(i);
            entry.timestamp = QDateTime::fromMSecsSinceEpoch(i * 1000);
            entry.body      = QStringLiteral("saved plaintext");
            entry.extra     = QStringLiteral("{\"psi:decoded\":true}");
            entry.xml       = QStringLiteral("<result/>");
            page.entries.append(entry);
        }
        if (!HistoryArchive::storePage(db, key, {}, 40, page))
            qFatal("Cannot seed history fixture");
    }
};
}

class HistorySqlReadWorkerTest final : public QObject {
    Q_OBJECT
private slots:
    void readErrorsAreNotCacheMisses_data()
    {
        QTest::addColumn<int>("type");
        QTest::addColumn<QString>("missingTable");
        QTest::newRow("get") << int(HistoryReadRequest::Get) << "events";
        QTest::newRow("find") << int(HistoryReadRequest::Find) << "events";
        QTest::newRow("local-page") << int(HistoryReadRequest::LocalPage) << "events";
        QTest::newRow("local-revision") << int(HistoryReadRequest::LocalPage) << "archive_items";
        QTest::newRow("displayed-cursor") << int(HistoryReadRequest::DisplayedCursor) << "message_state_refs";
        QTest::newRow("snapshot") << int(HistoryReadRequest::ArchiveBefore) << "archive_snapshots";
        QTest::newRow("archive-page") << int(HistoryReadRequest::ArchiveBefore) << "archive_page_items";
        QTest::newRow("archive-after") << int(HistoryReadRequest::ArchiveAfter) << "archive_items";
        QTest::newRow("archive-index") << int(HistoryReadRequest::ArchiveIndex) << "archive_index_page_items";
        QTest::newRow("resolved-revision") << int(HistoryReadRequest::ResolveBatch) << "archive_items";
        QTest::newRow("archive-lookup") << int(HistoryReadRequest::ArchiveLookup) << "archive_items";
        QTest::newRow("own-plaintext") << int(HistoryReadRequest::OwnPlaintext) << "events";
        QTest::newRow("media-links") << int(HistoryReadRequest::MediaLinks) << "message_media_refs";
        QTest::newRow("chat-media-links") << int(HistoryReadRequest::ChatMediaLinks) << "message_media_refs";
        QTest::newRow("chat-media-revisions") << int(HistoryReadRequest::ChatMediaLinks) << "message_revisions";
        QTest::newRow("media-linked") << int(HistoryReadRequest::MediaLinked) << "message_media_refs";
    }

    void readErrorsAreNotCacheMisses()
    {
        QFETCH(int, type);
        QFETCH(QString, missingTable);
        HistoryDatabase fixture;
        fixture.seed();
        QSqlQuery damage(fixture.db);
        damage.prepare(QStringLiteral("SELECT sql FROM sqlite_master WHERE type='table' AND name=:name"));
        damage.bindValue(QStringLiteral(":name"), missingTable);
        QVERIFY(damage.exec() && damage.next());
        const QString repair = damage.value(0).toString();
        damage.finish();
        QVERIFY(damage.exec(QStringLiteral("DROP TABLE ") + missingTable));
        QSignalSpy         completed(&fixture.worker, &HistorySqlReadWorker::completed);
        HistoryReadRequest request;
        request.id            = 123;
        request.type          = HistoryReadRequest::Type(type);
        request.account       = fixture.key.account;
        request.peer          = fixture.key.peer;
        request.archiveKey    = fixture.key;
        request.cursor        = type == HistoryReadRequest::ArchiveBefore ? QString() : QStringLiteral("1");
        request.archiveIndex  = 0;
        request.snapshotCount = 2;
        request.limit         = 1;
        request.eventIds      = { 1 };
        request.publicationIds = { QByteArrayLiteral("publication") };
        request.messageId     = QStringLiteral("client-id");
        request.sender        = fixture.key.archive + QStringLiteral("/desktop");
        request.messageTime   = QDateTime::fromMSecsSinceEpoch(1000);
        request.fileDatabasePath = fixture.directory.filePath("files.db");
        fixture.worker.read(request);
        QCOMPARE(completed.count(), 1);
        const auto result = qvariant_cast<HistoryReadResult>(completed.takeFirst().first());
        QCOMPARE(result.id, request.id);
        QVERIFY(!result.success);
        QVERIFY(result.rows.isEmpty());
        QVERIFY(result.resolvedRows.isEmpty());
        QVERIFY(!result.archivePage.found);

        // The failed snapshot must be rolled back, so the same worker can
        // see a repaired schema and serve subsequent requests normally.
        QVERIFY(damage.exec(repair));
        fixture.worker.read(request);
        QCOMPARE(completed.count(), 1);
        QVERIFY(qvariant_cast<HistoryReadResult>(completed.takeFirst().first()).success);
    }

    void emptyHistoryIsSuccessful_data() { readErrorsAreNotCacheMisses_data(); }

    void emptyHistoryIsSuccessful()
    {
        QFETCH(int, type);
        HistoryDatabase    fixture;
        QSignalSpy         completed(&fixture.worker, &HistorySqlReadWorker::completed);
        HistoryReadRequest request;
        request.type          = HistoryReadRequest::Type(type);
        request.account       = fixture.key.account;
        request.peer          = fixture.key.peer;
        request.archiveKey    = fixture.key;
        request.cursor        = QStringLiteral("missing-uid");
        request.archiveIndex  = 0;
        request.snapshotCount = 2;
        request.eventIds      = { 1 };
        request.publicationIds = { QByteArrayLiteral("publication") };
        request.messageId     = QStringLiteral("missing-client-id");
        request.sender        = fixture.key.archive + QStringLiteral("/desktop");
        request.messageTime   = QDateTime::fromMSecsSinceEpoch(1000);
        request.fileDatabasePath = fixture.directory.filePath("files.db");
        fixture.worker.read(request);
        QCOMPARE(completed.count(), 1);
        const auto result = qvariant_cast<HistoryReadResult>(completed.takeFirst().first());
        QVERIFY(result.success);
        QVERIFY(result.rows.isEmpty());
        QVERIFY(result.resolvedRows.isEmpty());
        QVERIFY(!result.archivePage.found);
        QVERIFY(result.cursor.messageId.isEmpty());
    }

    void exactArchiveLookupDoesNotScanNearbyMessages()
    {
        HistoryDatabase fixture;
        fixture.seed();
        QSignalSpy         completed(&fixture.worker, &HistorySqlReadWorker::completed);
        HistoryReadRequest request;
        request.type        = HistoryReadRequest::ArchiveLookup;
        request.archiveKey  = fixture.key;
        request.cursor      = QStringLiteral("1");
        request.messageTime = QDateTime::fromMSecsSinceEpoch(1000);
        fixture.worker.read(request);
        QCOMPARE(completed.count(), 1);
        auto result = qvariant_cast<HistoryReadResult>(completed.takeFirst().first());
        QVERIFY(result.success);
        QVERIFY(result.archiveExact);
        QCOMPARE(result.rows.size(), 1); // No ten-minute candidate scan on a decoded exact hit.

        QSqlQuery q(fixture.db);
        QVERIFY(q.exec(QStringLiteral("UPDATE events SET extra_data='{\"psi:decoded\":false}' WHERE id=1")));
        fixture.worker.read(request);
        result = qvariant_cast<HistoryReadResult>(completed.takeFirst().first());
        QVERIFY(result.success);
        QVERIFY(result.archiveExact);
        QCOMPARE(result.rows.size(), 3); // An undecoded hit still permits live-plaintext fallback.
    }

    void localDeletionIsACommitBarrierAndQueuedAppendsCannotResurrect()
    {
        HistoryDatabase     fixture;
        QSignalSpy          wrote(&fixture.worker, &HistorySqlReadWorker::writeCompleted);
        HistoryWriteRequest append;
        append.id            = 1;
        append.account       = fixture.key.account;
        append.contactPeer   = fixture.key.peer;
        append.jidType       = 1;
        append.encoded.valid = append.encoded.isMessage = true;
        append.encoded.type                             = 1;
        append.encoded.direction                        = 2;
        append.encoded.timestamp                        = QDateTime::fromMSecsSinceEpoch(1000);
        append.encoded.messageId                        = "private-id";
        append.encoded.body = append.encoded.matchBody = "private text";
        append.encoded.extras = "{\"psi:decoded\":true,\"psi:message\":{\"id\":\"private-id\"}}";
        fixture.worker.write(append);
        QCOMPARE(wrote.size(), 0);
        HistoryWriteRequest probe;
        probe.id                 = 2;
        probe.type               = HistoryWriteRequest::CheckSuppression;
        probe.archiveKey         = fixture.key;
        probe.identity.messageId = "ordinary-unrelated-message";
        probe.identity.direction = 2;
        fixture.worker.write(probe);
        QCOMPARE(wrote.size(), 1); // An identity miss must not force an append fsync.
        QCOMPARE(qvariant_cast<HistoryWriteResult>(wrote[0][0]).id, 2);
        HistoryWriteRequest remove;
        remove.id                 = 3;
        remove.type               = HistoryWriteRequest::DeleteLocal;
        remove.archiveKey         = fixture.key;
        remove.identity.messageId = "private-id";
        remove.identity.direction = 2;
        fixture.worker.write(remove);
        QCOMPARE(wrote.size(), 3);
        QCOMPARE(qvariant_cast<HistoryWriteResult>(wrote[1][0]).id, 1);
        const auto deleted = qvariant_cast<HistoryWriteResult>(wrote[2][0]);
        QVERIFY(deleted.success && deleted.localDeletion.suppressed);
        QVERIFY(!deleted.localDeletion.hiddenEvents.isEmpty());
        append.id = 4;
        fixture.worker.write(append); // Captured before the deletion committed.
        append.id                = 5;
        append.encoded.messageId = "future-correction";
        append.encoded.replaceId = "private-id";
        fixture.worker.write(append);
        QSignalSpy         read(&fixture.worker, &HistorySqlReadWorker::completed);
        HistoryReadRequest query;
        query.type    = HistoryReadRequest::Find;
        query.account = fixture.key.account;
        query.peer    = fixture.key.peer;
        query.text    = "private";
        fixture.worker.read(query);
        QCOMPARE(read.size(), 1);
        const auto found = qvariant_cast<HistoryReadResult>(read[0][0]);
        QVERIFY(found.success && found.rows.isEmpty());
        QCOMPARE(wrote.size(), 5);
        for (int i = 3; i < wrote.size(); ++i) {
            const auto result = qvariant_cast<HistoryWriteResult>(wrote[i][0]);
            QVERIFY(result.success && !result.inserted && result.localDeletion.suppressed);
        }
        QSqlQuery stored(fixture.db);
        QVERIFY(stored.exec("SELECT count(*) FROM events WHERE (message_flags & 16)<>0 AND extra_data=''"));
        QVERIFY(stored.next());
        QCOMPARE(stored.value(0).toInt(), 1);
    }

    void failedIdentityLookupRollsBackAppendBatch()
    {
        HistoryDatabase fixture;
        fixture.seed();
        QSqlQuery q(fixture.db);
        QVERIFY(q.exec(QStringLiteral("DROP TABLE archive_items")));
        QSignalSpy          wrote(&fixture.worker, &HistorySqlReadWorker::writeCompleted);
        HistoryWriteRequest append;
        append.id                = 1;
        append.account           = fixture.key.account;
        append.contactPeer       = fixture.key.peer;
        append.jidType           = 1;
        append.encoded.valid     = true;
        append.encoded.type      = 3; // An auth event starts a valid pending batch.
        append.encoded.timestamp = QDateTime::fromMSecsSinceEpoch(1000);
        fixture.worker.write(append);
        QCOMPARE(wrote.count(), 0);
        append.id                = 2;
        append.encoded.isMessage = true;
        append.encoded.type      = 1;
        append.encoded.sender    = fixture.key.peer;
        append.encoded.messageId = QStringLiteral("live-id");
        append.encoded.body      = QStringLiteral("live text");
        fixture.worker.write(append);
        fixture.worker.read({}); // Flush even if the broken lookup was mistaken for a miss.
        QCOMPARE(wrote.count(), 2);
        for (const auto &signal : wrote) {
            const auto result = qvariant_cast<HistoryWriteResult>(signal.first());
            QVERIFY(!result.success);
            QVERIFY(!result.inserted);
        }
        QVERIFY(q.exec(QStringLiteral("SELECT count(*) FROM events")) && q.next());
        QCOMPARE(q.value(0).toInt(), 2); // Neither append was committed.
    }

    void serialWritesAndReadsOnOldServer()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("history.db"));
        const QString name = QStringLiteral("history-single-worker-test");
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA foreign_keys=ON")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE system(key TEXT,value TEXT)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO system VALUES('version','0.1')")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE contacts(id INTEGER PRIMARY KEY,acc_id TEXT,type INTEGER,"
                                          "jid TEXT,lifetime INTEGER)")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE events(id INTEGER PRIMARY KEY,contact_id INTEGER "
                                          "REFERENCES contacts(id) ON DELETE CASCADE,resource TEXT,date TEXT,"
                                          "type INTEGER,direction INTEGER,subject TEXT,m_text TEXT,lang TEXT,"
                                          "extra_data TEXT)")));
            QVERIFY(HistoryArchive::migrate(db));
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
        qRegisterMetaType<HistoryWriteRequest>("HistoryWriteRequest");
        qRegisterMetaType<HistoryWriteResult>("HistoryWriteResult");
        qRegisterMetaType<HistoryReadRequest>("HistoryReadRequest");
        qRegisterMetaType<HistoryReadResult>("HistoryReadResult");

        QThread thread;
        auto *worker = new HistorySqlReadWorker(path);
        worker->moveToThread(&thread);
        connect(&thread, &QThread::started, worker, &HistorySqlReadWorker::open);
        connect(worker, &HistorySqlReadWorker::stopped, &thread, &QThread::quit, Qt::DirectConnection);
        connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        QSignalSpy wrote(worker, &HistorySqlReadWorker::writeCompleted);
        QSignalSpy read(worker, &HistorySqlReadWorker::completed);
        QVERIFY(wrote.isValid());
        QVERIFY(read.isValid());
        thread.start();

        const auto enqueueWrite = [worker](HistoryWriteRequest request) {
            return QMetaObject::invokeMethod(worker, "write", Qt::QueuedConnection,
                                             Q_ARG(HistoryWriteRequest, request));
        };
        const auto enqueueRead = [worker](HistoryReadRequest request) {
            return QMetaObject::invokeMethod(worker, "read", Qt::QueuedConnection,
                                             Q_ARG(HistoryReadRequest, request));
        };
        HistoryWriteRequest append;
        append.type = HistoryWriteRequest::Append;
        append.account = QStringLiteral("account");
        append.peer = QStringLiteral("bob@example.test/resource/with/slash");
        append.contactPeer = QStringLiteral("bob@example.test");
        append.resource = QStringLiteral("resource/with/slash");
        append.jidType = 1;
        append.encoded.valid = true;
        append.encoded.isMessage = true;
        append.encoded.type = 1;
        append.encoded.direction = 2;
        append.encoded.timestamp = QDateTime::fromMSecsSinceEpoch(1000, Qt::UTC);
        append.encoded.body = QStringLiteral("same text");
        append.encoded.matchBody = append.encoded.body;
        append.encoded.sender = append.peer;
        // Neither the server nor the legacy message supplies an ID. Two
        // distinct deliveries with equal timestamp/text must both be stored.
        append.id = 1;
        QVERIFY(enqueueWrite(append));
        append.id = 2;
        QVERIFY(enqueueWrite(append));
        HistoryReadRequest page;
        page.id = 3;
        page.account = append.account;
        page.peer = QStringLiteral("bob@example.test");
        page.type = HistoryReadRequest::LocalPage;
        page.limit = 40;
        QVERIFY(enqueueRead(page)); // Must observe both preceding writes.
        QTRY_COMPARE(wrote.count(), 2);
        auto first = qvariant_cast<HistoryWriteResult>(wrote.takeFirst().first());
        auto second = qvariant_cast<HistoryWriteResult>(wrote.takeFirst().first());
        QVERIFY(first.success && first.inserted);
        QVERIFY(second.success && second.inserted);
        QTRY_COMPARE(read.count(), 1);
        auto rows = qvariant_cast<HistoryReadResult>(read.takeFirst().first());
        QVERIFY(rows.success);
        QCOMPARE(rows.rows.size(), 2);
        QVERIFY(rows.rows.first().value(QStringLiteral("id")).toLongLong()
                != rows.rows.last().value(QStringLiteral("id")).toLongLong());

        // The MAM cache page is written atomically on the same worker as
        // live messages and read again without a separate SQL owner.
        HistoryWriteRequest store;
        store.id = 7;
        store.type = HistoryWriteRequest::StorePage;
        store.archiveKey = { QStringLiteral("account"), QStringLiteral("alice@example.test"),
                             QStringLiteral("bob@example.test"), false };
        store.limit = 40;
        HistoryArchive::Entry entry;
        entry.uid = QStringLiteral("archive-seven");
        entry.timestamp = QDateTime::fromMSecsSinceEpoch(2000, Qt::UTC);
        entry.messageId = QStringLiteral("client-seven");
        entry.sender = append.peer;
        entry.body = QStringLiteral("archived message");
        entry.xml = QStringLiteral("<result id='archive-seven' xmlns='urn:xmpp:mam:2'>"
                                   "<forwarded xmlns='urn:xmpp:forward:0'><delay "
                                   "xmlns='urn:xmpp:delay' stamp='1970-01-01T00:00:02Z'/>"
                                   "<message xmlns='jabber:client' id='client-seven' type='chat' "
                                   "from='bob@example.test/resource' to='alice@example.test'>"
                                   "<body>archived message</body></message></forwarded></result>");
        store.archivePage.entries.append(entry);
        store.archivePage.stable = true;
        store.archivePage.complete = true;
        QVERIFY(enqueueWrite(store));
        QTRY_COMPARE(wrote.count(), 1);
        const auto stored = qvariant_cast<HistoryWriteResult>(wrote.takeFirst().first());
        QVERIFY(stored.success);
        QCOMPARE(stored.archivePage.entries.size(), 1);
        QVERIFY(stored.archivePage.entries.first().eventId > 0);

        HistoryReadRequest archiveRead;
        archiveRead.id = 8;
        archiveRead.type = HistoryReadRequest::ArchiveBefore;
        archiveRead.archiveKey = store.archiveKey;
        archiveRead.limit = 40;
        QVERIFY(enqueueRead(archiveRead));
        QTRY_COMPARE(read.count(), 1);
        const auto loaded = qvariant_cast<HistoryReadResult>(read.takeFirst().first());
        QVERIFY(loaded.success);
        QVERIFY(loaded.archivePage.found);
        QCOMPARE(loaded.archivePage.entries.size(), 1);
        QCOMPARE(loaded.archivePage.entries.first().uid, entry.uid);
        QCOMPARE(loaded.archivePage.entries.first().eventId, stored.archivePage.entries.first().eventId);

        // Reproduce a busy history database without freezing the GUI event
        // loop. This is the critical regression on slow disks / checkpoints.
        const QString lockName = QStringLiteral("history-worker-lock-test");
        {
            auto blocker = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), lockName);
            blocker.setDatabaseName(path);
            QVERIFY(blocker.open());
            QVERIFY(blocker.transaction());
            QSqlQuery lock(blocker);
            QVERIFY(lock.exec(QStringLiteral("UPDATE system SET value='busy' WHERE key='version'")));
            append.id = 9;
            append.encoded.timestamp = QDateTime::fromMSecsSinceEpoch(3000, Qt::UTC);
            QVERIFY(enqueueWrite(append));

            bool guiDelivered = false;
            QTimer::singleShot(0, this, [&guiDelivered] { guiDelivered = true; });
            QTRY_VERIFY(guiDelivered);
            // The worker must be waiting on the locked database, but this
            // Qt event loop has remained responsive.
            QCOMPARE(wrote.count(), 0);
            QVERIFY(blocker.commit());
            QTRY_COMPARE(wrote.count(), 1);
            QVERIFY(qvariant_cast<HistoryWriteResult>(wrote.takeFirst().first()).success);
            blocker.close();
        }
        QSqlDatabase::removeDatabase(lockName);

        HistoryWriteRequest erase;
        erase.id = 4;
        erase.type = HistoryWriteRequest::Erase;
        erase.account = append.account;
        erase.peer = page.peer;
        QVERIFY(enqueueWrite(erase));
        page.id = 5;
        QVERIFY(enqueueRead(page));
        QTRY_COMPARE(wrote.count(), 1);
        QVERIFY(qvariant_cast<HistoryWriteResult>(wrote.takeFirst().first()).success);
        QTRY_COMPARE(read.count(), 1);
        rows = qvariant_cast<HistoryReadResult>(read.takeFirst().first());
        QVERIFY(rows.success);
        QVERIFY(rows.rows.isEmpty());

        // An invalid second append must roll back the first one in the same
        // batch. Neither handle may be told "success" before COMMIT.
        append.id = 10;
        append.encoded.timestamp = QDateTime::fromMSecsSinceEpoch(4000, Qt::UTC);
        HistoryWriteRequest invalid = append;
        invalid.id = 11;
        invalid.account.clear();
        QVERIFY(enqueueWrite(append));
        QVERIFY(enqueueWrite(invalid));
        QTRY_COMPARE(wrote.count(), 2);
        const auto rolledBack = qvariant_cast<HistoryWriteResult>(wrote.takeFirst().first());
        const auto rejected = qvariant_cast<HistoryWriteResult>(wrote.takeFirst().first());
        QCOMPARE(rolledBack.id, 10);
        QCOMPARE(rejected.id, 11);
        QVERIFY(!rolledBack.success);
        QVERIFY(!rejected.success);
        page.id = 12;
        QVERIFY(enqueueRead(page));
        QTRY_COMPARE(read.count(), 1);
        rows = qvariant_cast<HistoryReadResult>(read.takeFirst().first());
        QVERIFY(rows.success);
        QVERIFY(rows.rows.isEmpty());

        // Shutdown commits a final append even when neither its timer nor
        // another read has had a chance to flush the batch.
        append.id = 13;
        QVERIFY(enqueueWrite(append));
        QVERIFY(QMetaObject::invokeMethod(worker, "close", Qt::QueuedConnection));
        QVERIFY(thread.wait(5000));
        QCOMPARE(wrote.count(), 1);
        QVERIFY(qvariant_cast<HistoryWriteResult>(wrote.takeFirst().first()).success);
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("SELECT count(*) FROM events")) && q.next());
            QCOMPARE(q.value(0).toInt(), 1);
            q.finish();
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
    }

    void queuedReadsPreserveLegacyMessagesAndOrdering()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("history.db"));
        const QString connection = QStringLiteral("history-reader-test-write");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(path);
            QVERIFY2(db.open(), qPrintable(db.lastError().text()));
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA journal_mode=WAL")));
            q.finish();
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE contacts(id INTEGER PRIMARY KEY,acc_id TEXT,jid TEXT)")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE events("
                "id INTEGER PRIMARY KEY,contact_id INTEGER,date TEXT,history_date_ms INTEGER,"
                "type INTEGER,direction INTEGER,subject TEXT,m_text TEXT,lang TEXT,extra_data TEXT,"
                "history_state INTEGER DEFAULT 0,message_flags INTEGER DEFAULT 0)")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE message_revisions(event_id INTEGER,root_id INTEGER)")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE message_state_refs(event_id INTEGER,kind INTEGER,target TEXT)")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE archive_items(event_id INTEGER,uid TEXT,archive TEXT,xml TEXT)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO contacts VALUES (1,'account','old.example/peer')")));
            // A legacy server has no MAM, origin-id or stanza-id. Three
            // identical messages (including their timestamp) remain separate.
            QVERIFY(db.transaction());
            for (int i = 1; i <= 3; ++i) {
                q.prepare(QStringLiteral("INSERT INTO events(id,contact_id,date,history_date_ms,type,direction,"
                    "m_text,extra_data) VALUES(:id,1,'2026-11-09T19:00:00Z',1000,1,2,'hello','{}')"));
                q.bindValue(QStringLiteral(":id"), i);
                QVERIFY(q.exec());
            }
            QVERIFY(q.exec(QStringLiteral("UPDATE events SET message_flags=1 WHERE id=3")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO message_state_refs VALUES(3,1,'client-id-3')")));
            QVERIFY(db.commit());
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);

        qRegisterMetaType<HistoryReadRequest>("HistoryReadRequest");
        qRegisterMetaType<HistoryReadResult>("HistoryReadResult");
        QThread thread;
        auto *reader = new HistorySqlReadWorker(path);
        reader->moveToThread(&thread);
        connect(&thread, &QThread::started, reader, &HistorySqlReadWorker::open);
        connect(reader, &HistorySqlReadWorker::stopped, &thread, &QThread::quit, Qt::DirectConnection);
        connect(&thread, &QThread::finished, reader, &QObject::deleteLater);
        QSignalSpy spy(reader, &HistorySqlReadWorker::completed);
        QVERIFY(spy.isValid());
        thread.start();

        const auto send = [reader](HistoryReadRequest request) {
            return QMetaObject::invokeMethod(reader, "read", Qt::QueuedConnection,
                                             Q_ARG(HistoryReadRequest, request));
        };
        HistoryReadRequest local;
        local.id = 1;
        local.type = HistoryReadRequest::LocalPage;
        local.account = QStringLiteral("account");
        local.peer = QStringLiteral("old.example/peer");
        local.limit = 3;
        QVERIFY(send(local));
        QTRY_COMPARE(spy.count(), 1);
        auto result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
        QVERIFY(result.success);
        QCOMPARE(result.rows.size(), 3);
        for (int i = 0; i < 3; ++i)
            QCOMPARE(result.rows.at(i).value(QStringLiteral("id")).toInt(), i + 1);

        // Requesting the next page with the oldest local ID cannot replay
        // the same three records. No server-provided identity is necessary.
        local.id = 2;
        local.before = 1;
        QVERIFY(send(local));
        QTRY_COMPARE(spy.count(), 1);
        result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
        QVERIFY(result.success);
        QVERIFY(result.rows.isEmpty());

        HistoryReadRequest get;
        get.id = 3;
        get.type = HistoryReadRequest::Get;
        get.account = local.account;
        get.peer = local.peer;
        get.direction = 1; // EDB::Backward
        get.limit = 2;
        QVERIFY(send(get));
        QTRY_COMPARE(spy.count(), 1);
        result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
        QVERIFY(result.success);
        QCOMPARE(result.rows.size(), 2);
        QCOMPARE(result.rows.first().value(QStringLiteral("id")).toInt(), 3);
        QCOMPARE(result.rows.last().value(QStringLiteral("id")).toInt(), 2);
        QCOMPARE(result.beginRow, 2);

        HistoryReadRequest find;
        find.id = 4;
        find.type = HistoryReadRequest::Find;
        find.account = local.account;
        find.peer = local.peer;
        find.text = QStringLiteral("HELLO");
        QVERIFY(send(find));
        QTRY_COMPARE(spy.count(), 1);
        result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
        QVERIFY(result.success);
        QCOMPARE(result.rows.size(), 3);

        // The persisted XEP-0333 cursor is restored through the same
        // worker, without blocking an activated chat's GUI thread.
        HistoryReadRequest displayed;
        displayed.id = 7;
        displayed.type = HistoryReadRequest::DisplayedCursor;
        displayed.account = local.account;
        displayed.peer = local.peer;
        QVERIFY(send(displayed));
        QTRY_COMPARE(spy.count(), 1);
        result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
        QVERIFY(result.success);
        QCOMPARE(result.cursor.eventId, qint64(3));
        QCOMPARE(result.cursor.messageId, QStringLiteral("client-id-3"));

        // A writer can commit another live message concurrently. A worker
        // snapshot never leaks an uncommitted row or loses earlier messages.
        const QString concurrentName = QStringLiteral("history-reader-concurrent-writer");
        {
            auto writer = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), concurrentName);
            writer.setDatabaseName(path);
            QVERIFY(writer.open());
            QVERIFY(writer.transaction());
            QSqlQuery q(writer);
            QVERIFY(q.exec(QStringLiteral("INSERT INTO events(id,contact_id,date,history_date_ms,type,direction,"
                "m_text,extra_data) VALUES(4,1,'2026-11-09T19:00:00Z',1000,1,2,'hello','{}')")));
            get.id = 5;
            get.limit = 10;
            QVERIFY(send(get));
            QTRY_COMPARE(spy.count(), 1);
            result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
            QVERIFY(result.success);
            QCOMPARE(result.rows.size(), 3);
            QVERIFY(writer.commit());

            get.id = 6;
            QVERIFY(send(get));
            QTRY_COMPARE(spy.count(), 1);
            result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
            QVERIFY(result.success);
            QCOMPARE(result.rows.size(), 4);
            QCOMPARE(result.rows.first().value(QStringLiteral("id")).toInt(), 4);
            writer.close();
        }
        QSqlDatabase::removeDatabase(concurrentName);

        QVERIFY(QMetaObject::invokeMethod(reader, "close", Qt::QueuedConnection));
        QVERIFY(thread.wait(5000));
    }
};

QTEST_GUILESS_MAIN(HistorySqlReadWorkerTest)
#include "HistorySqlReadWorkerTest.moc"

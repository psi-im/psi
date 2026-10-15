#include "mediachunktransfer.h"
#include <QBuffer>
#include <QPointer>
#include <QtTest>

class FinalReadSource : public QIODevice {
public:
    FinalReadSource() { open(ReadOnly); }
    std::function<void()> onRead;
    qint64                bytesAvailable() const override { return (read_ ? 0 : 4) + QIODevice::bytesAvailable(); }
    bool                  isSequential() const override { return true; }
    qint64                readData(char *out, qint64 size) override
    {
        if (read_)
            return -1;
        Q_ASSERT(size >= 4);
        read_ = true;
        memcpy(out, "data", 4);
        onRead();
        return 4;
    }
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    bool read_ = false;
};

class ChunkTransferTest : public QObject {
    Q_OBJECT
private slots:
    void completionInsideFinalReadIsDeferred()
    {
        auto                      input = new FinalReadSource;
        QPointer<FinalReadSource> guard(input);
        int                       deliveries = 0;
        auto                      transfer   = new MediaChunkTransfer(
            input, 4, 0, 4,
            [&](QByteArray bytes, MediaChunkError error) {
                QVERIFY(error.isEmpty());
                QCOMPARE(bytes, QByteArray("data"));
                ++deliveries;
            },
            [] {});
        input->onRead = [transfer, guard] {
            transfer->succeeded();
            transfer->disconnected();
            QVERIFY(guard); // QIODevice must survive its active read stack.
        };
        transfer->metadata(4, std::nullopt);
        QCOMPARE(deliveries, 0);
        QTRY_COMPARE(deliveries, 1);
        QVERIFY(!guard);
    }
    void waitsForVerifiedCompletion()
    {
        auto input = new QBuffer;
        input->setData("data");
        input->open(QIODevice::ReadOnly);
        int  deliveries = 0, stops = 0;
        auto transfer = new MediaChunkTransfer(
            input, 100, 8, 4,
            [&](QByteArray bytes, MediaChunkError error) {
                QVERIFY(error.isEmpty());
                QCOMPARE(bytes, QByteArray("data"));
                ++deliveries;
            },
            [&] { ++stops; });
        transfer->metadata(std::nullopt,
                           MediaChunkTransfer::Range { 8, 4 }); // HTTP may report Content-Range */unknown.
        QCoreApplication::processEvents();
        QCOMPARE(deliveries, 0); // All bytes are not proof of transport success.
        transfer->succeeded();
        QTRY_COMPARE(deliveries, 1);
        QCOMPARE(stops, 1);
    }

    void failureAfterPayloadDoesNotCommit()
    {
        auto input = new QBuffer;
        input->setData("data");
        input->open(QIODevice::ReadOnly);
        int  deliveries = 0;
        auto transfer   = new MediaChunkTransfer(
            input, 4, 0, 4,
            [&](QByteArray bytes, MediaChunkError error) {
                QVERIFY(bytes.isEmpty());
                QVERIFY(!error.isEmpty());
                ++deliveries;
            },
            [] {});
        transfer->metadata(4, std::nullopt);
        transfer->failed(QStringLiteral("checksum mismatch"));
        transfer->succeeded();
        QTRY_COMPARE(deliveries, 1);
    }

    void rangeErrorsAreTyped_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<int>("code");
        QTest::newRow("ignored-range") << 0 << int(MediaChunkError::RangeUnsupported);
        QTest::newRow("wrong-start") << 1 << int(MediaChunkError::Failed);
        QTest::newRow("wrong-length") << 2 << int(MediaChunkError::Failed);
        QTest::newRow("wrong-size") << 3 << int(MediaChunkError::Failed);
        QTest::newRow("start-past-eof") << 4 << int(MediaChunkError::Failed);
    }
    void rangeErrorsAreTyped()
    {
        QFETCH(int, kind);
        QFETCH(int, code);
        auto input = new QBuffer;
        input->setData("data");
        input->open(QIODevice::ReadOnly);
        int  deliveries = 0;
        auto transfer   = new MediaChunkTransfer(
            input, 100, 8, 4,
            [&](QByteArray bytes, MediaChunkError error) {
                QVERIFY(bytes.isEmpty());
                QCOMPARE(int(error.code()), code);
                ++deliveries;
            },
            [] {});
        if (kind == 0)
            transfer->metadata(100, std::nullopt);
        if (kind == 1)
            transfer->metadata(100, MediaChunkTransfer::Range { 9, 4 });
        if (kind == 2)
            transfer->metadata(100, MediaChunkTransfer::Range { 8, 5 });
        if (kind == 3)
            transfer->metadata(99, MediaChunkTransfer::Range { 8, 4 });
        if (kind == 4)
            transfer->metadata(100, MediaChunkTransfer::Range { 101, 4 });
        QTRY_COMPARE(deliveries, 1);
    }

    void cancellationAndTruncatedSource()
    {
        for (bool cancel : { false, true }) {
            auto input = new QBuffer;
            input->setData("bad");
            input->open(QIODevice::ReadOnly);
            int  deliveries = 0, stops = 0;
            auto transfer = new MediaChunkTransfer(
                input, 4, 0, 4,
                [&](QByteArray bytes, MediaChunkError error) {
                    QVERIFY(bytes.isEmpty());
                    QCOMPARE(error.code(), cancel ? MediaChunkError::Cancelled : MediaChunkError::Failed);
                    ++deliveries;
                },
                [&] { ++stops; });
            transfer->metadata(4, MediaChunkTransfer::Range { 0, 0 });
            if (cancel)
                transfer->cancel();
            else
                transfer->succeeded();
            QTRY_COMPARE(deliveries, 1);
            QCOMPARE(stops, 1);
        }
    }

    void excessPayloadIsRejected()
    {
        auto input = new QBuffer;
        input->setData("extra");
        input->open(QIODevice::ReadOnly);
        int  deliveries = 0;
        auto transfer   = new MediaChunkTransfer(
            input, 4, 0, 4,
            [&](QByteArray bytes, MediaChunkError error) {
                QVERIFY(bytes.isEmpty());
                QCOMPARE(error.code(), MediaChunkError::Failed);
                ++deliveries;
            },
            [] {});
        transfer->metadata(4, std::nullopt);
        transfer->succeeded();
        QTRY_COMPARE(deliveries, 1);
    }

    void ownerCanBeDeletedByCompletion()
    {
        auto owner = new QObject;
        auto input = new QBuffer;
        input->setData("data");
        input->open(QIODevice::ReadOnly);
        int               stops = 0;
        QPointer<QObject> guard(owner);
        auto              transfer = new MediaChunkTransfer(
            input, 4, 0, 4,
            [&](QByteArray, MediaChunkError error) {
                QVERIFY(error.isEmpty());
                delete owner;
            },
            [&] { ++stops; }, owner);
        transfer->metadata(4, std::nullopt);
        transfer->succeeded();
        QTRY_VERIFY(!guard);
        QCOMPARE(stops, 1);
    }
};
QTEST_GUILESS_MAIN(ChunkTransferTest)
#include "chunktransfer.moc"

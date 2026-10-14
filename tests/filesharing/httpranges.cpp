#include "httputil.h"

#include <QTest>
#include <QUrl>
#include <limits>

class HttpRangesTest : public QObject {
    Q_OBJECT
private slots:
    void range_data()
    {
        QTest::addColumn<QByteArray>("header");
        QTest::addColumn<int>("result");
        QTest::addColumn<quint64>("start");
        QTest::addColumn<quint64>("length");
        QTest::newRow("head") << QByteArray("bytes=0-65535") << int(Http::Parsed) << quint64(0) << quint64(65536);
        QTest::newRow("tail") << QByteArray("bytes=2342971392-") << int(Http::Parsed) << quint64(2342971392)
                              << quint64(65536);
        QTest::newRow("suffix") << QByteArray("bytes=-65536") << int(Http::Parsed) << quint64(2342971392)
                                << quint64(65536);
        QTest::newRow("large-suffix") << QByteArray("bytes=-9999999999") << int(Http::Parsed) << quint64(0)
                                      << quint64(2343036928);
        QTest::newRow("clamp-end") << QByteArray("bytes=2342971392-9999999999") << int(Http::Parsed)
                                   << quint64(2342971392) << quint64(65536);
        QTest::newRow("full") << QByteArray("bytes=0-") << int(Http::Parsed) << quint64(0) << quint64(2343036928);
        QTest::newRow("past-end") << QByteArray("bytes=2343036928-") << int(Http::OutOfRange) << quint64(0)
                                  << quint64(0);
        QTest::newRow("empty-suffix") << QByteArray("bytes=-0") << int(Http::OutOfRange) << quint64(0) << quint64(0);
        QTest::newRow("reversed") << QByteArray("bytes=10-9") << int(Http::Unparsed) << quint64(0) << quint64(0);
        QTest::newRow("signed") << QByteArray("bytes=+1-2") << int(Http::Unparsed) << quint64(0) << quint64(0);
        QTest::newRow("multi") << QByteArray("bytes=0-2,4-5") << int(Http::NotImplementedMultirange) << quint64(0)
                               << quint64(0);
    }
    void range()
    {
        QFETCH(QByteArray, header);
        QFETCH(int, result);
        QFETCH(quint64, start);
        QFETCH(quint64, length);
        const auto [actualResult, actualStart, actualLength] = Http::parseRangeHeader(header, 2343036928ULL);
        QCOMPARE(int(actualResult), result);
        QCOMPARE(actualStart, start);
        QCOMPARE(actualLength, length);
    }
    void unknownAndEmptySize()
    {
        QCOMPARE(std::get<0>(Http::parseRangeHeader("bytes=-10")), Http::NotImplementedTailLoad);
        QCOMPARE(std::get<2>(Http::parseRangeHeader("bytes=42-")), quint64(0));
        QCOMPARE(std::get<0>(Http::parseRangeHeader("bytes=0-", 0)), Http::OutOfRange);
        QCOMPARE(std::get<0>(Http::parseRangeHeader("bytes=-10", 0)), Http::OutOfRange);
        QCOMPARE(std::get<0>(Http::parseRangeHeader("bytes=0-18446744073709551615")), Http::Unparsed);
    }
    void contentRange()
    {
        const auto tail = Http::parseContentRangeHeader("bytes 2342971392-2343036927/2343036928");
        QVERIFY(tail);
        QCOMPARE(std::get<0>(*tail), quint64(2342971392));
        QCOMPARE(std::get<1>(*tail), quint64(65536));
        QCOMPARE(*std::get<2>(*tail), quint64(2343036928));
        const auto unknown = Http::parseContentRangeHeader("bytes 10-19/*");
        QVERIFY(unknown);
        QCOMPARE(std::get<1>(*unknown), quint64(10));
        QVERIFY(!std::get<2>(*unknown));
        QVERIFY(!Http::parseContentRangeHeader("bytes 10-9/100"));
        QVERIFY(!Http::parseContentRangeHeader("bytes 0-100/100"));
        QVERIFY(!Http::parseContentRangeHeader("bytes */100"));
        QVERIFY(!Http::parseContentRangeHeader("bytes 0-18446744073709551615/*"));
    }
};

QTEST_GUILESS_MAIN(HttpRangesTest)
#include "httpranges.moc"

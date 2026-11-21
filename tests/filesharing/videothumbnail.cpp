// SPDX-License-Identifier: GPL-2.0-or-later
#include "videothumbnailgenerator.h"
#include <QtTest>

class VideoThumbnailTest : public QObject {
    Q_OBJECT
private slots:
    void candidatePositionsStayInTheFirstHalf()
    {
        for (qint64 duration : { qint64(0), qint64(1), qint64(750), qint64(3000),
                                 qint64(12000), qint64(200000) }) {
            const auto positions = VideoThumbnailGenerator::candidatePositions(duration);
            QVERIFY(!positions.isEmpty());
            for (const auto position : positions) {
                QVERIFY(position >= 0);
                QVERIFY(position <= duration / 2);
            }
        }
        const auto positions = VideoThumbnailGenerator::candidatePositions(12000);
        QVERIFY(positions.contains(2000));
        QVERIFY(positions.contains(4000));
        QVERIFY(positions.contains(500));
        QVERIFY(positions.contains(5000));
    }
    void videoThumbnailIsSmallAndDecodable()
    {
        QImage image(1024, 768, QImage::Format_RGB32);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                image.setPixel(x, y, qRgb((x + y) % 256, x % 256, y % 256));
        const auto jpeg = VideoThumbnailGenerator::encodeTinyJpeg(image);
        QVERIFY(!jpeg.isEmpty());
        QVERIFY(jpeg.size() <= 50 * 1024);
        const auto decoded = QImage::fromData(jpeg, "JPEG");
        QVERIFY(!decoded.isNull());
        QVERIFY(decoded.width() <= 240);
        QVERIFY(decoded.height() <= 240);
        QVERIFY(VideoThumbnailGenerator::encodeTinyJpeg(QImage()).isEmpty());
    }
};
QTEST_GUILESS_MAIN(VideoThumbnailTest)
#include "videothumbnail.moc"

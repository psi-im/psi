/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filedownload.h"
#include "filereader.h"
#include "mediachunkcache.h"
#include <QAudioOutput>
#include <QElapsedTimer>
#include <QFile>
#include <QMediaPlayer>
#include <QTemporaryDir>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtTest>
#include <algorithm>
#include <memory>

namespace {
struct Decoder {
    FileReader   &reader;
    QMediaPlayer *player = new QMediaPlayer;
    explicit Decoder(FileReader &reader) : reader(reader) { }
    ~Decoder()
    {
        reader.abort();
        delete player;
    }
};
}
class MediaDeviceTest : public QObject {
    Q_OBJECT
private slots:
    void decoderPlaysThroughDemandReader()
    {
        QFile fixture(QStringLiteral(PSI_MEDIA_DEVICE_FIXTURE));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        auto bytes = fixture.readAll();
        // Extend the existing Segment with a valid EBML Void. Its cues keep
        // their offsets, and a demuxer can seek past the padding without reading it.
        QCOMPARE(bytes.mid(36, 4), QByteArray::fromHex("18538067"));
        const quint64 padding     = 20 * 1024 * 1024;
        const quint64 size        = quint64(bytes.size()) + padding;
        const quint64 segmentSize = (quint64(1) << 56) | (size - 48);
        for (int i = 0; i < 8; ++i)
            bytes[40 + i] = char(segmentSize >> (56 - i * 8));
        bytes.append(char(0xec));
        const quint64 voidSize = (quint64(1) << 56) | (padding - 9);
        for (int i = 0; i < 8; ++i)
            bytes.append(char(voidSize >> (56 - i * 8)));
        QTemporaryDir   directory;
        MediaChunkCache cache(directory.path());
        quint64         fetched = 0;
        FileDownload    download(&cache, "movie", size, "account",
                                 [&](quint64 offset, quint64 length, FileDownload::FullDownloadAllowed whole,
                                  MediaChunkCompletion done) -> MediaChunkAbort {
                                  fetched += length;
                                  if (whole()) {
                                      done({}, QStringLiteral("Unexpected full download"));
                                      return {};
                                  }
                                  QByteArray part(int(length), '\0');
                                  if (offset < quint64(bytes.size())) {
                                      const auto count = std::min(length, quint64(bytes.size()) - offset);
                                      std::copy_n(bytes.constData() + offset, count, part.data());
                                  }
                                  auto cancelled = std::make_shared<bool>(false);
                                  QTimer::singleShot(5, &cache, [cancelled, part, done] {
                                      if (!*cancelled)
                                          done(part, {});
                                  });
                                  return [cancelled, done] {
                                      *cancelled = true;
                                      done({}, { MediaChunkError::Cancelled, "cancelled" });
                                  };
                              });
        FileReader      reader(&download);
        QVERIFY(reader.open());
        QVideoSink   sink;
        QAudioOutput audio;
        Decoder      decoder(reader);
        audio.setVolume(0);
        decoder.player->setAudioOutput(&audio);
        decoder.player->setVideoSink(&sink);
        int    frames    = 0;
        qint64 frameTime = -1;
        connect(&sink, &QVideoSink::videoFrameChanged, &sink, [&](const QVideoFrame &frame) {
            if (frame.isValid()) {
                ++frames;
                frameTime = frame.startTime();
            }
        });
        decoder.player->setSourceDevice(&reader, QUrl("file:///movie.webm"));
        decoder.player->play();
        QTRY_VERIFY_WITH_TIMEOUT(frames > 2 || decoder.player->error() != QMediaPlayer::NoError, 10000);
        QVERIFY2(decoder.player->error() == QMediaPlayer::NoError, qPrintable(decoder.player->errorString()));
        QVERIFY(frames > 2);
        QVERIFY(decoder.player->duration() > 0);
        QVERIFY(decoder.player->isSeekable());
        QVERIFY(fetched < download.size() / 2);
        const int before = frames;
        decoder.player->setPosition(decoder.player->duration() / 2);
        QTRY_VERIFY_WITH_TIMEOUT(frames > before && frameTime >= decoder.player->duration() / 2 * 1000, 5000);
    }
    void abortWakesDecoderBeforeTeardown()
    {
        QTemporaryDir   directory;
        MediaChunkCache cache(directory.path());
        int             starts = 0;
        FileDownload    download(
            &cache, "movie", 20 * 1024 * 1024, "account",
            [&](quint64, quint64, FileDownload::FullDownloadAllowed, MediaChunkCompletion done) -> MediaChunkAbort {
                ++starts;
                return [done] { done({}, { MediaChunkError::Cancelled, "cancelled" }); };
            });
        FileReader reader(&download);
        QVERIFY(reader.open());
        auto decoder = std::make_unique<Decoder>(reader);
        decoder->player->setSourceDevice(&reader, QUrl("file:///movie.webm"));
        decoder->player->play();
        QTRY_VERIFY(starts > 0);
        QElapsedTimer timer;
        timer.start();
        decoder.reset();
        QVERIFY(timer.elapsed() < 2000);
    }
};
QTEST_MAIN(MediaDeviceTest)
#include "mediadevice.moc"

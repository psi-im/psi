/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filedownload.h"
#include "filereader.h"
#include "httputil.h"
#include "mediachunkcache.h"
#include "mediaplaybackpolicy.h"
#include <QAudioOutput>
#include <QElapsedTimer>
#include <QFile>
#include <QMediaPlayer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtTest>
#include <algorithm>
#include <memory>

namespace {
// A loopback fixture over the production reader and HTTP range parser. It
// exercises the adapter needed by GUI-thread GStreamer without a second fetcher.
class ReaderHttpServer : public QTcpServer {
public:
    explicit ReaderHttpServer(FileDownload *download)
    {
        connect(this, &QTcpServer::newConnection, this, [this, download] {
            auto socket = nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [socket, download, header = QByteArray()]() mutable {
                if (socket->property("started").toBool())
                    return;
                header += socket->readAll();
                if (!header.contains("\r\n\r\n"))
                    return;
                socket->setProperty("started", true);
                quint64 offset = 0, length = download->size();
                bool    ranged = false;
                for (const auto &line : header.split('\n')) {
                    if (!line.toLower().startsWith("range:"))
                        continue;
                    const auto [status, start, count] = Http::parseRangeHeader(line.mid(6).trimmed(), download->size());
                    if (status != Http::Parsed) {
                        socket->write(
                            "HTTP/1.1 416 Range Not Satisfiable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                        socket->disconnectFromHost();
                        return;
                    }
                    ranged = true;
                    offset = start;
                    length = count;
                }
                QByteArray response = ranged ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n";
                response += "Accept-Ranges: bytes\r\nContent-Type: video/webm\r\nConnection: close\r\nContent-Length: "
                    + QByteArray::number(length) + "\r\n";
                if (ranged)
                    response += "Content-Range: bytes " + QByteArray::number(offset) + "-"
                        + QByteArray::number(offset + length - 1) + "/" + QByteArray::number(download->size()) + "\r\n";
                socket->write(response + "\r\n");
                if (header.startsWith("HEAD ")) {
                    socket->disconnectFromHost();
                    return;
                }
                auto reader = new FileReader(download, offset, length, socket);
                if (!reader->open()) {
                    socket->abort();
                    return;
                }
                const auto send = [socket, reader] {
                    if (socket->state() != QAbstractSocket::ConnectedState)
                        return;
                    if (socket->bytesToWrite() >= 64 * 1024)
                        return;
                    if (reader->atEnd()) {
                        socket->disconnectFromHost();
                        return;
                    }
                    const auto bytes = reader->read(16 * 1024);
                    if (!bytes.isEmpty())
                        socket->write(bytes);
                };
                connect(reader, &QIODevice::readyRead, socket, send);
                connect(reader, &FileReader::failed, socket, [socket] { socket->abort(); });
                connect(socket, &QTcpSocket::bytesWritten, socket, [send](qint64) { send(); });
                connect(socket, &QTcpSocket::disconnected, reader, &FileReader::abort);
                send();
            });
        });
        if (!listen(QHostAddress::LocalHost))
            qFatal("Cannot start the loopback reader fixture");
    }
    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/movie.webm").arg(serverPort())); }
};
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
        // Extend the Segment with sparse EBML Void cells below GStreamer's
        // element-size limit. Cues retain their offsets; the fixture does not
        // allocate the virtual padding.
        QCOMPARE(bytes.mid(36, 4), QByteArray::fromHex("18538067"));
        const quint64 padding     = 32 * 1024 * 1024;
        const quint64 size        = quint64(bytes.size()) + padding;
        const quint64 segmentSize = (quint64(1) << 56) | (size - 48);
        for (int i = 0; i < 8; ++i)
            bytes[40 + i] = char(segmentSize >> (56 - i * 8));
        QByteArray        voidHeader(1, char(0xec));
        constexpr quint64 blockSize = 8 * 1024 * 1024;
        const quint64     voidSize  = (quint64(1) << 56) | (blockSize - 9);
        for (int i = 0; i < 8; ++i)
            voidHeader.append(char(voidSize >> (56 - i * 8)));
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
                                  for (quint64 start = quint64(bytes.size()); start < size; start += blockSize) {
                                      const auto begin = std::max(start, offset);
                                      const auto end = std::min(start + quint64(voidHeader.size()), offset + length);
                                      if (begin < end)
                                          std::copy_n(voidHeader.constData() + (begin - start), end - begin,
                                                         part.data() + (begin - offset));
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
        int              frames    = 0;
        qint64           frameTime = -1;
        QVideoSink       sink;
        QAudioOutput     audio;
        ReaderHttpServer http(&download);
        Decoder          decoder(reader);
        audio.setVolume(0);
        decoder.player->setAudioOutput(&audio);
        decoder.player->setVideoSink(&sink);
        connect(&sink, &QVideoSink::videoFrameChanged, &sink, [&](const QVideoFrame &frame) {
            if (frame.isValid()) {
                ++frames;
                frameTime = frame.startTime();
            }
        });
        if (MediaPlaybackPolicy::deviceStreaming())
            decoder.player->setSourceDevice(&reader, QUrl("file:///movie.webm"));
        else
            decoder.player->setSource(http.url());
        decoder.player->play();
        QTRY_VERIFY_WITH_TIMEOUT(frames > 2 || decoder.player->error() != QMediaPlayer::NoError, 10000);
        QVERIFY2(decoder.player->error() == QMediaPlayer::NoError, qPrintable(decoder.player->errorString()));
        QVERIFY(frames > 2);
        QVERIFY(decoder.player->duration() > 0);
        QVERIFY(decoder.player->isSeekable());
        if (MediaPlaybackPolicy::deviceStreaming())
            QVERIFY(fetched < download.size() / 2);
        const int before = frames;
        decoder.player->setPosition(decoder.player->duration() / 2);
        QTRY_VERIFY_WITH_TIMEOUT(frames > before && frameTime >= decoder.player->duration() / 2 * 1000, 5000);
    }
    void abortWakesDecoderBeforeTeardown()
    {
        QTemporaryDir   directory;
        MediaChunkCache cache(directory.path());
        int             starts = 0, aborts = 0;
        FileDownload    download(
            &cache, "movie", 20 * 1024 * 1024, "account",
            [&](quint64, quint64, FileDownload::FullDownloadAllowed, MediaChunkCompletion done) -> MediaChunkAbort {
                ++starts;
                return [&, done] {
                    ++aborts;
                    done({}, { MediaChunkError::Cancelled, "cancelled" });
                };
            });
        FileReader reader(&download);
        QVERIFY(reader.open());
        ReaderHttpServer http(&download);
        auto             decoder = std::make_unique<Decoder>(reader);
        if (MediaPlaybackPolicy::deviceStreaming())
            decoder->player->setSourceDevice(&reader, QUrl("file:///movie.webm"));
        else
            decoder->player->setSource(http.url());
        decoder->player->play();
        QTRY_VERIFY(starts > 0);
        QElapsedTimer timer;
        timer.start();
        decoder.reset();
        QVERIFY(timer.elapsed() < 2000);
        QTRY_COMPARE(aborts, starts);
    }
};
QTEST_MAIN(MediaDeviceTest)
#include "mediadevice.moc"

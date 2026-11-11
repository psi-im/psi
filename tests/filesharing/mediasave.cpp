/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filedownload.h"
#include "filereader.h"
#include "filesharingmediasave.h"
#include "mediachunkcache.h"
#include "qitemedia.h"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <functional>

class HookBuffer : public QBuffer {
public:
    std::function<void()> onRead;
    bool                  reading = false;
    ~HookBuffer() override { Q_ASSERT(!reading); }
    qint64 readData(char *data, qint64 count) override
    {
        reading           = true;
        const auto result = QBuffer::readData(data, count);
        if (onRead)
            onRead();
        reading = false;
        return result;
    }
};

class MediaSaveTest : public QObject {
    Q_OBJECT
    static QByteArray read(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    }
private slots:
    void asynchronousPreparationExportsThroughTheSameSaveJob()
    {
        QTemporaryDir directory;
        ITEMediaState state;
        const auto    path      = directory.filePath("saved");
        int           completed = 0;
        new FileSharingMediaSave(
            [](QObject *owner, FileSharingMediaSave::OpenReady ready) -> MediaChunkAbort {
                QTimer::singleShot(0, owner, [owner, ready] {
                    auto input = new QBuffer(owner);
                    input->setData("verified");
                    input->open(QIODevice::ReadOnly);
                    ready(input, 8, {});
                });
                return {};
            },
            path, &state, this,
            [&](const QString &result, const QString &error) {
                QVERIFY(error.isEmpty());
                QCOMPARE(result, path);
                ++completed;
            });
        QTRY_COMPARE(completed, 1);
        QCOMPARE(read(path), QByteArray("verified"));
    }

    void cancellingAsynchronousPreparationPreservesTheDestination()
    {
        QTemporaryDir directory;
        const auto    path = directory.filePath("existing");
        QFile         previous(path);
        QVERIFY(previous.open(QIODevice::WriteOnly));
        previous.write("keep");
        previous.close();
        ITEMediaState state;
        int           started = 0, cancelled = 0, completed = 0;
        auto          job = new FileSharingMediaSave(
            [&](QObject *, FileSharingMediaSave::OpenReady) -> MediaChunkAbort {
                ++started;
                return [&] { ++cancelled; };
            },
            path, &state, this,
            [&](const QString &result, const QString &error) {
                QVERIFY(result.isEmpty());
                QVERIFY(!error.isEmpty());
                ++completed;
            });
        QTRY_COMPARE(started, 1);
        job->cancel();
        QCOMPARE(cancelled, 1);
        QCOMPARE(completed, 1);
        QCOMPARE(read(path), QByteArray("keep"));
    }

    void explicitGoalExportsAfterCompletion()
    {
        QTemporaryDir                  directory;
        MediaChunkCache                cache(directory.path() + "/cache");
        const QByteArray               bytes(128 * 1024, 'v');
        FileDownload                   download(&cache, "movie", quint64(bytes.size()), "account",
                                                [bytes](quint64 offset, quint64 length, FileDownload::FullDownloadAllowed whole,
                                      MediaChunkCompletion done) -> MediaChunkAbort {
                                  if (!whole())
                                      done({}, QStringLiteral("Missing full goal"));
                                  else
                                      done(bytes.mid(int(offset), int(length)), {});
                                  return {};
                              });
        ITEMediaState                  state;
        int                            finished     = 0;
        qreal                          lastProgress = 0;
        const auto                     path         = directory.filePath("saved");
        QPointer<FileSharingMediaSave> job          = new FileSharingMediaSave(
            &download, path, &state, this,
            [&](const QString &result, const QString &error) {
                QVERIFY(error.isEmpty());
                QCOMPARE(result, path);
                ++finished;
            },
            [&](qreal progress) {
                QVERIFY(progress >= lastProgress);
                lastProgress = progress;
            });
        QTRY_COMPARE(finished, 1);
        QTRY_VERIFY(job.isNull());
        QCOMPARE(read(path), bytes);
        QVERIFY(download.state().complete);
        QVERIFY(!state.downloading());
    }
    void cancellingGoalKeepsPlayerDemandAndExistingDestination()
    {
        QTemporaryDir        directory;
        MediaChunkCache      cache(directory.path() + "/cache");
        MediaChunkCompletion complete;
        quint64              requested = 0;
        int                  starts = 0, aborts = 0;
        FileDownload         download(&cache, "movie", 128 * 1024, "account",
                                      [&](quint64, quint64 length, FileDownload::FullDownloadAllowed,
                                  MediaChunkCompletion done) -> MediaChunkAbort {
                                  ++starts;
                                  requested = length;
                                  complete  = std::move(done);
                                  return [&] { ++aborts; };
                              });
        FileReader           reader(&download);
        QVERIFY(reader.open());
        QVERIFY(reader.read(1).isEmpty());
        QTRY_COMPARE(starts, 1);
        const auto path = directory.filePath("existing");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("keep");
        }
        ITEMediaState                  state;
        int                            finished = 0;
        QPointer<FileSharingMediaSave> job
            = new FileSharingMediaSave(&download, path, &state, this, [&](const QString &result, const QString &error) {
                  QVERIFY(result.isEmpty());
                  QVERIFY(!error.isEmpty());
                  ++finished;
              });
        QTest::qWait(20);
        job->cancel();
        QCOMPARE(finished, 1);
        QCOMPARE(aborts, 0);
        complete(QByteArray(int(requested), 'x'), {});
        QTRY_COMPARE(reader.bytesAvailable(), qint64(1));
        QCOMPARE(reader.read(1), QByteArray("x"));
        QCOMPARE(read(path), QByteArray("keep"));
        QVERIFY(!state.downloading());
        QTRY_VERIFY(job.isNull());
    }
    void waitsForValidationAndCopiesInBoundedTurns()
    {
        QTemporaryDir dir(QDir::currentPath() + "/media-save-XXXXXX");
        QVERIFY(dir.isValid());
        const auto       path = dir.filePath("media.bin");
        ITEMediaState    state;
        auto             input = new HookBuffer;
        const QByteArray data(2 * 1024 * 1024, 'x');
        input->setData(data);
        QPointer<FileSharingMediaSave> job = new FileSharingMediaSave(input, data.size(), true, path, &state, this);
        QTRY_COMPARE(input->pos(), qint64(data.size()));
        QVERIFY(state.downloading());
        QVERIFY(!QFile::exists(path)); // Do not publish before transport/hash validation.
        job->validated();
        QTRY_VERIFY(job.isNull());
        QVERIFY(!state.downloading());
        QCOMPARE(state.downloadProgress(), qreal(1));
        QCOMPARE(read(path), data);
    }
    void validationInsideReadDoesNotDestroyStream()
    {
        QTemporaryDir dir(QDir::currentPath() + "/media-save-XXXXXX");
        ITEMediaState state;
        auto          input = new HookBuffer;
        input->setData("media data");
        QPointer<FileSharingMediaSave> job
            = new FileSharingMediaSave(input, 10, true, dir.filePath("out"), &state, this);
        input->onRead = [job] { job->validated(); };
        QTRY_VERIFY(job.isNull());
        QCOMPARE(read(dir.filePath("out")), QByteArray("media data"));
    }
    void failureInsideReadPreservesExistingFile()
    {
        QTemporaryDir dir(QDir::currentPath() + "/media-save-XXXXXX");
        const auto    path = dir.filePath("out");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("previous");
        }
        ITEMediaState state;
        auto          input = new HookBuffer;
        input->setData("bad data");
        QPointer<FileSharingMediaSave> job = new FileSharingMediaSave(input, 8, true, path, &state, this);
        input->onRead                      = [job] { job->fail("checksum failed"); };
        QTRY_VERIFY(job.isNull());
        QCOMPARE(read(path), QByteArray("previous"));
        QCOMPARE(state.errorString(), QString("checksum failed"));
        QVERIFY(!state.downloading());
    }
    void incompleteAndUnavailableDestination()
    {
        QTemporaryDir dir(QDir::currentPath() + "/media-save-XXXXXX");
        ITEMediaState state;
        auto          input = new QBuffer;
        input->setData("short");
        QPointer<FileSharingMediaSave> job
            = new FileSharingMediaSave(input, 100, false, dir.filePath("out"), &state, this);
        QTRY_VERIFY(job.isNull());
        QVERIFY(!state.errorString().isEmpty());
        QVERIFY(!QFile::exists(dir.filePath("out")));
        input = new QBuffer;
        input->setData("data");
        job = new FileSharingMediaSave(input, 4, false, dir.filePath("missing/out"), &state, this);
        QTRY_VERIFY(job.isNull());
        QVERIFY(!state.downloading());
        QVERIFY(!state.errorString().isEmpty());
    }
    void prematureDisconnectReleasesDownloadState()
    {
        QTemporaryDir dir(QDir::currentPath() + "/media-save-XXXXXX");
        ITEMediaState state;
        auto          input = new HookBuffer;
        input->setData("short");
        QPointer<FileSharingMediaSave> job
            = new FileSharingMediaSave(input, {}, true, dir.filePath("out"), &state, this);
        input->onRead = [job] { job->sourceClosed(); };
        QTRY_VERIFY(job.isNull());
        QVERIFY(!state.downloading());
        QVERIFY(!state.errorString().isEmpty());
        QVERIFY(!QFile::exists(dir.filePath("out")));
    }
};
QTEST_GUILESS_MAIN(MediaSaveTest)
#include "mediasave.moc"

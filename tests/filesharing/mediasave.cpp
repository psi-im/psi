/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingmediasave.h"
#include "qitemedia.h"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
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

/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingfilejob.h"
#include "iris/xmpp_file-sharing.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QObject>
#include <QTemporaryFile>
#include <QtConcurrentRun>

#include <QPointer>
#include <QSet>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

namespace FileSharingFileJob {
namespace {
    using Cancellation = std::shared_ptr<std::atomic<bool>>;
    class CancellableFile : public QFile {
    public:
        CancellableFile(const QString &path, Cancellation cancelled) : QFile(path), cancelled_(std::move(cancelled)) { }

    protected:
        qint64 readData(char *data, qint64 size) override
        {
            return cancelled_->load() ? -1 : QFile::readData(data, size);
        }

    private:
        Cancellation cancelled_;
    };
    template <class Result> QFutureWatcher<Result> *watch(QObject *receiver, const Cancellation &cancelled)
    {
        auto watcher = new QFutureWatcher<Result>(QCoreApplication::instance());
        QObject::connect(receiver, &QObject::destroyed, watcher, [cancelled] { cancelled->store(true); });
        // QCA must outlive authentication workers. Cancellation interrupts their
        // next bounded read; only application shutdown waits for them to stop.
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, watcher, [watcher, cancelled] {
            cancelled->store(true);
            watcher->waitForFinished();
        });
        return watcher;
    }
    struct TemporaryFile {
        explicit TemporaryFile(QString fileName) : path(std::move(fileName)) { }
        TemporaryFile(const TemporaryFile &)            = delete;
        TemporaryFile &operator=(const TemporaryFile &) = delete;
        QString        path;
        ~TemporaryFile() { QFile::remove(path); }
    };
}

void sha256TemporaryFile(const QString &path, QObject *receiver, Completion complete)
{
    Q_ASSERT(receiver);
    // The callback and the worker share the cleanup token: deleting the
    // receiver while a multi-gigabyte hash is running cannot delete the file
    // underneath that worker. Once the callback moves it to FileCache,
    // removing the original temporary path is harmless.
    auto  temporary = std::make_shared<TemporaryFile>(path);
    auto *watcher   = new QFutureWatcher<XMPP::Hash>(receiver);
    QObject::connect(watcher, &QFutureWatcher<XMPP::Hash>::finished, receiver,
                     [watcher, temporary, complete = std::move(complete)] {
                         const XMPP::Hash hash = watcher->result();
                         watcher->deleteLater();
                         complete(hash);
                     });
    watcher->setFuture(QtConcurrent::run([temporary] {
        // Use the same QIODevice streaming primitive as MediaChunkStore's
        // worker. Qt's hasher has no QCA lifetime dependency during shutdown,
        // and addData(QIODevice*) does not buffer the whole payload in RAM.
        QFile              file(temporary->path);
        QCryptographicHash digest(QCryptographicHash::Sha256);
        if (!file.open(QIODevice::ReadOnly) || !digest.addData(&file) || file.error() != QFile::NoError)
            return XMPP::Hash();
        return XMPP::Hash(XMPP::Hash::Sha256, digest.result());
    }));
}
void decryptTemporaryFile(const QString &path, const QByteArray &key, const QByteArray &iv, QObject *receiver,
                          std::function<void(const QString &, MediaChunkError)> complete)
{
    struct Result {
        std::shared_ptr<TemporaryFile> plaintext;
        MediaChunkError                error;
    };
    auto                    ciphertext = std::make_shared<TemporaryFile>(path);
    auto                    cancelled  = std::make_shared<std::atomic<bool>>(false);
    auto                    watcher    = watch<Result>(receiver, cancelled);
    const QPointer<QObject> guard(receiver);
    QObject::connect(watcher, &QFutureWatcher<Result>::finished, watcher,
                     [watcher, guard, complete = std::move(complete)] {
                         const auto result = watcher->result();
                         watcher->deleteLater();
                         if (guard)
                             complete(result.plaintext ? result.plaintext->path : QString(), result.error);
                     });
    watcher->setFuture(QtConcurrent::run([ciphertext, key, iv, cancelled] {
        CancellableFile input(ciphertext->path, cancelled);
        QTemporaryFile  output(QDir::tempPath() + QStringLiteral("/psi-media-plain-XXXXXX"));
        if (!input.open(QIODevice::ReadOnly) || !output.open())
            return Result { {}, { MediaChunkError::StorageUnavailable, QObject::tr("Cannot open encrypted media") } };
        if (!XMPP::StatelessFileSharing::decryptToDevice(XMPP::StatelessFileSharing::Cipher::Aes256Gcm, &input, &output,
                                                         key, iv))
            return Result { {},
                            { (input.error() != QFile::NoError || output.error() != QFile::NoError)
                                  ? MediaChunkError::StorageUnavailable
                                  : MediaChunkError::IntegrityError,
                              QObject::tr("Media authentication failed") } };
        if (!output.flush())
            return Result { {}, { MediaChunkError::StorageUnavailable, output.errorString() } };
        output.setAutoRemove(false);
        return Result { std::make_shared<TemporaryFile>(output.fileName()), {} };
    }));
}
void verifyTemporaryFile(const QString &path, const QList<XMPP::Hash> &expected, QObject *receiver,
                         std::function<void(QList<XMPP::Hash>, MediaChunkError)> complete)
{
    struct Result {
        QList<XMPP::Hash> hashes;
        MediaChunkError   error;
    };
    auto                    temporary = std::make_shared<TemporaryFile>(path);
    auto                    cancelled = std::make_shared<std::atomic<bool>>(false);
    auto                    watcher   = watch<Result>(receiver, cancelled);
    const QPointer<QObject> guard(receiver);
    QObject::connect(watcher, &QFutureWatcher<Result>::finished, watcher,
                     [watcher, guard, temporary, complete = std::move(complete)] {
                         const auto result = watcher->result();
                         watcher->deleteLater();
                         if (guard)
                             complete(result.hashes, result.error);
                     });
    watcher->setFuture(QtConcurrent::run([temporary, expected, cancelled] {
        CancellableFile input(temporary->path, cancelled);
        if (!input.open(QIODevice::ReadOnly))
            return Result { {}, input.errorString() };
        QSet<int> algorithms { int(XMPP::Hash::Sha256) };
        for (const auto &h : expected)
            algorithms.insert(int(h.type()));
        std::vector<std::unique_ptr<XMPP::StreamHash>> hashes;
        for (const auto type : algorithms)
            hashes.push_back(std::make_unique<XMPP::StreamHash>(XMPP::Hash::Type(type)));
        while (!input.atEnd()) {
            const auto bytes = input.read(64 * 1024);
            if (bytes.isEmpty())
                return Result { {}, input.errorString() };
            for (const auto &h : hashes)
                if (!h->addData(bytes))
                    return Result { {},
                                    { MediaChunkError::IntegrityError, QObject::tr("Cannot verify media checksum") } };
        }
        Result result;
        for (const auto &h : hashes) {
            const auto hash = h->final();
            if (!hash.isValid() || hash.data().isEmpty())
                return Result { {}, { MediaChunkError::IntegrityError, QObject::tr("Cannot verify media checksum") } };
            result.hashes.append(hash);
        }
        for (const auto &claim : expected)
            if (!result.hashes.contains(claim))
                return Result { {}, { MediaChunkError::IntegrityError, QObject::tr("Media checksum mismatch") } };
        return result;
    }));
}
} // namespace FileSharingFileJob

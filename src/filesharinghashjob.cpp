/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharinghashjob.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFutureWatcher>
#include <QObject>
#include <QtConcurrentRun>

#include <memory>
#include <utility>

namespace FileSharingHashJob {
namespace {
struct TemporaryFile {
    explicit TemporaryFile(QString fileName) : path(std::move(fileName)) { }
    TemporaryFile(const TemporaryFile &) = delete;
    TemporaryFile &operator=(const TemporaryFile &) = delete;
    QString path;
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
    auto temporary = std::make_shared<TemporaryFile>(path);
    auto *watcher = new QFutureWatcher<XMPP::Hash>(receiver);
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
        QFile file(temporary->path);
        QCryptographicHash digest(QCryptographicHash::Sha256);
        if (!file.open(QIODevice::ReadOnly) || !digest.addData(&file)
            || file.error() != QFile::NoError)
            return XMPP::Hash();
        return XMPP::Hash(XMPP::Hash::Sha256, digest.result());
    }));
}
} // namespace FileSharingHashJob

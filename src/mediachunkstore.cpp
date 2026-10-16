#include "mediachunkstore.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QThread>
#include <utility>

namespace {
const QByteArray magic("PSICHNK1");
constexpr int    headerSize = 8 + 32 + 32;
QByteArray       digest(const QByteArray &bytes) { return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256); }

class DiskWorker : public QObject {
    Q_OBJECT
public:
    DiskWorker(QString directory, quint64 limit) : directory_(std::move(directory)), diskLimit_(limit) { }
    void initialize()
    {
        QDir dir(directory_);
        dir.mkpath(QStringLiteral("."));
        const QRegularExpression name(QStringLiteral("^[0-9a-f]{64}\\.chunk$"));
        const auto files = dir.entryInfoList({ QStringLiteral("*.chunk") }, QDir::Files | QDir::NoSymLinks,
                                             QDir::Time | QDir::Reversed);
        for (const auto &file : files) {
            if (!name.match(file.fileName()).hasMatch())
                continue;
            const auto key = file.completeBaseName();
            entries_.insert(key, { quint64(file.size()), ++age_ });
            emit entryChanged(key, true);
            diskBytes_ += quint64(file.size());
        }
        trim(0);
    }
    void read(quint64 id, const QString &key, quint64 expected)
    {
        auto bytes = load(key, expected);
        emit completed(id, std::move(bytes), diskBytes_);
    }
    void write(quint64 id, const QString &key, const QByteArray &bytes)
    {
        store(key, bytes);
        emit completed(id, {}, diskBytes_);
    }
    void touch(const QString &key)
    {
        auto it = entries_.find(key);
        if (it != entries_.end())
            it->age = ++age_;
    }
signals:
    void completed(quint64 id, QByteArray bytes, quint64 diskBytes);
    void entryChanged(const QString &key, bool available);

private:
    struct Entry {
        quint64 size, age;
    };
    void                  discard(const QString &key);
    bool                  trim(quint64 incoming);
    QByteArray            load(const QString &key, quint64 expected);
    void                  store(const QString &key, const QByteArray &bytes);
    QString               directory_;
    quint64               diskLimit_, diskBytes_ = 0, age_ = 0;
    QHash<QString, Entry> entries_;
};
void DiskWorker::discard(const QString &key)
{
    const auto it = entries_.find(key);
    if (it == entries_.end())
        return;
    const QString path = directory_ + '/' + key + QStringLiteral(".chunk");
    if (QFile::remove(path) || !QFileInfo::exists(path)) {
        diskBytes_ -= it->size;
        // trim() passes oldest.key() by reference. Own it before erasing the
        // hash entry; queued availability notifications must copy a live key.
        const auto removedKey = it.key();
        entries_.erase(it);
        emit entryChanged(removedKey, false);
    }
}

bool DiskWorker::trim(quint64 incoming)
{
    if (incoming > diskLimit_)
        return false;
    while ((diskBytes_ > diskLimit_ - incoming || entries_.size() + (incoming ? 1 : 0) > 2048) && !entries_.isEmpty()) {
        auto oldest = entries_.begin();
        for (auto it = entries_.begin(); it != entries_.end(); ++it)
            if (it->age < oldest->age)
                oldest = it;
        const auto before = entries_.size();
        discard(oldest.key());
        if (entries_.size() == before)
            return false; // A read-only cache must not exceed its disk budget.
    }
    return true;
}

QByteArray DiskWorker::load(const QString &key, quint64 expected)
{
    if (!entries_.contains(key))
        return {};
    QFile file(directory_ + '/' + key + QStringLiteral(".chunk"));
    if (file.open(QIODevice::ReadOnly) && quint64(file.size()) == headerSize + expected) {
        const auto header = file.read(headerSize);
        const auto bytes  = file.read(qint64(expected));
        if (header.size() == headerSize && header.startsWith(magic)
            && header.mid(8, 32) == QByteArray::fromHex(key.toLatin1()) && quint64(bytes.size()) == expected
            && header.mid(40, 32) == digest(bytes)) {
            entries_[key].age = ++age_;
            return bytes;
        }
    }
    file.close();
    discard(key); // Truncation or local corruption is a cache miss, never valid data.
    return {};
}

void DiskWorker::store(const QString &key, const QByteArray &bytes)
{
    discard(key);
    if (!entries_.contains(key) && trim(headerSize + quint64(bytes.size()))) {
        QSaveFile  file(directory_ + '/' + key + QStringLiteral(".chunk"));
        const auto header = magic + QByteArray::fromHex(key.toLatin1()) + digest(bytes);
        if (file.open(QIODevice::WriteOnly) && file.write(header) == header.size() && file.write(bytes) == bytes.size()
            && file.commit()) {
            const auto size = quint64(headerSize + bytes.size());
            entries_.insert(key, { size, ++age_ });
            diskBytes_ += size;
            emit entryChanged(key, true);
        }
    }
}

}

class MediaChunkStore::Private {
public:
    QThread                    thread;
    DiskWorker                *worker;
    quint64                    next = 0, diskBytes = 0;
    QHash<quint64, Completion> pending;
    QSet<QString>              keys;
};

MediaChunkStore::MediaChunkStore(QString directory, quint64 diskLimit, QObject *parent) :
    QObject(parent), d(new Private)
{
    d->worker = new DiskWorker(std::move(directory), diskLimit);
    d->thread.setObjectName(QStringLiteral("shared-media-cache"));
    d->worker->moveToThread(&d->thread);
    connect(&d->thread, &QThread::finished, d->worker, &QObject::deleteLater);
    connect(d->worker, &DiskWorker::entryChanged, this, [this](const QString &key, bool available) {
        if (available)
            d->keys.insert(key);
        else
            d->keys.remove(key);
        emit availabilityChanged();
    });
    connect(d->worker, &DiskWorker::completed, this, [this](quint64 id, QByteArray bytes, quint64 size) {
        d->diskBytes = size;
        auto done    = d->pending.take(id);
        if (done)
            done(std::move(bytes));
    });
    d->thread.start();
    QMetaObject::invokeMethod(d->worker, [worker = d->worker] { worker->initialize(); }, Qt::QueuedConnection);
}

MediaChunkStore::~MediaChunkStore()
{
    // Blocking invocation is only used at teardown, never on the read path.
    QMetaObject::invokeMethod(d->worker, [] { }, Qt::BlockingQueuedConnection);
    d->thread.quit();
    d->thread.wait();
}

quint64 MediaChunkStore::diskBytes() const { return d->diskBytes; }
bool    MediaChunkStore::contains(const QString &key) const { return d->keys.contains(key); }
void    MediaChunkStore::touch(const QString &key)
{
    QMetaObject::invokeMethod(d->worker, [worker = d->worker, key] { worker->touch(key); }, Qt::QueuedConnection);
}

void MediaChunkStore::read(const QString &key, quint64 expected, Completion done)
{
    const auto id = ++d->next;
    d->pending.insert(id, std::move(done));
    QMetaObject::invokeMethod(
        d->worker, [worker = d->worker, id, key, expected] { worker->read(id, key, expected); }, Qt::QueuedConnection);
}

void MediaChunkStore::write(const QString &key, QByteArray bytes, Completion done)
{
    const auto id = ++d->next;
    d->pending.insert(id, std::move(done));
    QMetaObject::invokeMethod(
        d->worker, [worker = d->worker, id, key, bytes = std::move(bytes)] { worker->write(id, key, bytes); },
        Qt::QueuedConnection);
}

#include "mediachunkstore.moc"

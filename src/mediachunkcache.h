#pragma once

#include "mediachunkscheduler.h"
#include "mediachunkstore.h"

#include <QHash>
#include <QPointer>
#include <QSet>

// Shared payload and range metadata. A source identity must include account,
// immutable content identity and size. Fetchers validate ranges before completion.
class MediaChunkCache : public QObject {
    Q_OBJECT
public:
    static constexpr quint64 MaxChunkSize     = MediaChunkMaxSize;
    static constexpr int     MaxRequests      = 256;
    static constexpr int     MaxActiveFetches = 4;
    using Completion                          = MediaChunkScheduler::Completion;
    using Abort                               = MediaChunkScheduler::Abort;
    using Fetch                               = std::function<Abort(quint64 offset, quint64 length, Completion)>;

    explicit MediaChunkCache(QString directory, QObject *parent = nullptr, quint64 chunkSize = 4 * 1024 * 1024,
                             quint64 diskLimit = 256 * 1024 * 1024, MediaChunkStore::Hasher hasher = {});
    explicit MediaChunkCache(MediaChunkStore *store, QObject *parent = nullptr, quint64 chunkSize = 4 * 1024 * 1024);
    ~MediaChunkCache() override;

    // Return at most length bytes, bounded by the current chunk and EOF.
    // Cache hits also complete asynchronously. Each reader owns its own ticket.
    // Zero rejects admission without retaining a callback; the caller handles overload.
    quint64 read(QString source, quint64 fileSize, quint64 offset, quint64 length, QString lane, Fetch fetch,
                 Completion done, bool urgent = true, quint64 quantum = 0);
    quint64 ensure(QString source, quint64 size, quint64 offset, quint64 length, QString lane, Fetch fetch,
                   Completion done, quint64 quantum = 0);
    void    checkpoint(Completion done = {});
    bool    cancel(quint64 ticket);
    void    shutdown();
    quint64 diskBytes() const;
    quint64 chunkSize() const { return chunkSize_; }
    // Available ranges. Durable checkpoints are coalesced and restored after restart.
    // Hashes are verified when their complete span becomes available.
    using Ranges = QList<QPair<quint64, quint64>>; // offset, length
    Ranges         cachedRanges(const QString &source, quint64 fileSize) const;
    void           configure(const QString &source, quint64 fileSize, QList<MediaFileChecksum> checksums,
                             QVariantMap metadata = {});
    MediaFileState fileState(const QString &source, quint64 fileSize) const;
    quint64        availableLength(const QString &source, quint64 size, quint64 offset, quint64 maximum) const;
    QString        resolveSource(const QString &source, const QString &account, quint64 size,
                                 const QList<MediaFileChecksum> &checksums) const;
    QString        localFile(const QString &source, quint64 size) const;
    bool           isCompleteFile(const QString &path) const;
    void           resume(const QString &source, quint64 fileSize);
    void           pin(const QString &source, quint64 fileSize, bool active);
    void invalidate(const QString &source, quint64 fileSize, quint64 offset, quint64 length, const QString &message);
    void storeBytes(const QString &source, quint64 fileSize, quint64 offset, QByteArray bytes, Completion done);
    void readStored(const QString &source, quint64 fileSize, quint64 offset, quint64 length, Completion done);

signals:
    void rangesChanged(const QString &source, quint64 fileSize);
    void integrityError(const QString &source, quint64 fileSize, const QString &message);

private:
    struct Request {
        quint64    schedulerTicket = 0;
        Completion done;
        QString    key, file, source, lane;
        quint64    size = 0, start = 0, expected = 0, inside = 0, count = 0;
        Fetch      fetch;
        bool       urgent             = true;
        bool       payload            = true;
        bool       storageReadPending = false;
    };
    QString recordKey(const QString &source, quint64 size, quint64 start, quint64 length) const;
    quint64 request(QString source, quint64 size, quint64 offset, quint64 length, QString lane, Fetch fetch,
                    Completion done, bool payload, bool urgent, quint64 quantum);
    QString fileKey(const QString &source, quint64 size) const;
    Abort   fetchChunk(const QString &file, quint64 size, quint64 start, quint64 expected, Fetch fetch,
                       Completion complete);
    void    deliver(quint64 ticket, QByteArray bytes, MediaChunkError error);
    void    schedule(quint64 ticket);
    void    readAvailable(quint64 ticket);
    void    notifyRanges(const QString &source, quint64 size);
    QHash<QString, QPair<QString, quint64>> files_;
    QSet<QString>                           resuming_;
    QSet<QPair<QString, quint64>>           pendingRanges_;
    bool                                    rangesPending_ = false;

    quint64                 chunkSize_, nextTicket_ = 0;
    bool                    stopped_ = false;
    MediaChunkScheduler     scheduler_;
    QHash<quint64, Request> requests_;
    MediaChunkStore        *store_;
};

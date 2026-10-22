#pragma once

#include "mediachunkscheduler.h"
#include "mediachunkstore.h"

#include <QCache>
#include <QHash>
#include <QIODevice>
#include <QPointer>
#include <QSet>
#include <memory>
#include <optional>

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
                             quint64 diskLimit = 256 * 1024 * 1024, MediaChunkStore::Hasher hasher = { });
    ~MediaChunkCache() override;

    // Return at most length bytes, bounded by the current chunk and EOF.
    // Cache hits also complete asynchronously. Each reader owns its own ticket.
    // Zero rejects admission without retaining a callback; the caller handles overload.
    quint64 read(QString source, quint64 fileSize, quint64 offset, quint64 length, QString lane, Fetch fetch,
                 Completion done, bool urgent = true);
    bool    cancel(quint64 ticket);
    void    shutdown();
    quint64 diskBytes() const;
    quint64 chunkSize() const { return chunkSize_; }
    // Persisted downloaded ranges, including bytes restored after a restart.
    // Hashes are verified when their complete span becomes available.
    using Ranges = QList<QPair<quint64, quint64>>; // offset, length
    Ranges         cachedRanges(const QString &source, quint64 fileSize) const;
    void           configure(const QString &source, quint64 fileSize, QList<MediaFileChecksum> checksums,
                             QVariantMap metadata = { });
    MediaFileState fileState(const QString &source, quint64 fileSize) const;
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
        quint64    schedulerTicket;
        Completion done;
        QByteArray cachedSlice;
    };
    QString recordKey(const QString &source, quint64 size, quint64 index) const;
    QString fileKey(const QString &source, quint64 size) const;
    Abort   fetchChunk(const QString &key, const QString &file, quint64 size, quint64 start, quint64 expected,
                       Fetch fetch, Completion complete);
    void    deliver(quint64 ticket, QByteArray bytes, MediaChunkError error);
    void    remember(const QString &key, const QString &source, quint64 size, quint64 index);
    void    refreshAvailability();
    void    notifyRanges(const QString &source, quint64 size);
    struct Record {
        QString source;
        quint64 size, index;
    };
    QHash<QString, Record>                  records_;
    QHash<QString, QPair<QString, quint64>> files_;
    QSet<QString>                           resuming_;
    QSet<QPair<QString, quint64>>           pendingRanges_;
    bool                                    rangesPending_ = false;

    quint64                          chunkSize_, nextTicket_ = 0;
    bool                             stopped_ = false;
    MediaChunkScheduler              scheduler_;
    QCache<QString, QByteArray>      memory_;
    QHash<quint64, Request>          requests_;
    std::unique_ptr<MediaChunkStore> store_;
};

// A 64 KiB consumer buffer and a bounded window of lookahead within the
// requested range. Network work never blocks readData().
class MediaChunkReader : public QIODevice {
    Q_OBJECT
public:
    MediaChunkReader(MediaChunkCache *cache, QString source, quint64 fileSize, quint64 offset, quint64 length,
                     QString lane, MediaChunkCache::Fetch fetch, QObject *parent = nullptr);
    ~MediaChunkReader() override;
    bool                  open(OpenMode mode = ReadOnly) override;
    void                  abort();
    bool                  isSequential() const override { return true; }
    qint64                bytesAvailable() const override;
    bool                  isConnected() const;
    MediaChunkError::Code errorCode() const { return errorCode_; }

signals:
    void failed();
    void disconnected();

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    void                      schedule();
    void                      prefetch();
    QPointer<MediaChunkCache> cache_;
    QString                   source_, lane_;
    quint64                   size_, offset_, left_, ticket_ = 0;
    QHash<quint64, quint64>   prefetchTickets_; // offset -> ticket, zero after completion
    MediaChunkCache::Fetch    fetch_;
    QByteArray                buffer_;
    bool                      stopped_ = false, scheduled_ = false, first_ = true, ended_ = false, pinned_ = false;
    MediaChunkError::Code     errorCode_ = MediaChunkError::None;
};

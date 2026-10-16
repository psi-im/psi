#pragma once

#include "mediachunkscheduler.h"

#include <QCache>
#include <QHash>
#include <QIODevice>
#include <QPointer>
#include <QSet>
#include <memory>
#include <optional>

class MediaChunkStore;

// Plaintext, disposable range cache. A source identity must include account,
// immutable content identity and size. Fetchers validate ranges before completion.
class MediaChunkCache : public QObject {
    Q_OBJECT
public:
    static constexpr quint64 MaxChunkSize = MediaChunkMaxSize;
    static constexpr int     MaxRequests  = 256;
    using Completion                      = MediaChunkScheduler::Completion;
    using Abort                           = MediaChunkScheduler::Abort;
    using Fetch                           = std::function<Abort(quint64 offset, quint64 length, Completion)>;

    explicit MediaChunkCache(QString directory, QObject *parent = nullptr, quint64 chunkSize = 4 * 1024 * 1024,
                             quint64 diskLimit = 256 * 1024 * 1024);
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
    // Verified chunks observed in this process, including disk cache hits.
    // Byte ranges shrink on eviction; this is not a persistent file manifest.
    using Ranges = QList<QPair<quint64, quint64>>; // offset, length
    Ranges cachedRanges(const QString &source, quint64 fileSize) const;

signals:
    void rangesChanged(const QString &source, quint64 fileSize);

private:
    struct Request {
        quint64    schedulerTicket;
        Completion done;
        QByteArray cachedSlice;
    };
    QString recordKey(const QString &source, quint64 size, quint64 index) const;
    Abort   fetchChunk(const QString &key, quint64 start, quint64 expected, Fetch fetch, Completion complete);
    void    deliver(quint64 ticket, QByteArray bytes, MediaChunkError error);
    void    remember(const QString &key, const QString &source, quint64 size, quint64 index);
    void    refreshAvailability();
    void    notifyRanges(const QString &source, quint64 size);
    struct Record {
        QString source;
        quint64 size, index;
    };
    QHash<QString, Record>        records_;
    QSet<QPair<QString, quint64>> pendingRanges_;
    bool                          rangesPending_ = false;

    quint64                          chunkSize_, nextTicket_ = 0;
    bool                             stopped_ = false;
    MediaChunkScheduler              scheduler_;
    QCache<QString, QByteArray>      memory_;
    QHash<quint64, Request>          requests_;
    std::unique_ptr<MediaChunkStore> store_;
};

// A 64 KiB consumer buffer and at most one chunk of lookahead within the
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
    quint64                   prefetchTicket_ = 0;
    std::optional<quint64>    prefetchOffset_;
    MediaChunkCache::Fetch    fetch_;
    QByteArray                buffer_;
    bool                      stopped_ = false, scheduled_ = false, first_ = true, ended_ = false;
    MediaChunkError::Code     errorCode_ = MediaChunkError::None;
};

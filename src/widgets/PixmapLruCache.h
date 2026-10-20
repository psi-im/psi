/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QTimer>

#include <functional>
#include <list>

namespace Psi {

// A GUI-thread-only, bounded cache for temporary rasterized QPixmaps.
// Like MatterLeast's SvgRasterCache, keys are caller-defined stable identities.
// Include requested size, device pixel ratio and content revision in keys when
// those affect the bitmap. QPixmap has implicit sharing; the cache never takes
// ownership of the source QImage, nor extends the lifetime of returned copies.
class PixmapLruCache final {
public:
    static PixmapLruCache &instance()
    {
        static PixmapLruCache cache;
        return cache;
    }

    QPixmap getOrCreate(const QString &key, const std::function<QPixmap()> &factory)
    {
        if (key.isEmpty())
            return factory();
        const qint64 now = clockNow();
        auto it = entries_.find(key);
        if (it != entries_.end()) {
            if (now - it->lastUsedMs < idleTimeoutMs_) {
                it->lastUsedMs = now;
                lru_.splice(lru_.begin(), lru_, it->lruPosition);
                return it->pixmap;
            }
            remove(it);
        }
        QPixmap pixmap = factory();
        if (pixmap.isNull() || !maxBytes_)
            return pixmap;
        const qint64 bytes = static_cast<qint64>(pixmap.width()) * pixmap.height()
            * qMax(1, pixmap.depth() / 8);
        if (bytes > maxBytes_)
            return pixmap;
        lru_.push_front(key);
        Entry entry;
        entry.pixmap = pixmap;
        entry.bytes = bytes;
        entry.lastUsedMs = now;
        entry.lruPosition = lru_.begin();
        entries_.insert(key, entry);
        usedBytes_ += bytes;
        evictToBudget();
        return pixmap;
    }

    // The source QImage remains in its owner's resource store. Include the
    // QImage revision/cacheKey, so a replaced image with the same logical ID
    // does not return stale pixels. No pixmap survives only because a document
    // still holds an image resource.
    QPixmap fromImage(const QString &identity, const QImage &image)
    {
        if (image.isNull())
            return {};
        const QString key = identity + QLatin1Char('|')
            + QString::number(static_cast<qulonglong>(image.cacheKey()));
        return getOrCreate(key, [&image] { return QPixmap::fromImage(image); });
    }

    void setIdleTimeoutMs(qint64 ms) { idleTimeoutMs_ = qMax<qint64>(1, ms); pruneStale(); }
    qint64 idleTimeoutMs() const { return idleTimeoutMs_; }

    void setMaxBytes(qint64 bytes) { maxBytes_ = qMax<qint64>(0, bytes); evictToBudget(); }
    qint64 maxBytes() const { return maxBytes_; }
    qint64 usedBytes() const { return usedBytes_; }
    int entryCount() const { return entries_.size(); }

    void setCleanupIntervalMs(int ms)
    {
        cleanupTimer_.setInterval(qMax(1, ms));
    }
    int cleanupIntervalMs() const { return cleanupTimer_.interval(); }

    void pruneStale()
    {
        const qint64 now = clockNow();
        // Items are ordered by LRU access, so stale items form a suffix.
        while (!lru_.empty()) {
            const auto it = entries_.find(lru_.back());
            if (it == entries_.end()) {
                lru_.pop_back();
                continue;
            }
            if (now - it->lastUsedMs < idleTimeoutMs_)
                break;
            remove(it);
        }
    }

    void clear()
    {
        entries_.clear();
        lru_.clear();
        usedBytes_ = 0;
    }

private:
    struct Entry {
        QPixmap pixmap;
        qint64 bytes = 0;
        qint64 lastUsedMs = 0;
        std::list<QString>::iterator lruPosition;
    };

    PixmapLruCache()
    {
        clock_.start();
        cleanupTimer_.setInterval(10000);
        cleanupTimer_.setTimerType(Qt::VeryCoarseTimer);
        QObject::connect(&cleanupTimer_, &QTimer::timeout, [] {
            PixmapLruCache::instance().pruneStale();
        });
        cleanupTimer_.start();
    }

    qint64 clockNow() const { return clock_.elapsed(); }

    void remove(QHash<QString, Entry>::iterator it)
    {
        usedBytes_ -= it->bytes;
        lru_.erase(it->lruPosition);
        entries_.erase(it);
    }

    void evictToBudget()
    {
        while (usedBytes_ > maxBytes_ && !lru_.empty()) {
            const auto it = entries_.find(lru_.back());
            if (it == entries_.end()) {
                lru_.pop_back();
                continue;
            }
            remove(it);
        }
    }

    QHash<QString, Entry> entries_;
    std::list<QString> lru_;
    QElapsedTimer clock_;
    QTimer cleanupTimer_;
    qint64 usedBytes_ = 0;
    qint64 maxBytes_ = 32LL * 1024 * 1024;
    qint64 idleTimeoutMs_ = 60LL * 1000;
};

} // namespace Psi

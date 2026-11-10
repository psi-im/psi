/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <iterator>
#include <limits>
#include <list>
#include <optional>
#include <utility>

// Owner-thread RAM tier. A generation is a file record ID, never a content claim
// or a reusable source name. Consumer buffers and uncommitted writes are separate.
class FileCacheMemory {
    struct Span {
        QByteArray generation;
        quint64    offset = 0, length = 0;
        bool       operator==(const Span &other) const
        {
            return generation == other.generation && offset == other.offset && length == other.length;
        }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        friend uint qHash(const Span &span, uint seed = 0)
#else
        friend size_t qHash(const Span &span, size_t seed = 0)
#endif
        {
            return qHash(span.generation, seed) ^ qHash(span.offset, seed + 1) ^ qHash(span.length, seed + 2);
        }
    };
    struct Entry {
        QByteArray                bytes;
        std::list<Span>::iterator age;
    };

public:
    static constexpr int MaxEntries = 8192;
    explicit FileCacheMemory(quint64 limit) : limit_(limit) { }
    FileCacheMemory(const FileCacheMemory &)            = delete;
    FileCacheMemory &operator=(const FileCacheMemory &) = delete;
    quint64          limit() const { return limit_; }
    quint64          bytes() const { return bytes_; }
    qsizetype        count() const { return entries_.size(); }
    void             setLimit(quint64 limit)
    {
        limit_ = limit;
        trim();
    }
    bool contains(const QByteArray &generation, quint64 offset, quint64 length) const
    {
        return bool(find(generation, offset, length));
    }
    std::optional<QByteArray> get(const QByteArray &generation, quint64 offset, quint64 length)
    {
        const auto span = find(generation, offset, length);
        if (!span)
            return std::nullopt;
        auto it = entries_.find(*span);
        order_.splice(order_.end(), order_, it->age);
        return it->bytes.mid(qsizetype(offset - span->offset), qsizetype(length));
    }
    void put(const QByteArray &generation, quint64 offset, QByteArray bytes)
    {
        const auto length = quint64(bytes.size());
        if (generation.isEmpty() || !length || length > std::numeric_limits<quint64>::max() - offset)
            return;
        remove(generation, offset, length);
        if (length > limit_)
            return;
        const Span span { generation, offset, length };
        order_.push_back(span);
        entries_.insert(span, { std::move(bytes), std::prev(order_.end()) });
        generations_[generation].insert(span);
        bytes_ += length;
        trim();
    }
    void remove(const QByteArray &generation)
    {
        const auto spans = generations_.value(generation);
        for (const auto &span : spans)
            erase(span);
    }
    void remove(const QByteArray &generation, quint64 offset, quint64 length)
    {
        if (!length)
            return;
        const auto spans = generations_.value(generation);
        for (const auto &span : spans)
            if (span.offset <= offset ? offset - span.offset < span.length : span.offset - offset < length)
                erase(span);
    }

private:
    std::optional<Span> find(const QByteArray &generation, quint64 offset, quint64 length) const
    {
        if (!length)
            return std::nullopt;
        const Span exact { generation, offset, length };
        if (entries_.contains(exact))
            return exact;
        const auto spans = generations_.constFind(generation);
        if (spans == generations_.cend())
            return std::nullopt;
        for (const auto &span : *spans)
            if (span.offset <= offset && offset - span.offset <= span.length
                && length <= span.length - (offset - span.offset))
                return span;
        return std::nullopt;
    }
    void erase(const Span &span)
    {
        auto it = entries_.find(span);
        if (it == entries_.end())
            return;
        bytes_ -= span.length;
        order_.erase(it->age);
        entries_.erase(it);
        auto generation = generations_.find(span.generation);
        generation->remove(span);
        if (generation->isEmpty())
            generations_.erase(generation);
    }
    void trim()
    {
        while ((bytes_ > limit_ || entries_.size() > MaxEntries) && !order_.empty()) {
            const auto oldest = order_.front();
            erase(oldest);
        }
    }

    quint64                       limit_, bytes_ = 0;
    std::list<Span>               order_;
    QHash<Span, Entry>            entries_;
    QHash<QByteArray, QSet<Span>> generations_;
};

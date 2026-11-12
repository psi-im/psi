/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "historymediarefs.h"
#include <QObject>
#include <functional>

// One position per unique resource referenced by this conversation.
// Metadata browsing never creates a download, pins a file or refreshes its TTL.
class ChatMedia : public QObject {
    Q_OBJECT
public:
    using Cursor       = HistoryMediaRefs::Cursor;
    using Availability = HistoryMediaRefs::Availability;
    using Storage      = HistoryMediaRefs::Storage;
    using Statistics   = HistoryMediaRefs::Statistics;
    using Filter       = HistoryMediaRefs::Filter;
    using Query        = HistoryMediaRefs::Query;
    struct Entry {
        Cursor                         cursor;
        QByteArray                     publicationId;
        HistoryMediaRefs::Announcement announcement;
        Storage                        storage;
        QString                        name, mime;
        int                            type  = HistoryMediaRefs::Other;
        quint64                        index = 0, total = 0;
        QString                        sourceId;
    };
    struct Page {
        QList<Entry> entries;
        Cursor       next;
        bool         hasMore = false;
        bool         stale   = false;
    };
    using LinksDone = std::function<void(QList<HistoryMediaRefs::Link>, bool)>;
    using Read      = std::function<void(Query, QObject *, LinksDone)>;

    using PageDone       = std::function<void(Page, bool)>;
    using StatisticsDone = std::function<void(Statistics, bool)>;
    using Count          = std::function<void(Filter, QObject *, StatisticsDone)>;
    ChatMedia(Read read, Count count, QObject *parent = nullptr);
    // Initial empty cursor starts at the newest/oldest position. Bound: 1..200.
    // Callbacks run on this object's thread; deleting it cancels delivery.
    void requestPage(Cursor cursor, bool older, int limit, PageDone done);
    void requestEntry(const QString &sourceId, PageDone done);
    void requestAdjacent(const QString &sourceId, bool older, PageDone done);
    void requestAdjacent(const Entry &, bool older, PageDone done);
    // Message provenance is queried only when a consumer explicitly asks.
    struct MessageLink {
        qint64    eventId = 0;
        QDateTime timestamp;
        QString   preview;
        bool      sent = false;
    };
    struct MessagePage {
        QList<MessageLink> entries;
        qint64             next    = 0;
        bool               hasMore = false, stale = false;
    };
    using MessagesDone = std::function<void(MessagePage, bool)>;
    void   requestMessages(Cursor resource, qint64 afterEvent, int limit, MessagesDone done);
    Filter filter() const { return filter_; }
    void   setFilter(Filter);
    void requestStatistics(StatisticsDone done);
    void invalidate();
signals:
    void changed();

private:
    void    request(Query, PageDone);
    Read    read_;
    Count   count_;
    quint64 generation_ = 0;
    Filter  filter_;
};

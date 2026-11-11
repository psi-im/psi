/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "historymediarefs.h"
#include <QObject>
#include <functional>

// One position per stored attachment occurrence, including repeated shares.
// Metadata browsing never creates a download, pins a file or refreshes its TTL.
class ChatMedia : public QObject {
    Q_OBJECT
public:
    using Cursor       = HistoryMediaRefs::Cursor;
    using Availability = HistoryMediaRefs::Availability;
    using Storage      = HistoryMediaRefs::Storage;
    using Statistics   = HistoryMediaRefs::Statistics;
    struct Entry {
        Cursor                         cursor;
        QByteArray                     publicationId;
        HistoryMediaRefs::Announcement announcement;
        Storage                        storage;
    };
    struct Page {
        QList<Entry> entries;
        Cursor       next;
        bool         hasMore = false;
    };
    using LinksDone = std::function<void(QList<HistoryMediaRefs::Link>, bool)>;
    using Read      = std::function<void(Cursor, bool older, int limit, QObject *, LinksDone)>;

    using PageDone       = std::function<void(Page, bool)>;
    using StatisticsDone = std::function<void(Statistics, bool)>;
    using Count          = std::function<void(QObject *, StatisticsDone)>;
    ChatMedia(Read read, Count count, QObject *parent = nullptr);
    // Initial empty cursor starts at the newest/oldest position. Bound: 1..200.
    // Callbacks run on this object's thread; deleting it cancels delivery.
    void requestPage(Cursor cursor, bool older, int limit, PageDone done);
    void requestStatistics(StatisticsDone done);
    void invalidate();
signals:
    void changed();

private:
    Read    read_;
    Count   count_;
    quint64 generation_ = 0;
};

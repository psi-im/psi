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
    using Cursor = HistoryMediaRefs::Cursor;
    enum class Availability { Remote, Partial, Complete };
    struct Storage {
        Availability availability    = Availability::Remote;
        quint64      downloadedBytes = 0;
    };
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
    struct Statistics {
        quint64 complete = 0, partial = 0, remote = 0;
        // History/storage changed during the bounded asynchronous scan.
        bool stale = false;
    };
    using LinksDone      = std::function<void(QList<HistoryMediaRefs::Link>, bool)>;
    using Read           = std::function<void(Cursor, bool older, int limit, QObject *, LinksDone)>;
    using Inspect        = std::function<Storage(const HistoryMediaRefs::Announcement &)>;
    using PageDone       = std::function<void(Page, bool)>;
    using StatisticsDone = std::function<void(Statistics, bool)>;
    ChatMedia(Read read, Inspect inspect, QObject *parent = nullptr);
    // Initial empty cursor starts at the newest/oldest position. Bound: 1..200.
    // Callbacks run on this object's thread; deleting it cancels delivery.
    void requestPage(Cursor cursor, bool older, int limit, PageDone done);
    void requestStatistics(StatisticsDone done);
    void invalidate();
signals:
    void changed();

private:
    void    statisticsStep(QObject *request, Cursor cursor, quint64 generation, Statistics stats, StatisticsDone done);
    Read    read_;
    Inspect inspect_;
    quint64 generation_ = 0;
};

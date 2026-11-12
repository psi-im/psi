/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "chatmedia.h"
#include <QPointer>
#include <QTimer>
#include <utility>

ChatMedia::ChatMedia(Read read, Count count, QObject *parent) :
    QObject(parent), read_(std::move(read)), count_(std::move(count))
{
}

void ChatMedia::invalidate()
{
    ++generation_;
    emit changed();
}
void ChatMedia::requestPage(Cursor cursor, bool older, int limit, PageDone done)
{
    Query q;
    q.cursor = cursor;
    q.older  = older;
    q.limit  = limit;
    q.filter = filter_;
    request(q, std::move(done));
}
void ChatMedia::requestEntry(const QString &sourceId, PageDone done)
{
    Query q;
    q.source = sourceId;
    q.limit  = 1;
    q.filter = filter_;
    if (sourceId.isEmpty()) {
        QTimer::singleShot(0, this, [done] {
            if (done)
                done({}, false);
        });
        return;
    }
    request(q, std::move(done));
}
void ChatMedia::requestEntryUnfiltered(const QString &sourceId, PageDone done)
{
    if (sourceId.isEmpty()) {
        QTimer::singleShot(0, this, [done] {
            if (done)
                done({}, false);
        });
        return;
    }
    Query q;
    q.source = sourceId;
    q.limit = 1;
    q.filter = { 7, 7 };
    request(q, std::move(done));
}

void ChatMedia::setFilter(Filter filter)
{
    filter.types &= 7;
    filter.availability &= 7;
    if (filter == filter_)
        return;
    filter_ = filter;
    invalidate();
}
void ChatMedia::requestAdjacent(const QString &sourceId, bool older, PageDone done)
{
    Query q;
    q.source   = sourceId;
    q.adjacent = true;
    q.older    = older;
    q.limit    = 1;
    q.filter   = filter_;
    if (sourceId.isEmpty()) {
        requestPage({}, older, 1, std::move(done));
        return;
    }
    request(q, std::move(done));
}
void ChatMedia::requestAdjacent(const Entry &entry, bool older, PageDone done)
{
    Query q;
    q.source   = entry.sourceId;
    q.cursor   = entry.cursor;
    q.adjacent = true;
    q.older    = older;
    q.limit    = 1;
    q.filter   = filter_;
    request(q, std::move(done));
}
void ChatMedia::requestMessages(Cursor resource, qint64 afterEvent, int limit, MessagesDone done)
{
    if (!done)
        return;
    if (!read_ || resource.assetId <= 0 || limit < 1 || limit > 200) {
        QTimer::singleShot(0, this, [done] { done({}, false); });
        return;
    }
    Query q;
    q.messagesFor = resource.assetId;
    q.afterEvent  = afterEvent;
    q.limit       = limit + 1;
    const QPointer<ChatMedia> guard(this);
    const auto                generation = generation_;
    read_(q, this, [guard, generation, limit, done = std::move(done)](QList<HistoryMediaRefs::Link> links, bool ok) {
        if (!guard)
            return;
        if (!ok) {
            done({}, false);
            return;
        }
        MessagePage page;
        page.hasMore = links.size() > limit;
        page.stale   = generation != guard->generation_;
        if (page.hasMore)
            links.removeLast();
        for (const auto &link : links)
            page.entries.append({ link.eventId,
                                  link.undated ? QDateTime() : QDateTime::fromMSecsSinceEpoch(link.timestamp).toUTC(),
                                  link.body, link.direction == 1 });
        if (!page.entries.isEmpty())
            page.next = page.entries.last().eventId;
        done(std::move(page), true);
    });
}
void ChatMedia::request(Query query, PageDone done)
{
    const auto limit = query.limit;
    if (!done)
        return;
    if (!read_ || limit < 1 || limit > 200) {
        QTimer::singleShot(0, this, [done] { done({}, false); });
        return;
    }
    const QPointer<ChatMedia> guard(this);
    const auto                generation = generation_;
    query.limit                          = limit + 1;
    read_(query, this,
          [guard, limit, generation, done = std::move(done)](QList<HistoryMediaRefs::Link> links, bool ok) {
              if (!guard)
                  return;
              if (!ok) {
                  done({}, false);
                  return;
              }
              Page page;
              page.stale   = generation != guard->generation_;
              page.hasMore = links.size() > limit;
              if (page.hasMore)
                  links.removeLast();
              for (const auto &link : links) {
                  const auto announcement = HistoryMediaRefs::announcement(link);
                  if (!announcement) {
                      done({}, false);
                      return;
                  }
                  const auto &file   = announcement->file;
                  const auto  source = file.hasContentHashes()
                       ? file.reference.mediaSharing().file.computedHashes().first().toString()
                       : MessageFileShares::urlIdentity(file.url()).toString();
                  page.entries.append({ link.cursor, link.publicationId, *announcement, link.storage, link.name,
                                        link.mime, link.kind, link.index, link.total, source });
              }
              if (!links.isEmpty())
                  page.next = links.last().cursor;
              done(std::move(page), true);
          });
}
void ChatMedia::requestStatistics(StatisticsDone done)
{
    if (!done)
        return;
    if (!count_) {
        QTimer::singleShot(0, this, [done] { done({}, false); });
        return;
    }
    const QPointer<ChatMedia> guard(this);
    const auto                generation = generation_;
    count_(filter_, this, [guard, generation, done = std::move(done)](Statistics stats, bool ok) {
        if (!guard)
            return;
        stats.stale = stats.stale || generation != guard->generation_;
        done(stats, ok);
    });
}

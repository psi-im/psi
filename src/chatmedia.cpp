/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "chatmedia.h"
#include <QPointer>
#include <QTimer>
#include <utility>

ChatMedia::ChatMedia(Read read, Inspect inspect, QObject *parent) :
    QObject(parent), read_(std::move(read)), inspect_(std::move(inspect))
{
}

void ChatMedia::invalidate()
{
    ++generation_;
    emit changed();
}
void ChatMedia::requestPage(Cursor cursor, bool older, int limit, PageDone done)
{
    if (!done)
        return;
    if (!read_ || !inspect_ || limit < 1 || limit > 200) {
        QTimer::singleShot(0, this, [done] { done({}, false); });
        return;
    }
    const QPointer<ChatMedia> guard(this);
    read_(cursor, older, limit + 1, this,
          [guard, limit, done = std::move(done)](QList<HistoryMediaRefs::Link> links, bool ok) {
              if (!guard)
                  return;
              if (!ok) {
                  done({}, false);
                  return;
              }
              Page page;
              page.hasMore = links.size() > limit;
              if (page.hasMore)
                  links.removeLast();
              for (const auto &link : links) {
                  const auto announcement = HistoryMediaRefs::announcement(link);
                  if (!announcement) {
                      done({}, false);
                      return;
                  }
                  page.entries.append(
                      { link.cursor, link.publicationId, *announcement, guard->inspect_(*announcement) });
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
    auto request = new QObject(this);
    statisticsStep(request, {}, generation_, {}, std::move(done));
}
void ChatMedia::statisticsStep(QObject *request, Cursor cursor, quint64 generation, Statistics stats,
                               StatisticsDone done)
{
    const QPointer<ChatMedia> guard(this);
    requestPage(cursor, false, 100,
                [guard, request, generation, stats, done = std::move(done)](Page page, bool ok) mutable {
                    if (!guard)
                        return;
                    if (!ok) {
                        request->deleteLater();
                        done({}, false);
                        return;
                    }
                    for (const auto &entry : page.entries) {
                        switch (entry.storage.availability) {
                        case Availability::Complete:
                            ++stats.complete;
                            break;
                        case Availability::Partial:
                            ++stats.partial;
                            break;
                        case Availability::Remote:
                            ++stats.remote;
                            break;
                        }
                    }
                    if (!page.hasMore) {
                        stats.stale = guard->generation_ != generation;
                        request->deleteLater();
                        done(stats, true);
                    } else {
                        // Yield between batches even when an adapter completes synchronously.
                        QTimer::singleShot(
                            0, request, [guard, request, next = page.next, generation, stats, done = std::move(done)] {
                                if (guard)
                                    guard->statisticsStep(request, next, generation, stats, done);
                            });
                    }
                });
}

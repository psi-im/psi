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
    if (!done)
        return;
    if (!read_ || limit < 1 || limit > 200) {
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
                  page.entries.append({ link.cursor, link.publicationId, *announcement, link.storage });
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
    count_(this, [guard, generation, done = std::move(done)](Statistics stats, bool ok) {
        if (!guard)
            return;
        stats.stale = stats.stale || generation != guard->generation_;
        done(stats, ok);
    });
}

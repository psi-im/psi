/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "eventdb.h"
#include "historysync.h"
#include "psiaccount.h"

namespace {
HistorySync::Context accountContext(PsiAccount *account)
{
    HistorySync::Context context;
    context.accountId = account->id();
    context.owner     = account->jid();
    context.client    = account->client();
    context.connected = [account] { return !account->isServerless() && account->isConnected(); };
    context.logging
        = [account] { return account->userAccount().opt_log && (account->edb()->features() & EDB::ArchiveCache); };
    context.read = [account](const HistoryArchive::Key &key, const QString &before, int limit) {
        return account->edb()->archivePage(key, before, limit);
    };
    context.merge
        = [account](const HistoryArchive::Key &key, const QString &before, int limit, HistoryArchive::Page &page) {
              return page.checkpoint ? account->edb()->mergeArchivePage(key, before, limit, page)
                                     : account->edb()->mergeArchiveIsland(key, limit, page);
          };
    context.readSnapshot = [account](const HistoryArchive::Key &key) { return account->edb()->archiveSnapshot(key); };
    context.readIndex    = [account](const HistoryArchive::Key &key, int index, int limit, int snapshot) {
        return account->edb()->archivePageAtIndex(key, index, limit, snapshot);
    };
    context.readAfter = [account](const HistoryArchive::Key &key, const QString &after, int limit, int snapshot) {
        return account->edb()->archivePageAfter(key, after, limit, snapshot);
    };
    context.lookup = [account](const HistoryArchive::Key &key, const QString &uid, const QDateTime &time) {
        HistorySync::CachedMessage cached;
        if (auto saved = account->edb()->archivedEvent(key, uid, time))
            if (saved->event()->type() == PsiEvent::Message) {
                cached.message      = saved->event().staticCast<MessageEvent>()->message();
                cached.protocolOnly = saved->protocolOnly() || cached.message.messageReceipt() == XMPP::ReceiptReceived
                    || !cached.message.displayedMarkerId().isEmpty() || !cached.message.reactions().targetId.isEmpty()
                    || !cached.message.retraction().isEmpty();
            }
        return cached;
    };
    context.ownPlaintext
        = [account](const HistoryArchive::Key &key, const QString &id, const QString &sender, const QDateTime &time) {
              HistorySync::CachedMessage cached;
              if (auto saved = account->edb()->ownPlaintext(key, id, sender, time))
                  if (saved->event()->type() == PsiEvent::Message) {
                      cached.message = saved->event().staticCast<MessageEvent>()->message();
                      // Legacy outgoing EDB records may lack their envelope sender.
                      cached.message.setFrom(XMPP::Jid(sender));
                      cached.message.setTo(XMPP::Jid(key.peer));
                      cached.protocolOnly = saved->protocolOnly()
                          || cached.message.messageReceipt() == XMPP::ReceiptReceived
                          || !cached.message.displayedMarkerId().isEmpty()
                          || !cached.message.reactions().targetId.isEmpty() || !cached.message.retraction().isEmpty();
                  }
              return cached;
          };
    context.readAsync = [account](const HistoryArchive::Key &key, HistorySync::Query query, QObject *receiver,
                                  std::function<void(HistoryArchive::Page, HistoryArchive::Snapshot, bool)> finished) {
        const auto mode = query.mode == HistorySync::Query::After ? EDB::ArchiveReadMode::After
                          : query.mode == HistorySync::Query::Index ? EDB::ArchiveReadMode::Index
                                                                    : EDB::ArchiveReadMode::Before;
        account->edb()->requestArchivePage(mode, key, query.cursor, query.index, query.limit,
                                           query.snapshotCount, receiver, std::move(finished));
    };
    context.mergeAsync = [account](const HistoryArchive::Key &key, const QString &before, int limit,
                                   HistoryArchive::Page page, QObject *receiver,
                                   std::function<void(HistoryArchive::Page, bool)> finished) {
        account->edb()->requestStoreArchivePage(key, before, limit, std::move(page), receiver, std::move(finished));
    };
    context.resolveAsync = [account](
        const QList<qint64> &ids, QObject *receiver,
        std::function<void(QHash<qint64, HistorySync::ResolvedMessage>, bool)> finished) {
        account->edb()->requestResolvedHistoryEvents(
            ids, receiver,
            [finished](QHash<qint64, EDBItemPtr> rows, bool ok) {
                QHash<qint64, HistorySync::ResolvedMessage> values;
                for (auto it = rows.cbegin(); it != rows.cend(); ++it) {
                    const auto &item = it.value();
                    if (!item || !item->event() || item->event()->type() != PsiEvent::Message)
                        continue;
                    HistorySync::ResolvedMessage value;
                    value.message = item->event().staticCast<MessageEvent>()->message().displayMessage();
                    value.historyId = item->id().toLongLong();
                    value.messageState = item->messageFlags();
                    value.revisionTime = item->revisionTime();
                    value.reactionTarget = item->reactionTarget();
                    value.retractionTarget = item->retractionTarget();
                    value.found = true;
                    values.insert(it.key(), value);
                }
                finished(std::move(values), ok);
            });
    };
    return context;
}
}

HistorySync::HistorySync(PsiAccount *account, const QString &peer, bool room, const QString &before, QObject *parent,
                         int pageSize) : HistorySync(accountContext(account), peer, room, before, parent, pageSize)
{
}

HistorySync::HistorySync(PsiAccount *account, Query query, const QString &peer, bool room, QObject *parent) :
    HistorySync(accountContext(account), query, peer, room, parent)
{
}

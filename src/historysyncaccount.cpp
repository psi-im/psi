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
    context.connected = [account] { return account->isConnected(); };
    context.logging
        = [account] { return account->userAccount().opt_log && (account->edb()->features() & EDB::ArchiveCache); };
    context.read = [account](const HistoryArchive::Key &key, const QString &before, int limit) {
        return account->edb()->archivePage(key, before, limit);
    };
    context.merge
        = [account](const HistoryArchive::Key &key, const QString &before, int limit, HistoryArchive::Page &page) {
              return account->edb()->mergeArchivePage(key, before, limit, page);
          };
    context.lookup = [account](const HistoryArchive::Key &key, const QString &uid, const QDateTime &time) {
        HistorySync::CachedMessage cached;
        if (auto saved = account->edb()->archivedEvent(key, uid, time))
            if (saved->event()->type() == PsiEvent::Message) {
                cached.message      = saved->event().staticCast<MessageEvent>()->message();
                cached.protocolOnly = saved->protocolOnly() || cached.message.messageReceipt() == XMPP::ReceiptReceived
                    || !cached.message.reactions().targetId.isEmpty() || !cached.message.retraction().isEmpty();
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
                  }
              return cached;
          };
    return context;
}
}

HistorySync::HistorySync(PsiAccount *account, const QString &peer, bool room, const QString &before, QObject *parent) :
    HistorySync(accountContext(account), peer, room, before, parent)
{
}

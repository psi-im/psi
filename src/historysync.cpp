/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historysync.h"

#include "eventdb.h"
#include "historymessageextras.h"
#include "iris/xmpp_forwarding.h"
#include "iris/xmpp_mammanager.h"
#include "iris/xmpp_xmlcommon.h"
#include "psiaccount.h"
#include "widgets/MamArchivePeerPolicy.h"
#include "widgets/MamHistorySourcePolicy.h"

#include <QTimer>
#include <algorithm>

namespace {
bool parseXml(QDomDocument &doc, const QString &xml)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return bool(doc.setContent(xml, QDomDocument::ParseOption::UseNamespaceProcessing));
#else
    return doc.setContent(xml, true);
#endif
}
}

HistorySync::HistorySync(PsiAccount *account, const QString &peer, bool room, const QString &before, QObject *parent) :
    QObject(parent), account_(account), before_(before)
{
    key_ = { account->id(), room ? peer : account->jid().bare(), peer, room };
}

HistorySync::~HistorySync()
{
    if (task_)
        task_->safeDelete();
}

void HistorySync::safeDelete()
{
    cancelled_ = true;
    if (task_) {
        task_->disconnect(this);
        task_->safeDelete();
        task_.clear();
    }
    disconnect();
    deleteLater();
}

QList<QDomElement> HistorySync::archive() const
{
    QList<QDomElement> results;
    for (const auto &entry : page_.entries) {
        QDomDocument doc;
        if (parseXml(doc, entry.xml))
            results.append(doc.documentElement());
    }
    return results;
}

qint64 HistorySync::eventId(const QString &uid) const
{
    for (const auto &entry : page_.entries)
        if (entry.uid == uid)
            return entry.eventId;
    return 0;
}

bool HistorySync::parseResults(const QList<QDomElement> &results, HistoryArchive::Page &page)
{
    QSet<QString> ids;
    for (const auto &result : results) {
        HistoryArchive::Entry entry;
        entry.uid = result.attribute(QStringLiteral("id"));
        const auto forwarded
            = childElementsByTagNameNS(result, QStringLiteral("urn:xmpp:forward:0"), QStringLiteral("forwarded"))
                  .item(0)
                  .toElement();
        XMPP::Forwarding wrapper;
        if (entry.uid.isEmpty() || ids.contains(entry.uid) || !wrapper.fromXml(forwarded, account_->client()))
            return false; // Never certify a page with missing/invalid results.
        ids.insert(entry.uid);
        const auto message = wrapper.message();
        if (key_.room) {
            if (message.type() != XMPP::Message::Type::Groupchat || message.from().bare() != key_.peer)
                return false;
        } else if (!PsiMam::belongsToConversation(account_->jid().bare(), key_.peer, message.from().bare(),
                                                  message.to().bare())) {
            return false;
        }
        entry.timestamp = wrapper.timeStamp().isValid() ? wrapper.timeStamp() : message.timeStamp();
        if (!entry.timestamp.isValid())
            return false;
        QDomDocument doc;
        doc.appendChild(doc.importNode(result, true));
        entry.xml       = doc.toString(-1);
        entry.messageId = message.id();
        entry.replaceId = message.replaceId();
        entry.sender    = message.from().full();
        entry.body      = message.body();
        entry.subject   = message.subject();
        entry.language  = message.lang();
        entry.extra     = HistoryMessageExtras::encode(message);
        entry.type      = key_.room                                ? 9
                 : message.type() == XMPP::Message::Type::Chat     ? 1
                 : message.type() == XMPP::Message::Type::Error    ? 4
                 : message.type() == XMPP::Message::Type::Headline ? 5
                                                                   : 0;
        entry.direction = !key_.room && message.from().bare() == account_->jid().bare() ? 1 : 2;
        entry.storable  = !(message.processingHints() & (XMPP::Message::NoStore | XMPP::Message::NoPermanentStore));
        page.entries.append(entry);
    }
    return true;
}

void HistorySync::go()
{
    // Always asynchronous, including cache hits, to match the old Task contract.
    QTimer::singleShot(0, this, [this] {
        if (cancelled_)
            return;
        const bool logging = account_->userAccount().opt_log && (account_->edb()->features() & EDB::ArchiveCache);
        if (logging) {
            auto saved = account_->edb()->archivePage(key_, before_, PageSize);
            // The moving tail is refreshed whenever a connected chat opens.
            // Isolated trusted rows never prove completeness of a page.
            if (PsiMam::mayReuseArchivePage(before_.isEmpty(), account_->isConnected(), saved.found, saved.stable,
                                            saved.entries.size(), PageSize, saved.complete)) {
                HistoryArchive::Page validated;
                if (parseResults(
                        [&saved] {
                            QList<QDomElement> results;
                            for (const auto &entry : saved.entries) {
                                QDomDocument doc;
                                if (!parseXml(doc, entry.xml))
                                    return QList<QDomElement>();
                                results.append(doc.documentElement());
                            }
                            return results;
                        }(),
                        validated)
                    && validated.entries.size() == saved.entries.size()
                    && std::equal(validated.entries.cbegin(), validated.entries.cend(), saved.entries.cbegin(),
                                  [](const HistoryArchive::Entry &parsed, const HistoryArchive::Entry &stored) {
                                      return parsed.uid == stored.uid && parsed.storable;
                                  })) {
                    page_    = std::move(saved);
                    success_ = cached_ = true;
                    emit finished();
                    return;
                }
            }
        }
        if (!account_->isConnected()) {
            error_ = tr("Account is offline");
            emit finished();
            return;
        }
        XMPP::MAMManager manager(account_->client(), PageSize, PageSize, false, true);
        task_ = manager.getArchiveByIDRange(key_.room ? XMPP::Jid() : XMPP::Jid(key_.peer), {}, before_, key_.room);
        if (key_.room)
            task_->setArchiveJid(XMPP::Jid(key_.archive));
        connect(task_, &XMPP::Task::finished, this, [this] {
            auto *task = task_.data();
            task_.clear();
            page_.complete = task->isComplete();
            page_.stable   = task->isStable();
            success_       = task->success() && parseResults(task->archive(), page_);
            if (success_) {
                if (account_->userAccount().opt_log && (account_->edb()->features() & EDB::ArchiveCache)
                    && !account_->edb()->mergeArchivePage(key_, before_, PageSize, page_)) {
                    // Network results remain usable, but no IDs from a rolled
                    // back transaction may be adopted into the presentation.
                    for (auto &entry : page_.entries)
                        entry.eventId = 0;
                    qWarning("[history-cache] Could not persist MAM page");
                }
            } else {
                page_.entries.clear();
                error_ = task->success() ? tr("Invalid archive page") : task->statusString();
            }
            task->deleteLater();
            emit finished();
        });
        task_->go();
    });
}

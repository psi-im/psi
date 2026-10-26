/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historysync.h"
#include "widgets/MamHistoryLog.h"

#include "historymessageextras.h"
#include "iris/xmpp_encryption.h"
#include "iris/xmpp_forwarding.h"
#include "iris/xmpp_mammanager.h"
#include "iris/xmpp_stream.h"
#include "iris/xmpp_xmlcommon.h"
#include "widgets/MamArchivePeerPolicy.h"
#include "widgets/MamHistorySourcePolicy.h"
#include <QJsonDocument>
#include <QJsonObject>

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

HistorySync::HistorySync(Context context, const QString &peer, bool room, const QString &before, QObject *parent,
                         int pageSize) :
    HistorySync(std::move(context), Query { Query::Before, before, -1, pageSize }, peer, room, parent)
{
}

HistorySync::HistorySync(Context context, Query query, const QString &peer, bool room, QObject *parent) :
    QObject(parent), context_(std::move(context)), query_(query),
    before_(query.mode == Query::Before ? query.cursor : QString()),
    after_(query.mode == Query::After ? query.cursor : QString()), pageSize_(qBound(1, query.limit, PageSize))
{
    key_ = { context_.accountId, room ? peer : context_.owner.bare(), peer, room };
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
        const auto element = readResult(entry.xml);
        if (!element.isNull())
            results.append(element);
    }
    return results;
}

QDomElement HistorySync::readResult(const QString &xml) const
{
    QDomDocument doc;
    if (!parseXml(doc, xml))
        return {};
    documents_.append(doc);
    return documents_.last().documentElement();
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
        if (entry.uid.isEmpty() || ids.contains(entry.uid) || !wrapper.fromXml(forwarded, context_.client))
            return false; // Never certify a page with missing/invalid results.
        ids.insert(entry.uid);
        const auto message = decoded_.contains(entry.uid) ? decoded_.value(entry.uid) : wrapper.message();
        if (key_.room) {
            if (message.type() != XMPP::Message::Type::Groupchat || message.from().bare() != key_.peer)
                return false;
        } else if (!PsiMam::belongsToConversation(context_.owner.bare(), key_.peer, message.from().bare(),
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
        entry.body      = decryptionFailures_.contains(entry.uid) ? QString() : message.body();
        entry.subject   = message.subject();
        entry.language  = message.lang();
        entry.extra     = HistoryMessageExtras::encode(message);
        auto extra      = QJsonDocument::fromJson(entry.extra.toUtf8()).object();
        // Presence of an empty <body/> is content, unlike a receipt/chatstate.
        const auto stanza  = forwarded.firstChildElement(QStringLiteral("message"));
        const bool hasBody = !stanza.firstChildElement("body").isNull() || bodyElements_.contains(entry.uid);
        extra.insert("psi:has-body", hasBody);
        extra.insert("psi:decoded",
                     !decryptionFailures_.contains(entry.uid)
                         && (decoded_.contains(entry.uid) || stanza.firstChildElement("encrypted").isNull()));
        if (decryptionFailures_.contains(entry.uid))
            extra.insert("psi:decryption-error", true);
        const bool textless = !stanza.isNull() && !hasBody && stanza.firstChildElement("subject").isNull()
            && stanza.firstChildElement("html").isNull() && message.references().isEmpty()
            && message.urlList().isEmpty() && message.body().isEmpty();
        if (protocolOnly_.contains(entry.uid) || textless)
            extra.insert("psi:protocol-only", true);
        entry.extra     = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
        entry.type      = key_.room                                ? 9
                 : message.type() == XMPP::Message::Type::Chat     ? 1
                 : message.type() == XMPP::Message::Type::Error    ? 4
                 : message.type() == XMPP::Message::Type::Headline ? 5
                                                                   : 0;
        entry.direction = !key_.room && message.from().bare() == context_.owner.bare() ? 1 : 2;
        entry.storable  = !((message.processingHints() | wrapper.message().processingHints())
                           & (XMPP::Message::NoStore | XMPP::Message::NoPermanentStore));
        page.entries.append(entry);
    }
    return true;
}

XMPP::Message HistorySync::message(const QString &uid) const
{
    if (decoded_.contains(uid))
        return decoded_.value(uid);
    for (const auto &entry : page_.entries) {
        if (entry.uid != uid)
            continue;
        const auto element = readResult(entry.xml);
        if (element.isNull())
            break;
        const auto forwarded = element.firstChildElement("forwarded");
        // A cached plaintext revision can be replayed without decrypting the
        // same OMEMO ciphertext a second time (ratchets may reject duplicates).
        const auto saved = context_.lookup(
            key_, uid, QDateTime::fromString(forwarded.firstChildElement("delay").attribute("stamp"), Qt::ISODate));
        const auto &plain = saved.message;
        if (!plain.isNull()
            && (!plain.body().isEmpty() || saved.protocolOnly || plain.messageReceipt() == XMPP::ReceiptReceived
                || !plain.displayedMarkerId().isEmpty() || !plain.reactions().targetId.isEmpty()
                || !plain.retraction().isEmpty()))
            return plain;
        XMPP::Forwarding wrapper;
        if (wrapper.fromXml(element.firstChildElement("forwarded"), context_.client))
            return wrapper.message();
    }
    return {};
}

bool HistorySync::protocolOnly(const QString &uid) const
{
    for (const auto &entry : page_.entries)
        if (entry.uid == uid)
            return QJsonDocument::fromJson(entry.extra.toUtf8()).object().value("psi:protocol-only").toBool();
    return false;
}

void HistorySync::go()
{
    QTimer::singleShot(0, this, [this] {
        if (cancelled_)
            return;
        const bool logging = context_.logging();
        if (logging) {
            if (context_.readSnapshot)
                snapshot_ = context_.readSnapshot(key_);
            HistoryArchive::Page saved;
            if (query_.mode == Query::Before)
                saved = context_.read(key_, before_, pageSize_);
            else if (query_.mode == Query::Index && context_.readIndex && query_.snapshotCount >= 0)
                saved = context_.readIndex(key_, query_.index, pageSize_, query_.snapshotCount);
            else if (query_.mode == Query::After && context_.readAfter)
                saved = context_.readAfter(key_, after_, pageSize_, query_.snapshotCount);
            qCDebug(PsiMam::historyLog) << "cache account=" << key_.account << "peer=" << key_.peer
                                        << "mode=" << query_.mode << "index=" << query_.index << "before=" << before_
                                        << "after=" << after_ << "limit=" << pageSize_ << "hit=" << saved.found
                                        << "stable=" << saved.stable << "events=" << saved.entries.size();
            HistoryArchive::Page validated;
            QList<QDomElement>   xml;
            for (const auto &entry : saved.entries) {
                const auto element = readResult(entry.xml);
                if (element.isNull()) {
                    xml.clear();
                    break;
                }
                xml.append(element);
                const auto row = context_.lookup(
                    key_, entry.uid,
                    QDateTime::fromString(
                        element.firstChildElement("forwarded").firstChildElement("delay").attribute("stamp"),
                        Qt::ISODate));
                if (!row.message.isNull()) {
                    decoded_.insert(entry.uid, row.message);
                    if (!row.message.body().isNull())
                        bodyElements_.insert(entry.uid);
                    if (row.protocolOnly)
                        protocolOnly_.insert(entry.uid);
                }
            }
            if (saved.found && saved.stable && xml.size() == saved.entries.size() && parseResults(xml, validated)
                && std::equal(
                    validated.entries.cbegin(), validated.entries.cend(), saved.entries.cbegin(),
                    [](const HistoryArchive::Entry &a, const HistoryArchive::Entry &b) { return a.uid == b.uid; })) {
                validated.found    = true;
                validated.stable   = saved.stable;
                validated.complete = saved.complete;
                validated.resultCount   = saved.resultCount;
                validated.firstIndex    = saved.firstIndex;
                validated.requestIndex  = saved.requestIndex;
                validated.queryComplete = saved.queryComplete;
                validated.checkpoint    = saved.checkpoint;
                validated.previous      = saved.previous;
                for (int i = 0; i < validated.entries.size(); ++i)
                    validated.entries[i].eventId = saved.entries.at(i).eventId;
                replaySaved_ = query_.mode == Query::Before
                    ? PsiMam::mayReuseArchivePage(before_.isEmpty(), context_.connected(), true, true,
                                                  saved.entries.size(), pageSize_, saved.complete)
                    : saved.entries.size() == pageSize_ || saved.queryComplete;
                if (replaySaved_
                    || (query_.mode == Query::Before && before_.isEmpty() && !validated.entries.isEmpty())) {
                    tail_          = validated;
                    results_       = xml;
                    decodingSaved_ = true;
                    decodeIndex_   = 0;
                    // Cached pages do not trigger snapshot-only MAM traffic.
                    // A real, user-driven MAM page updates the count and UID.
                    decodeNext();
                    return;
                }
            }
        }
        if (!context_.connected()) {
            finishError(tr("Account is offline"));
            return;
        }
        requestPage();
    });
}

void HistorySync::finishError(const QString &error)
{
    qCWarning(PsiMam::historyLog) << "source-error account=" << key_.account << "peer=" << key_.peer
                                  << "before=" << before_ << "error=" << error;
    page_.entries.clear();
    success_ = false;
    error_   = error;
    emit finished();
}

void HistorySync::requestPage()
{
    if (cancelled_)
        return;
    if (!after_.isEmpty() && visitedAfter_.contains(after_)) {
        finishError(tr("Archive catch-up repeated a cursor"));
        return;
    }
    if (!after_.isEmpty())
        visitedAfter_.insert(after_);
    qCDebug(PsiMam::historyLog) << "network account=" << key_.account << "peer=" << key_.peer << "before=" << before_
                                << "after=" << after_ << "limit=" << pageSize_;
    const bool       forward = !after_.isEmpty();
    XMPP::MAMManager manager(context_.client, pageSize_, pageSize_, false, !forward);
    if (query_.mode == Query::Index)
        task_ = manager.getArchiveAtIndex(key_.room ? XMPP::Jid() : XMPP::Jid(key_.peer), query_.index, pageSize_,
                                          key_.room);
    else
        task_ = manager.getArchiveByIDRange(key_.room ? XMPP::Jid() : XMPP::Jid(key_.peer), after_,
                                            forward ? QString() : before_, key_.room);
    if (key_.room)
        task_->setArchiveJid(XMPP::Jid(key_.archive));
    connect(task_, &XMPP::Task::finished, this, [this] {
        auto *task = task_.data();
        task_.clear();
        networkComplete_ = task->isComplete();
        networkStable_   = task->isStable();
        resultCount_      = task->resultCount();
        firstResultIndex_ = task->firstResultIndex();
        if (!task->success()) {
            const bool expired = query_.mode == Query::Before && !after_.isEmpty()
                && task->error().condition == XMPP::Stanza::Error::ErrorCond::ItemNotFound;
            const QString error = task->statusString();
            task->deleteLater();
            if (expired) {
                // Retention invalidated the saved cursor. Fetch one new tail;
                // do not infer connectivity across the expired gap.
                after_.clear();
                tail_ = {};
                requestPage();
            } else
                finishError(error);
            return;
        }
        if (query_.mode == Query::Index && firstResultIndex_ < 0) {
            // An ignored index can return an unrelated beginning/tail page.
            // Do not decrypt, cache or project it as the requested island.
            task->deleteLater();
            finishError(tr("Archive response omitted the requested index"));
            return;
        }
        results_ = task->archive();
        qCDebug(PsiMam::historyLog) << "network-result account=" << key_.account << "peer=" << key_.peer
                                    << "events=" << results_.size() << "count=" << resultCount_
                                    << "firstIndex=" << firstResultIndex_ << "stable=" << networkStable_
                                    << "complete=" << networkComplete_;
        task->deleteLater();
        decodeIndex_ = 0;
        decodeNext();
    });
    task_->go();
}

void HistorySync::decodeNext()
{
    if (cancelled_)
        return;
    while (decodeIndex_ < results_.size()) {
        const auto    result    = results_.at(decodeIndex_++);
        const QString uid       = result.attribute("id");
        const auto    forwarded = result.firstChildElement("forwarded");
        const auto    stanza    = forwarded.firstChildElement("message");
        auto         *crypto    = context_.client->encryptionManager();
        const bool    supported = crypto && crypto->methodForStanza(stanza);
        const auto    encrypted = stanza.firstChildElement("encrypted");
        if (!supported && encrypted.namespaceURI() != QLatin1String("urn:xmpp:omemo:2")
            && encrypted.namespaceURI() != QLatin1String("eu.siacs.conversations.axolotl"))
            continue;
        const auto stamp = QDateTime::fromString(forwarded.firstChildElement("delay").attribute("stamp"), Qt::ISODate);
        const auto saved = context_.lookup(key_, uid, stamp);
        if (!saved.message.isNull()) {
            decoded_.insert(uid, saved.message);
            if (!saved.message.body().isNull())
                bodyElements_.insert(uid);
            if (saved.protocolOnly)
                protocolOnly_.insert(uid);
            continue;
        }
        // OMEMO does not encrypt a sent message for the sending device. Only
        // an archived sender belonging to our account may use its uniquely
        // recorded outgoing plaintext. The account JID is bare, and a restart
        // can bind a different resource. Storage checks the recorded sender;
        // peers' unverified client IDs must never use this fallback.
        if (!key_.room && XMPP::Jid(stanza.attribute("from")).compare(context_.owner, false) && context_.ownPlaintext
            && PsiMam::belongsToConversation(context_.owner.bare(), key_.peer,
                                             XMPP::Jid(stanza.attribute("from")).bare(),
                                             XMPP::Jid(stanza.attribute("to")).bare())) {
            const auto time
                = QDateTime::fromString(forwarded.firstChildElement("delay").attribute("stamp"), Qt::ISODate);
            const auto own = context_.ownPlaintext(key_, stanza.attribute("id"), stanza.attribute("from"), time);
            if (!own.message.isNull()) {
                decoded_.insert(uid, own.message);
                if (!own.message.body().isNull())
                    bodyElements_.insert(uid);
                if (own.protocolOnly)
                    protocolOnly_.insert(uid);
                continue;
            }
        }
        if (!supported) {
            XMPP::Forwarding wrapper;
            wrapper.fromXml(forwarded, context_.client);
            auto plain = wrapper.message();
            plain.setBody(tr("Unable to decrypt archived message"));
            decoded_.insert(uid, plain);
            decryptionFailures_.insert(uid);
            continue;
        }
        XMPP::EncryptionContext context;
        context.setArchiveReplay();
        auto *job    = crypto->decrypt(stanza, context);
        auto  finish = [this, job, uid] {
            if (cancelled_) {
                job->deleteLater();
                return;
            }
            XMPP::Forwarding wrapper;
            wrapper.fromXml(results_.at(decodeIndex_ - 1).firstChildElement("forwarded"), context_.client);
            XMPP::Message plain = wrapper.message();
            if (job->success()) {
                if (!job->stanza().isNull()) {
                    plain.fromStanza(XMPP::Stanza::fromXml(job->stanza()));
                    if (!job->stanza().firstChildElement("body").isNull())
                        bodyElements_.insert(uid);
                }
                plain.setWasEncrypted(true);
                if (job->metadata().protocolOnly)
                    protocolOnly_.insert(uid);
            } else {
                // Never silently turn an encrypted message into a blank row.
                plain.setBody(tr("Unable to decrypt archived message"));
                decryptionFailures_.insert(uid); // Retry later instead of certifying an error placeholder.
            }
            decoded_.insert(uid, plain);
            job->deleteLater();
            QTimer::singleShot(0, this, &HistorySync::decodeNext);
        };
        if (job->isFinished())
            finish();
        else {
            connect(job, &XMPP::EncryptionJob::finished, this, finish);
            connect(job, &XMPP::EncryptionJob::finished, job, &QObject::deleteLater);
        }
        return;
    }
    acceptPage();
}

void HistorySync::acceptPage()
{
    HistoryArchive::Page received;
    received.stable   = networkStable_;
    received.complete      = query_.mode == Query::Index ? firstResultIndex_ == 0 : networkComplete_;
    received.resultCount   = resultCount_;
    received.firstIndex    = firstResultIndex_;
    received.checkpoint    = query_.mode == Query::Before;
    received.requestIndex  = query_.mode == Query::Index ? query_.index : -1;
    received.queryComplete = networkComplete_;
    if (!parseResults(results_, received)) {
        finishError(tr("Invalid archive page"));
        return;
    }
    if (decodingSaved_) {
        decodingSaved_    = false;
        received.complete = tail_.complete;
        received.stable   = tail_.stable;
        received.resultCount   = tail_.resultCount;
        received.firstIndex    = tail_.firstIndex;
        received.requestIndex  = tail_.requestIndex;
        received.queryComplete = tail_.queryComplete;
        received.checkpoint    = tail_.checkpoint;
        received.previous      = tail_.previous;
        for (int i = 0; i < received.entries.size(); ++i)
            received.entries[i].eventId = tail_.entries.at(i).eventId;
        // Repair old cache projections through the same transaction as a
        // network page; no message/receipt notifications are emitted.
        if (context_.logging() && !context_.merge(key_, before_, pageSize_, received)) {
            finishError(tr("Could not repair cached archive page"));
            return;
        }
        if (replaySaved_) {
            page_    = received;
            success_ = cached_ = true;
            emit finished();
        } else {
            tail_  = received;
            after_ = received.entries.last().uid;
            requestPage();
        }
        return;
    }
    const bool latest = (query_.mode == Query::Before && before_.isEmpty() && after_.isEmpty())
        || (!after_.isEmpty() && networkComplete_)
        || (query_.mode == Query::Index && firstResultIndex_ >= 0 && resultCount_ >= 0
            && firstResultIndex_ + received.entries.size() == resultCount_);
    // Optional metadata can disappear on individual responses. Keep the last
    // actual count/UID pair as an estimate; don't invent a new count for a
    // different tail or erase a useful old snapshot.
    if (resultCount_ >= 0 || (latest && snapshot_.resultCount < 0)) {
        snapshot_.observedAt = QDateTime::currentDateTimeUtc();
        snapshot_.stable     = networkStable_;
        if (resultCount_ != snapshot_.resultCount)
            snapshot_.lastUid.clear();
        snapshot_.resultCount = resultCount_;
        if (latest)
            snapshot_.lastUid = received.entries.isEmpty() ? after_ : received.entries.last().uid;
    }
    received.snapshot     = snapshot_;
    const QString next    = received.entries.isEmpty() ? QString() : received.entries.last().uid;
    const bool    forward = !after_.isEmpty();
    if (forward && query_.mode == Query::Before) {
        if (!networkComplete_ && (next.isEmpty() || next == after_)) {
            finishError(tr("Archive catch-up did not advance"));
            return;
        }
        auto entries = tail_.entries;
        entries.append(received.entries);
        QString previous = tail_.previous;
        bool    complete = tail_.complete;
        int     firstIndex = tail_.firstIndex >= 0            ? tail_.firstIndex
                : received.firstIndex >= tail_.entries.size() ? received.firstIndex - tail_.entries.size()
                                                              : -1;
        while (entries.size() > pageSize_) {
            previous = entries.takeFirst().uid;
            if (firstIndex >= 0)
                ++firstIndex;
            complete = false;
        }
        page_.entries  = entries;
        page_.previous = previous;
        page_.complete = complete;
        page_.stable   = tail_.stable && received.stable;
        page_.firstIndex  = firstIndex;
        page_.resultCount = received.resultCount >= 0 ? received.resultCount : tail_.resultCount;
        page_.snapshot    = snapshot_;
    } else {
        page_ = received;
        if (query_.mode == Query::After) {
            page_.previous = after_;
            page_.complete = false; // Forward fin does not certify the archive's beginning.
        }
    }
    if (context_.logging()) {
        if (!context_.merge(key_, before_, pageSize_, page_)) {
            // A failed write must never advance a persistent watermark.
            finishError(tr("Could not persist archive page"));
            return;
        }
    }
    if (query_.mode == Query::Before && forward && !networkComplete_ && networkStable_) {
        // A large offline gap is not a request to scan the entire archive.
        // The bounded delta is now persisted with real adjacency. Jump once
        // to the latest page and leave the gap for explicit scroll/seek demand.
        // No connection is invented between the delta and the new tail.
        tail_ = {};
        after_.clear();
        QTimer::singleShot(0, this, &HistorySync::requestPage);
        return;
    }
    success_ = true;
    emit finished();
}

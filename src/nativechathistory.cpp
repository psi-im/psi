/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "nativechathistory.h"
#include "eventdb.h"
#include "filesharingmanager.h"
#include "historysync.h"
#include "iris/xmpp_htmlelement.h"
#include "messagefileshares.h"
#include "messageview.h"
#include "psiaccount.h"
#include "psioptions.h"
#include "widgets/MamChatWidget.h"
#include <QPointer>
#ifdef PSI_PLUGINS
#include "pluginmanager.h"
#endif

MessageView nativeHistoryMessage(PsiAccount *account, const XMPP::Jid &peer, const XMPP::Message &message, bool local)
{
    const bool subject = message.body().isEmpty() && !message.subject().isNull()
        && !MessageFileShares::hasFileShares(message) && message.html().body().isNull();
    QString body = subject ? message.subject() : message.body();
    auto    html = message.html();
#ifdef PSI_PLUGINS
    if (!subject) {
        auto element = html.body();
        PluginManager::instance()->appendingChatMessage(account, peer.full(), body, element, local);
        if (!element.isNull())
            html.setBody(element);
    }
#endif
    auto view = MessageView::fromPlainText(body, subject ? MessageView::Subject : MessageView::Message);
    if (PsiOptions::instance()->getOption("options.html.chat.render").toBool() && !html.body().isNull())
        view.setHtml(html.toString("span"));
    account->psi()->fileSharingManager()->fillMessageView(view, message, account);
    view.setMarkable(message.isMarkable());
    view.setReplyMetadata(message);
    return view;
}

namespace {
void applyLocalHistoryPage(MamChatWidget *timeline, PsiAccount *account, const QString &peer, bool room,
                           const QString &ownNick, const EDBResult &entries,
                           const std::function<QString(bool)> &nickname)
{
    QList<QPair<QString, MessageView>> page;
    for (const auto &entry : entries) {
        const auto event = entry->event();
        if (event->type() != PsiEvent::Message)
            continue;
        const auto  me      = event.staticCast<MessageEvent>();
        const auto  message = me->message().displayMessage();
        MessageView protocol(MessageView::System);
        const bool  local = room ? message.from().resource() == ownNick : me->originLocal();
        if (MessageView::protocolMessage(message, local, protocol)) {
            timeline->appendMessage(protocol);
            continue;
        }
        if (entry->protocolOnly())
            continue;
        auto mv = nativeHistoryMessage(account, XMPP::Jid(peer), message, local);
        mv.setHistoryId(entry->id().toLongLong());
        mv.setMessageState(entry->messageFlags());
        mv.setRevisionTime(entry->revisionTime());
        mv.setReactionTargetId(MessageView::reactionTarget(message));
        if (!entry->reactionTarget().isEmpty())
            mv.setReactionTargetId(entry->reactionTarget());
        mv.setRetractionTargetId(MessageView::retractionTarget(message));
        mv.setEncrypted(message.wasEncrypted(), message.encryptionProtocol());
        if (!entry->retractionTarget().isEmpty())
            mv.setRetractionTargetId(entry->retractionTarget());
        mv.setArchiveId(entry->archiveId());
        mv.setMessageId(message.id());
        mv.setReplaceId(message.replaceId());
        mv.setUserId(message.from().full());
        mv.setNick(room             ? message.from().resource()
                       : nickname   ? nickname(me->originLocal())
                       : me->originLocal() ? ownNick
                                           : peer);
        mv.setLocal(room ? mv.nick() == ownNick : me->originLocal());
        mv.setDateTime(message.timeStamp());
        mv.setSpooled(true);
        mv.setMarkable(message.isMarkable());
        page.append(qMakePair(mv.archiveId(), mv));
    }
    timeline->prependArchivePage(page);
}
}

// A local page is read without executing SQLite on the GUI thread. Its
// presentation projection still runs on the GUI, where account and plugin
// objects are valid. Completion is tied to the lifetime of the timeline.
void prependLocalChatHistoryAsync(MamChatWidget *timeline, PsiAccount *account, const QString &peer, bool room,
                                  const QString &ownNick, qint64 before, bool exhausted,
                                  const std::function<void(qint64, bool, bool)> &finished,
                                  const std::function<QString(bool)> &nickname,
                                  const std::function<bool()> &stillCurrent)
{
    if (!timeline || !account)
        return;
    if (exhausted) {
        finished(before, true, true);
        return;
    }
    QPointer<MamChatWidget> target(timeline);
    QPointer<PsiAccount> owner(account);
    account->edb()->requestLocalHistoryPage(
        account->id(), XMPP::Jid(peer), before, HistorySync::PageSize, timeline,
        [target, owner, peer, room, ownNick, before, finished, nickname, stillCurrent](EDBResult entries, bool success) {
            if (!target || !owner || (stillCurrent && !stillCurrent()))
                return;
            if (!success) {
                finished(before, false, false);
                return; // A failed read is not an archive boundary.
            }
            const bool done = entries.size() < HistorySync::PageSize;
            const qint64 nextBefore = entries.isEmpty() ? before : entries.first()->id().toLongLong();
            applyLocalHistoryPage(target, owner, peer, room, ownNick, entries, nickname);
            finished(nextBefore, done, true);
        });
}

NativeHistoryPage nativeArchivePage(PsiAccount *account, HistorySync *task, const XMPP::Jid &peer, bool room,
                                    const QString &ownNick, const std::function<QString(bool)> &nickname)
{
    NativeHistoryPage page;
    for (const auto &entry : task->entries()) {
        const QString uid = entry.uid;
        if (uid.isEmpty())
            continue;
        page.uids.append(uid);
        if (entry.suppressed)
            continue;
        auto archived = task->message(uid);
        if (entry.timestamp.isValid())
            archived.setTimeStamp(entry.timestamp);
        QDateTime revisionTime;
        qint64    historyId        = task->eventId(uid);
        MessageState::Flags messageState;
        QString   reactionTarget   = MessageView::reactionTarget(archived);
        QString   retractionTarget = MessageView::retractionTarget(archived);
        if (reactionTarget.isEmpty())
            reactionTarget = uid;
        if (retractionTarget.isEmpty())
            retractionTarget = uid;
        // HistorySync has already resolved the entire page in the SQL
        // worker. Never synchronously query SQLite while painting 40 rows.
        const auto resolved = task->resolvedMessage(uid);
        if (resolved.found) {
            archived = resolved.message;
            historyId = resolved.historyId; // Root ID preserves the original position.
            messageState = resolved.messageState;
            revisionTime = resolved.revisionTime;
            if (!resolved.reactionTarget.isEmpty())
                reactionTarget = resolved.reactionTarget;
            if (!resolved.retractionTarget.isEmpty())
                retractionTarget = resolved.retractionTarget;
        }
        const bool local
            = room ? archived.from().resource() == ownNick : archived.from().compare(account->jid(), false);
        MessageView protocol(MessageView::System);
        if (MessageView::protocolMessage(archived, local, protocol)) {
            page.messages.append(qMakePair(uid, protocol));
            ++page.protocolEvents;
            continue;
        }
        if (task->protocolOnly(uid)) {
            ++page.protocolEvents;
            continue;
        }
        if (room && (!archived.from().compare(peer, false) || archived.type() != XMPP::Message::Type::Groupchat))
            continue;
        auto view = nativeHistoryMessage(account, peer, archived, local);
        if (room && archived.from().resource().isEmpty() && view.type() == MessageView::Message)
            view = MessageView::fromPlainText(archived.body(), MessageView::System);
        view.setArchiveId(uid);
        view.setHistoryId(historyId);
        view.setMessageState(messageState);
        view.setRevisionTime(revisionTime);
        view.setReactionTargetId(reactionTarget);
        view.setRetractionTargetId(retractionTarget);
        view.setEncrypted(archived.wasEncrypted(), archived.encryptionProtocol());
        view.setMessageId(archived.id());
        view.setReplaceId(archived.replaceId());
        view.setUserId(archived.from().full());
        view.setNick(room ? archived.from().resource() : nickname ? nickname(local) : local ? ownNick : peer.bare());
        view.setLocal(local);
        view.setSpooled(true);
        view.setDateTime(archived.timeStamp());
        if (view.type() == MessageView::Message)
            ++page.visibleMessages;
        page.messages.append(qMakePair(uid, view));
    }
    return page;
}

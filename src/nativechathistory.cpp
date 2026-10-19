/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "nativechathistory.h"
#include "eventdb.h"
#include "filesharingmanager.h"
#include "historysync.h"
#include "iris/xmpp_htmlelement.h"
#include "messageview.h"
#include "psiaccount.h"
#include "psioptions.h"
#include "widgets/MamChatWidget.h"
#ifdef PSI_PLUGINS
#include "pluginmanager.h"
#endif

MessageView nativeHistoryMessage(PsiAccount *account, const XMPP::Jid &peer, const XMPP::Message &message, bool local)
{
    const bool subject = message.body().isEmpty() && !message.subject().isNull() && message.references().isEmpty()
        && message.urlList().isEmpty() && message.html().body().isNull();
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
    if (body.isEmpty() && html.body().isNull() && !message.urlList().isEmpty()) {
        QStringList urls;
        for (const auto &url : message.urlList())
            urls.append(url.url());
        body = urls.join(QLatin1Char('\n'));
    }
    auto view = MessageView::fromPlainText(body, subject ? MessageView::Subject : MessageView::Message);
    if (PsiOptions::instance()->getOption("options.html.chat.render").toBool() && !html.body().isNull())
        view.setHtml(html.toString("span"));
    account->psi()->fileSharingManager()->fillMessageView(view, message, account);
    view.setMarkable(message.isMarkable());
    return view;
}

void prependLocalChatHistory(MamChatWidget *timeline, PsiAccount *account, const QString &peer, bool room,
                             const QString &ownNick, qint64 &before, bool &exhausted)
{
    if (exhausted)
        return;
    const auto entries
        = account->edb()->localHistoryPage(account->id(), XMPP::Jid(peer), before, HistorySync::PageSize);
    QList<QPair<QString, MessageView>> page;
    exhausted = entries.size() < HistorySync::PageSize;
    if (!entries.isEmpty())
        before = entries.first()->id().toLongLong(); // Ascending order, deterministic timestamp/id cursor.
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
        mv.setNick(room ? message.from().resource() : me->originLocal() ? ownNick : peer);
        mv.setLocal(room ? mv.nick() == ownNick : me->originLocal());
        mv.setDateTime(message.timeStamp());
        mv.setSpooled(true);
        mv.setMarkable(message.isMarkable());
        page.append(qMakePair(mv.archiveId(), mv));
    }
    timeline->prependArchivePage(page);
}

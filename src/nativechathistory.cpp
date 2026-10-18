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
        const auto me      = event.staticCast<MessageEvent>();
        const auto message = me->message().displayMessage();
        if (message.body().isEmpty() && message.subject().isEmpty())
            continue;
        auto mv = MessageView::fromPlainText(message.body().isEmpty() ? message.subject() : message.body(),
                                             message.body().isEmpty() ? MessageView::Subject : MessageView::Message);
        if (PsiOptions::instance()->getOption("options.html.chat.render").toBool() && !message.html().body().isNull())
            mv.setHtml(message.html().toString("span"));
        mv.setHistoryId(entry->id().toLongLong());
        mv.setArchiveId(entry->archiveId());
        mv.setMessageId(message.id());
        mv.setReplaceId(message.replaceId());
        mv.setUserId(message.from().full());
        mv.setNick(room ? message.from().resource() : me->originLocal() ? ownNick : peer);
        mv.setLocal(room ? mv.nick() == ownNick : me->originLocal());
        mv.setDateTime(message.timeStamp());
        mv.setSpooled(true);
        account->psi()->fileSharingManager()->fillMessageView(mv, message, account);
        page.append(qMakePair(mv.archiveId(), mv));
    }
    timeline->prependArchivePage(page);
}

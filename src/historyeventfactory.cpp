/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historyeventfactory.h"

#include "historymessageextras.h"
#include "iris/xmpp_forwarding.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psicontactlist.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QJsonDocument>
#include <QSqlRecord>

// UI-thread-only reconstruction of legacy PsiEvent objects from immutable
// SQL records. SQL workers may return records as values, but must never touch
// PsiAccount or Client: both belong to the GUI thread.
using namespace XMPP;

PsiEvent::Ptr HistoryEventFactory::decode(PsiCon *psi, const QSqlRecord &record)
{
    PsiAccount *pa = psi->contactList()->getAccount(record.value("acc_id").toString());

    int type = record.value("type").toInt();

    if (type == 0 || type == 1 || type == 4 || type == 5 || type == 9) {
        Message m;
        m.setTimeStamp(record.value("date").toDateTime());
        if (type == 1)
            m.setType(Message::Type::Chat);
        else if (type == 4)
            m.setType(Message::Type::Error);
        else if (type == 5)
            m.setType(Message::Type::Headline);
        else if (type == 9)
            m.setType(Message::Type::Groupchat);
        else
            m.setType(Message::Type::Normal);
        m.setFrom(Jid(record.value("jid").toString()));
        QVariant text = record.value("m_text");
        if (!text.isNull()) {
            m.setBody(text.toString());
            m.setLang(record.value("lang").toString());
            m.setSubject(record.value("subject").toString());
        }
        // Older development versions stored protocol-only MAM rows without
        // their event attributes. Recover those from the retained raw envelope.
        if (pa && !record.value("archive_xml").toString().isEmpty()) {
            QDomDocument doc;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            const bool parsed = bool(doc.setContent(record.value("archive_xml").toString(),
                                                    QDomDocument::ParseOption::UseNamespaceProcessing));
#else
            const bool parsed = doc.setContent(record.value("archive_xml").toString(), true);
#endif
            XMPP::Forwarding wrapper;
            if (parsed && wrapper.fromXml(doc.documentElement().firstChildElement("forwarded"), pa->client())) {
                const auto raw = wrapper.message();
                if (raw.messageReceipt() == ReceiptReceived || !raw.displayedMarkerId().isEmpty()
                    || !raw.reactions().targetId.isEmpty()
                    || !raw.retraction().isEmpty())
                    m = raw;
            }
        }
        m.setSpooled(true);
        HistoryMessageExtras::restore(record.value("extra_data").toString(), m);
        if (QJsonDocument::fromJson(record.value("extra_data").toString().toUtf8())
                .object()
                .value("psi:decryption-error")
                .toBool())
            m.setBody(QCoreApplication::translate("EDBSqLite", "Unable to decrypt archived message"));
        MessageEvent::Ptr me(new MessageEvent(m, pa));
        me->setOriginLocal((record.value("direction").toInt() == 1));
        return me.staticCast<PsiEvent>();
    }

    if (type == 2 || type == 3 || type == 6 || type == 7 || type == 8) {
        QString subType = "subscribe";
        // if(type == 2) { // Not used (stupid "system message" from Psi <= 0.8.6)
        if (type == 3)
            subType = "subscribe";
        else if (type == 6)
            subType = "subscribed";
        else if (type == 7)
            subType = "unsubscribe";
        else if (type == 8)
            subType = "unsubscribed";

        AuthEvent::Ptr ae(new AuthEvent(Jid(record.value("jid").toString()), subType, pa));
        ae->setTimeStamp(record.value("date").toDateTime());
        return ae.staticCast<PsiEvent>();
    }
    return PsiEvent::Ptr();
}

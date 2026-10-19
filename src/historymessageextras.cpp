/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historymessageextras.h"
#include "iris/xmpp_htmlelement.h"
#include "iris/xmpp_reference.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
const QString fallbackNS = QStringLiteral("urn:xmpp:fallback:0");

// XEP-0428 ranges as [start, end] pairs; an empty pair is a whole element.
QJsonArray encodeRanges(const QList<XMPP::Message::FallbackRange> &ranges)
{
    QJsonArray array;
    for (const auto &range : ranges)
        array.append(range.isWhole() ? QJsonArray() : QJsonArray { range.start, range.end });
    return array;
}

QList<XMPP::Message::FallbackRange> decodeRanges(const QJsonArray &array)
{
    QList<XMPP::Message::FallbackRange> ranges;
    for (const auto &value : array) {
        const auto pair = value.toArray();
        if (pair.isEmpty())
            ranges.append(XMPP::Message::FallbackRange());
        else if (pair.size() == 2 && pair.at(0).toInt(-1) >= 0 && pair.at(1).toInt(-1) >= pair.at(0).toInt())
            ranges.append({ pair.at(0).toInt(), pair.at(1).toInt() });
    }
    return ranges;
}

bool parseReferenceXml(QDomDocument &doc, const QString &xml)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return bool(doc.setContent(xml, QDomDocument::ParseOption::UseNamespaceProcessing));
#else
    return doc.setContent(xml, true);
#endif
}
}

QString HistoryMessageExtras::encode(const XMPP::Message &message)
{
    if (message.isNull())
        return {};
    QJsonObject extra;
    extra.insert("psi:decoded", true);
    QJsonArray urls;
    for (const auto &url : message.urlList()) {
        if (url.url().isEmpty())
            continue;
        QJsonArray pair;
        pair.append(url.url());
        if (!url.desc().isEmpty())
            pair.append(url.desc());
        urls.append(pair);
    }
    if (!urls.isEmpty())
        extra.insert(QStringLiteral("jabber:x:oob"), urls);
    QJsonArray references;
    for (const auto &reference : message.references()) {
        if (!reference.isValid())
            continue;
        QDomDocument doc;
        doc.appendChild(reference.toXml(&doc));
        references.append(doc.toString(-1));
    }
    if (!references.isEmpty())
        extra.insert(XMPP::REFERENCE_NS, references);
    // Shares may locate their body text only through XEP-0428 fallbacks.
    QJsonArray fallbacks;
    for (const auto &fallback : message.fallbacks())
        fallbacks.append(QJsonObject { { "for", fallback.for_ },
                                       { "body", encodeRanges(fallback.body) },
                                       { "subject", encodeRanges(fallback.subject) } });
    if (!fallbacks.isEmpty())
        extra.insert(fallbackNS, fallbacks);

    QJsonObject events;
    if (message.messageReceipt() == XMPP::ReceiptReceived)
        events.insert("receipt", message.messageReceiptId());
    if (!message.displayedMarkerId().isEmpty())
        events.insert("displayed", message.displayedMarkerId());
    if (message.isMarkable())
        extra.insert(QStringLiteral("urn:xmpp:chat-markers:0"), true);
    if (!message.reactions().targetId.isEmpty()) {
        events.insert("target", message.reactions().targetId);
        QJsonArray reactions;
        for (const auto &reaction : message.reactions().reactions)
            reactions.append(reaction);
        events.insert("reactions", reactions);
    }
    if (!message.retraction().isEmpty())
        events.insert("retract", message.retraction());
    if (!events.isEmpty())
        extra.insert("psi:events", events);
    if (!message.html().body().isNull())
        extra.insert("psi:html", message.html().toString());
    QJsonArray stanzaIds;
    for (const auto &id : message.stanzaIds())
        stanzaIds.append(QJsonObject { { "by", id.by.full() }, { "id", id.id } });
    if (!stanzaIds.isEmpty())
        extra.insert("psi:ids", stanzaIds);
    if (!message.originId().isEmpty())
        extra.insert("psi:origin", message.originId());
    if (message.wasEncrypted()) {
        extra.insert("psi:encrypted", true);
        extra.insert("psi:encryption-protocol", message.encryptionProtocol());
    }

    // Preserve message identity even without media references. This is
    // essential for correlating existing local history with MAM later.
    // The stanza id is *not* an authenticated MAM archive UID.
    if (!message.id().isEmpty() || !message.from().isEmpty() || !message.to().isEmpty()) {
        extra.insert(QStringLiteral("psi:message"),
                     QJsonObject { { "version", 1 },
                                   { "id", message.id() },
                                   { "from", message.from().full() },
                                   { "to", message.to().full() },
                                   { "replace", message.replaceId() } });
    }
    return extra.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
}

void HistoryMessageExtras::restore(const QString &json, XMPP::Message &message)
{
    const auto extra = QJsonDocument::fromJson(json.toUtf8()).object();
    if (extra.value("psi:has-body").toBool() && message.body().isEmpty())
        message.setBody(QStringLiteral(""));
    if (extra.value("psi:encrypted").toBool()) {
        message.setWasEncrypted(true);
        message.setEncryptionProtocol(extra.value("psi:encryption-protocol").toString());
    }
    const auto events = extra.value("psi:events").toObject();
    if (events.contains("receipt")) {
        message.setMessageReceipt(XMPP::ReceiptReceived);
        message.setMessageReceiptId(events.value("receipt").toString());
    }
    if (events.contains("displayed"))
        message.setDisplayedMarkerId(events.value("displayed").toString());
    if (extra.value(QStringLiteral("urn:xmpp:chat-markers:0")).toBool())
        message.setMarkable(true);
    if (events.contains("target")) {
        QSet<QString> reactions;
        for (const auto &reaction : events.value("reactions").toArray())
            reactions.insert(reaction.toString());
        message.setReactions({ events.value("target").toString(), reactions });
    }
    if (events.contains("retract"))
        message.setRetraction(events.value("retract").toString());
    if (extra.contains("psi:html")) {
        QDomDocument doc;
        if (parseReferenceXml(doc, extra.value("psi:html").toString()))
            message.setHTML(XMPP::HTMLElement(doc.documentElement()));
    }
    for (const auto &value : extra.value("psi:ids").toArray()) {
        const auto id = value.toObject();
        message.addStanzaId({ XMPP::Jid(id.value("by").toString()), id.value("id").toString() });
    }
    for (const auto &value : extra.value(QStringLiteral("jabber:x:oob")).toArray()) {
        const auto pair = value.toArray();
        if (!pair.isEmpty() && pair.first().isString() && !pair.first().toString().isEmpty())
            message.urlAdd(XMPP::Url(pair.first().toString(), pair.size() > 1 ? pair.at(1).toString() : QString()));
    }
    QList<XMPP::Reference> references;
    for (const auto &value : extra.value(XMPP::REFERENCE_NS).toArray()) {
        QDomDocument    doc;
        XMPP::Reference reference;
        if (value.isString() && parseReferenceXml(doc, value.toString())
            && doc.documentElement().namespaceURI() == XMPP::REFERENCE_NS && reference.fromXml(doc.documentElement()))
            references.append(reference);
    }
    if (!references.isEmpty())
        message.setReferences(references);
    QList<XMPP::Message::Fallback> fallbacks;
    for (const auto &value : extra.value(fallbackNS).toArray()) {
        const auto object = value.toObject();
        fallbacks.append({ object.value("for").toString(), decodeRanges(object.value("body").toArray()),
                           decodeRanges(object.value("subject").toArray()) });
    }
    if (!fallbacks.isEmpty())
        message.setFallbacks(fallbacks);

    const auto envelope = extra.value(QStringLiteral("psi:message")).toObject();
    if (envelope.value("version").toInt() == 1) {
        message.setId(envelope.value("id").toString());
        message.setReplaceId(envelope.value("replace").toString());
        const XMPP::Jid from(envelope.value("from").toString()), to(envelope.value("to").toString());
        if (from.isValid() && !from.isEmpty())
            message.setFrom(from);
        if (to.isValid() && !to.isEmpty())
            message.setTo(to);
    }
    // setId() also sets Iris's default origin-id; restore the explicit origin
    // only after restoring the stanza ID.
    if (extra.contains("psi:origin"))
        message.setOriginId(extra.value("psi:origin").toString());
}

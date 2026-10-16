/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historymessageextras.h"
#include "iris/xmpp_reference.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
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
    QJsonObject extra;
    QJsonArray  urls;
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
    if (!references.isEmpty()) {
        extra.insert(XMPP::REFERENCE_NS, references);
        // In particular retain the full sender resource for Jingle retrieval.
        extra.insert(QStringLiteral("psi:message"),
                     QJsonObject { { "version", 1 },
                                   { "id", message.id() },
                                   { "from", message.from().full() },
                                   { "to", message.to().full() } });
    }
    return extra.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
}

void HistoryMessageExtras::restore(const QString &json, XMPP::Message &message)
{
    const auto extra = QJsonDocument::fromJson(json.toUtf8()).object();
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
    if (!references.isEmpty()) {
        message.setReferences(references);
        const auto envelope = extra.value(QStringLiteral("psi:message")).toObject();
        if (envelope.value("version").toInt() == 1) {
            message.setId(envelope.value("id").toString());
            const XMPP::Jid from(envelope.value("from").toString()), to(envelope.value("to").toString());
            if (from.isValid() && !from.isEmpty())
                message.setFrom(from);
            if (to.isValid() && !to.isEmpty())
                message.setTo(to);
        }
    }
}

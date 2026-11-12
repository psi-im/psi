/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historymessageextras.h"
#include "iris/xmpp_htmlelement.h"
#include "iris/xmpp_reference.h"
#include "oobmediapolicy.h"
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <algorithm>

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

void restoreUrlsAndReferences(const QJsonObject &extra, XMPP::Message &message, bool includeSfs = true)
{
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
    if (!includeSfs)
        return;
    for (const auto &value : extra.value(XMPP::StatelessFileSharing::NS).toArray()) {
        QDomDocument doc;
        if (value.isString() && parseReferenceXml(doc, value.toString())) {
            const XMPP::StatelessFileSharing::FileSharing sharing(doc.documentElement());
            if (sharing.isValid())
                message.addFileSharing(sharing);
        }
    }
}
}

namespace {
// Namespace prefixes and attribute iteration order are not identity. This
// representation remains stable across Qt versions and parser round trips.
QJsonArray canonicalElement(const QDomElement &element)
{
    QJsonObject attributes;
    const auto  map = element.attributes();
    for (int i = 0; i < map.size(); ++i) {
        const auto attribute = map.item(i).toAttr();
        if (attribute.name() == QLatin1String("xmlns") || attribute.prefix() == QLatin1String("xmlns"))
            continue;
        attributes.insert(attribute.namespaceURI() + QLatin1Char('|')
                              + (attribute.localName().isEmpty() ? attribute.name() : attribute.localName()),
                          attribute.value());
    }
    QJsonArray children;
    for (auto child = element.firstChild(); !child.isNull(); child = child.nextSibling()) {
        if (child.isElement())
            children.append(canonicalElement(child.toElement()));
        else if (child.isText())
            children.append(child.nodeValue());
    }
    return { element.namespaceURI(), element.localName().isEmpty() ? element.tagName() : element.localName(),
             attributes, children };
}
}

QList<MessageFileShares::FileShare> HistoryMessageExtras::indexedFiles(const XMPP::Message &message, int version)
{
    if (version == mediaRefsVersion)
        return MessageFileShares::collect(message).files;
    if (version != 1)
        return {};

    // Frozen version-1 projection. Normalize SIMS/SFS first, retaining the
    // original non-file slots, then process OOB in its historical order. In
    // particular, an unsupported reference could consume an OOB description.
    // Current presentation code must never reinterpret persisted ordinals.
    XMPP::Message withoutOob;
    withoutOob.setReferences(message.references());
    for (const auto &sharing : message.fileSharings())
        withoutOob.addFileSharing(sharing);
    const auto                          files = MessageFileShares::collect(withoutOob).files;
    QList<MessageFileShares::FileShare> legacy;
    int                                 next = 0;
    for (const auto &reference : message.references()) {
        const MessageFileShares::FileShare file { reference, {} };
        legacy.append(file.hasContentHashes() || file.url().isValid() ? files.at(next++) : file);
    }
    for (; next < files.size(); ++next)
        legacy.append(files.at(next));

    QSet<QString> blocked, seen;
    for (const auto &sharing : message.fileSharings()) {
        if (sharing.file().hasComputedHashes())
            continue;
        for (const auto &source : sharing.sources().items()) {
            if (source.type() != XMPP::StatelessFileSharing::Source::Type::UrlData)
                continue;
            const auto url = OobMediaPolicy::normalizedHttpSource(source.url().toString(QUrl::FullyEncoded));
            if (url.isValid() && url.scheme() != QLatin1String("https"))
                blocked.insert(url.toString(QUrl::FullyEncoded));
        }
    }
    // Reuse source eligibility and manual-fetch policy, but not version 2's
    // description distribution or its compact reference ordinal space.
    const auto normalized = MessageFileShares::collect(message).files;
    const auto body       = message.body();
    for (const auto &oob : message.urlList()) {
        const auto source = OobMediaPolicy::normalizedHttpSource(oob.url());
        if (!source.isValid())
            continue;
        const auto canonical = source.toString(QUrl::FullyEncoded);
        if (blocked.contains(canonical))
            continue;
        const auto candidate = std::find_if(normalized.cbegin(), normalized.cend(), [&source](const auto &file) {
            return file.fromOob && file.url() == source;
        });
        const bool redundant = seen.contains(canonical) || candidate == normalized.cend();
        seen.insert(canonical);
        if (redundant) {
            if (oob.desc().isEmpty())
                continue;
            for (auto &file : legacy) {
                const auto &r = file.reference;
                if (!file.description.isEmpty() || !r.isValid() || r.type() != XMPP::Reference::Data)
                    continue;
                const auto sources = r.mediaSharing().isValid() ? r.mediaSharing().sources : QStringList { r.uri() };
                if (std::any_of(sources.cbegin(), sources.cend(), [&source](const auto &uri) {
                        return OobMediaPolicy::normalizedHttpSource(uri) == source;
                    })) {
                    file.description = oob.desc();
                    break;
                }
            }
            continue;
        }
        auto inBody = oob.url();
        int  at     = body.indexOf(inBody);
        if (at < 0 && inBody.indexOf(QLatin1Char('#')) > 0) {
            inBody = inBody.left(inBody.indexOf(QLatin1Char('#')));
            at     = body.indexOf(inBody);
        }
        const bool onlyUrl = at >= 0 && body.trimmed() == inBody;
        if (at >= 0 && !onlyUrl)
            continue;
        auto file           = *candidate;
        file.description    = oob.desc();
        const int  end      = at + int(inBody.size()) - 1;
        const bool overlaps = std::any_of(legacy.cbegin(), legacy.cend(), [at, end](const auto &other) {
            const auto &r = other.reference;
            return r.begin() >= 0 && r.end() >= at && r.begin() <= end;
        });
        if (onlyUrl && !overlaps)
            file.reference.setRange(at, end);
        legacy.append(file);
    }
    if (legacy.isEmpty() && AesGcmUrl::parse(QUrl(message.body().trimmed())))
        return MessageFileShares::collect(message).files;
    return legacy;
}

QJsonArray HistoryMessageExtras::mediaRefs(const XMPP::Message &message, int version)
{
    QJsonArray index;
    const auto files = indexedFiles(message, version);
    for (int ordinal = 0; ordinal < files.size(); ++ordinal) {
        const auto &file = files.at(ordinal);
        if (!file.hasContentHashes() && !file.url().isValid())
            continue;
        QDomDocument doc;
        const auto  &reference = file.reference;
        const auto  &media     = reference.mediaSharing();
        // Reference::toXml omits media without sources. Pending SFS files still
        // need their metadata to distinguish a replacement at the same ordinal.
        const QJsonArray content { int(reference.type()),
                                   reference.uri(),
                                   reference.begin(),
                                   reference.end(),
                                   reference.anchor(),
                                   canonicalElement(media.file.toMetadataXml(&doc)),
                                   QJsonArray::fromStringList(media.sources),
                                   file.description,
                                   file.manualFetch };
        const auto       fingerprint = QCryptographicHash::hash(QJsonDocument(content).toJson(QJsonDocument::Compact),
                                                                QCryptographicHash::Sha256)
                                     .toHex();
        QJsonArray hashes;
        for (const auto &hash : media.file.computedHashes())
            hashes.append(QJsonObject { { "algorithm", hash.stringType() },
                                        { "digest", QString::fromLatin1(hash.data().toHex()) },
                                        { "id", hash.toString() } });
        const auto name = media.file.name().isEmpty() ? QFileInfo(file.url().path()).fileName() : media.file.name();
        auto       mime = media.file.mediaType().toLower();
        if (mime.isEmpty() || mime == QLatin1String("application/octet-stream"))
            mime = QMimeDatabase().mimeTypeForFile(name, QMimeDatabase::MatchExtension).name();
        const int kind = mime.startsWith(QLatin1String("image/")) ? 1
            : mime.startsWith(QLatin1String("video/")) || mime.startsWith(QLatin1String("audio/")) ? 2
                                                                                                    : 4;
        QJsonObject resource { { "hashes", hashes }, { "name", name }, { "mime", mime }, { "kind", kind } };
        if (const auto size = media.file.size())
            resource.insert("size", QString::number(*size));
        if (!file.hasContentHashes())
            resource.insert("url", MessageFileShares::urlIdentity(file.url()).toString());
        index.append(QJsonObject {
            { "ordinal", ordinal }, { "fingerprint", QString::fromLatin1(fingerprint) }, { "resource", resource } });
    }
    return index;
}

HistoryArchive::Entry HistoryMessageExtras::identity(const XMPP::Message &message, bool local, const QString &uid)
{
    HistoryArchive::Entry entry;
    entry.uid       = uid;
    entry.direction = local ? 1 : 2;
    entry.type      = message.type() == XMPP::Message::Type::Chat ? 1
             : message.type() == XMPP::Message::Type::Normal      ? 0
                                                                  : -1;
    entry.messageId = message.id();
    entry.replaceId = message.replaceId();
    entry.sender    = message.from().full();
    QJsonObject extra;
    if (!message.originId().isEmpty())
        extra.insert("psi:origin", message.originId());
    QJsonArray ids;
    for (const auto &id : message.stanzaIds())
        ids.append(QJsonObject { { "by", id.by.full() }, { "id", id.id } });
    extra.insert("psi:ids", ids);
    if (message.messageReceipt() == XMPP::ReceiptReceived || !message.displayedMarkerId().isEmpty()
        || !message.reactions().targetId.isEmpty() || !message.retraction().isEmpty())
        extra.insert("psi:protocol-only", true);
    entry.extra = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
    return entry;
}

QString HistoryMessageExtras::encode(const XMPP::Message &message)
{
    if (message.isNull())
        return {};
    QJsonObject extra;
    extra.insert("psi:decoded", true);
    if (MessageFileShares::hasFileShares(message)) {
        extra.insert("psi:media-refs",
                     QJsonObject { { "version", mediaRefsVersion }, { "items", mediaRefs(message) } });
        if (extra.contains("psi:media-refs"))
            extra.insert("psi:media-resources", 2);
    }
    if (message.reply().isValid())
        extra.insert(QStringLiteral("urn:xmpp:reply:0"),
                     QJsonObject { { "id", message.reply().id }, { "to", message.reply().to.full() } });
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
    QJsonArray fileSharings;
    for (const auto &sharing : message.fileSharings()) {
        QDomDocument doc;
        doc.appendChild(sharing.toXml(&doc));
        fileSharings.append(doc.toString(-1));
    }
    if (!fileSharings.isEmpty())
        extra.insert(XMPP::StatelessFileSharing::NS, fileSharings);
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
    if (message.type() == XMPP::Message::Type::Groupchat || !message.occupantId().isEmpty())
        extra.insert("psi:occupant", message.occupantId());
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

void HistoryMessageExtras::restoreFileSharing(const QString &json, XMPP::Message &message, bool includeSfs)
{
    restoreUrlsAndReferences(QJsonDocument::fromJson(json.toUtf8()).object(), message, includeSfs);
}

void HistoryMessageExtras::restore(const QString &json, XMPP::Message &message)
{
    const auto extra = QJsonDocument::fromJson(json.toUtf8()).object();
    const auto reply = extra.value(QStringLiteral("urn:xmpp:reply:0")).toObject();
    if (!reply.isEmpty())
        message.setReply({ XMPP::Jid(reply.value("to").toString()), reply.value("id").toString() });
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
    restoreUrlsAndReferences(extra, message);
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
    if (extra.contains("psi:occupant"))
        message.setOccupantId(extra.value("psi:occupant").toString());
}

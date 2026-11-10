/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "messageattachments.h"

#include "iris/xmpp_message.h"
#include "oobmediapolicy.h"

#include <QCryptographicHash>
#include <QSet>
#include <algorithm>
#include <optional>
#include <utility>

namespace MessageAttachments {
namespace {
    // The HTTP(S) sources a reference provides, in the form OOB URLs are compared in.
    QStringList httpSources(const XMPP::Reference &reference)
    {
        QStringList sources;
        if (!reference.isValid() || reference.type() != XMPP::Reference::Data)
            return sources;
        const auto       &media      = reference.mediaSharing();
        const QStringList candidates = media.isValid() ? media.sources : QStringList { reference.uri() };
        for (const auto &candidate : candidates) {
            const QUrl http = OobMediaPolicy::normalizedHttpSource(candidate);
            if (http.isValid())
                sources.append(http.toString(QUrl::FullyEncoded));
        }
        return sources;
    }

    // A file sharing in the form references have, or nothing if none of its sources can
    // be used. Without sources it is pending, and the sender may still have it.
    std::optional<Attachment> fromFileSharing(const XMPP::StatelessFileSharing::FileSharing &sharing)
    {
        using XMPP::StatelessFileSharing::Disposition;
        using XMPP::StatelessFileSharing::Source;
        QStringList sources, secure;
        const auto  items = sharing.sources().items();
        for (const auto &source : items) {
            if (source.type() == Source::Type::UrlData) {
                sources.append(source.url().toString(QUrl::FullyEncoded));
                if (source.url().scheme() == QLatin1String("https"))
                    secure.append(sources.last());
            } else if (source.type() == Source::Type::JinglePub) {
                sources.append(QStringLiteral("xmpp:%1?jingle-ft").arg(source.jinglePub().from().full()));
            }
        }
        if (!items.isEmpty() && sources.isEmpty())
            return {};
        Attachment attachment;
        attachment.manualFetch = sharing.disposition() == Disposition::Attachment;
        if (sharing.file().hasComputedHashes()) {
            XMPP::MediaSharing media;
            media.file    = sharing.file();
            media.sources = sources;
            attachment.reference = XMPP::Reference(XMPP::Reference::Data, sources.value(0));
            attachment.reference.setMediaSharing(media);
            return attachment;
        }
        // Without hashes nothing could tell a tampered file, so XEP-0447 leaves only secure sources.
        if (secure.isEmpty())
            return {};
        attachment.reference   = XMPP::Reference(XMPP::Reference::Data, secure.first());
        attachment.description = sharing.file().description();
        return attachment;
    }

    bool sameFile(const XMPP::Reference &reference, const XMPP::Reference &other)
    {
        if (!reference.isValid() || !reference.mediaSharing().isValid())
            return false;
        const auto hashes = reference.mediaSharing().file.computedHashes();
        const auto others = other.mediaSharing().file.computedHashes();
        return std::any_of(hashes.cbegin(), hashes.cend(),
                           [&others](const XMPP::Hash &hash) { return others.contains(hash); });
    }

    bool overlapsRange(const QList<Attachment> &attachments, int begin, int end)
    {
        for (const auto &attachment : attachments) {
            const auto &reference = attachment.reference;
            if (reference.begin() >= 0 && reference.end() >= begin && reference.begin() <= end)
                return true;
        }
        return false;
    }
} // namespace

bool Attachment::isFile() const
{
    return reference.isValid() && reference.mediaSharing().isValid() && reference.mediaSharing().file.hasComputedHashes();
}

QUrl Attachment::url() const
{
    if (!reference.isValid() || reference.mediaSharing().isValid() || reference.type() != XMPP::Reference::Data)
        return {};
    return OobMediaPolicy::normalizedHttpSource(reference.uri());
}

Result collect(const XMPP::Message &message)
{
    Result        result;
    QSet<QString> covered;
    for (const auto &reference : message.references())
        result.attachments.append({ reference, {} });
    // After the references: links in history count attachments by their position.
    for (const auto &sharing : message.fileSharings()) {
        const auto attachment = fromFileSharing(sharing);
        if (attachment
            && std::none_of(result.attachments.cbegin(), result.attachments.cend(), [&attachment](const Attachment &a) {
                   return sameFile(attachment->reference, a.reference);
               }))
            result.attachments.append(*attachment);
    }
    for (const auto &attachment : std::as_const(result.attachments))
        for (const auto &source : httpSources(attachment.reference))
            covered.insert(source);

    const auto  urls = message.urlList();
    QStringList originals;
    for (const auto &url : urls)
        originals.append(url.url());
    const auto candidates = OobMediaPolicy::classify(covered, originals);

    const QString body = message.body();
    for (int i = 0; i < urls.size(); ++i) {
        const auto &url       = urls.at(i);
        const auto &candidate = candidates.at(i);
        if (!candidate.source.isValid()) {
            result.links.append(url);
            continue;
        }
        const QString canonical = candidate.source.toString(QUrl::FullyEncoded);
        if (candidate.redundant) {
            // Already an attachment, but the OOB description may still name it.
            if (!url.desc().isEmpty()) {
                for (auto &attachment : result.attachments) {
                    if (attachment.description.isEmpty() && httpSources(attachment.reference).contains(canonical)) {
                        attachment.description = url.desc();
                        break;
                    }
                }
            }
            continue;
        }
        // The body may spell the URL as sent or without its fragment.
        QString inBody = url.url();
        int     at     = body.indexOf(inBody);
        if (at < 0 && inBody.indexOf(QLatin1Char('#')) > 0) {
            inBody = inBody.left(inBody.indexOf(QLatin1Char('#')));
            at     = body.indexOf(inBody);
        }
        // As in Conversations, a URL within other text stays an ordinary
        // link. A body that is just the URL gives way to the attachment.
        const bool onlyUrl = at >= 0 && body.trimmed() == inBody;
        if (at >= 0 && !onlyUrl)
            continue;
        XMPP::Reference reference(XMPP::Reference::Data, url.url());
        const int       end = at + int(inBody.size()) - 1; // Psi treats 'end' as inclusive
        if (onlyUrl && !overlapsRange(result.attachments, at, end))
            reference.setRange(at, end);
        result.attachments.append({ reference, url.desc() });
    }
    return result;
}

XMPP::Hash urlIdentity(const QUrl &url)
{
    const QByteArray key = QByteArrayLiteral("psi-oob-url-v1:") + url.toString(QUrl::FullyEncoded).toUtf8();
    return XMPP::Hash(XMPP::Hash::Sha256, QCryptographicHash::hash(key, QCryptographicHash::Sha256));
}
} // namespace MessageAttachments

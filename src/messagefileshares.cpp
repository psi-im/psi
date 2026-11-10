/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "messagefileshares.h"

#include "iris/xmpp_message.h"
#include "oobmediapolicy.h"

#include <QCryptographicHash>
#include <QSet>
#include <algorithm>
#include <optional>
#include <utility>

namespace MessageFileShares {
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
    std::optional<FileShare> fromFileSharing(const XMPP::StatelessFileSharing::FileSharing &sharing)
    {
        using XMPP::StatelessFileSharing::Disposition;
        using XMPP::StatelessFileSharing::Source;
        QStringList sources, secure;
        const auto  items = sharing.sources().items();
        for (const auto &source : items) {
            if (source.type() == Source::Type::UrlData) {
                if (!OobMediaPolicy::normalizedHttpSource(source.url().toString(QUrl::FullyEncoded)).isValid())
                    continue;
                sources.append(source.url().toString(QUrl::FullyEncoded));
                if (source.url().scheme() == QLatin1String("https"))
                    secure.append(sources.last());
            } else if (source.type() == Source::Type::JinglePub) {
                sources.append(QStringLiteral("xmpp:%1?jingle-ft").arg(source.jinglePub().from().full()));
            }
        }
        if (!items.isEmpty() && sources.isEmpty())
            return {};
        FileShare file;
        file.manualFetch = sharing.disposition() == Disposition::Attachment;
        if (sharing.file().hasComputedHashes()) {
            XMPP::MediaSharing media;
            media.file     = sharing.file();
            media.sources  = sources;
            file.reference = XMPP::Reference(XMPP::Reference::Data, sources.value(0));
            file.reference.setMediaSharing(media);
            return file;
        }
        // Without hashes nothing could tell a tampered file, so XEP-0447 leaves only secure sources.
        if (secure.isEmpty())
            return {};
        file.reference   = XMPP::Reference(XMPP::Reference::Data, secure.first());
        file.description = sharing.file().description();
        return file;
    }

    bool mergeFile(FileShare &target, const FileShare &incoming)
    {
        if (!target.hasContentHashes() || !incoming.hasContentHashes())
            return false;
        auto       media  = target.reference.mediaSharing();
        const auto other  = incoming.reference.mediaSharing();
        const auto hashes = media.file.computedHashes();
        const auto others = other.file.computedHashes();
        const bool common = std::any_of(hashes.cbegin(), hashes.cend(), [&others](const XMPP::Hash &hash) {
            return hash.type() != XMPP::Hash::Sha1 && !hash.data().isEmpty() && others.contains(hash);
        });
        if (!common || (media.file.size() && other.file.size() && media.file.size() != other.file.size())
            || !media.file.merge(other.file))
            return false;
        media.sources += other.sources;
        media.sources.removeDuplicates();
        target.reference.setMediaSharing(media);
        target.manualFetch = target.manualFetch || incoming.manualFetch;
        if (target.description.isEmpty())
            target.description = incoming.description;
        return true;
    }

    bool overlapsRange(const QList<FileShare> &files, int begin, int end)
    {
        for (const auto &file : files) {
            const auto &reference = file.reference;
            if (reference.begin() >= 0 && reference.end() >= begin && reference.begin() <= end)
                return true;
        }
        return false;
    }
} // namespace

bool FileShare::hasContentHashes() const
{
    return reference.isValid() && reference.mediaSharing().isValid()
        && reference.mediaSharing().file.hasComputedHashes();
}

QUrl FileShare::url() const
{
    if (!reference.isValid() || reference.mediaSharing().isValid() || reference.type() != XMPP::Reference::Data)
        return {};
    return OobMediaPolicy::normalizedHttpSource(reference.uri());
}

Result collect(const XMPP::Message &message)
{
    Result        result;
    QSet<QString> covered, blocked, manualSources;
    for (const auto &sharing : message.fileSharings()) {
        for (const auto &source : sharing.sources().items()) {
            if (source.type() != XMPP::StatelessFileSharing::Source::Type::UrlData)
                continue;
            const auto url = OobMediaPolicy::normalizedHttpSource(source.url().toString(QUrl::FullyEncoded));
            if (!url.isValid())
                continue;
            const auto canonical = url.toString(QUrl::FullyEncoded);
            if (sharing.disposition() == XMPP::StatelessFileSharing::Disposition::Attachment)
                manualSources.insert(canonical);
            if (!sharing.file().hasComputedHashes() && url.scheme() != QLatin1String("https"))
                blocked.insert(canonical);
        }
    }
    for (const auto &reference : message.references()) {
        const FileShare file { reference, {} };
        if (file.hasContentHashes() || file.url().isValid())
            result.files.append(file);
    }
    // Preserve protocol order; persisted history ordinals address this projection.
    for (const auto &sharing : message.fileSharings()) {
        const auto file = fromFileSharing(sharing);
        if (!file)
            continue;
        bool merged = false;
        for (auto &existing : result.files) {
            if (mergeFile(existing, *file)) {
                merged = true;
                break;
            }
        }
        if (!merged)
            result.files.append(*file);
    }
    // Even an unsupported SIMS descriptor can identify its compatibility OOB
    // copy. Do not turn that copy into a less constrained URL-only download.
    for (const auto &reference : message.references())
        for (const auto &source : httpSources(reference))
            covered.insert(source);
    for (const auto &file : std::as_const(result.files))
        for (const auto &source : httpSources(file.reference))
            covered.insert(source);

    const auto  urls = message.urlList();
    QStringList originals;
    for (const auto &url : urls)
        originals.append(url.url());
    const auto candidates = OobMediaPolicy::classify(covered, originals);

    for (int i = 0; i < urls.size(); ++i) {
        const auto &url       = urls.at(i);
        const auto &candidate = candidates.at(i);
        if (!candidate.source.isValid()) {
            result.links.append(url);
            continue;
        }
        const QString canonical = candidate.source.toString(QUrl::FullyEncoded);
        if (blocked.contains(canonical))
            continue; // Preserve the fallback body instead of automatically fetching an insecure SFS source.
        if (candidate.redundant) {
            // Already an file, but the OOB description may still name it.
            if (!url.desc().isEmpty()) {
                for (auto &file : result.files) {
                    if (file.description.isEmpty() && httpSources(file.reference).contains(canonical)) {
                        file.description = url.desc();
                        break;
                    }
                }
            }
            continue;
        }
        result.files.append({ XMPP::Reference(XMPP::Reference::Data, url.url()), url.desc(), false, true });
    }
    // Compatibility copies with only weak/no common hashes still must not
    // automatically fetch the very HTTP source declared manual by SFS.
    for (auto &file : result.files)
        for (const auto &source : httpSources(file.reference))
            if (manualSources.contains(source))
                file.manualFetch = true;
    return result;
}

Result forDisplay(const XMPP::Message &message)
{
    auto       result = collect(message);
    const auto body   = message.body();
    for (auto it = result.files.begin(); it != result.files.end();) {
        if (!it->fromOob) {
            ++it;
            continue;
        }
        // The body may spell the OOB URL as sent or without its fragment.
        QString inBody = it->reference.uri();
        int     at     = body.indexOf(inBody);
        if (at < 0 && inBody.indexOf(QLatin1Char('#')) > 0) {
            inBody = inBody.left(inBody.indexOf(QLatin1Char('#')));
            at     = body.indexOf(inBody);
        }
        const bool onlyUrl = at >= 0 && body.trimmed() == inBody;
        if (at >= 0 && !onlyUrl) {
            it = result.files.erase(it);
            continue;
        }
        const int end      = at + int(inBody.size()) - 1; // Psi treats 'end' as inclusive
        bool      overlaps = false;
        for (const auto &reference : message.references())
            if (reference.begin() >= 0 && reference.end() >= at && reference.begin() <= end)
                overlaps = true;
        if (onlyUrl && !overlaps && !overlapsRange(result.files, at, end))
            it->reference.setRange(at, end);
        ++it;
    }
    // XEP-0385 senders may locate a share's body text with XEP-0428
    // fallbacks instead of reference begin/end. Assign those ranges, in
    // body order, to the media shares that have no range of their own.
    QList<QPair<int, int>> fallbackRanges; // UTF-16 [from, to)
    for (const auto &ns :
         { QStringLiteral("urn:xmpp:sims:1"), XMPP::StatelessFileSharing::NS, QStringLiteral("jabber:x:oob") }) {
        for (const auto &range : message.bodyFallbacks(ns)) {
            int from = -1, to = -1;
            if (range.toUtf16(body, from, to) && to > from && !fallbackRanges.contains({ from, to }))
                fallbackRanges.append({ from, to });
        }
        if (!fallbackRanges.isEmpty())
            break; // Ranges for the shares take precedence; OOB ranges are a fallback for them.
    }
    std::sort(fallbackRanges.begin(), fallbackRanges.end());
    for (auto &file : result.files) {
        if (fallbackRanges.isEmpty())
            break;
        auto &r = file.reference;
        if (r.begin() < 0 && r.mediaSharing().isValid()) {
            const auto range = fallbackRanges.takeFirst();
            r.setRange(range.first, range.second - 1); // Psi treats 'end' as inclusive
        }
    }

    return result;
}

XMPP::Hash urlIdentity(const QUrl &url)
{
    const QByteArray key = QByteArrayLiteral("psi-oob-url-v1:") + url.toString(QUrl::FullyEncoded).toUtf8();
    return XMPP::Hash(XMPP::Hash::Sha256, QCryptographicHash::hash(key, QCryptographicHash::Sha256));
}
} // namespace MessageFileShares

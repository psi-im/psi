/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingnormalizer.h"

#include "oobmediapolicy.h"
#include "iris/xmpp_message.h"
#include "iris/xmpp_reference.h"
#include "iris/xmpp_url.h"

#include <QSet>

namespace FileSharingNormalizer {

XMPP::UrlList remainingLinks(const XMPP::Message &message)
{
    XMPP::UrlList links;
    for (const auto &url : message.urlList()) {
        if (!OobMediaPolicy::normalizedHttpSource(url.url()).isValid())
            links.append(url);
    }
    return links;
}

PreparedMedia prepareForDisplay(const XMPP::Message &message)
{
    PreparedMedia result { message.references(), {} };
    const auto originalUrls = message.urlList();
    if (originalUrls.isEmpty())
        return result;

    auto &refs = result.references;
    const QString body = message.body();
    QSet<QString> simsSources;
    for (const auto &reference : refs) {
        if (!reference.isValid() || reference.type() != XMPP::Reference::Data)
            continue;
        const auto &media = reference.mediaSharing();
        // Incomplete SIMS cannot replace an OOB fallback. A previously
        // normalized URL-only reference can, so encoding is idempotent.
        if (media.isValid() && media.file.hasComputedHashes()) {
            for (const auto &source : media.sources) {
                const QUrl http = OobMediaPolicy::normalizedHttpSource(source);
                if (http.isValid())
                    simsSources.insert(http.toString(QUrl::FullyEncoded));
            }
        } else if (!media.isValid()) {
            const QUrl http = OobMediaPolicy::normalizedHttpSource(reference.uri());
            if (http.isValid())
                simsSources.insert(http.toString(QUrl::FullyEncoded));
        }
    }

    QStringList urlStrings;
    for (const auto &oob : originalUrls)
        urlStrings.append(oob.url());
    const auto classified = OobMediaPolicy::classify(simsSources, urlStrings);

    for (int i = 0; i < originalUrls.size(); ++i) {
        const auto &oob = originalUrls.at(i);
        const auto &candidate = classified.at(i);
        if (!candidate.source.isValid()) {
            result.remainingOob.append(oob); // Non-HTTP OOB stays a link.
            continue;
        }
        const QString canonical = candidate.source.toString(QUrl::FullyEncoded);
        if (candidate.redundant) {
            // Preserve descriptions of OOB fallbacks even when their URLs
            // already exist as media references.
            if (!oob.desc().isEmpty()) {
                for (auto &existing : refs) {
                    if (!existing.isValid() || existing.type() != XMPP::Reference::Data
                        || !existing.anchor().isEmpty())
                        continue;
                    bool matches = OobMediaPolicy::normalizedHttpSource(existing.uri()).toString(QUrl::FullyEncoded)
                        == canonical;
                    for (const auto &source : existing.mediaSharing().sources)
                        matches |= OobMediaPolicy::normalizedHttpSource(source).toString(QUrl::FullyEncoded)
                            == canonical;
                    if (matches) {
                        existing.setAnchor(oob.desc());
                        break;
                    }
                }
            }
            continue;
        }

        // A canonical URL-only reference. The share backend resolves it
        // without confusing its address identity with a content checksum.
        XMPP::Reference reference(XMPP::Reference::Data, canonical);
        if (!oob.desc().isEmpty())
            reference.setAnchor(oob.desc());

        // An exact URL in the body can be replaced in place, unless a
        // pre-existing SIMS reference already owns those character positions.
        int searchFrom = 0;
        while (true) {
            const int start = body.indexOf(oob.url(), searchFrom);
            if (start < 0)
                break;
            const int end = start + oob.url().size() - 1;
            bool overlaps = false;
            for (const auto &existing : refs) {
                if (existing.begin() >= 0 && existing.end() >= start && existing.begin() <= end) {
                    overlaps = true;
                    break;
                }
            }
            if (!overlaps) {
                reference.setRange(start, end);
                break;
            }
            searchFrom = end + 1;
        }
        refs.append(reference);
    }

    return result;
}

} // namespace FileSharingNormalizer

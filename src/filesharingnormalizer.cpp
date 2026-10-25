/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingnormalizer.h"

#include "oobmediapolicy.h"
#include "iris/xmpp_message.h"
#include "iris/xmpp_reference.h"
#include "iris/xmpp_url.h"

#include <QSet>

namespace FileSharingNormalizer {

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
        const auto &media = reference.mediaSharing();
        // Only a renderable SIMS share can replace an OOB fallback. When
        // metadata lacks computed hashes the current file backend cannot
        // resolve it, so keep the OOB URL as its working download source.
        if (!media.isValid() || !media.file.hasComputedHashes())
            continue;
        for (const auto &source : media.sources) {
            const QUrl http = OobMediaPolicy::normalizedHttpSource(source);
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
        if (candidate.redundant)
            continue;
        const QString canonical = candidate.source.toString(QUrl::FullyEncoded);

        // A display-only reference without SIMS metadata. The share backend
        // recognizes its HTTP URL and constructs a URL-only FileSharingItem.
        XMPP::Reference reference(XMPP::Reference::Data, canonical);

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

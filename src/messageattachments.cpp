/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "messageattachments.h"

#include "iris/xmpp_message.h"
#include "oobmediapolicy.h"

#include <QSet>

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

Result collect(const XMPP::Message &message)
{
    Result        result;
    QSet<QString> covered;
    for (const auto &reference : message.references()) {
        result.attachments.append({ reference, {} });
        for (const auto &source : httpSources(reference))
            covered.insert(source);
    }

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
} // namespace MessageAttachments

/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QSet>
#include <QStringList>
#include <QUrl>

// Rendering policy only. A URL is never a checksum for the referenced bytes.
namespace OobMediaPolicy {

inline QUrl normalizedHttpSource(const QString &source)
{
    const QUrl url = QUrl::fromEncoded(source.toUtf8(), QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http")))
        return {};
    // Fragments are local to the referring client, not part of HTTP fetches.
    return QUrl(url.toString(QUrl::FullyEncoded | QUrl::RemoveFragment));
}

struct Candidate {
    QString original;
    QUrl    source; // Invalid for non-HTTP URLs, retained by the normalizer
    bool    redundant; // already a SIMS source or repeated in OOB
};

inline QList<Candidate> classify(const QSet<QString> &simsSources, const QStringList &oobUrls)
{
    QList<Candidate> result;
    QSet<QString> seen;
    for (const QString &original : oobUrls) {
        const QUrl source = normalizedHttpSource(original);
        if (!source.isValid()) {
            result.append({ original, {}, false }); // Preserve original OOB link.
            continue;
        }
        const QString canonical = source.toString(QUrl::FullyEncoded);
        const bool redundant = simsSources.contains(canonical) || seen.contains(canonical);
        result.append({ original, source, redundant });
        seen.insert(canonical);
    }
    return result;
}

} // namespace OobMediaPolicy

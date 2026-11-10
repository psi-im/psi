/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_message.h"
#include "iris/xmpp_reference.h"
#include "iris/xmpp_url.h"

#include <QList>
#include <QString>
#include <QUrl>

// Message-local file announcements normalized from SIMS, SFS and OOB. These
// values own no transfers, cache entries or publication lifetime. They are
// unrelated to attaching one message to another (XEP-0367).
namespace MessageFileShares {
// A cheap, conservative payload check, including unsupported references/URLs.
inline bool hasFileShares(const XMPP::Message &message)
{
    return !message.references().isEmpty() || !message.fileSharings().isEmpty() || !message.urlList().isEmpty();
}

struct FileShare {
    XMPP::Reference reference;           // the message's own, or a URL-only data reference for an OOB URL
    QString         description;         // the OOB description of that URL, if any
    bool            manualFetch = false; // XEP-0447: not to be fetched without the user asking
    bool            fromOob     = false; // OOB-only announcement; the view chooses its placement

    // A file known by its hashes.
    bool hasContentHashes() const;
    // The HTTP(S) URL of a file known by nothing else, invalid for any other share.
    QUrl url() const;
};

struct Result {
    QList<FileShare> files;
    XMPP::UrlList    links; // OOB URLs that are not HTTP(S), shown as plain links
};

// Stable protocol order: file references, then SFS, then remaining HTTP(S) OOB.
// Does not infer body ranges or apply presentation filtering.
Result collect(const XMPP::Message &message);

// Presentation projection: leave OOB URLs embedded in prose as ordinary links,
// locate standalone URLs and apply file-sharing fallback ranges in the body.
// Does not change the message.
Result forDisplay(const XMPP::Message &message);

// Identifies a URL-only file by its address. This is never a checksum of the content.
XMPP::Hash urlIdentity(const QUrl &url);
}

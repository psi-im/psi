/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_message.h"
#include "iris/xmpp_reference.h"
#include "iris/xmpp_url.h"

#include <QList>
#include <QString>
#include <QUrl>

// What a message shows as attachments: its own references (SIMS), its XEP-0447 file
// sharings and its XEP-0066 out-of-band HTTP(S) URLs, without duplicates. Computed for
// display only; the message itself is never changed.
namespace MessageAttachments {
// Whether there may be something to show, even with no ordinary <body>.
inline bool hasAttachments(const XMPP::Message &message)
{
    return !message.references().isEmpty() || !message.fileSharings().isEmpty() || !message.urlList().isEmpty();
}

struct Attachment {
    XMPP::Reference reference;   // the message's own, or a URL-only data reference for an OOB URL
    QString         description; // the OOB description of that URL, if any
    bool            manualFetch = false; // XEP-0447: not to be fetched without the user asking

    // A file known by its hashes.
    bool isFile() const;
    // The HTTP(S) URL of a file known by nothing else, invalid for any other attachment.
    QUrl url() const;
};

struct Result {
    QList<Attachment> attachments;
    XMPP::UrlList     links; // OOB URLs that are not HTTP(S), shown as plain links
};

Result collect(const XMPP::Message &message);

// Identifies a URL-only file by its address. This is never a checksum of the content.
XMPP::Hash urlIdentity(const QUrl &url);
}

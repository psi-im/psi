/*
 * Copyright (C) 2026  Psi Project
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PSIMEDIAJINGLECAPABILITYTRANSACTION_H
#define PSIMEDIAJINGLECAPABILITYTRANSACTION_H

#include "psimediajingle.h"

#include <iris/jingle-rtp.h>

#include <functional>
#include <memory>

/**
 * Commit one authoritative native-call capability snapshot.
 *
 * The RTP provider must be replaced before the caller refreshes disco/caps:
 * XMPP::Client::makeDiscoResult() also includes Jingle manager features, so
 * publishing the new feature set first can advertise media which the current
 * provider cannot create yet (or keep advertising media already removed).
 *
 * Returns true when a snapshot/provider was committed. An identical snapshot
 * with an existing provider is a no-op. The callback runs only when the
 * advertised audio/video set changes, and always after RTP::Manager owns the
 * matching provider.
 */
inline bool commitPsiMediaJingleCapabilities(XMPP::Jingle::RTP::Manager                        *rtpManager,
                                             PsiMediaJingleCapabilities                        &current,
                                             std::shared_ptr<XMPP::Jingle::RTP::MediaProvider> &provider,
                                             const PsiMediaJingleCapabilities                  &next,
                                             const std::function<void()> &advertisedMediaChanged = {})
{
    if (!rtpManager)
        return false;
    if (provider && next == current)
        return false;

    const auto oldAdvertisedMedia = current.mediaTypes();
    const auto newAdvertisedMedia = next.mediaTypes();

    current  = next;
    provider = makePsiMediaJingleProvider(current);
    rtpManager->setMediaProvider(provider);

    if (oldAdvertisedMedia != newAdvertisedMedia && advertisedMediaChanged)
        advertisedMediaChanged();
    return true;
}

#endif // PSIMEDIAJINGLECAPABILITYTRANSACTION_H

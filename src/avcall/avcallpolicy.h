// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef AVCALLPOLICY_H
#define AVCALLPOLICY_H

#include <iris/xmpp-im/jingle-rtp.h>
#include <iris/jingle.h>

namespace AvCallPolicy {

using Origin = XMPP::Jingle::Origin;

inline bool mediaTypeSupported(bool backendAvailable, bool probeComplete, bool secureRtp, bool hasModes)
{
    return backendAvailable && probeComplete && secureRtp && hasModes;
}

inline Origin peerRole(Origin localRole)
{
    return localRole == Origin::Initiator ? Origin::Responder : Origin::Initiator;
}

inline bool allowsSender(Origin senders, Origin role) { return senders == Origin::Both || senders == role; }

inline Origin withoutLocalSender(Origin senders, Origin localRole)
{
    const auto remoteRole = peerRole(localRole);
    if (senders == Origin::Both)
        return remoteRole;
    if (senders == localRole)
        return Origin::None;
    return senders;
}

inline Origin sendersForCaptureAvailability(Origin desiredWithCapture, Origin localRole, bool captureAvailable)
{
    return captureAvailable ? desiredWithCapture : withoutLocalSender(desiredWithCapture, localRole);
}

inline bool shouldTransmit(bool hasMedia, bool captureConsent, bool senderAllowed, bool captureAvailable)
{
    return hasMedia && captureConsent && senderAllowed && captureAvailable;
}

// Application policy for content-add in an established call. Incoming offers
// remain Created until the application decides to prepare their answer; Pending
// is a signaling state reached later, not an invitation to accept the offer.
inline void prepareIncomingContent(XMPP::Jingle::RTP::Application *content, bool callActive)
{
    namespace Jingle = XMPP::Jingle;
    namespace RTP    = Jingle::RTP;
    if (!callActive || !content || !content->isRemote() || content->state() != Jingle::State::Created)
        return;
    if (content->media() != QLatin1String("video")) {
        content->remove(Jingle::Reason::UnsupportedApplications);
        return;
    }
    QPointer<RTP::Application> guard(content);
    auto                       pad = content->pad().staticCast<RTP::Pad>();
    pad->directionController()->setLocalSending(content, false);
    if (guard)
        content->prepare();
}

} // namespace AvCallPolicy

#endif // AVCALLPOLICY_H

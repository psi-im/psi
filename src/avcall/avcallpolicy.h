// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef AVCALLPOLICY_H
#define AVCALLPOLICY_H

#include <iris/jingle.h>

#include <optional>

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

inline bool allowsSender(Origin senders, Origin role)
{
    return senders == Origin::Both || senders == role;
}

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

inline bool shouldRequestSenders(Origin current, const std::optional<Origin> &pendingTarget, Origin desired)
{
    return pendingTarget ? *pendingTarget != desired : current != desired;
}

inline void reconcilePolicyTarget(Origin senders, std::optional<Origin> &pendingTarget)
{
    if (pendingTarget && *pendingTarget == senders)
        pendingTarget.reset();
}

inline bool shouldTransmit(bool hasMedia, bool captureConsent, bool senderAllowed, bool captureAvailable)
{
    return hasMedia && captureConsent && senderAllowed && captureAvailable;
}

} // namespace AvCallPolicy

#endif // AVCALLPOLICY_H

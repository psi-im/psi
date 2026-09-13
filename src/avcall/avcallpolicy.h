// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef AVCALLPOLICY_H
#define AVCALLPOLICY_H

#include <iris/jingle.h>

namespace AvCallPolicy {

using Origin = XMPP::Jingle::Origin;

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

inline bool shouldTransmit(bool hasMedia, bool captureConsent, Origin senders, Origin localRole, bool captureAvailable)
{
    return hasMedia && captureConsent && captureAvailable && allowsSender(senders, localRole);
}

} // namespace AvCallPolicy

#endif // AVCALLPOLICY_H

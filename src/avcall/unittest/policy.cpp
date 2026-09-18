// SPDX-License-Identifier: GPL-2.0-or-later
#include "../avcallpolicy.h"

#include <cstdio>
#include <cstdlib>

namespace Policy = AvCallPolicy;
using Origin     = Policy::Origin;

static void check(bool condition, const char *message)
{
    if (condition)
        return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

static Origin expectedWithoutLocal(Origin senders, Origin localRole)
{
    if (senders == Origin::None)
        return Origin::None;
    if (senders == Origin::Both)
        return localRole == Origin::Initiator ? Origin::Responder : Origin::Initiator;
    if (senders == localRole)
        return Origin::None;
    return senders;
}

int main()
{
    const Origin roles[]      = { Origin::Initiator, Origin::Responder };
    const Origin directions[] = { Origin::None, Origin::Both, Origin::Initiator, Origin::Responder };

    // Codec/media capability is independent of capture-device presence. The
    // production gate intentionally has no microphone/camera input parameter.
    for (int backendAvailable = 0; backendAvailable <= 1; ++backendAvailable) {
        for (int probeComplete = 0; probeComplete <= 1; ++probeComplete) {
            for (int secureRtp = 0; secureRtp <= 1; ++secureRtp) {
                for (int hasModes = 0; hasModes <= 1; ++hasModes) {
                    const bool expected = backendAvailable && probeComplete && secureRtp && hasModes;
                    check(Policy::mediaTypeSupported(backendAvailable, probeComplete, secureRtp, hasModes) == expected,
                          "media capability gate truth table mismatch");
                }
            }
        }
    }
    check(Policy::mediaTypeSupported(true, true, true, true),
          "usable audio modes were not advertised without a capture dependency");

    check(Policy::peerRole(Origin::Initiator) == Origin::Responder, "initiator peer role is not responder");
    check(Policy::peerRole(Origin::Responder) == Origin::Initiator, "responder peer role is not initiator");

    for (const auto localRole : roles) {
        for (const auto senders : directions) {
            const bool expectedAllowed = senders == Origin::Both || senders == localRole;
            check(Policy::allowsSender(senders, localRole) == expectedAllowed, "sender permission matrix mismatch");

            const auto withoutCapture = expectedWithoutLocal(senders, localRole);
            check(Policy::withoutLocalSender(senders, localRole) == withoutCapture,
                  "local sender removal matrix mismatch");
            check(Policy::sendersForCaptureAvailability(senders, localRole, false) == withoutCapture,
                  "no-capture direction matrix mismatch");
            check(Policy::sendersForCaptureAvailability(senders, localRole, true) == senders,
                  "capture availability did not restore the desired direction");

            // No microphone/camera may never leave the local role authorized to
            // send, regardless of the original direction.
            check(!Policy::allowsSender(withoutCapture, localRole),
                  "no-capture direction still authorizes the local sender");

            // Restoring capture must preserve the peer's original direction: a
            // remote-only offer stays remote-only instead of being promoted to both.
            check(Policy::allowsSender(Policy::sendersForCaptureAvailability(senders, localRole, true), localRole)
                      == expectedAllowed,
                  "capture restoration changed the peer-authorized direction");
        }
    }

    // Outgoing audio starts receive-only when no microphone exists, for either
    // Jingle role. This is the concrete no-mic invariant used by AvCall.
    check(Policy::sendersForCaptureAvailability(Origin::Both, Origin::Initiator, false) == Origin::Responder,
          "initiator without microphone did not become receive-only");
    check(Policy::sendersForCaptureAvailability(Origin::Both, Origin::Responder, false) == Origin::Initiator,
          "responder without microphone did not become receive-only");

    // The media transmit gate is deliberately a pure conjunction. Device loss
    // therefore stops capture immediately even before content-modify is ACKed,
    // while device gain cannot start capture unless senders already authorize it.
    for (int hasMedia = 0; hasMedia <= 1; ++hasMedia) {
        for (int consent = 0; consent <= 1; ++consent) {
            for (int senderAllowed = 0; senderAllowed <= 1; ++senderAllowed) {
                for (int captureAvailable = 0; captureAvailable <= 1; ++captureAvailable) {
                    const bool expected = hasMedia && consent && senderAllowed && captureAvailable;
                    check(Policy::shouldTransmit(hasMedia, consent, senderAllowed, captureAvailable) == expected,
                          "transmit gate truth table mismatch");
                }
            }
        }
    }

    check(!Policy::shouldTransmit(true, true, true, false),
          "device loss did not synchronously close the transmit gate");
    check(!Policy::shouldTransmit(true, true, false, true),
          "device gain bypassed negotiated senders");
    check(Policy::shouldTransmit(true, true, true, true), "fully authorized media did not open the transmit gate");

    return 0;
}

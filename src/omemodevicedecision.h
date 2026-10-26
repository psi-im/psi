// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef OMEMODEVICEDECISION_H
#define OMEMODEVICEDECISION_H

#ifdef IRIS_ENABLE_OMEMO
#include <iris/xmpp_omemo.h>
#include <optional>
#endif

namespace PsiOmemo {
enum class DeviceDecision { VerifyIdentity, RepairSession };

#ifdef IRIS_ENABLE_OMEMO
bool needsManualTrustDecision(XMPP::EncryptionTrustLevel level);

// A sender filter and permission to repair an existing session are independent.
// Receiving from an already verified sender must not prompt about its other
// devices or the same numeric id in another wire profile.
QList<XMPP::OmemoDeviceInfo> devicesForDecision(const QList<XMPP::OmemoDeviceInfo> &devices, quint32 deviceId,
                                                std::optional<XMPP::OmemoProtocol> profile, DeviceDecision purpose);
#endif
} // namespace PsiOmemo
#endif

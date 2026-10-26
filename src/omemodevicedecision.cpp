// SPDX-License-Identifier: GPL-2.0-or-later
#include "omemodevicedecision.h"

#ifdef IRIS_ENABLE_OMEMO
#include <algorithm>

namespace PsiOmemo {
bool needsManualTrustDecision(XMPP::EncryptionTrustLevel level)
{
    return level == XMPP::EncryptionTrustLevel::Undecided || level == XMPP::EncryptionTrustLevel::AutomaticallyTrusted;
}

QList<XMPP::OmemoDeviceInfo> devicesForDecision(const QList<XMPP::OmemoDeviceInfo> &devices, quint32 deviceId,
                                                std::optional<XMPP::OmemoProtocol> profile, DeviceDecision purpose)
{
    QList<XMPP::OmemoDeviceInfo> result;
    for (const auto &device : devices) {
        if ((deviceId && device.id != deviceId) || (profile && device.protocol != *profile)
            || device.identityKey.isEmpty() || !device.active)
            continue;
        const bool pending = purpose == DeviceDecision::RepairSession
            ? device.trust != XMPP::EncryptionTrustLevel::Distrusted
            : needsManualTrustDecision(device.trust);
        if (!pending)
            continue;
        auto existing = std::find_if(result.begin(), result.end(), [&device](const auto &candidate) {
            return candidate.owner.bare() == device.owner.bare() && candidate.id == device.id
                && candidate.identityKey == device.identityKey;
        });
        if (existing == result.end()) {
            result.append(device);
        } else {
            existing->protocols |= device.protocols;
            existing->hasSession = existing->hasSession || device.hasSession;
            if (existing->label.isEmpty())
                existing->label = device.label;
        }
    }
    return result;
}
} // namespace PsiOmemo
#endif

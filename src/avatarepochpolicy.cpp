/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "avatarepochpolicy.h"

quint64 AvatarEpochPolicy::generation(const QString &identity) const
{
    return epochs_.value(identity, 0);
}

quint64 AvatarEpochPolicy::advance(const QString &identity)
{
    auto &generation = epochs_[identity];
    return ++generation;
}

void AvatarEpochPolicy::forgetPrefix(const QString &prefix)
{
    // Never recycle a generation to zero while a disconnected task might
    // still complete. Account teardown can destroy the entire policy object,
    // but live account reconnects must retain the generation fence.
    for (auto it = epochs_.begin(); it != epochs_.end(); ++it) {
        if (it.key().startsWith(prefix))
            ++it.value();
    }
}

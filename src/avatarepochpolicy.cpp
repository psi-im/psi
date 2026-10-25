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
    for (auto it = epochs_.begin(); it != epochs_.end();) {
        if (it.key().startsWith(prefix))
            it = epochs_.erase(it);
        else
            ++it;
    }
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

namespace PsiMam {
inline bool mayFetchArchive(bool /*initialPage*/, bool localPreloadPending,
                            bool /*localHistoryPopulated*/)
{
    // Wait for EDB identity metadata, then merge instead of choosing one source.
    return !localPreloadPending;
}

inline bool mayReuseArchivePage(bool initialPage, bool connected, bool found, bool stable,
                               int entries, int requested, bool complete)
{
    // A trusted row alone says nothing about gaps. Only a whole contiguous
    // page or a certified archive boundary permits skipping the network.
    return found && stable && (entries == requested || complete) && (!initialPage || !connected);
}
} // namespace PsiMam

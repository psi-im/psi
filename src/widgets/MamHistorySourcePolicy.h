/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

namespace PsiMam {

// No legacy SQLite row has a verified MAM archive UID. Mixing SQLite's tail
// with initial MAM pagination would duplicate messages and scramble ordering.
// Once an archive cursor exists, ordinary previous-page fetches may continue.
inline bool mayFetchArchive(bool initialPage, bool localPreloadPending,
                            bool localHistoryPopulated)
{
    return !initialPage || (!localPreloadPending && !localHistoryPopulated);
}

} // namespace PsiMam

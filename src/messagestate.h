/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QFlags>

// State of one logical message, independent of archive coverage and UI flags.
// Persist these values verbatim; new flags must use previously unused bits.
namespace MessageState {
// Unlike Retracted, this flag is strictly device-local and never transmits a protocol event.
enum Flag { Displayed = 0x1, Delivered = 0x2, Edited = 0x4, Retracted = 0x8, LocalDeleted = 0x10 };
Q_DECLARE_FLAGS(Flags, Flag)
}
Q_DECLARE_OPERATORS_FOR_FLAGS(MessageState::Flags)

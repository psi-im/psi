/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QFlags>

// State of one logical message, independent of archive coverage and UI flags.
// Persist these values verbatim; new flags must use previously unused bits.
namespace MessageState {
enum Flag { Displayed = 0x1, Delivered = 0x2, Edited = 0x4, Retracted = 0x8 };
Q_DECLARE_FLAGS(Flags, Flag)
}
Q_DECLARE_OPERATORS_FOR_FLAGS(MessageState::Flags)

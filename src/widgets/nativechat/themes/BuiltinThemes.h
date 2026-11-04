/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "../../NativeChatRenderers.h"
class NativeChatThemes;

// Built-ins provide the same description and optional renderer as plugins.
namespace NativeChatBuiltins {
NativeChatTheme modern();
NativeChatTheme classic();
NativeChatTheme bubble();
const NativeChatMessageRenderer *bubbleRenderer();
// Add built-ins to the provider list in BuiltinThemes.cpp only.
void registerThemes(NativeChatThemes *registry);
}

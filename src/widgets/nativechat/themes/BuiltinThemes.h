/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "../../../../plugins/include/nativechatthemeprovider.h"

class NativeChatThemes;
namespace NativeChatBuiltins {
NativeChatTheme modern();
NativeChatTheme classic();
NativeChatTheme bubble();
// Built-in registration is centralized in BuiltinThemes.cpp.
void registerThemes(NativeChatThemes *registry);
}

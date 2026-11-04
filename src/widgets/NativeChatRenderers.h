/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "../../plugins/include/nativechatrenderer.h"
#include "../../plugins/include/nativechatthemeprovider.h"
#include <memory>

// Generic value-only theme renderer; built-ins and plugins can also supply
// their own NativeChatMessageRenderer through NativeChatThemes.
namespace NativeChatRenderers {

// Lays out a theme from its NativeChatTheme description alone: the built-in
// Modern and Classic themes, and plugin themes without a renderer of their
// own (including those with a JSON layoutSpec).
std::unique_ptr<NativeChatMessageRenderer> forDescription(const NativeChatTheme &theme);

}

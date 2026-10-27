/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "../../plugins/include/nativechatrenderer.h"
#include "../../plugins/include/nativechatthemeprovider.h"
#include <memory>

// Psi's own message renderers, written against the same public API that
// plugins use (plugins/include/nativechatrenderer.h).
namespace NativeChatRenderers {

// Lays out a theme from its NativeChatTheme description alone: the built-in
// Modern and Classic themes, and plugin themes without a renderer of their
// own (including those with a JSON layoutSpec).
std::unique_ptr<NativeChatMessageRenderer> forDescription(const NativeChatTheme &theme);

// The built-in speech bubble renderer ("bubble" theme).
const NativeChatMessageRenderer *bubble();

// The bubble's text area is limited to this many average characters of the
// chat font, whatever the width of the log.
constexpr int BubbleMaxTextColumns = 80;

}

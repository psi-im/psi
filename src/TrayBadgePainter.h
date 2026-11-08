#pragma once

#include <cstdint>

#include <QPixmap>

#include "TrayBadgePolicy.h"

namespace Mattermost {

// Paints a single bottom-right tray badge into an existing icon pixmap.
// The surrounding halo is cut from the icon's alpha channel, not painted
// using any guessed system-tray background color.
class TrayBadgePainter
{
public:
    static void paint(QPixmap& pixmap, TrayBadgeKind kind,
                      std::uint32_t attentionCount = 0);
};

} // namespace Mattermost

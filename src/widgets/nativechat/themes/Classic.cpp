/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BuiltinThemes.h"
#include "ThemeLayoutHelpers.h"
#include <QCoreApplication>
#include <memory>

namespace {
class ClassicRenderer final : public NativeChatMessageRenderer {
public:
    explicit ClassicRenderer(int spacing) : spacing_(spacing) { }

    NativeChatPartOptions partOptions(const NativeChatMessageInfo &info) const override
    {
        NativeChatPartOptions options;
        options.avatarSize = 0;
        options.directionColors = true;
        // Classic sets its fixed clock format in the theme definition.
        options.boldAuthor = false;
        options.boldBody = info.alert;
        options.timeInNickColor = info.groupChat;
        // One document: the message icon, timestamp, nickname and text all
        // wrap together from the left edge, as in Psi's classic text view.
        options.header = NativeChatPartOptions::HeaderInText;
        options.authorPrefix = QStringLiteral("<");
        options.authorSuffix = QStringLiteral(">");
        return options;
    }

    void arrange(NativeChatMessageCanvas &canvas, const NativeChatMessageInfo &info) const override
    {
        auto *row = NativeChatThemeLayout::rowLayout(canvas, QMargins(12, 2, 12, 2), spacing_);
        if (info.alert) {
            const bool light = canvas.row()->palette().color(QPalette::Base).lightness() > 128;
            canvas.setBodyTextColor(light ? QColor(185, 35, 45) : QColor(255, 120, 125));
        }
        row->addWidget(canvas.take(NativeChatMessageCanvas::Body), 1, Qt::AlignTop);
    }

private:
    int spacing_;
};
}

namespace NativeChatBuiltins {
NativeChatTheme classic()
{
    NativeChatTheme theme;
    theme.id = QStringLiteral("classic");
    theme.name = QCoreApplication::translate("NativeChatThemes", "Classic");
    theme.description = QCoreApplication::translate("NativeChatThemes", "Compact text layout");
    theme.avatarSize = 0;
    theme.spacing = 4;
    theme.hoverHighlight = false;
    theme.timestampFormat = QStringLiteral("HH:mm:ss");
    theme.setRenderer(std::make_shared<ClassicRenderer>(theme.spacing));
    return theme;
}
}

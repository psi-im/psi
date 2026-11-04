/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BuiltinThemes.h"
#include "ThemeLayoutHelpers.h"
#include <QCoreApplication>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <memory>

namespace {
class ModernRenderer final : public NativeChatMessageRenderer {
public:
    explicit ModernRenderer(int avatarSize, int spacing) : avatarSize_(avatarSize), spacing_(spacing) { }

    NativeChatPartOptions partOptions(const NativeChatMessageInfo &) const override
    {
        NativeChatPartOptions options;
        options.avatarSize = avatarSize_;
        options.directionColors = true;
        options.boldAuthor = true;
        options.timePointDelta = -1.0;
        options.timeAlpha = 188;
        return options;
    }

    void arrange(NativeChatMessageCanvas &canvas, const NativeChatMessageInfo &info) const override
    {
        auto *outer = NativeChatThemeLayout::rowLayout(canvas, QMargins(12, 8, 12, 8), spacing_);
        if (auto *avatar = canvas.take(NativeChatMessageCanvas::Avatar))
            outer->addWidget(avatar, 0, Qt::AlignTop);
        auto *content = new QWidget(canvas.row());
        content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        outer->addWidget(content, 1, Qt::AlignTop);
        auto *column = new QVBoxLayout(content);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(3);
        if (info.kind == NativeChatMessageInfo::Message)
            column->addWidget(NativeChatThemeLayout::messageHeader(canvas, content), 0, Qt::AlignTop);
        column->addWidget(canvas.take(NativeChatMessageCanvas::Body), 0, Qt::AlignTop);
    }

private:
    int avatarSize_;
    int spacing_;
};
}

namespace NativeChatBuiltins {
NativeChatTheme modern()
{
    NativeChatTheme theme;
    theme.id = QStringLiteral("modern");
    theme.name = QCoreApplication::translate("NativeChatThemes", "Modern");
    theme.description = QCoreApplication::translate("NativeChatThemes", "Avatars and message headers");
    theme.setRenderer(std::make_shared<ModernRenderer>(theme.avatarSize, theme.spacing));
    return theme;
}
}

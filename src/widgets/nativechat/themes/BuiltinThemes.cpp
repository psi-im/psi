/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BuiltinThemes.h"
#include "../../NativeChatThemeRegistry.h"
#include <QHash>

// The single registration list for built-in native chat themes.
// All built-in and plugin themes enter the registry through the same API.
namespace NativeChatBuiltins {
namespace {
struct Entry {
    NativeChatTheme theme;
    const NativeChatMessageRenderer *renderer;
};

class Provider final : public QObject, public NativeChatThemeProvider, public NativeChatRendererProvider {
public:
    explicit Provider(QObject *parent) : QObject(parent)
    {
        const QList<Entry> entries {
            { modern(), nullptr },
            { classic(), nullptr },
            { bubble(), bubbleRenderer() },
        };
        for (const auto &entry : entries) {
            themes_.append(entry.theme);
            if (entry.renderer)
                renderers_.insert(entry.theme.id, entry.renderer);
        }
    }
    QList<NativeChatTheme> nativeChatThemes() const override { return themes_; }
    const NativeChatMessageRenderer *nativeChatRenderer(const QString &themeId) const override
    {
        return renderers_.value(themeId, nullptr);
    }
private:
    QList<NativeChatTheme> themes_;
    QHash<QString, const NativeChatMessageRenderer *> renderers_;
};
}
void registerThemes(NativeChatThemes *registry)
{
    auto *provider = new Provider(registry);
    registry->add(provider, provider->nativeChatThemes(), provider);
}
}

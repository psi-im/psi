/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BuiltinThemes.h"
#include "../../NativeChatThemeRegistry.h"

// Built-ins and external plugins register through NativeChatThemeProvider.
namespace NativeChatBuiltins {
namespace {
class Provider final : public QObject, public NativeChatThemeProvider {
public:
    explicit Provider(QObject *parent) : QObject(parent) { }
    QList<NativeChatTheme> nativeChatThemes() const override
    {
        // Add a new built-in theme here; no changes to the registry or chat widget.
        return { modern(), classic(), bubble() };
    }
};
}
void registerThemes(NativeChatThemes *registry)
{
    auto *provider = new Provider(registry);
    registry->add(provider, provider->nativeChatThemes());
}
}

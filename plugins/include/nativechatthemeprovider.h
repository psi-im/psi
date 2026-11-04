/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef NATIVECHATTHEMEPROVIDER_H
#define NATIVECHATTHEMEPROVIDER_H

#include <QColor>
#include <QList>
#include <QString>
#include <QtPlugin>
#include <memory>
#include <utility>

class NativeChatMessageRenderer;

// A theme is its complete public definition, including its optional renderer.
// Copies retain the renderer's lifetime for existing chat rows. Without a C++
// renderer, renderer() creates the standard description/JSON renderer.
struct NativeChatTheme {
    NativeChatTheme() = default;
    NativeChatTheme(QString themeId, QString themeName, int size = 40, int gap = 10,
                    QString css = {}, QString tree = {})
        : id(std::move(themeId)), name(std::move(themeName)), avatarSize(size), spacing(gap),
          styleSheet(std::move(css)), layoutSpec(std::move(tree)) { }

    QString id;             // Unique ID, e.g. org.example.theme.
    QString name;           // Displayed in the theme picker.
    int     avatarSize = 40;
    int     spacing    = 10;
    QString styleSheet;     // Scoped to each message row.
    QString layoutSpec;     // Optional JSON slot tree (author, timestamp, avatar, body, unreadSeparator).
    bool    hoverHighlight = true;
    QColor  background;
    QString backgroundImage;
    enum ImageMode { Tile, Scale };
    ImageMode backgroundImageMode = Tile;
    QString description;    // Theme picker description.

    // Theme renderers own the visual layout, not the registry or widget.
    // The shared handle keeps a plugin's renderer alive until the last row
    // and theme copy are gone. Plugin code must remain loaded until then.
    void setRenderer(std::shared_ptr<const NativeChatMessageRenderer> renderer)
    {
        renderer_ = std::move(renderer);
    }
    std::shared_ptr<const NativeChatMessageRenderer> renderer() const;
    bool hasCustomRenderer() const { return bool(renderer_); }

private:
    std::shared_ptr<const NativeChatMessageRenderer> renderer_;
};

class NativeChatThemeProvider {
public:
    virtual ~NativeChatThemeProvider()                      = default;
    virtual QList<NativeChatTheme> nativeChatThemes() const = 0;
};
// Intentionally incompatible with the old description-only provider API.
Q_DECLARE_INTERFACE(NativeChatThemeProvider, "org.psi-im.NativeChatThemeProvider/2.0")
#endif

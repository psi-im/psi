/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "nativechattheme.h"
#include "avatars.h"
#include "coloropt.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psioptions.h"
#include "theme_p.h"
#include "widgets/MamChatWidget.h"
#include "widgets/OverlayScrollBarManager.h"
#include "widgets/emojiregistry.h"
#include "widgets/iconselect.h"
#include <QApplication>
#include <QHash>
#include <QSignalBlocker>
#include <memory>

namespace {
struct NativeChatAvatarState {
    QString                peerJid;
    XMPP::Jid              peer;
    bool                   mucPrivate = false;
    QHash<QString, QPixmap> cache;
};

class NativeThemePrivate : public ThemePrivate {
public:
    NativeThemePrivate(PsiThemeProvider *provider, const NativeChatTheme &theme) : ThemePrivate(provider)
    {
        id          = theme.id;
        name        = theme.name;
        description = theme.layout == NativeChatTheme::Classic ? QObject::tr("Compact text layout")
                                                               : QObject::tr("Avatars and message headers");
    }
    bool exists() override { return true; }
    bool load(const QString &) override
    {
        state = Theme::State::Loaded;
        return true;
    }
    bool load(const QString &style, std::function<void(bool)> callback) override
    {
        callback(load(style));
        return true;
    }
};
}
NativeChatViewThemeProvider::NativeChatViewThemeProvider(PsiCon *psi, bool groupChat) :
    PsiThemeProvider(psi), groupChat_(groupChat)
{
    connect(NativeChatThemes::instance(), &NativeChatThemes::changed, this, [this] {
        emit themeListChanged();
        emit themeChanged();
    });
}
const char *NativeChatViewThemeProvider::type() const { return groupChat_ ? "groupchatview" : "chatview"; }
QString     NativeChatViewThemeProvider::option() const
{
    return groupChat_ ? QStringLiteral("options.ui.muc.native-theme") : QStringLiteral("options.ui.chat.native-theme");
}
const QStringList NativeChatViewThemeProvider::themeIds() const
{
    QStringList ids;
    for (const auto &theme : NativeChatThemes::instance()->themes())
        ids.append(theme.id);
    return ids;
}
Theme NativeChatViewThemeProvider::theme(const QString &id)
{
    return Theme(new NativeThemePrivate(this, NativeChatThemes::instance()->theme(id)));
}
PsiThemeProvider::LoadRestult NativeChatViewThemeProvider::loadCurrent() { return LoadSuccess; }
Theme                         NativeChatViewThemeProvider::current() const
{
    return const_cast<NativeChatViewThemeProvider *>(this)->theme(
        PsiOptions::instance()->getOption(option()).toString());
}
void NativeChatViewThemeProvider::setCurrentTheme(const QString &id, const QString &)
{
    PsiOptions::instance()->setOption(option(), id);
    emit themeChanged();
}
QString NativeChatViewThemeProvider::optionsName() const
{
    return groupChat_ ? tr("Groupchat Message Style") : tr("Chat Message Style");
}
QString NativeChatViewThemeProvider::optionsDescription() const { return tr("Choose the native message layout"); }

void configureNativeChat(MamChatWidget *view, PsiAccount *account, const XMPP::Jid &peer, bool groupChat,
                         bool mucPrivate)
{
    view->setMediaOpener(account->fileSharingDeviceOpener());
    // A separate instance of Psi's picker avoids also inserting the chosen
    // reaction into the composer's shared emoticon popup.
    auto *picker = new IconSelectPopup(view);
    picker->setIconset(account->psi()->iconSelectPopup()->iconset());
    picker->setEmojiSortingEnabled(true);
    QObject::connect(view, &MamChatWidget::emojiChooserRequested, picker,
                     [picker](const QString &target, const QPoint &where) {
                         picker->setProperty("messageTarget", target);
                         picker->popup(where);
                     });
    QObject::connect(picker, &IconSelectPopup::textSelected, view, [view, picker](const QString &emoji) {
        if (EmojiRegistry::instance().isEmoji(emoji))
            view->toggleReaction(picker->property("messageTarget").toString(), emoji);
    });
    const QString option
        = groupChat ? QStringLiteral("options.ui.muc.native-theme") : QStringLiteral("options.ui.chat.native-theme");
    auto apply = [view, option] {
        view->setTheme(NativeChatThemes::instance()->theme(PsiOptions::instance()->getOption(option).toString()));
    };
    apply();
    auto applyFont = [view] {
        QFont font;
        font.fromString(PsiOptions::instance()->getOption(QStringLiteral("options.ui.look.font.chat")).toString());
        view->setFont(font);
    };
    applyFont();
    QObject::connect(PsiOptions::instance(), &PsiOptions::optionChanged, view, [apply, option](const QString &name) {
        if (name == option)
            apply();
    });
    QObject::connect(PsiOptions::instance(), &PsiOptions::optionChanged, view, [applyFont](const QString &name) {
        if (name == QLatin1String("options.ui.look.font.chat"))
            applyFont();
    });
    QObject::connect(NativeChatThemes::instance(), &NativeChatThemes::changed, view, apply);
    auto applyColors = [view] {
        QPalette palette = QApplication::palette();
        palette.setColor(QPalette::Base,
                         ColorOpt::instance()->color(QStringLiteral("options.ui.look.colors.chat.background")));
        palette.setColor(QPalette::Window, palette.color(QPalette::Base));
        palette.setColor(QPalette::Text,
                         ColorOpt::instance()->color(QStringLiteral("options.ui.look.colors.chat.text")));
        palette.setColor(QPalette::Link,
                         ColorOpt::instance()->color(QStringLiteral("options.ui.look.colors.messages.link")));
        palette.setColor(QPalette::LinkVisited,
                         ColorOpt::instance()->color(QStringLiteral("options.ui.look.colors.messages.link-visited")));
        {
            const QSignalBlocker blocker(view);
            view->setPalette(palette);
            view->viewport()->setPalette(view->palette());
        }
        view->setInformationalColor(ColorOpt::ensureContrast(
            ColorOpt::instance()->color(QStringLiteral("options.ui.look.colors.messages.informational")),
            view->palette().color(QPalette::Base), view->palette().color(QPalette::Text)));
        view->setQuoteColor(ColorOpt::instance()->color(QStringLiteral("options.ui.look.colors.messages.quote")));
    };
    applyColors();
    QObject::connect(ColorOpt::instance(), &ColorOpt::changed, view, [applyColors](const QString &name) {
        if (name == QLatin1String("options.ui.look.colors.messages.informational")
            || name == QLatin1String("options.ui.look.colors.messages.quote")
            || name.startsWith(QLatin1String("options.ui.look.colors.chat."))
            || name.startsWith(QLatin1String("options.ui.look.colors.messages.link")))
            applyColors();
    });
    QObject::connect(view, &MamChatWidget::paletteChanged, view, applyColors);
    // One cache per timeline: no AvatarFactory lookup for every message row.
    auto avatarState = std::make_shared<NativeChatAvatarState>();
    avatarState->peerJid = peer.full();
    avatarState->peer = peer;
    avatarState->mucPrivate = mucPrivate;
    view->setProperty("nativeChatPeer", peer.full());
    view->setProperty("nativeChatMucPrivate", mucPrivate);
    view->setAvatarResolver([account, view, groupChat, avatarState](const MessageView &message) {
        // Only resolve a new JID when the chat itself changes its peer.
        const QString currentPeer = view->property("nativeChatPeer").toString();
        const bool currentPrivate = view->property("nativeChatMucPrivate").toBool();
        if (avatarState->peerJid != currentPeer || avatarState->mucPrivate != currentPrivate) {
            avatarState->peerJid = currentPeer;
            avatarState->peer = XMPP::Jid(currentPeer);
            avatarState->mucPrivate = currentPrivate;
            avatarState->cache.clear();
        }

        const bool mucSender = groupChat || (!message.isLocal() && avatarState->mucPrivate);
        const XMPP::Jid sender = groupChat ? avatarState->peer.withResource(message.nick())
            : message.isLocal() ? account->jid().withResource({}) : avatarState->peer;
        const QString key = mucSender ? QStringLiteral("muc:") + sender.full()
                                      : QStringLiteral("contact:") + sender.bare();
        const auto cached = avatarState->cache.constFind(key);
        if (cached != avatarState->cache.cend())
            return cached.value(); // Cache missing avatars as well as real pixmaps.
        QPixmap avatar = mucSender ? account->avatarFactory()->getMucAvatar(sender)
                                   : account->avatarFactory()->getAvatar(sender);
        avatarState->cache.insert(key, avatar);
        return avatar;
    });
    QObject::connect(account->avatarFactory(), &AvatarFactory::avatarChanged, view,
                     [view, avatarState](const XMPP::Jid &jid) {
        // Contacts use bare JIDs; MUC occupants use full JIDs.
        const bool contactChanged = avatarState->cache.remove(QStringLiteral("contact:") + jid.bare()) > 0;
        const bool mucChanged = avatarState->cache.remove(QStringLiteral("muc:") + jid.full()) > 0;
        if (contactChanged || mucChanged)
            view->refreshAvatars();
    });
    Mattermost::OverlayScrollBarManager::install(*qApp);
    Mattermost::OverlayScrollBarManager::setScrollToStartButtonEnabled(*view, false);
}

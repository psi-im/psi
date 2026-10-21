/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "psithemeprovider.h"
#include "widgets/NativeChatThemeRegistry.h"

class MamChatWidget;
class PsiAccount;
namespace XMPP {
class Jid;
}

class NativeChatViewThemeProvider : public PsiThemeProvider {
    Q_OBJECT
public:
    NativeChatViewThemeProvider(PsiCon *psi, bool groupChat);
    const char       *type() const override;
    const QStringList themeIds() const override;
    Theme             theme(const QString &id) override;
    LoadRestult       loadCurrent() override;
    void              unloadCurrent() override { }
    void              cancelCurrentLoading() override { }
    Theme             current() const override;
    void              setCurrentTheme(const QString &id, const QString &style = {}) override;
    QString           optionsName() const override;
    QString           optionsDescription() const override;

private:
    QString option() const;
    bool    groupChat_;
};

// Application policy (options, avatars) stays outside the presentation widget.
void configureNativeChat(MamChatWidget *view, PsiAccount *account, const XMPP::Jid &peer, bool groupChat,
                         bool mucPrivate = false);

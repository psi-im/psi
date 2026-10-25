/*
 * psiaccount.cpp - handles a Psi account
 * Copyright (C) 2001-2005  Justin Karneges
 * Copyright (C) 2020  Boris Pek <tehnick-8@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PSICHATDLG_H
#define PSICHATDLG_H

#include "actionlist.h"
#include "applicationinfo.h"
#include "chatdlg.h"
#include "mcmdcompletion.h"
#include "mcmdmanager.h"
#include "mcmdsimplesite.h"
#include "minicmd.h"
#include "typeaheadfind.h"
#include "ui_chatdlg.h"
#include "widgets/MamHistorySource.h"
#include "widgets/actionlineedit.h"

#include <QPointer>
#include <QSet>

class IconAction;
class PsiContact;
class MamChatWidget;
class HistorySync;
namespace XMPP { class MAMTask; }

class PsiChatDlg : public ChatDlg {
    Q_OBJECT
public:
    PsiChatDlg(const Jid &jid, PsiAccount *account, TabManager *tabManager);
    ~PsiChatDlg();

    void setVSplitterPosition(int log, int chat) override;
    void         setJid(const Jid &jid) override;

    XMPP::EncryptedSession *encryptionSession() const override;
    QString                 encryptionMethodId() const override;

protected:
    // reimplemented
    void contextMenuEvent(QContextMenuEvent *) override;
    void doSend() override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setContactToolTip(QString text);
    void setEncryptionMethod(const QString &methodId, bool persist);
    void loadOlderMamMessages();
    void loadMamRange(const MamHistorySource::Request &);
    void sendDisplayedMarkerForVisibleMessage();
    bool displayNativeMessage(const MessageView &mv) override;
    bool scrollNativeHistory(bool down) override;
    void clearNativeHistory() override;
    void rebuildEncryptionSession();
    void showOwnFingerprint();
    void sendOwnPublicKey();
    void sendPublicKey();
    void sendMessage(const QString &body);

private slots:
    void    toggleSmallChat();
    void    doClearButton();
    void    doMiniCmd();
    void    doMinimize();
    void    addContact();
    void    buildMenu();
    void    updateCounter();
    void    updateIdentityVisibility();
    void    updateCountVisibility();
    void    updateContactAdding(PsiContact *c = nullptr);
    void    updateContactAdding(const Jid &j);
    void    verticalSplitterMoved(int, int);
    void    contactChanged();
    QString makeContactName(const QString &name, const Jid &jid) const;
    void    updateToolbuttons();
    void    doSwitchJidMode();
    void    copyUserJid();
    void    actActiveContacts();
    void    buildEncryptionMenu();
    void    sendButtonMenu();
    void    editTemplates();
    void    doPasteAndSend();
    void    sendTemp(const QString &);
    void    psButtonEnabled();

    // reimplemented
    void chatEditCreated() override;

private:
    void initToolBar();
    void initToolButtons();

    // reimplemented
    void      initUi() override;
    void      capsChanged() override;
    // Shows the call action as a call or as a whiteboard, with "Whiteboard only" when both are possible.
    void      updateCallAction();
    bool      isPgpEncryptionEnabled() const override;
    void      updateJidWidget(const QList<UserListItem *> &ul, int status, bool fromPresence) override;
    void      contactUpdated(UserListItem *u, int status, const QString &statusString) override;
    void      updateAvatar() override;
    void      updateAvatar(const XMPP::Jid &jid) override;
    void      optionsUpdate() override;
    void      updateEncryption() override;
    void      activated() override;
    void      setLooks() override;
    void      setShortcuts() override;
    void      appendSysMsg(const QString &) override;
    ChatView *chatView() const override;
    ChatEdit *chatEdit() const override;
    void      updateAutojidIcon();
    void      setJidComboItem(int pos, const QString &text, const Jid &jid, const QString &icon_str);

private:
    Ui::ChatDlg ui_;
    MamChatWidget *nativeTimeline_ = nullptr;
    bool              roomOccupantChat_ = false;
    bool              isRoomOccupantChat() const;
    QPointer<HistorySync> mamTask_;
    QString mamBeforeId_;
    QSet<QString>         mamSeenBeforeIds_;
    bool mamRefreshTail_ = false;
    qint64 localBeforeId_ = 0;
    bool localExhausted_ = false;
    bool mamExhausted_ = false;
    bool mamResumeAfterConnect_ = false;

    QMenu *pm_settings_;
    QMenu *encryptionMenu_ = nullptr;
    QMenu *callMenu_       = nullptr;

    QString                          selectedEncryptionMethod_;
    QPointer<XMPP::EncryptedSession> encryptionSession_;

    ActionList       *actions_;
    QAction          *act_mini_cmd_;
    QAction          *act_minimize_;
    TypeAheadFindBar *typeahead_;

    ActionLineEdit *le_autojid;
    IconAction     *act_autojid;
    IconAction     *act_active_contacts;
    IconAction     *act_pastesend_;

    MCmdManager    mCmdManager_;
    MCmdSimpleSite mCmdSite_;

    MCmdTabCompletion tabCompletion;

    bool smallChat_;
    class ChatDlgMCmdProvider;

    static PsiIcon *throbber_icon;

    int logHeight;
    int chateditHeight;
};

#endif // PSICHATDLG_H

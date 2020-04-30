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
#include "widgets/actionlineedit.h"

class IconAction;
class PsiContact;

class PsiChatDlg : public ChatDlg {
    Q_OBJECT
public:
    PsiChatDlg(const Jid &jid, PsiAccount *account, TabManager *tabManager);
    ~PsiChatDlg();

    virtual void setVSplitterPosition(int log, int chat);

protected:
    // reimplemented
    void contextMenuEvent(QContextMenuEvent *);
    void doSend();
    bool eventFilter(QObject *obj, QEvent *event);

private:
    void setContactToolTip(QString text);
    void showOwnFingerprint();

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
    void    actPgpToggled(bool);
    void    sendButtonMenu();
    void    editTemplates();
    void    doPasteAndSend();
    void    sendTemp(const QString &);
    void    psButtonEnabled();

    // reimplemented
    void chatEditCreated();

private:
    void initToolBar();
    void initToolButtons();

    // reimplemented
    void      initUi();
    void      capsChanged();
    bool      isEncryptionEnabled() const;
    void      updateJidWidget(const QList<UserListItem *> &ul, int status, bool fromPresence);
    void      contactUpdated(UserListItem *u, int status, const QString &statusString);
    void      updateAvatar();
    void      optionsUpdate();
    void      updatePGP();
    void      checkPGPAutostart();
    void      setPGPEnabled(bool enabled);
    void      activated();
    void      setLooks();
    void      setShortcuts();
    void      appendSysMsg(const QString &);
    ChatView *chatView() const;
    ChatEdit *chatEdit() const;
    void      updateAutojidIcon();
    void      setJidComboItem(int pos, const QString &text, const Jid &jid, const QString &icon_str);

private:
    Ui::ChatDlg ui_;

    QMenu *pm_settings_;

    ActionList *      actions_;
    QAction *         act_mini_cmd_;
    QAction *         act_minimize_;
    TypeAheadFindBar *typeahead_;

    ActionLineEdit *le_autojid;
    IconAction *    act_autojid;
    IconAction *    act_active_contacts;
    IconAction *    act_pastesend_;

    MCmdManager    mCmdManager_;
    MCmdSimpleSite mCmdSite_;

    MCmdTabCompletion tabCompletion;

    bool autoPGP_;
    bool smallChat_;
    class ChatDlgMCmdProvider;

    static PsiIcon *throbber_icon;

    int logHeight;
    int chateditHeight;
};

#endif // PSICHATDLG_H

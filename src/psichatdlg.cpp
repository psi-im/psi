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

#include "psichatdlg.h"

#include "accountlabel.h"
#include "actionlist.h"
#include "activecontactsmenu.h"
#include "avatars.h"
#include "avcall/avcall.h"
#include "coloropt.h"
#include "fancylabel.h"
#include "filesharingmanager.h"
#include "iconaction.h"
#include "iconlabel.h"
#include "iconselect.h"
#include "iconwidget.h"
#include "jidutil.h"
#include "lastactivitytask.h"
#include "messageview.h"
#include "msgmle.h"
#include "pgputil.h"
#include "psiaccount.h"
#include "psiactionlist.h"
#include "psicon.h"
#include "psicontact.h"
#include "psicontactlist.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "psitooltip.h"
#include "shortcutmanager.h"
#include "stretchwidget.h"
#include "tabdlg.h"
#include "textutil.h"
#include "userlist.h"
#include "xmpp_caps.h"
#include "xmpp_tasks.h"
#ifdef PSI_PLUGINS
#include "filesharedlg.h"
#include "pluginmanager.h"
#endif

#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDebug>
#include <QDragEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QResizeEvent>
#include <QSplitter>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>

#define MCMDCHAT "https://psi-im.org/ids/mcmd#chatmain"

PsiIcon *PsiChatDlg::throbber_icon = nullptr;

class PsiChatDlg::ChatDlgMCmdProvider : public QObject, public MCmdProviderIface {
    Q_OBJECT
public:
    ChatDlgMCmdProvider(PsiChatDlg *dlg) : QObject(dlg), dlg_(dlg) {};

    virtual bool mCmdTryStateTransit(MCmdStateIface *oldstate, QStringList command, MCmdStateIface *&newstate,
                                     QStringList &preset)
    {
        Q_UNUSED(preset);
        if (oldstate->getName() == MCMDCHAT) {
            QString cmd;
            if (command.count() > 0)
                cmd = command[0].toLower();
            if (cmd == "version") {
                JT_ClientVersion *version = new JT_ClientVersion(dlg_->account()->client()->rootTask());
                connect(version, SIGNAL(finished()), SLOT(version_finished()));

                // qDebug() << "querying: " << dlg_->jid().full();
                version->get(dlg_->jid());
                version->go();
                newstate = nullptr;
            } else if (cmd == "idle") {
                LastActivityTask *idle = new LastActivityTask(dlg_->jid(), dlg_->account()->client()->rootTask());
                connect(idle, SIGNAL(finished()), SLOT(lastactivity_finished()));
                idle->go();
                newstate = nullptr;
            } else if (cmd == "clear") {
                dlg_->doClear();
                newstate = nullptr;
            } else if (cmd == "vcard") {
                dlg_->doInfo();
                newstate = nullptr;
            } else if (cmd == "auth") {
                if (command.count() == 2) {
                    if (command[1].toLower() == "request") {
                        dlg_->account()->actionAuthRequest(dlg_->jid());
                    }
                }
                newstate = nullptr;
            } else if (cmd == "compact") {
                if (command.count() == 2) {
                    QString sub = command[1].toLower();
                    if ("on" == sub) {
                        dlg_->smallChat_ = true;
                    } else if ("off" == sub) {
                        dlg_->smallChat_ = false;
                    } else {
                        dlg_->appendSysMsg("usage: compact {on,off}");
                    }
                } else {
                    dlg_->smallChat_ = !dlg_->smallChat_;
                }
                dlg_->setLooks();
                newstate = nullptr;
            } else if (!cmd.isEmpty()) {
                return false;
            }
            return true;
        } else {
            return false;
        }
    };

    virtual QStringList mCmdTryCompleteCommand(MCmdStateIface *state, QString query, QStringList partcommand, int item)
    {
        Q_UNUSED(partcommand);
        QStringList all;
        if (state->getName() == MCMDCHAT) {
            if (item == 0) {
                all << "version"
                    << "idle"
                    << "clear"
                    << "vcard"
                    << "auth"
                    << "compact";
            }
        }
        QStringList res;
        for (const QString &cmd : all) {
            if (cmd.startsWith(query)) {
                res << cmd;
            }
        }
        return res;
    };

    virtual void mCmdSiteDestroyed() {};
    virtual ~ChatDlgMCmdProvider() {};

public slots:
    void version_finished()
    {
        JT_ClientVersion *version = qobject_cast<JT_ClientVersion *>(sender());
        if (!version) {
            dlg_->appendSysMsg("No version information available.");
            return;
        }
        dlg_->appendSysMsg(TextUtil::escape(
            QString("Version response: N: %2 V: %3 OS: %4").arg(version->name(), version->version(), version->os())));
    };

    void lastactivity_finished()
    {
        LastActivityTask *idle = static_cast<LastActivityTask *>(sender());

        if (!idle->success()) {
            dlg_->appendSysMsg("Could not determine time of last activity.");
            return;
        }

        if (idle->status().isEmpty()) {
            dlg_->appendSysMsg(QString("Last activity at %1").arg(idle->time().toString()));
        } else {
            dlg_->appendSysMsg(
                QString("Last activity at %1 (%2)").arg(idle->time().toString(), TextUtil::escape(idle->status())));
        }
    }

private:
    PsiChatDlg *dlg_;
};

PsiChatDlg::PsiChatDlg(const Jid &jid, PsiAccount *pa, TabManager *tabManager) :
    ChatDlg(jid, pa, tabManager), actions_(new ActionList("", 0, false)), mCmdManager_(&mCmdSite_),
    tabCompletion(&mCmdManager_)
{
    connect(account()->psi(), SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
    connect(account(), SIGNAL(addedContact(PsiContact *)), SLOT(updateContactAdding(PsiContact *)));
    connect(account(), SIGNAL(removedContact(PsiContact *)), SLOT(updateContactAdding(PsiContact *)));
    connect(account(), SIGNAL(updateContact(const Jid &)), SLOT(updateContactAdding(const Jid &)));
    mCmdManager_.registerProvider(new ChatDlgMCmdProvider(this));
    SendButtonTemplatesMenu *menu = getTemplateMenu();
    if (menu) {
        connect(menu, SIGNAL(doPasteAndSend()), this, SLOT(doPasteAndSend()));
        connect(menu, SIGNAL(doEditTemplates()), this, SLOT(editTemplates()));
        connect(menu, SIGNAL(doTemplateText(const QString &)), this, SLOT(sendTemp(const QString &)));
    }
}

PsiChatDlg::~PsiChatDlg()
{
    SendButtonTemplatesMenu *menu = getTemplateMenu();
    if (menu) {
        disconnect(menu, SIGNAL(doPasteAndSend()), this, SLOT(doPasteAndSend()));
        disconnect(menu, SIGNAL(doEditTemplates()), this, SLOT(editTemplates()));
        disconnect(menu, SIGNAL(doTemplateText(const QString &)), this, SLOT(sendTemp(const QString &)));
    }
    delete actions_;
}

void PsiChatDlg::initUi()
{
    ui_.setupUi(this);

    le_autojid = new ActionLineEdit(ui_.le_jid);
    ui_.le_jid->setLineEdit(le_autojid);
    ui_.le_jid->lineEdit()->setReadOnly(true);
    if (autoSelectContact_) {
        QStringList excl = PsiOptions::instance()
                               ->getOption("options.ui.chat.default-jid-mode-ignorelist")
                               .toString()
                               .toLower()
                               .split(",", QString::SkipEmptyParts);
        if (excl.indexOf(jid().bare()) == -1) {
            ui_.le_jid->insertItem(0, "auto", jid().full());
            ui_.le_jid->setCurrentIndex(0);
        } else {
            autoSelectContact_ = false;
        }
    }
    connect(ui_.le_jid, SIGNAL(activated(int)), this, SLOT(contactChanged()));
    UserListItem *ul = account()->findFirstRelevant(jid());
    if (!ul || !ul->isPrivate()) {
        act_autojid = new IconAction(this);
        updateAutojidIcon();
        connect(act_autojid, SIGNAL(triggered()), SLOT(doSwitchJidMode()));
        le_autojid->addAction(act_autojid);

        QAction *act_copy_user_jid = new QAction(tr("Copy user JID"), this);
        le_autojid->addAction(act_copy_user_jid);
        connect(act_copy_user_jid, SIGNAL(triggered()), SLOT(copyUserJid()));
    }

    ui_.lb_ident->setAccount(account());
    ui_.lb_ident->setShowJid(false);

    PsiToolTip::install(ui_.lb_status);
    ui_.lb_status->setPsiIcon(IconsetFactory::iconPtr("status/noauth"));
    setTabIcon(IconsetFactory::iconPtr("status/noauth")->icon());

    ui_.tb_emoticons->setIcon(IconsetFactory::icon("psi/smile").icon());

    connect(ui_.mle, SIGNAL(textEditCreated(QTextEdit *)), SLOT(chatEditCreated()));
    chatEditCreated();

#ifdef Q_OS_MAC
    // connect(chatView(), SIGNAL(selectionChanged()), SLOT(logSelectionChanged()));
#endif

    initToolButtons();
    initToolBar();
    updateAvatar();

    PsiToolTip::install(ui_.avatar);

    connect(account()->avatarFactory(), SIGNAL(avatarChanged(const Jid &)), this, SLOT(updateAvatar(const Jid &)));

    pm_settings_ = new QMenu(this);
    connect(pm_settings_, SIGNAL(aboutToShow()), SLOT(buildMenu()));
    ui_.tb_actions->setMenu(pm_settings_);
    ui_.tb_actions->setIcon(IconsetFactory::icon("psi/select").icon());
    ui_.tb_actions->setStyleSheet(" QToolButton::menu-indicator { image:none } ");

    connect(account()->client()->capsManager(), SIGNAL(capsChanged(const Jid &)), SLOT(capsChanged(const Jid &)));

    logHeight      = PsiOptions::instance()->getOption("options.ui.chat.log-height").toInt();
    chateditHeight = PsiOptions::instance()->getOption("options.ui.chat.chatedit-height").toInt();
    setVSplitterPosition(logHeight, chateditHeight);

    connect(ui_.splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(verticalSplitterMoved(int, int)));

    smallChat_ = PsiOptions::instance()->getOption("options.ui.chat.use-small-chats").toBool();
    ui_.pb_send->setIcon(IconsetFactory::icon("psi/action_button_send").icon());
    connect(ui_.pb_send, SIGNAL(clicked()), this, SLOT(doSend()));
    connect(ui_.pb_send, SIGNAL(customContextMenuRequested(const QPoint)), SLOT(sendButtonMenu()));

    act_mini_cmd_ = new QAction(this);
    act_mini_cmd_->setText(tr("Input command..."));
    connect(act_mini_cmd_, SIGNAL(triggered()), SLOT(doMiniCmd()));
    addAction(act_mini_cmd_);

    connect(ui_.log->textWidget(), SIGNAL(quote(const QString &)), ui_.mle->chatEdit(),
            SLOT(insertAsQuote(const QString &)));

    act_pastesend_ = new IconAction(tr("Paste and Send"), "psi/action_paste_and_send", tr("Paste and Send"), 0, this);
    connect(act_pastesend_, SIGNAL(triggered()), SLOT(doPasteAndSend()));

    ui_.log->realTextWidget()->installEventFilter(this);
    ui_.mini_prompt->hide();

    if (throbber_icon == nullptr) {
        throbber_icon = const_cast<PsiIcon *>(IconsetFactory::iconPtr("psi/throbber"));
    }
#ifdef PSI_PLUGINS
    PluginManager::instance()->setupChatTab(this, account(), jid().full());
#endif
    if (PsiOptions::instance()->getOption("options.media.audio-message").toBool())
        ui_.mle->chatEdit()->addSoundRecButton();
}

void PsiChatDlg::verticalSplitterMoved(int, int)
{
    QList<int> list = ui_.splitter->sizes();
    logHeight       = list.first();
    chateditHeight  = list.last();
    PsiOptions::instance()->setOption("options.ui.chat.log-height", logHeight);
    PsiOptions::instance()->setOption("options.ui.chat.chatedit-height", chateditHeight);

    emit vSplitterMoved(logHeight, chateditHeight);
}

void PsiChatDlg::setVSplitterPosition(int log, int chat)
{
    QList<int> list;
    logHeight      = log;
    chateditHeight = chat;
    list << log << chat;
    ui_.splitter->setSizes(list);
}

void PsiChatDlg::updateCountVisibility()
{
    if (PsiOptions::instance()->getOption("options.ui.message.show-character-count").toBool() && !smallChat_) {
        ui_.lb_count->show();
    } else {
        ui_.lb_count->hide();
    }
}

void PsiChatDlg::setLooks()
{
    ChatDlg::setLooks();

    const QString css = PsiOptions::instance()->getOption("options.ui.chat.css").toString();
    if (!css.isEmpty()) {
        setStyleSheet(css);
        chatEdit()->setCssString(css);
    }
    ui_.splitter->optionsChanged();
    ui_.mle->optionsChanged();

    int s = PsiIconset::instance()->system().iconSize();
    ui_.lb_status->setFixedSize(s, s);
    ui_.lb_client->setFixedSize(s, s);

    ui_.tb_pgp->hide();
    if (smallChat_) {
        ui_.lb_status->hide();
        ui_.le_jid->hide();
        ui_.tb_actions->hide();
        ui_.tb_emoticons->hide();
        ui_.toolbar->hide();
        ui_.tb_voice->hide();
        ui_.lb_client->hide();
    } else {
        ui_.lb_client->show();
        ui_.lb_status->show();
        ui_.le_jid->show();
        if (PsiOptions::instance()->getOption("options.ui.contactlist.toolbars.m0.visible").toBool()) {
            ui_.toolbar->show();
            ui_.tb_actions->hide();
            ui_.tb_emoticons->hide();
            ui_.tb_voice->hide();
        } else {
            ui_.toolbar->hide();
            ui_.tb_emoticons->setVisible(
                PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool());
            ui_.tb_actions->show();
            ui_.tb_voice->setVisible(AvCallManager::isSupported());
        }
    }

    updateIdentityVisibility();
    updateCountVisibility();
    updateContactAdding();

    // toolbuttons
    QIcon i;
    i.addPixmap(IconsetFactory::icon("psi/cryptoNo").impix(), QIcon::Normal, QIcon::Off);
    i.addPixmap(IconsetFactory::icon("psi/cryptoYes").impix(), QIcon::Normal, QIcon::On);
    actions_->action("chat_pgp")->setPsiIcon(nullptr);
    actions_->action("chat_pgp")->setIcon(i);
}

void PsiChatDlg::setShortcuts()
{
    ChatDlg::setShortcuts();

    actions_->action("chat_clear")->setShortcuts(ShortcutManager::instance()->shortcuts("chat.clear"));
    // typeahead find bar
    actions_->action("chat_find")->setShortcuts(ShortcutManager::instance()->shortcuts("chat.find"));
    // -- typeahead
    actions_->action("chat_info")->setShortcuts(ShortcutManager::instance()->shortcuts("common.user-info"));
    actions_->action("chat_history")->setShortcuts(ShortcutManager::instance()->shortcuts("common.history"));

    act_mini_cmd_->setShortcuts(ShortcutManager::instance()->shortcuts("chat.quick-command"));

    act_minimize_ = new QAction(this);

    connect(act_minimize_, SIGNAL(triggered()), SLOT(doMinimize()));
    addAction(act_minimize_);
    act_minimize_->setShortcuts(ShortcutManager::instance()->shortcuts("chat.minimize"));
}

void PsiChatDlg::updateIdentityVisibility()
{
    if (!smallChat_) {
        bool visible = account()->psi()->contactList()->enabledAccounts().count() > 1;
        ui_.lb_ident->setVisible(visible);
    } else {
        ui_.lb_ident->setVisible(false);
    }

    if (PsiOptions::instance()->getOption("options.ui.disable-send-button").toBool()) {
        ui_.pb_send->hide();
    } else {
        ui_.pb_send->show();
    }
}

void PsiChatDlg::updateContactAdding(PsiContact *c)
{
    if (!c || realJid().compare(c->jid(), false)) {
        Jid           rj = realJid();
        UserListItem *uli;
        if (rj.isNull() || ((uli = account()->findFirstRelevant(rj)) && (uli->inList() || uli->isSelf()))) {
            actions_->action("chat_add_contact")->setVisible(false);
        } else {
            actions_->action("chat_add_contact")->setVisible(true);
        }
    }
}

void PsiChatDlg::updateToolbuttons()
{
    ui_.toolbar->clear();
    PsiOptions *options      = PsiOptions::instance();
    QStringList actionsNames = options->getOption("options.ui.contactlist.toolbars.m0.actions").toStringList();
    for (const QString &actionName : actionsNames) {
        if (actionName == "chat_voice" && !AvCallManager::isSupported()) {
            continue;
        }
        if (actionName == "chat_pgp" && !options->getOption("plugins.auto-load.openpgp").toBool()) {
            continue;
        }

#ifdef PSI_PLUGINS
        if (actionName.endsWith("-plugin")) {
            QString name = PluginManager::instance()->pluginName(actionName.mid(0, actionName.length() - 7));
            if (!name.isEmpty())
                PluginManager::instance()->addToolBarButton(this, ui_.toolbar, account(), jid().full(), name);
            continue;
        }
#endif

        // Hack. separator action can be added only once.
        if (actionName == "separator") {
            ui_.toolbar->addSeparator();
            continue;
        }

        IconAction *action = actions_->action(actionName);
        if (action) {
            action->addTo(ui_.toolbar);
            if (actionName == QLatin1String("chat_icon") || actionName == QLatin1String("chat_templates")) {
                static_cast<QToolButton *>(ui_.toolbar->widgetForAction(action))
                    ->setPopupMode(QToolButton::InstantPopup);
            }
        }
    }
}

void PsiChatDlg::copyUserJid() { QApplication::clipboard()->setText(jid().bare()); }

void PsiChatDlg::updateContactAdding(const Jid &j)
{
    if (realJid().compare(j, false)) {
        updateContactAdding();
    }
}

void PsiChatDlg::initToolButtons()
{
    // typeahead find
    QHBoxLayout *hb3a = new QHBoxLayout();
    typeahead_        = new TypeAheadFindBar(ui_.log->textWidget(), tr("Find toolbar"), nullptr);
    hb3a->addWidget(typeahead_);
    ui_.vboxLayout1->addLayout(hb3a);
    // -- typeahead

    ActionList *list = account()->psi()->actionList()->actionLists(PsiActionList::Actions_Chat).at(0);
    for (const QString &name : list->actions()) {
        IconAction *action = list->action(name)->copy();
        action->setParent(this);
        actions_->addAction(name, action);
        if (name == QString::fromLatin1("chat_clear")) {
            connect(action, SIGNAL(triggered()), SLOT(doClearButton()));
        } else if (name == QString::fromLatin1("chat_find")) {
            // typeahead find
            connect(action, SIGNAL(triggered()), typeahead_, SLOT(toggleVisibility()));
            // -- typeahead
        } else if (name == QString::fromLatin1("chat_html_text")) {
            connect(action, SIGNAL(triggered()), chatEdit(), SLOT(doHTMLTextMenu()));
        } else if (name == QString::fromLatin1("chat_add_contact")) {
            connect(action, SIGNAL(triggered()), SLOT(addContact()));
        } else if (name == QString::fromLatin1("chat_icon")) {
            connect(account()->psi()->iconSelectPopup(), SIGNAL(textSelected(QString)), this,
                    SLOT(addEmoticon(QString)));
            action->setMenu(account()->psi()->iconSelectPopup());
            ui_.tb_emoticons->setMenu(account()->psi()->iconSelectPopup());
        } else if (name == QString::fromLatin1("chat_voice")) {
            connect(action, SIGNAL(triggered()), SLOT(doVoice()));
            // act_voice_->setEnabled(false);
            ui_.tb_voice->setDefaultAction(actions_->action("chat_voice"));
        } else if (name == QString::fromLatin1("chat_file")) {
            connect(action, SIGNAL(triggered()), SLOT(doFile()));
        } else if (name == QString::fromLatin1("chat_pgp")) {
            ui_.tb_pgp->setDefaultAction(actions_->action("chat_pgp"));
            connect(action, SIGNAL(triggered(bool)), SLOT(actPgpToggled(bool)));
        } else if (name == QString::fromLatin1("chat_info")) {
            connect(action, SIGNAL(triggered()), SLOT(doInfo()));
        } else if (name == QString::fromLatin1("chat_history")) {
            connect(action, SIGNAL(triggered()), SLOT(doHistory()));
        } else if (name == QString::fromLatin1("chat_compact")) {
            connect(action, SIGNAL(triggered()), SLOT(toggleSmallChat()));
        } else if (name == QString::fromLatin1("chat_active_contacts")) {
            connect(action, SIGNAL(triggered()), SLOT(actActiveContacts()));
        } else if (name == QString::fromLatin1("chat_share_files")) {
            connect(action, &QAction::triggered, account(), [this]() {
                FileShareDlg::shareFiles(
                    account(), account()->selfContact()->jid(),
                    [this](const QList<XMPP::Reference> &&rl, const QString &desc) {
                        doFileShare(std::move(rl), desc);
                    },
                    this);
            });
        } else if (name == "chat_pin_tab") {
            connect(action, SIGNAL(triggered()), SLOT(pinTab()));
        } else if (name == "chat_templates") {
            action->setMenu(getTemplateMenu());
        }
    }

    list = account()->psi()->actionList()->actionLists(PsiActionList::Actions_Common).at(0);
    for (const QString &name : list->actions()) {
        IconAction *action = list->action(name)->copy();
        action->setParent(this);
        actions_->addAction(name, action);
    }
}

void PsiChatDlg::initToolBar()
{
    ui_.toolbar->setWindowTitle(tr("Chat Toolbar"));
    int s = PsiIconset::instance()->system().iconSize();
    ui_.toolbar->setIconSize(QSize(s, s));

    updateToolbuttons();
}

void PsiChatDlg::contextMenuEvent(QContextMenuEvent *) { pm_settings_->exec(QCursor::pos()); }

void PsiChatDlg::capsChanged()
{
    ChatDlg::capsChanged();

    QString       resource = jid().resource();
    UserListItem *ul       = account()->findFirstRelevant(jid());
    if (resource.isEmpty() && ul && !ul->userResourceList().isEmpty()) {
        resource = (*(ul->userResourceList().priority())).name();
    }
    // act_voice_->setEnabled(!account()->capsManager()->isEnabled() || (ul && ul->isAvailable() &&
    // account()->capsManager()->features(jid().withResource(resource)).canVoice()));
}

void PsiChatDlg::activated()
{
    ChatDlg::activated();

    updateCountVisibility();
}

void PsiChatDlg::setContactToolTip(QString text)
{
    ui_.lb_status->setToolTip(text);
    ui_.avatar->setToolTip(text);
}

void PsiChatDlg::showOwnFingerprint()
{
#ifdef HAVE_PGPUTIL
    const QString &&fingerprint = PGPUtil::getFingerprint(account()->pgpKeyId());
    if (!fingerprint.isEmpty()) {
        const QString &&msg = tr("Fingerprint for account \"%1\": %2").arg(account()->name()).arg(fingerprint);
        appendSysMsg(msg);
    }
#endif // HAVE_PGPUTIL
}

void PsiChatDlg::sendOwnPublicKey()
{
#ifdef HAVE_PGPUTIL
    if (!account()->hasPgp())
        return;

    const auto &&keyData = PGPUtil::getPublicKeyData(account()->pgpKeyId());
    sendMessage(keyData);

    const auto &&keyOwnerName = PGPUtil::getKeyOwnerName(account()->pgpKeyId());
    const auto &&shortId      = account()->pgpKeyId().right(8);
    appendSysMsg(tr("Public key \"%1\" sent").arg(keyOwnerName + " " + shortId));
#endif // HAVE_PGPUTIL
}

void PsiChatDlg::sendPublicKey()
{
#ifdef HAVE_PGPUTIL
    // Select a key
    QString &&keyId = PGPUtil::chooseKey(PGPKeyDlg::Public, QString(), tr("Choose Public Key"));
    if (keyId.isEmpty())
        return;

    const auto &&keyData = PGPUtil::getPublicKeyData(keyId);
    sendMessage(keyData);

    const auto &&keyOwnerName = PGPUtil::getKeyOwnerName(keyId);
    const auto &&shortId      = keyId.right(8);
    appendSysMsg(tr("Public key \"%1\" sent").arg(keyOwnerName + " " + shortId));
#endif // HAVE_PGPUTIL
}

void PsiChatDlg::sendMessage(const QString &body)
{
    // Copy-paste from PluginHost::sendMessage()!
    // TODO(mck): yeah, that's sick..
    const QString &&xml = QString("<message to='%1' type='%4'><subject>%3</subject><body>%2</body></message>")
                              .arg(TextUtil::escape(jid().bare()))
                              .arg(TextUtil::escape(body))
                              .arg(TextUtil::escape(""))
                              .arg(TextUtil::escape("chat"));

    account()->client()->send(xml);
}

void PsiChatDlg::updateJidWidget(const QList<UserListItem *> &ul, int status, bool fromPresence)
{
    static bool internal_change = false;
    if (!internal_change) {
        // Filling jid's combobox
        const UserListItem *u = ul.first();
        if (!u)
            return;
        UserResourceList resList  = u->userResourceList();
        const QString    name     = u->name();
        QComboBox *      jidCombo = ui_.le_jid;
        if (!u->isPrivate()) {
            // If no conference private chat
            const int combo_idx = jidCombo->currentIndex();
            Jid       old_jid   = (combo_idx != -1) ? Jid(jidCombo->itemData(combo_idx).toString()) : Jid();
            // if (fromPresence || jid() != old_jid) {
            bool auto_mode    = autoSelectContact_;
            Jid  new_auto_jid = jid();
            if (auto_mode) {
                if (fromPresence && !resList.isEmpty()) {
                    UserResourceList::ConstIterator it = resList.priority();
                    new_auto_jid                       = jid().withResource((*it).name());
                }
            }
            // Filling address combobox
            QString   iconStr;
            const int resCnt = resList.size();
            if (resCnt == 1) {
                UserResourceList::ConstIterator it = resList.begin();
                if (it != resList.end() && (*it).name().isEmpty()) {
                    // Empty resource,  but online. Transport?
                    QString client(u->findClient(*it));
                    if (!client.isEmpty()) {
                        iconStr = "clients/" + client;
                    }
                }
            }
            setJidComboItem(0, makeContactName(name, u->jid().bare()), u->jid().bare(), iconStr);
            int new_index  = -1;
            int curr_index = 1;
            for (UserResourceList::ConstIterator it = resList.begin(); it != resList.end(); it++) {
                UserResource r = *it;
                if (!r.name().isEmpty()) {
                    Jid     tmp_jid(u->jid().withResource(r.name()));
                    QString client(u->findClient(r));
                    QString iconStr2;
                    if (!client.isEmpty()) {
                        iconStr2 = "clients/" + client;
                    }
                    setJidComboItem(curr_index, makeContactName(name, tmp_jid), tmp_jid, iconStr2);
                    if (new_index == -1 && tmp_jid == new_auto_jid) {
                        new_index = curr_index;
                    }
                    curr_index++;
                }
            }
            if (new_index == -1) {
                new_index = 0;
                if (autoSelectContact_) {
                    new_auto_jid = jid().bare();
                } else {
                    if (!jid().resource().isEmpty()) {
                        new_index = jidCombo->count();
                        setJidComboItem(curr_index, makeContactName(name, jid()), jid(), iconStr);
                        new_index = curr_index++;
                    }
                }
            }
            // Сlean combobox's tail
            while (curr_index < jidCombo->count())
                jidCombo->removeItem(curr_index);

            ui_.le_jid->setCurrentIndex(new_index);
            if (new_auto_jid != jid()) {
                internal_change = true;
                setJid(new_auto_jid);
                if (old_jid != new_auto_jid) {
                    if (autoSelectContact_ && (status != XMPP::Status::Offline || !new_auto_jid.resource().isEmpty())) {
                        appendSysMsg(tr("Contact has been switched: %1")
                                         .arg(TextUtil::escape(JIDUtil::toString(new_auto_jid, true))));
                    }
                }
            }
            //}
        } else {
            // Conference private chat
            QString                         iconStr;
            Jid                             tmp_jid = jid();
            UserResourceList::ConstIterator it      = resList.begin();
            if (it != resList.end()) {
                QString client(u->findClient(*it));
                if (!client.isEmpty()) {
                    iconStr = "clients/" + client;
                }
                tmp_jid = u->jid().withResource((*it).name());
            } else if (jidCombo->count() > 0) {
                tmp_jid = Jid(jidCombo->itemData(0).toString());
            }
            if (tmp_jid.isValid()) {
                setJidComboItem(0, makeContactName(name, tmp_jid), tmp_jid, iconStr);
            }
            // Clean combobox's tail
            while (jidCombo->count() > 1)
                jidCombo->removeItem(1);
            //-
            jidCombo->setCurrentIndex(0);
        }
        jidCombo->setToolTip(jidCombo->currentText());
    }
    internal_change = false;
}

void PsiChatDlg::actActiveContacts()
{
    ActiveContactsMenu *acm = new ActiveContactsMenu(account()->psi(), this);
    if (!acm->actions().isEmpty())
        acm->exec(QCursor::pos());
    delete acm;
}

void PsiChatDlg::contactUpdated(UserListItem *u, int status, const QString &statusString)
{
    Q_UNUSED(statusString);

    if (status == -1 || !u) {
        ui_.lb_status->setPsiIcon(IconsetFactory::iconPtr("status/noauth"));
        setTabIcon(IconsetFactory::iconPtr("status/noauth")->icon());
    } else {
        ui_.lb_status->setPsiIcon(PsiIconset::instance()->statusPtr(jid(), status));
        setTabIcon(PsiIconset::instance()->statusPtr(jid(), status)->icon());
    }

    if (u) {
        setContactToolTip(u->makeTip(true, false));
    } else {
        setContactToolTip(QString());
    }

    if (u) {
        UserResourceList srl = u->userResourceList();
        if (!srl.isEmpty()) {
            UserResource r;
            if (!jid().resource().isEmpty()) {
                QString                         res = jid().resource();
                UserResourceList::ConstIterator it  = srl.find(res);
                if (it != srl.end())
                    r = *it;
            }
            if (r.clientName().isEmpty()) {
                srl.sort();
                r = srl.first();
            }
            QString client(u->findClient(r));
            if (!client.isEmpty()) {
                const QPixmap &pix = IconsetFactory::iconPixmap("clients/" + client);
                ui_.lb_client->setPixmap(pix);
            }
            ui_.lb_client->setToolTip(r.versionString());
        }
    }
}

void PsiChatDlg::updateAvatar()
{
    QString res;
    QString client;

    if (!PsiOptions::instance()->getOption("options.ui.chat.avatars.show").toBool()) {
        ui_.avatar->hide();
        return;
    }

    UserListItem *ul       = account()->findFirstRelevant(jid());
    bool          private_ = false;
    if (ul && !ul->userResourceList().isEmpty()) {
        UserResourceList::Iterator it = ul->userResourceList().find(jid().resource());
        if (it == ul->userResourceList().end())
            it = ul->userResourceList().priority();

        res      = (*it).name();
        client   = (*it).clientName();
        private_ = ul->isPrivate();
    }
    // QPixmap p = account()->avatarFactory()->getAvatar(jid().withResource(res),client);
    QPixmap p = private_ ? account()->avatarFactory()->getMucAvatar(jid().withResource(res))
                         : account()->avatarFactory()->getAvatar(jid().withResource(res));
    if (p.isNull()) {
        if (!PsiOptions::instance()->getOption("options.ui.contactlist.avatars.use-default-avatar").toBool()) {
            ui_.avatar->hide();
            return;
        }
        p = IconsetFactory::iconPixmap("psi/default_avatar");
    }
    int optSize = PsiOptions::instance()->getOption("options.ui.chat.avatars.size").toInt();
    ui_.avatar->setFixedSize(optSize, optSize);
    int avatarSize = p.width(); // qMax(p.width(), p.height());
    if (avatarSize > optSize)
        avatarSize = optSize;
    ui_.avatar->setPixmap(p.scaled(QSize(avatarSize, avatarSize), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui_.avatar->show();
}

void PsiChatDlg::optionsUpdate()
{
    smallChat_ = PsiOptions::instance()->getOption("options.ui.chat.use-small-chats").toBool();

    updateToolbuttons();
    ChatDlg::optionsUpdate();
    if (!ui_.mle->chatEdit()->hasSoundRecButton()
        && PsiOptions::instance()->getOption("options.media.audio-message").toBool()) {
        ui_.mle->chatEdit()->addSoundRecButton();
    } else if (ui_.mle->chatEdit()->hasSoundRecButton()) {
        ui_.mle->chatEdit()->removeSoundRecButton();
    }
    // typeahead find bar
    typeahead_->optionsUpdate();
}

void PsiChatDlg::updatePgp()
{
    if (account()->hasPgp()) {
        actions_->action("chat_pgp")->setEnabled(true);
        actions_->action("chat_pgp")->setToolTip(tr("OpenPGP encryption"));
    } else {
        setPgpEnabled(false);
        actions_->action("chat_pgp")->setEnabled(false);
        actions_->action("chat_pgp")->setToolTip(tr("OpenPGP key is not set in your account settings!"));
    }

    checkPgpAutostart();

    ui_.tb_pgp->setVisible(
        account()->hasPgp() && !smallChat_
        && !PsiOptions::instance()->getOption("options.ui.contactlist.toolbars.m0.visible").toBool());
    ui_.log->setPgpEncryptionEnabled(isPgpEncryptionEnabled());
}

void PsiChatDlg::checkPgpAutostart()
{
    const bool enabled = PsiOptions::instance()->getOption("plugins.auto-load.openpgp").toBool();
    if (account()->hasPgp() && enabled) {
        const bool alwaysEnabled = PsiOptions::instance()->getOption("options.pgp.always-enabled").toBool();
        if (alwaysEnabled) {
            setPgpEnabled(true);
        } else {
            // Get stored data from account settings:
            setPgpEnabled(account()->isPgpEnabled(jid()));
        }
    } else {
        setPgpEnabled(false);
    }
}

void PsiChatDlg::actPgpToggled(bool b)
{
#ifdef HAVE_PGPUTIL
    actions_->action("chat_pgp")->setChecked(!b);

    if (!account()->hasPgp() || !PGPUtil::instance().pgpAvailable())
        return;

    QMenu *  menu                  = new QMenu();
    QAction *actEnablePgp          = new QAction(tr("Enable OpenPGP encryption"), this);
    QAction *actDisablePgp         = new QAction(tr("Disable OpenPGP encryption"), this);
    QAction *actAssignKey          = new QAction(tr("Assign Open&PGP Key"), this);
    QAction *actUnassignKey        = new QAction(tr("Unassign Open&PGP Key"), this);
    QAction *actShowOwnFingerprint = new QAction(tr("Show own &fingerprint"), this);
    QAction *actSendOwnPublicKey   = new QAction(tr("Send own public key"), this);
    QAction *actSendPublicKey      = new QAction(tr("Send public key..."), this);

    actEnablePgp->setVisible(b);
    actDisablePgp->setVisible(!b);

    menu->addAction(actEnablePgp);
    menu->addAction(actDisablePgp);

    UserListItem *item = account()->findFirstRelevant(jid());
    if (item) {
        actAssignKey->setVisible(item->publicKeyID().isEmpty());
        actUnassignKey->setVisible(!item->publicKeyID().isEmpty());

        menu->addSeparator();
        menu->addAction(actAssignKey);
        menu->addAction(actUnassignKey);
    }
    menu->addAction(actShowOwnFingerprint);
    menu->addSeparator();
    menu->addAction(actSendOwnPublicKey);
    menu->addAction(actSendPublicKey);

    QAction *act = menu->exec(QCursor::pos());
    if (act == actEnablePgp) {
        ui_.log->setPgpEncryptionEnabled(true);
        actions_->action("chat_pgp")->setChecked(true);
        account()->setPgpEnabled(jid(), true);
    } else if (act == actDisablePgp) {
        ui_.log->setPgpEncryptionEnabled(false);
        actions_->action("chat_pgp")->setChecked(false);
        account()->setPgpEnabled(jid(), false);
    } else if (act == actAssignKey) {
        if (item) {
            account()->actionAssignPgpKey(jid());
        }
    } else if (act == actUnassignKey) {
        if (item) {
            account()->actionUnassignPgpKey(jid());
        }
    } else if (act == actShowOwnFingerprint) {
        showOwnFingerprint();
    } else if (act == actSendOwnPublicKey) {
        sendOwnPublicKey();
    } else if (act == actSendPublicKey) {
        sendPublicKey();
    }

    delete menu;
#endif // HAVE_PGPUTIL
}

void PsiChatDlg::doClearButton()
{
    if (PsiOptions::instance()->getOption("options.ui.chat.warn-before-clear").toBool()) {
        switch (QMessageBox::warning(
            this, tr("Warning"),
            tr("Are you sure you want to clear the chat window?\n(note: does not affect saved history)"),
            QMessageBox::Yes, QMessageBox::YesAll, QMessageBox::No)) {
        case QMessageBox::No:
            break;
        case QMessageBox::YesAll:
            PsiOptions::instance()->setOption("options.ui.chat.warn-before-clear", false);
            // fall-through
        case QMessageBox::Yes:
            doClear();
        }
    } else {
        doClear();
    }
}

void PsiChatDlg::setPgpEnabled(bool enabled)
{
    actions_->action("chat_pgp")->setChecked(enabled);
    ui_.log->setPgpEncryptionEnabled(enabled);
}

void PsiChatDlg::toggleSmallChat()
{
    smallChat_ = !smallChat_;
    setLooks();
}

void PsiChatDlg::buildMenu()
{
    // Dialog menu
    pm_settings_->clear();
    pm_settings_->addAction(actions_->action("chat_compact"));
    pm_settings_->addAction(actions_->action("chat_clear"));
    pm_settings_->addSeparator();

    pm_settings_->addAction(actions_->action("chat_icon"));
    pm_settings_->addAction(actions_->action("chat_templates"));
    pm_settings_->addAction(act_pastesend_);
    pm_settings_->addAction(actions_->action("chat_file"));
    if (AvCallManager::isSupported()) {
        pm_settings_->addAction(actions_->action("chat_voice"));
    }
    pm_settings_->addAction(actions_->action("chat_pgp"));
    pm_settings_->addSeparator();

    pm_settings_->addAction(actions_->action("chat_info"));
    pm_settings_->addAction(actions_->action("chat_history"));
    auto dlg = getManagingTabDlg();
    if (dlg && PsiOptions::instance()->getOption("options.ui.tabs.multi-rows").toBool()) {
        pm_settings_->addAction(actions_->action("chat_pin_tab"));

    } // else it's not tabbed dialog
#ifdef PSI_PLUGINS
    if (!PsiOptions::instance()->getOption("options.ui.contactlist.toolbars.m0.visible").toBool()) {
        pm_settings_->addSeparator();
        PluginManager::instance()->addToolBarButton(this, pm_settings_, account(), jid().full());
    }
#endif
}

void PsiChatDlg::updateCounter() { ui_.lb_count->setNum(chatEdit()->toPlainText().length()); }

bool PsiChatDlg::isPgpEncryptionEnabled() const { return actions_->action("chat_pgp")->isChecked(); }

void PsiChatDlg::appendSysMsg(const QString &str)
{
    MessageView mv = MessageView::systemMessage(str);
    dispatchMessage(mv);
}

ChatView *PsiChatDlg::chatView() const { return ui_.log; }

ChatEdit *PsiChatDlg::chatEdit() const { return ui_.mle->chatEdit(); }

void PsiChatDlg::chatEditCreated()
{
    ChatDlg::chatEditCreated();

    connect(chatEdit(), SIGNAL(textChanged()), this, SLOT(updateCounter()));
    chatEdit()->installEventFilter(this);

    mCmdSite_.setInput(chatEdit());
    mCmdSite_.setPrompt(ui_.mini_prompt);
    tabCompletion.setTextEdit(chatEdit());

    connect(chatEdit(), &ChatEdit::fileSharingRequested, this, [this](const QMimeData *data) {
        FileShareDlg::shareFiles(
            account(), account()->selfContact()->jid(), data,
            [this](const QList<XMPP::Reference> &&rl, const QString &desc) { doFileShare(std::move(rl), desc); }, this);
    });
}

void PsiChatDlg::sendButtonMenu()
{
    SendButtonTemplatesMenu *menu = getTemplateMenu();
    if (menu) {
        menu->setParams(true);
        menu->exec(QCursor::pos());
        menu->setParams(false);
        chatEdit()->setFocus();
    }
}

void PsiChatDlg::editTemplates()
{
    if (ChatDlg::isActiveTab()) {
        showTemplateEditor();
    }
}

void PsiChatDlg::doPasteAndSend()
{
    if (ChatDlg::isActiveTab()) {
        chatEdit()->paste();
        doSend();
        act_pastesend_->setEnabled(false);
        QTimer::singleShot(2000, this, SLOT(psButtonEnabled()));
    }
}

void PsiChatDlg::psButtonEnabled() { act_pastesend_->setEnabled(true); }

void PsiChatDlg::sendTemp(const QString &templText)
{
    if (ChatDlg::isActiveTab()) {
        if (!templText.isEmpty()) {
            chatEdit()->textCursor().insertText(templText);
            if (!PsiOptions::instance()->getOption("options.ui.chat.only-paste-template").toBool())
                doSend();
        }
    }
}

void PsiChatDlg::doSend()
{
    tabCompletion.reset();
    if (mCmdSite_.isActive()) {
        QString str = chatEdit()->toPlainText();
        if (!mCmdManager_.processCommand(str)) {
            appendSysMsg(tr("Error: Can not parse command: ") + TextUtil::escape(str));
        }
    } else {
        ChatDlg::doSend();
    }
}

void PsiChatDlg::doMiniCmd() { mCmdManager_.open(new MCmdSimpleState(MCMDCHAT, tr("Command>")), QStringList()); }

void PsiChatDlg::addContact()
{
    Jid           j(realJid());
    UserListItem *uli  = account()->findFirstRelevant(jid());
    QString       name = uli && !uli->name().isEmpty() ? uli->name() : j.node();
    account()->openAddUserDlg(j.withResource(""), name.isEmpty() ? j.node() : name, "");
}

bool PsiChatDlg::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == chatEdit()) {
        if (ev->type() == QEvent::KeyPress) {
            QKeyEvent *e = static_cast<QKeyEvent *>(ev);

            if (e->key() == Qt::Key_Tab) {
                tabCompletion.tryComplete();
                return true;
            }

            tabCompletion.reset();
        }
    }

    else if (obj == ui_.log->realTextWidget()) {
        if (ev->type() == QEvent::MouseButtonPress)
            chatEdit()->setFocus();
    }

    return ChatDlg::eventFilter(obj, ev);
}

void PsiChatDlg::doMinimize() { window()->showMinimized(); }

QString PsiChatDlg::makeContactName(const QString &name, const Jid &jid) const
{
    QString name_;
    QString j = JIDUtil::toString(jid, true);
    if (!name.isEmpty())
        name_ = name + QString(" <%1>").arg(j);
    else
        name_ = j;
    return name_;
}

void PsiChatDlg::contactChanged() /* current jid was chanegd in Jid combobox.TODO rename this func*/
{
    int curr_index = ui_.le_jid->currentIndex();
    Jid jid_(ui_.le_jid->itemData(curr_index).toString());
    if (jid_ != jid()) {
        autoSelectContact_ = false;
        setJid(jid_);
        updateAutojidIcon();
    }
}

void PsiChatDlg::updateAutojidIcon()
{
    QIcon   icon(IconsetFactory::iconPixmap("psi/autojid"));
    QPixmap pix;
    QString text;
    if (autoSelectContact_) {
        pix  = icon.pixmap(QSize(16, 16), QIcon::Normal, QIcon::Off);
        text = tr("turn off autojid");
    } else {
        pix  = icon.pixmap(QSize(16, 16), QIcon::Disabled, QIcon::Off);
        text = tr("turn on autojid");
    }
    act_autojid->setIcon(QIcon(pix));
    act_autojid->setText(text);
    act_autojid->setToolTip(text);
    act_autojid->setStatusTip(text);
}

void PsiChatDlg::setJidComboItem(int pos, const QString &text, const Jid &jid, const QString &icon_str)
{
    // Warning! If pos >= items count, the element will be added in a list tail
    //-
    QIcon      icon;
    QComboBox *jid_combo = ui_.le_jid;
    if (!icon_str.isEmpty()) {
        const PsiIcon picon = IconsetFactory::icon(icon_str);
        icon                = picon.icon();
    }
    if (jid_combo->count() > pos) {
        jid_combo->setItemText(pos, text);
        jid_combo->setItemData(pos, JIDUtil::toString(jid, true));
        jid_combo->setItemIcon(pos, icon);
    } else {
        jid_combo->addItem(icon, text, JIDUtil::toString(jid, true));
    }
}

void PsiChatDlg::doSwitchJidMode()
{
    autoSelectContact_ = !autoSelectContact_;
    updateAutojidIcon();
    if (autoSelectContact_) {
        const QList<UserListItem *> ul         = account()->findRelevant(jid().bare());
        UserStatus                  userStatus = userStatusFor(jid(), ul, false);
        updateJidWidget(ul, userStatus.statusType, true);
        userStatus = userStatusFor(jid(), ul, false);
        contactUpdated(userStatus.userListItem, userStatus.statusType, userStatus.status);
    }
}

#include "psichatdlg.moc"

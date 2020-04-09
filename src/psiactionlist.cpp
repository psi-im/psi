/*
 * psiactionlist.cpp - the customizeable action list for Psi
 * Copyright (C) 2004  Michail Pishchagin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

#include "psiactionlist.h"

#include "iconset.h"
#include "mainwin_p.h"
#ifdef PSI_PLUGINS
#include "pluginmanager.h"
#endif
#include "psioptions.h"
#include "shortcutmanager.h"

#include <QObject>
#include <QPointer>
#include <map>
#include <utility>

using namespace std;

//----------------------------------------------------------------------------
// PsiActionList::Private
//----------------------------------------------------------------------------

class PsiActionList::Private : public QObject {
    Q_OBJECT
public:
    Private(PsiActionList *_list, PsiCon *_psi);
    ~Private();

private:
    PsiActionList *            list;
    PsiCon *                   psi;
    QPointer<ActionList>       statusActionList;
    map<QString, IconAction *> actionmap;

    void createCommon();
    void createMainWin();
    void createMessageChatGroupchat();
    void createMessageChat();
    void createChatGroupchat();
    void createMessage();
    void createChat();
    void createGroupchat();

    void addPluginsActions(ActionsType type);

    struct ActionNames {
        const char *name;
        IconAction *action;
    };

    ActionList *createActionList(QString name, int id, ActionNames *);

private slots:
    void optionsChanged();
};

PsiActionList::Private::Private(PsiActionList *_list, PsiCon *_psi)
{
    list = _list;
    psi  = _psi;

    createCommon();
    createMainWin();
    createMessageChatGroupchat();
    createMessageChat();
    createChatGroupchat();
    createMessage();
    createChat();
    createGroupchat();

    connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString &)), SLOT(optionsChanged()));
    optionsChanged();
}

PsiActionList::Private::~Private() { list->clear(); }

ActionList *PsiActionList::Private::createActionList(QString name, int id, ActionNames *actionlist)
{
    static const QStringList skipList = QStringList() << "separator"
                                                      << "spacer";

    ActionList *actions = new ActionList(name, id, false);

    QString aName;
    for (int i = 0; !(aName = QString(actionlist[i].name)).isEmpty(); i++) {
        IconAction *action = actionlist[i].action;
        if (action) {
            actions->addAction(aName, action);
            if (!skipList.contains(aName)) {
                action->setShortcuts(ShortcutManager::instance()->shortcuts("alist." + aName));
                actionmap[aName] = action;
            }
        }
    }

    list->addList(actions);
    return actions;
}

void PsiActionList::Private::createCommon()
{
    IconAction *separatorAction = new SeparatorAction(this);
    IconAction *spacerAction    = new SpacerAction(this);

    ActionNames actions[] = { { "separator", separatorAction }, { "spacer", spacerAction }, { "", nullptr } };

    createActionList(tr("Common Actions"), Actions_Common, actions);
}

void PsiActionList::Private::createMainWin()
{
    {
        IconActionGroup *viewGroups = new IconActionGroup(this);
        viewGroups->setText(tr("View Groups"));
        viewGroups->setToolTip(tr("Toggle visibility of special roster groups"));
        viewGroups->setUsesDropDown(true);
        viewGroups->setExclusive(false);

        IconAction *actEnableGroups = new IconAction(tr("Show Roster Groups"), "psi/enable-groups",
                                                     tr("Show Roster Groups"), 0, viewGroups, nullptr, true);
        actEnableGroups->setToolTip(tr("Enable/disable groups in roster"));

        IconAction *showOffline = new IconAction(tr("Show Offline Contacts"), "psi/show_offline",
                                                 tr("Show Offline Contacts"), 0, viewGroups, nullptr, true);
        showOffline->setToolTip(tr("Toggles visibility of offline contacts in roster"));

        /*IconAction *showAway = new IconAction(tr("Show Away/XA/DnD Contacts"), "psi/show_away", tr("Show Away/XA/DnD
        Contacts"), 0, PsiOptions::instance()->getOption("options.ui.menu.view.show-away").toBool() ?
        (QObject*)viewGroups : (QObject*)this, 0, true); showAway->setToolTip(tr("Toggles visibility of away/xa/dnd
        contacts in roster"));*/

        IconAction *showHidden = new IconAction(tr("Show Hidden Contacts"), "psi/show_hidden",
                                                tr("Show Hidden Contacts"), 0, viewGroups, nullptr, true);
        showHidden->setToolTip(tr("Toggles visibility of hidden contacts in roster"));

        IconAction *showAgents = new IconAction(tr("Show Agents/Transports"), "psi/disco", tr("Show Agents/Transports"),
                                                0, viewGroups, nullptr, true);
        showAgents->setToolTip(tr("Toggles visibility of agents/transports in roster"));

        IconAction *showSelf = new IconAction(tr("Show Self Contact"), "psi/show_self", tr("Show Self Contact"), 0,
                                              viewGroups, nullptr, true);
        showSelf->setToolTip(tr("Toggles visibility of self contact in roster"));

        IconAction *showStatusMsg = new IconAction(tr("Show Status Messages"), "psi/statusmsg",
                                                   tr("Show Status Messages"), 0, viewGroups, nullptr, true);
        showSelf->setToolTip(tr("Toggles visibility of status messages of contacts"));

        ActionNames actions[] = { { "view_groups", viewGroups },
                                  { "enable_groups", actEnableGroups },
                                  { "show_offline", showOffline },
                                  //{ "show_away",    showAway    },
                                  { "show_hidden", showHidden },
                                  { "show_agents", showAgents },
                                  { "show_self", showSelf },
                                  { "show_statusmsg", showStatusMsg },
                                  { "", nullptr } };

        createActionList(tr("Show Contacts"), Actions_MainWin, actions);
    }

    {
        PopupAction *optionsButton = new PopupAction(tr("&Psi"), nullptr, this, "optionsButton");
        optionsButton->setToolTip(tr("The main Psi button, that provides access to many actions"));
        optionsButton->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred));
        optionsButton->setIcon(IconsetFactory::iconPtr("psi/main"), false);

        PopupAction *statusButton = new PopupAction(tr("&Status"), nullptr, this, "statusButton");
        statusButton->setToolTip(tr("Provides a convenient way to change and to get information about current status"));
        statusButton->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

        IconAction *eventNotifier = new EventNotifierAction(this, "EventNotifierAction");
        eventNotifier->setToolTip(tr("Special item that displays number of pending events"));

        IconAction *actActiveContacts
            = new IconAction(tr("Active contacts"), "psi/jabber", tr("Active contacts"), 0, this);
        actActiveContacts->setToolTip(tr("Simple way to find contacts with opened chats"));

        ActionNames actions[] = { { "button_options", optionsButton },
                                  { "button_status", statusButton },
                                  { "active_contacts", actActiveContacts },
                                  { "event_notifier", eventNotifier },
                                  { "", nullptr } };

        createActionList(tr("Buttons"), Actions_MainWin, actions);
    }

    {
        IconAction *add_act = nullptr;
        if (!PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool())
            add_act = new MAction(IconsetFactory::icon("psi/addContact"), tr("&Add a Contact"), 0, psi, this);

        IconAction *lw_act = new MAction(IconsetFactory::icon("psi/xml"), tr("&XML Console"), 2, psi, this);

        IconAction *actDisco = nullptr;
        if (!PsiOptions::instance()->getOption("options.ui.contactlist.disable-service-discovery").toBool())
            actDisco = new MAction(IconsetFactory::icon("psi/disco"), tr("Service &Discovery"), 3, psi, this);

        //        IconAction *actReadme = new IconAction (tr("ReadMe"), tr("&ReadMe"), 0, this);
        //        actReadme->setToolTip (tr("Show ReadMe file"));
        //
        //        IconAction *actOnlineWiki = new IconAction (tr("Wiki (Online)"), tr("Wiki (Online)"), 0, this);
        //        actOnlineWiki->setToolTip (tr("Wiki (Online)"));
        //
        //        IconAction *actOnlineHome = new IconAction (tr("Home Page (Online)"), tr("Home Page (Online)"), 0,
        //        this); actOnlineHome->setToolTip (tr("Home Page (Online)"));
        //
        //        IconAction *actBugReport = new IconAction (tr("Report a Bug"), tr("Report a &Bug"), 0, this);
        //        actBugReport->setToolTip (tr("Report a Bug"));

        IconAction *actNewMessage
            = new IconAction(tr("New Blank Message"), "psi/sendMessage", tr("New &Blank Message"), 0, this);
        IconAction *actJoinGroupchat
            = new IconAction(tr("Join Groupchat"), "psi/groupChat", tr("Join &Groupchat"), 0, this);

        IconAction *actOptions = new IconAction(tr("Options"), "psi/options", tr("&Options"), 0, this);
        actOptions->setMenuRole(QAction::PreferencesRole);

        IconAction *actToolbars
            = new IconAction(tr("Configure Toolbars"), "psi/toolbars", tr("Configure Tool&bars"), 0, this);
        IconAction *actChangeProfile
            = new IconAction(tr("Change Profile"), "psi/profile", tr("&Change Profile"), 0, this);

        IconAction *actPlaySounds
            = new IconAction(tr("Play Sounds"), "psi/playSounds", tr("Play &Sounds"), 0, this, nullptr, true);
        actPlaySounds->setToolTip(tr("Toggles whether sound should be played or not"));

        IconAction *actQuit = new IconAction(tr("Quit"), "psi/quit", tr("&Quit"), 0, this);
        actQuit->setMenuRole(QAction::QuitRole);
        actQuit->setToolTip(tr("Quits Psi"));

        // TODO: probably we want to lock down filetransfer, right?
        IconAction *actFileTrans
            = new IconAction(tr("Transfer Manager"), "psi/filemanager", tr("Trans&fer Manager"), 0, this);
        actFileTrans->setToolTip(tr("Opens the transfer manager dialog"));

        ActionNames actions[] = { { "menu_disco", actDisco },
                                  { "menu_add_contact", add_act },
                                  { "menu_new_message", actNewMessage },
                                  { "menu_join_groupchat", actJoinGroupchat },
                                  { "menu_options", actOptions },
                                  { "menu_file_transfer", actFileTrans },
                                  { "menu_toolbars", actToolbars },
                                  { "menu_xml_console", lw_act },
                                  { "menu_change_profile", actChangeProfile },
                                  { "menu_play_sounds", actPlaySounds },
                                  { "menu_quit", actQuit },
                                  { "", nullptr } };

        createActionList(tr("Menu Items"), Actions_MainWin, actions);
    }

#ifdef USE_PEP
    {
        IconAction *actPublishTune
            = new IconAction(tr("Publish Tune"), "psi/publishTune", tr("Publish &Tune"), 0, this, nullptr, true);
        actPublishTune->setToolTip(tr("Toggles whether the currently playing tune should be published or not"));

        IconAction *actSetMood = new IconAction(tr("Set Mood"), "pep/mood", tr("Set Mood"), 0, this);
        actSetMood->setToolTip(tr("Set Mood for all active accounts"));

        IconAction *actSetActivity = new IconAction(tr("Set Activity"), "pep/activities", tr("Set Activity"), 0, this);
        actSetActivity->setToolTip(tr("Set Activity for all active accounts"));

        IconAction *actSetGeoloc
            = new IconAction(tr("Set Geolocation"), "pep/geolocation", tr("Set Geolocation"), 0, this);
        actSetGeoloc->setToolTip(tr("Set Geolocation for all active accounts"));

        ActionNames actions[] = { { "publish_tune", actPublishTune },
                                  { "set_mood", actSetMood },
                                  { "set_activity", actSetActivity },
                                  { "set_geoloc", actSetGeoloc },
                                  { "", nullptr } };

        createActionList(tr("Publish"), Actions_MainWin, actions);
    }
#endif

    {
        // status actions
        IconActionGroup *statusGroup = new IconActionGroup(this);
        statusGroup->setVisible(false);

        IconAction *statusSmallerAlt = new IconAction(this);
        statusSmallerAlt->setText(tr("Set Status"));
        statusSmallerAlt->setToolTip(tr("Smaller alternative to the Status button"));

        QString setStatusStr = tr("Changes your global status to '%1'");

        bool        statusExl    = true;
        IconAction *statusOnline = new IconAction(status2txt(STATUS_ONLINE), "status/online", status2txt(STATUS_ONLINE),
                                                  0, statusGroup, QString::number(STATUS_ONLINE), statusExl);
        statusOnline->setToolTip(setStatusStr.arg(tr("Online")));

        IconAction *statusChat = new IconAction(status2txt(STATUS_CHAT), "status/chat", status2txt(STATUS_CHAT), 0,
                                                statusGroup, QString::number(STATUS_CHAT), true);
        statusChat->setToolTip(setStatusStr.arg(tr("Free for Chat")));

        statusGroup->addSeparator();

        IconAction *statusAway = new IconAction(status2txt(STATUS_AWAY), "status/away", status2txt(STATUS_AWAY), 0,
                                                statusGroup, QString::number(STATUS_AWAY), statusExl);
        statusAway->setToolTip(setStatusStr.arg(tr("Away")));

        IconAction *statusXa = new IconAction(status2txt(STATUS_XA), "status/xa", status2txt(STATUS_XA), 0, statusGroup,
                                              QString::number(STATUS_XA), statusExl);
        statusXa->setToolTip(setStatusStr.arg(tr("XA")));

        IconAction *statusDnd = new IconAction(status2txt(STATUS_DND), "status/dnd", status2txt(STATUS_DND), 0,
                                               statusGroup, QString::number(STATUS_DND), statusExl);
        statusDnd->setToolTip(setStatusStr.arg(tr("DND")));

        statusGroup->addSeparator();

        IconAction *chooseStatus
            = new IconAction(tr("Choose status..."), "psi/action_direct_presence", tr("Choose..."), 0, statusGroup);
        chooseStatus->setToolTip(tr("Show dialog to set your status"));

        IconAction *reconnectAll = new IconAction(tr("Reconnect"), "psi/reload", tr("Reconnect"), 0, statusGroup);
        reconnectAll->setToolTip(tr("Reconnect all active accounts"));

        statusGroup->addSeparator();

        IconAction *statusInvisible
            = new IconAction(status2txt(STATUS_INVISIBLE), "status/invisible", status2txt(STATUS_INVISIBLE), 0,
                             statusGroup, QString::number(STATUS_INVISIBLE), statusExl);
        statusInvisible->setToolTip(setStatusStr.arg(tr("Invisible")));

        statusGroup->addSeparator();

        IconAction *statusOffline
            = new IconAction(status2txt(STATUS_OFFLINE), "status/offline", status2txt(STATUS_OFFLINE), 0, statusGroup,
                             QString::number(STATUS_OFFLINE), statusExl);
        statusOffline->setToolTip(setStatusStr.arg(tr("Offline")));

        ActionNames actions[] = { { "status_group", statusGroup },     { "status_all", statusSmallerAlt },
                                  { "status_chat", statusChat },       { "status_online", statusOnline },
                                  { "status_away", statusAway },       { "status_xa", statusXa },
                                  { "status_dnd", statusDnd },         { "status_invisible", statusInvisible },
                                  { "status_offline", statusOffline }, { "choose_status", chooseStatus },
                                  { "reconnect_all", reconnectAll },   { "", nullptr } };

        statusActionList = createActionList(tr("Status"), Actions_MainWin, actions);
    }

    {
        IconAction *actReadme = new IconAction(tr("ReadMe"), tr("&ReadMe"), 0, this);
        actReadme->setToolTip(tr("Show ReadMe file"));

        IconAction *actTip = new IconAction(tr("Tip of the Day"), "psi/tip", tr("&Tip of the Day"), 0, this);
        actTip->setToolTip(tr("See many useful tips"));

        IconAction *actOnlineWiki = new IconAction(tr("Wiki (Online)"), tr("&Wiki (Online)"), 0, this);
        actOnlineWiki->setToolTip(tr("Wiki (Online)"));

        IconAction *actOnlineHome = new IconAction(tr("Home Page (Online)"), tr("&Home Page (Online)"), 0, this);
        actOnlineHome->setToolTip(tr("Home Page (Online)"));

        IconAction *actOnlineForum = new IconAction(
#ifdef PSI_PLUS
            tr("Psi+ Forum (Online)"), tr("Psi+ &Forum (Online)")
#else
            tr("Psi Forum (Online)"), tr("Psi &Forum (Online)")
#endif
                                           ,
            0, this);
        actOnlineForum->setToolTip(
#ifdef PSI_PLUS
            tr("Psi+ Forum (Online)")
#else
            tr("Psi Forum (Online)")
#endif
        );

        IconAction *actPsiMUC = new IconAction(
#ifdef PSI_PLUS
            tr("Join Psi+ Discussion Room (Online)"), tr("&Join Psi+ Discussion Room (Online)")
#else
            tr("Join Psi Discussion Room (Online)"), tr("&Join Psi Discussion Room (Online)")
#endif
                                                          ,
            0, this);
        actOnlineHome->setToolTip(tr("Join Psi Discussion Room (Online)"));

        IconAction *actBugReport = new IconAction(tr("Report a Bug (Online)"), tr("Report a &Bug (Online)"), 0, this);
        actBugReport->setToolTip(tr("Report a Bug (Online)"));

        IconAction *actAbout = new IconAction(tr("About"), "psi/logo_32", tr("&About"), 0, this);
        actAbout->setMenuRole(QAction::AboutRole);

        IconAction *actAboutQt = new IconAction(tr("About Qt"), tr("About &Qt"), 0, this);
        actAboutQt->setMenuRole(QAction::AboutQtRole);

        IconAction *actAboutPsiMedia = new IconAction(tr("About GStreamer"), tr("About &GStreamer"), 0, this);
        // no role otherwise it may conflict with About Psi
        actAboutPsiMedia->setMenuRole(QAction::NoRole);

        IconAction *actDiagQCAPlugin = new IconAction(tr("Security Plugins"), tr("Security &Plugins"), 0, this);

        IconAction *actDiagQCAKeyStore = new IconAction(tr("Key Storage"), tr("&Key Storage"), 0, this);

        ActionNames actions[] = { { "help_readme", actReadme },
                                  { "help_tip", actTip },
                                  { "help_online_wiki", actOnlineWiki },
                                  { "help_online_home", actOnlineHome },
                                  { "help_online_forum", actOnlineForum },
                                  { "help_psi_muc", actPsiMUC },
                                  { "help_report_bug", actBugReport },
                                  { "help_about", actAbout },
                                  { "help_about_qt", actAboutQt },
                                  { "help_about_psimedia", actAboutPsiMedia },
                                  { "help_diag_qcaplugin", actDiagQCAPlugin },
                                  { "help_diag_qcakeystore", actDiagQCAKeyStore },
                                  { "", nullptr } };

        createActionList(tr("Help"), Actions_MainWin, actions);
    }
}

void PsiActionList::Private::createMessageChatGroupchat() {}

void PsiActionList::Private::createMessageChat() {}

void PsiActionList::Private::createChatGroupchat() {}

void PsiActionList::Private::createMessage() {}

void PsiActionList::Private::createChat()
{
    {
        IconAction *actClear
            = new IconAction(tr("Clear Chat Window"), "psi/clearChat", tr("Clear Chat Window"), 0, this);
        IconAction *actFind     = new IconAction(tr("Find"), "psi/search", tr("&Find"), 0, this, "", true);
        IconAction *actHtmlText = new IconAction(tr("Set Text Format"), "psi/text", tr("Set Text Format"), 0, this);
        IconAction *actAddContact
            = new IconAction(tr("Add Contact To Roster"), "psi/addContact", tr("Add Contact"), 0, this);
        IconAction *actIcon  = new IconAction(tr("Select Icon"), "psi/smile", tr("Select Icon"), 0, this);
        IconAction *actVoice = new IconAction(tr("Voice Call"), "psi/avcall", tr("Voice Call"), 0, this);
        IconAction *actFile  = new IconAction(tr("Send File"), "psi/upload", tr("Send File"), 0, this);
        IconAction *actPgp
            = new IconAction(tr("Toggle Encryption"), "psi/cryptoYes", tr("Toggle Encryption"), 0, this, nullptr, true);
        IconAction *actInfo    = new IconAction(tr("User Info"), "psi/vCard", tr("User Info"), 0, this);
        IconAction *actHistory = new IconAction(tr("Message History"), "psi/history", tr("Message History"), 0, this);
        IconAction *actCompact
            = new IconAction(tr("Toggle Compact/Full Size"), "psi/compact", tr("Toggle Compact/Full Size"), 0, this);
        IconAction *actActiveContacts
            = new IconAction(tr("Active contacts"), "psi/jabber", tr("Active contacts"), 0, this);
        IconAction *actShareFiles = new IconAction(tr("Share Files"), "psi/share_file", tr("Share Files"), 0, this);
        IconAction *actPinTab     = new IconAction(tr("Pin/UnPin Tab"), "psi/pin", tr("Pin/UnPin Tab"), 0, this);
        IconAction *actTemplates  = new IconAction(tr("Templates"), "psi/action_templates", tr("Templates"), 0, this);

        ActionNames actions[] = { { "chat_clear", actClear },
                                  { "chat_find", actFind },
                                  { "chat_html_text", actHtmlText },
                                  { "chat_add_contact", actAddContact },
                                  { "chat_icon", actIcon },
                                  { "chat_voice", actVoice },
                                  { "chat_file", actFile },
                                  { "chat_pgp", actPgp },
                                  { "chat_info", actInfo },
                                  { "chat_history", actHistory },
                                  { "chat_compact", actCompact },
                                  { "chat_active_contacts", actActiveContacts },
                                  { "chat_share_files", actShareFiles },
                                  { "chat_pin_tab", actPinTab },
                                  { "chat_templates", actTemplates },
                                  { "", nullptr } };

        createActionList(tr("Chat basic buttons"), Actions_Chat, actions);
    }

    addPluginsActions(Actions_Chat);
}

void PsiActionList::Private::createGroupchat()
{
    {
        IconAction *actClear
            = new IconAction(tr("Clear Chat Window"), "psi/clearChat", tr("Clear Chat Window"), 0, this);
        IconAction *actFind     = new IconAction(tr("Find"), "psi/search", tr("&Find"), 0, this, "", true);
        IconAction *actHtmlText = new IconAction(tr("Set Text Format"), "psi/text", tr("Set Text Format"), 0, this);
        IconAction *actConfigure
            = new IconAction(tr("Configure Room"), "psi/configure-room", tr("Configure Room"), 0, this);
        IconAction *actIcon       = new IconAction(tr("Select Icon"), "psi/smile", tr("Select Icon"), 0, this);
        IconAction *actShareFiles = new IconAction(tr("Share Files"), "psi/share_file", tr("Share Files"), 0, this);
        IconAction *actPinTab     = new IconAction(tr("Pin/UnPin Tab"), "psi/pin", tr("Pin/UnPin Tab"), 0, this);
        IconAction *actTemplates  = new IconAction(tr("Templates"), "psi/action_templates", tr("Templates"), 0, this);

        ActionNames actions[] = { { "gchat_clear", actClear },
                                  { "gchat_find", actFind },
                                  { "gchat_html_text", actHtmlText },
                                  { "gchat_configure", actConfigure },
                                  { "gchat_icon", actIcon },
                                  { "gchat_share_files", actShareFiles },
                                  { "gchat_pin_tab", actPinTab },
                                  { "gchat_templates", actTemplates },
                                  { "", nullptr } };

        createActionList(tr("Groupchat basic buttons"), Actions_Groupchat, actions);
    }

    addPluginsActions(Actions_Groupchat);
}

void PsiActionList::Private::addPluginsActions(ActionsType type)
{
#ifdef PSI_PLUGINS
    PluginManager *pm      = PluginManager::instance();
    QStringList    plugins = pm->availablePlugins();
    ActionList *   actions = new ActionList(tr("Plugins"), type, false);
    foreach (const QString &plugin, plugins) {
        if ((type == Actions_Chat && !pm->hasToolBarButton(plugin))
            || (type == Actions_Groupchat && !pm->hasGCToolBarButton(plugin))) {

            continue;
        }

        IconAction *action = new IconAction(plugin, "", plugin, 0, this);
        action->setIcon(pm->icon(plugin));
        actions->addAction(pm->shortName(plugin) + "-plugin", action);
    }
    list->addList(actions);
#else
    Q_UNUSED(type)
#endif
}

void PsiActionList::Private::optionsChanged()
{
    Q_ASSERT(!statusActionList.isNull());
    if (statusActionList.isNull())
        return;
    statusActionList->action("status_chat")
        ->setVisible(PsiOptions::instance()->getOption("options.ui.menu.status.chat").toBool());
    statusActionList->action("status_xa")
        ->setVisible(PsiOptions::instance()->getOption("options.ui.menu.status.xa").toBool());
    statusActionList->action("status_invisible")
        ->setVisible(PsiOptions::instance()->getOption("options.ui.menu.status.invisible").toBool());

    for (map<QString, IconAction *>::iterator i = actionmap.begin(); i != actionmap.end(); i++) {
        i->second->setShortcuts(ShortcutManager::instance()->shortcuts("alist." + i->first));
    }
}

//----------------------------------------------------------------------------
// PsiActionList
//----------------------------------------------------------------------------

PsiActionList::PsiActionList(PsiCon *psi) { d = new Private(this, psi); }

PsiActionList::~PsiActionList() { delete d; }

#include "psiactionlist.moc"

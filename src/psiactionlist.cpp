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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "psiactionlist.h"
#include "iconset.h"
#include "psioptions.h"

#include <qobject.h>

#include "mainwin_p.h"

//----------------------------------------------------------------------------
// PsiActionList::Private
//----------------------------------------------------------------------------

class PsiActionList::Private : public QObject
{
	Q_OBJECT
public:
	Private(PsiActionList *_list, PsiCon *_psi);
	~Private();

private:
	PsiActionList *list;
	PsiCon *psi;

	void createCommon();
	void createMainWin();
	void createMessageChatGroupchat();
	void createMessageChat();
	void createChatGroupchat();
	void createMessage();
	void createChat();
	void createGroupchat();

	struct ActionNames {
		const char *name;
		IconAction *action;
	};

	void createActionList( QString name, int id, ActionNames * );

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

	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionsChanged()));
	optionsChanged();
}

PsiActionList::Private::~Private()
{
	list->clear();
}

void PsiActionList::Private::createActionList( QString name, int id, ActionNames *actionlist )
{
	ActionList *actions = new ActionList( name, id, false );

	QString aName;
	for ( int i = 0; !(aName = QString(actionlist[i].name)).isEmpty(); i++ ) {
		IconAction *action = actionlist[i].action;
		if (action)
			actions->addAction( aName, action );
	}

	list->addList( actions );
}

void PsiActionList::Private::createCommon()
{
	IconAction *separatorAction = new SeparatorAction(this);
	IconAction *spacerAction    = new SpacerAction(this);

	ActionNames actions[] = {
		{ "separator", separatorAction },
		{ "spacer", spacerAction },
		{ "", 0 }
	};

	createActionList( tr( "Common Actions" ), Actions_Common, actions );
}

void PsiActionList::Private::createMainWin()
{
	{
		IconActionGroup *viewGroups = new IconActionGroup ( this );
		viewGroups->setMenuText (tr("View Groups"));
		viewGroups->setWhatsThis (tr("Toggle visibility of special roster groups"));
		viewGroups->setUsesDropDown (true);
		viewGroups->setExclusive (false);

		//IconAction *showOffline = new IconAction (tr("Show Offline Contacts"), "psi/show_offline", tr("Show Offline Contacts"), 0, viewGroups, 0, true);
		IconAction *showOffline = new IconAction (tr("Show Offline Contacts"), tr("Show Offline Contacts"), 0, viewGroups, 0, true);
		showOffline->setWhatsThis (tr("Toggles visibility of offline contacts in roster"));

		//IconAction *showAway = new IconAction (tr("Show Away/XA/DnD Contacts"), "psi/show_away", tr("Show Away/XA/DnD Contacts"), 0, viewGroups, 0, true);
		IconAction *showAway = new IconAction (tr("Show Away/XA/DnD Contacts"), tr("Show Away/XA/DnD Contacts"), 0, viewGroups, 0, true);
		showAway->setWhatsThis (tr("Toggles visibility of away/xa/dnd contacts in roster"));

		//IconAction *showHidden = new IconAction (tr("Show Hidden Contacts"), "psi/show_hidden", tr("Show Hidden Contacts"), 0, viewGroups, 0, true);
		IconAction *showHidden = new IconAction (tr("Show Hidden Contacts"), tr("Show Hidden Contacts"), 0, viewGroups, 0, true);
		showHidden->setWhatsThis (tr("Toggles visibility of hidden contacts in roster"));

		//IconAction *showAgents = new IconAction (tr("Show Agents/Transports"), "psi/disco", tr("Show Agents/Transports"), 0, viewGroups, 0, true);
		IconAction *showAgents = new IconAction (tr("Show Agents/Transports"), tr("Show Agents/Transports"), 0, viewGroups, 0, true);
		showAgents->setWhatsThis (tr("Toggles visibility of agents/transports in roster"));

		//IconAction *showSelf = new IconAction (tr("Show Self Contact"), "psi/show_self", tr("Show Self Contact"), 0, viewGroups, 0, true);
		IconAction *showSelf = new IconAction (tr("Show Self Contact"), tr("Show Self Contact"), 0, viewGroups, 0, true);
		showSelf->setWhatsThis (tr("Toggles visibility of self contact in roster"));

		//IconAction *showStatusMsg = new IconAction (tr("Show Status Messages"), "psi/statusmsg", tr("Show Status Messages"), 0, viewGroups, 0, true);
		IconAction *showStatusMsg = new IconAction (tr("Show Status Messages"), tr("Show Status Messages"), 0, viewGroups, 0, true);
		showSelf->setWhatsThis (tr("Toggles visibility of status messages of contacts"));

		ActionNames actions[] = {
			{ "view_groups",  viewGroups  },
			{ "show_offline", showOffline },
			{ "show_away",    showAway    },
			{ "show_hidden",  showHidden  },
			{ "show_agents",  showAgents  },
			{ "show_self",    showSelf    },
			{ "show_statusmsg", showStatusMsg },
			{ "", 0 }
		};

		createActionList( tr( "Show Contacts" ), Actions_MainWin, actions );
	}

	{
		PopupAction *optionsButton = new PopupAction (tr("&Barracuda"), 0, this, "optionsButton");
		optionsButton->setWhatsThis (tr("The main Psi button, that provides access to many actions"));
		optionsButton->setSizePolicy ( QSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred ) );
		//optionsButton->setIcon ( IconsetFactory::iconPtr("psi/main"), false );

		PopupAction *statusButton = new PopupAction (tr("&Status"), 0, this, "statusButton");
		statusButton->setWhatsThis (tr("Provides a convenient way to change and to get information about current status"));
		statusButton->setSizePolicy ( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) );

		IconAction *eventNotifier = new EventNotifierAction(this, "EventNotifierAction");
		eventNotifier->setWhatsThis (tr("Special item that displays number of pending events"));

		ActionNames actions[] = {
			{ "button_options", optionsButton },
			{ "button_status",  statusButton  },
			{ "event_notifier", eventNotifier },
			{ "", 0 }
		};

		createActionList( tr( "Buttons" ), Actions_MainWin, actions );
	}


	{
		IconAction *add_act = 0;
		if (!PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool())
			//add_act = new MAction(IconsetFactory::icon("psi/addContact"), tr("&Add a contact"), 0, psi, this);
			add_act = new MAction(PsiIcon(), tr("&Add a contact"), 0, psi, this);

		//IconAction *lw_act = new MAction(IconsetFactory::icon("psi/xml"), tr("&XML Console"), 2, psi, this);
		IconAction *lw_act = new MAction(PsiIcon(), tr("&XML Console"), 2, psi, this);

		IconAction *actDisco = 0;
		if(!PsiOptions::instance()->getOption("options.ui.contactlist.disable-service-discovery").toBool())
			//actDisco = new MAction(IconsetFactory::icon("psi/disco"), tr("Service &Discovery"), 3, psi, this);
			actDisco = new MAction(PsiIcon(), tr("Service &Discovery"), 3, psi, this);

//		IconAction *actReadme = new IconAction (tr("ReadMe"), tr("&ReadMe"), 0, this);
//		actReadme->setWhatsThis (tr("Show Read Me file"));
//
//		IconAction *actOnlineHelp = new IconAction (tr("User Guide (Online)"), tr("User Guide (Online)"), 0, this);
//		actOnlineHelp->setWhatsThis (tr("User Guide (Online)"));
//		
//		IconAction *actOnlineWiki = new IconAction (tr("Wiki (Online)"), tr("Wiki (Online)"), 0, this);
//		actOnlineWiki->setWhatsThis (tr("Wiki (Online)"));
//		
//		IconAction *actOnlineHome = new IconAction (tr("Home Page (Online)"), tr("Home Page (Online)"), 0, this);
//		actOnlineHome->setWhatsThis (tr("Home Page (Online)"));
//
//		IconAction *actBugReport = new IconAction (tr("Report a Bug"), tr("Report a &Bug"), 0, this);
//		actBugReport->setWhatsThis (tr("Report a Bug"));

		//IconAction *actNewMessage = new IconAction (tr("New Multi Message"), "psi/sendMessage", tr("New &Multi Message"), 0, this);
		IconAction *actNewMessage = new IconAction (tr("New Multi Message"), tr("New &Multi Message"), 0, this);
		//IconAction *actJoinGroupchat = new IconAction (tr("Join/Create Group Chat"), "psi/groupChat", tr("Join/Create &Group Chat"), 0, this);
		IconAction *actJoinGroupchat = new IconAction (tr("Join/Create Group Chat"), tr("Join/Create &Group Chat"), 0, this);
		IconAction *actAccountSetup = 0;
		//actAccountSetup = new IconAction (tr("Account Setup"), "psi/account", tr("Acc&ount Setup"), 0, this);
		actAccountSetup = new IconAction (tr("Account Setup"), tr("Acc&ount Setup"), 0, this);
		IconAction *actOptions = 0;
		//actOptions = new IconAction (tr("Preferences"), "psi/options", tr("&Preferences"), 0, this);
		actOptions = new IconAction (tr("Preferences"), tr("&Preferences"), 0, this);
		//IconAction *actToolbars = new IconAction(tr("Configure Toolbars"), "psi/toolbars", tr("Configure Tool&bars"), 0, this);
		IconAction *actToolbars = new IconAction(tr("Configure Toolbars"), tr("Configure Tool&bars"), 0, this);
		IconAction *actChangeProfile = 0;
		//actChangeProfile = new IconAction (tr("Change Profile"), "psi/profile", tr("&Change profile"), 0, this);
		actChangeProfile = new IconAction (tr("Change Profile"), tr("&Change profile"), 0, this);

		//IconAction *actPlaySounds = new IconAction (tr("Play sounds"), "psi/playSounds", tr("Play &sounds"), 0, this, 0, true);
		IconAction *actPlaySounds = new IconAction (tr("Play sounds"), tr("Play &sounds"), 0, this, 0, true);
		actPlaySounds->setWhatsThis (tr("Toggles whether sound should be played or not"));
		
		//IconAction *actQuit = new IconAction (tr("Quit"), "psi/quit", tr("&Quit"), 0, this);
		IconAction *actQuit = new IconAction (tr("Quit"), tr("&Quit"), 0, this);
		actQuit->setWhatsThis (tr("Quits Psi"));

		//IconAction *actTip = new IconAction (tr("Tip of the Day"), "psi/tip", tr("&Tip of the Day"), 0, this);
		IconAction *actTip = new IconAction (tr("Tip of the Day"), tr("&Tip of the Day"), 0, this);
		actTip->setWhatsThis (tr("See many useful tips"));

		// TODO: probably we want to lock down filetransfer, right?
		//IconAction *actFileTrans = new IconAction (tr("Transfer Manager"), "psi/filemanager", tr("Trans&fer Manager"), 0, this);
		IconAction *actFileTrans = new IconAction (tr("Transfer Manager"), tr("Trans&fer Manager"), 0, this);
		actFileTrans->setWhatsThis (tr("Opens the Transfer Manager dialog"));

		//IconAction *actLogOn = new IconAction (tr("Log On"), "psi/xxxx", tr("&Log On"), 0, this);
		IconAction *actLogOn = new IconAction (tr("Log On"), tr("&Log On"), 0, this);
		//IconAction *actLogOff = new IconAction (tr("Log Off"), "psi/xxxx", tr("Log &Off"), 0, this);
		IconAction *actLogOff = new IconAction (tr("Log Off"), tr("Log &Off"), 0, this);
		//IconAction *actAccountModify = new IconAction (tr("Account Settings"), "psi/account", tr("&Account Settings"), 0, this);
		IconAction *actAccountModify = new IconAction (tr("Account Settings"), tr("&Account Settings"), 0, this);
		//IconAction *actEditVCard = new IconAction (tr("Edit Personal Details"), "psi/xxxx", tr("Edit Personal &Details"), 0, this);
		IconAction *actEditVCard = new IconAction (tr("Edit Personal Details"), tr("Edit Personal &Details"), 0, this);
		//IconAction *actLinkHistory = new IconAction (tr("Message History"), "psi/xxxx", tr("Message &History"), 0, this);
		IconAction *actLinkHistory = new IconAction (tr("Message History"), tr("Message &History"), 0, this);
		//IconAction *actLinkPassword = new IconAction (tr("Change Password"), "psi/xxxx", tr("Change &Password"), 0, this);
		IconAction *actLinkPassword = new IconAction (tr("Change Password"), tr("Change &Password"), 0, this);
		//IconAction *actRegAim = new IconAction (tr("AIM"), "psi/aim", tr("&AIM"), 0, this);
		IconAction *actRegAim = new IconAction (tr("AIM"), tr("&AIM"), 0, this);
		//IconAction *actRegMsn = new IconAction (tr("MSN"), "psi/msn", tr("&MSN"), 0, this);
		IconAction *actRegMsn = new IconAction (tr("MSN"), tr("&MSN"), 0, this);
		//IconAction *actRegYahoo = new IconAction (tr("Yahoo"), "psi/yahoo", tr("&Yahoo"), 0, this);
		IconAction *actRegYahoo = new IconAction (tr("Yahoo"), tr("&Yahoo"), 0, this);
		//IconAction *actRegIcq = new IconAction (tr("ICQ"), "psi/icq", tr("&ICQ"), 0, this);
		IconAction *actRegIcq = new IconAction (tr("ICQ"), tr("&ICQ"), 0, this);
		IconAction *actTransports = new IconAction (tr("Public IM Accounts"), tr("&Public IM Accounts"), 0, this);

		ActionNames actions[] = {
			{ "menu_disco",           actDisco         },
			{ "menu_add_contact",     add_act          },
			{ "menu_new_message",     actNewMessage    },
			{ "menu_join_groupchat",  actJoinGroupchat },
			{ "menu_account_setup",   actAccountSetup  },
			{ "menu_options",         actOptions       },
			{ "menu_file_transfer",   actFileTrans     },
			{ "menu_toolbars",        actToolbars      },
			{ "menu_xml_console",     lw_act           },
			{ "menu_change_profile",  actChangeProfile },
			{ "menu_play_sounds",     actPlaySounds    },
			{ "menu_quit",            actQuit          },
			{ "log_on",               actLogOn         },
			{ "log_off",              actLogOff        },
			{ "menu_account_modify",  actAccountModify },
			{ "menu_edit_vcard",      actEditVCard     },
			{ "link_history",         actLinkHistory   },
			{ "link_password",        actLinkPassword  },
			{ "reg_aim",              actRegAim        },
			{ "reg_msn",              actRegMsn        },
			{ "reg_yahoo",            actRegYahoo      },
			{ "reg_icq",              actRegIcq        },
			{ "reg_trans",            actTransports    },
			{ "", 0 }
		};

		createActionList( tr( "Menu Items" ), Actions_MainWin, actions );
	}

#ifdef USE_PEP
	{
		//IconAction *actPublishTune = new IconAction (tr("Publish tune"), "psi/publishTune", tr("Publish &tune"), 0, this, 0, true);
		IconAction *actPublishTune = new IconAction (tr("Publish tune"), tr("Publish &tune"), 0, this, 0, true);
		actPublishTune->setWhatsThis (tr("Toggles whether the currently playing tune should be published or not."));

		ActionNames actions[] = {
			{ "publish_tune", actPublishTune },
			{ "", 0 }
		};

		createActionList( tr( "Publish" ), Actions_MainWin, actions );
	}
#endif

	{
		// status actions
		IconActionGroup *statusGroup = new IconActionGroup ( this );
		statusGroup->setMenuText (tr("Set status"));
		statusGroup->setWhatsThis (tr("Smaller alternative to the Status Button"));
		statusGroup->setExclusive(false);
		statusGroup->setUsesDropDown (true);

		QString setStatusStr = tr("Changes your global status to '%1'");

		bool statusExl = true;
		IconAction *statusOnline = new IconAction (status2txt(STATUS_ONLINE), "status/online", status2txt(STATUS_ONLINE), 0, statusGroup, QString::number(STATUS_ONLINE), statusExl);
		statusOnline->setWhatsThis (setStatusStr.arg(tr("Online")));

		IconAction *statusChat = new IconAction (status2txt(STATUS_CHAT), "status/chat", status2txt(STATUS_CHAT), 0, statusGroup, QString::number(STATUS_CHAT), true);
		statusChat->setWhatsThis (setStatusStr.arg(tr("Free for Chat")));

		statusGroup->addSeparator();

		IconAction *statusAway = new IconAction (status2txt(STATUS_AWAY), "status/away", status2txt(STATUS_AWAY), 0, statusGroup, QString::number(STATUS_AWAY), statusExl);
		statusAway->setWhatsThis (setStatusStr.arg(tr("Away")));

		IconAction *statusXa = new IconAction (status2txt(STATUS_XA), "status/xa", status2txt(STATUS_XA), 0, statusGroup, QString::number(STATUS_XA), statusExl);
		statusXa->setWhatsThis (setStatusStr.arg(tr("XA")));

		IconAction *statusDnd = new IconAction (status2txt(STATUS_DND), "status/dnd", status2txt(STATUS_DND), 0, statusGroup, QString::number(STATUS_DND), statusExl);
		statusDnd->setWhatsThis (setStatusStr.arg(tr("DND")));

		statusGroup->addSeparator();

		IconAction *statusInvisible = new IconAction (status2txt(STATUS_INVISIBLE), "status/invisible", status2txt(STATUS_INVISIBLE), 0, statusGroup, QString::number(STATUS_INVISIBLE), statusExl);
		statusInvisible->setWhatsThis (setStatusStr.arg(tr("Invisible")));

		statusGroup->addSeparator();

		IconAction *statusOffline = new IconAction (status2txt(STATUS_OFFLINE), "status/offline", status2txt(STATUS_OFFLINE), 0, statusGroup, QString::number(STATUS_OFFLINE), statusExl);
		statusOffline->setWhatsThis (setStatusStr.arg(tr("Offline")));

		ActionNames actions[] = {
			{ "status_all",       statusGroup     },
			{ "status_chat",      statusChat      },
			{ "status_online",    statusOnline    },
			{ "status_away",      statusAway      },
			{ "status_xa",        statusXa        },
			{ "status_dnd",       statusDnd       },
			{ "status_invisible", statusInvisible },
			{ "status_offline",   statusOffline   },
			{ "", 0 }
		};

		createActionList( tr( "Status" ), Actions_MainWin, actions );
	}

	{
		IconAction *actReadme = new IconAction (tr("ReadMe"), tr("&ReadMe"), 0, this);
		actReadme->setWhatsThis (tr("Show Read Me file"));

		//IconAction *actTip = new IconAction (tr("Tip of the Day"), "psi/tip", tr("&Tip of the Day"), 0, this);
		IconAction *actTip = new IconAction (tr("Tip of the Day"), tr("&Tip of the Day"), 0, this);
		actTip->setWhatsThis (tr("See many useful tips"));

		IconAction *actOnlineHelp = new IconAction (tr("User Guide (Online)"), tr("&User Guide (Online)"), 0, this);
		actOnlineHelp->setWhatsThis (tr("User Guide (Online)"));
		
		IconAction *actOnlineWiki = new IconAction (tr("Wiki (Online)"), tr("&Wiki (Online)"), 0, this);
		actOnlineWiki->setWhatsThis (tr("Wiki (Online)"));
		
		IconAction *actOnlineHome = new IconAction (tr("Home Page (Online)"), tr("&Home Page (Online)"), 0, this);
		actOnlineHome->setWhatsThis (tr("Home Page (Online)"));

		IconAction *actPsiMUC = new IconAction (tr("Join Psi Discussion Room (Online)"), tr("&Join Psi Discussion Room (Online)"), 0, this);
		actOnlineHome->setWhatsThis (tr("Join Psi Discussion Room (Online)"));

		IconAction *actBugReport = new IconAction (tr("Report a Bug (Online)"), tr("Report a &Bug (Online)"), 0, this);
		actBugReport->setWhatsThis (tr("Report a Bug (Online)"));

		//IconAction *actAbout = new IconAction (tr("About"), "psi/logo_16", tr("&About"), 0, this);
		IconAction *actAbout = new IconAction (tr("About"), tr("&About"), 0, this);

		IconAction *actAboutQt = new IconAction (tr("About Qt"), tr("About &Qt"), 0, this);

		IconAction *actDiagQCAPlugin = new IconAction (tr("Security Plugins"), tr("Security &Plugins"), 0, this);

		IconAction *actDiagQCAKeyStore = new IconAction (tr("Key Storage"), tr("&Key Storage"), 0, this);

		ActionNames actions[] = {
			{ "help_readme",           actReadme          },
			{ "help_tip",              actTip             },
			{ "help_online_help",      actOnlineHelp      },
			{ "help_online_wiki",      actOnlineWiki      },
			{ "help_online_home",      actOnlineHome      },
			{ "help_psi_muc",          actPsiMUC          },
			{ "help_report_bug",       actBugReport       },
			{ "help_about",            actAbout           },
			{ "help_about_qt",         actAboutQt         },
			{ "help_diag_qcaplugin",   actDiagQCAPlugin  },
			{ "help_diag_qcakeystore", actDiagQCAKeyStore },
			{ "", 0 }
		};

		createActionList( tr( "Help" ), Actions_MainWin, actions );
	}
}

void PsiActionList::Private::createMessageChatGroupchat()
{
}

void PsiActionList::Private::createMessageChat()
{
}

void PsiActionList::Private::createChatGroupchat()
{
}

void PsiActionList::Private::createMessage()
{
}

void PsiActionList::Private::createChat()
{
}

void PsiActionList::Private::createGroupchat()
{
}

void PsiActionList::Private::optionsChanged()
{
	ActionList *statusList = list->actionList(tr("Status"));
	statusList->action("status_chat")->setVisible(PsiOptions::instance()->getOption("options.ui.menu.status.chat").toBool());
	statusList->action("status_xa")->setVisible(PsiOptions::instance()->getOption("options.ui.menu.status.xa").toBool());
	statusList->action("status_invisible")->setVisible(PsiOptions::instance()->getOption("options.ui.menu.status.invisible").toBool());
}

//----------------------------------------------------------------------------
// PsiActionList
//----------------------------------------------------------------------------

PsiActionList::PsiActionList( PsiCon *psi )
{
	d = new Private( this, psi );
}

PsiActionList::~PsiActionList()
{
	delete d;
}

#include "psiactionlist.moc"

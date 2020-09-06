/*
 * mainwin.cpp - the main window.  holds contactlist and buttons.
 * Copyright (C) 2001-2003  Justin Karneges, Michail Pishchagin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "mainwin.h"

#include <QMessageBox>
#include <QIcon>
#include <QApplication>
#include <QTimer>
#include <QObject>
#include <QPainter>
#include <QSignalMapper>
#include <QMenuBar>
#include <QPixmap>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QEvent>
#include <QVBoxLayout>
#include <QSplitter>
#include <QMenu>
#include <QtAlgorithms>
#include <QShortcut>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "tabmanager.h"
#include "tabdlg.h"
#include "common.h"
#include "showtextdlg.h"
#include "psicon.h"
#include "textutil.h"
#include "psiiconset.h"
#include "serverinfomanager.h"
#include "applicationinfo.h"
#include "psiaccount.h"
#include "psievent.h"
#include "psitooltip.h"
#include "psitrayicon.h"
#include "psitoolbar.h"
#include "aboutdlg.h"
#include "psitoolbar.h"
#include "psioptions.h"
#include "tipdlg.h"
#include "mucjoindlg.h"
#include "psicontactlist.h"
#include "desktoputil.h"
#include "statusdlg.h"
#include "globalstatusmenu.h"
#include "psirosterwidget.h"
#include "mooddlg.h"
#include "activitydlg.h"
#include "geolocationdlg.h"
#include "activecontactsmenu.h"

#include "mainwin_p.h"

#include "psimedia/psimedia.h"
#include "avcall/avcall.h"

#include "rosteravatarframe.h"
#include "avatars.h"

using namespace XMPP;

static const QString showStatusMessagesOptionPath = "options.ui.contactlist.status-messages.show";

// FIXME: this is a really corny way of getting the GStreamer version...
QString extract_gst_version(const QString &in)
{
	int start = in.indexOf("GStreamer ");
	if(start == -1)
		return QString();
	start += 10;
	int end = in.indexOf(",", start);
	if(end == -1)
		return QString();
	return in.mid(start, end - start);
}

//----------------------------------------------------------------------------
// MainWin::Private
//----------------------------------------------------------------------------

class MainWin::Private
{
public:
	Private(PsiCon *, MainWin *);
	~Private();

	bool onTop, asTool;
	QMenu* mainMenu, *optionsMenu, *toolsMenu;
	GlobalStatusMenu *statusMenu;
#ifdef Q_OS_LINUX
	// Status menu for MenuBar.
	// Workaround a Unity bug.
	GlobalStatusMenu *statusMenuMB;
#endif
	int sbState;
	QString nickname;
	PsiTrayIcon* tray;
	QMenu* trayMenu;
	QVBoxLayout *vb_roster;
	QSplitter *splitter;
	TabDlg *mainTabs;
	QString statusTip;
	PsiToolBar *viewToolBar;
	int tabsSize;
	int rosterSize;
	bool isLeftRoster;

	PopupAction* optionsButton, *statusButton;
	IconActionGroup* statusGroup;
	IconAction* statusSmallerAlt;
	EventNotifierAction* eventNotifier;
	PsiCon* psi;
	MainWin* mainWin;
	RosterAvatarFrame* rosterAvatar;
	QPointer<PsiAccount> defaultAccount;

	QLineEdit* searchText;
	QToolButton* searchPb;
	QWidget* searchWidget;

	QTimer *hideTimer;
	QSignalMapper* statusMapper;

	PsiIcon* nextAnim;
	int nextAmount;

	QMap<QAction *, int> statusActions;

	int lastStatus;
	bool filterActive, prefilterShowOffline, prefilterShowAway;
	bool squishEnabled;

	PsiRosterWidget* rosterWidget_;

	void registerActions();
	IconAction* getAction( QString name );
	void updateMenu(QStringList actions, QMenu* menu);

	QString ToolTipText;

	QPointer<MoodDlg> moodDlg;
	QPointer<ActivityDlg> activityDlg;
	QPointer<GeoLocationDlg> geolocationDlg;
};

MainWin::Private::Private(PsiCon* _psi, MainWin* _mainWin) : splitter(0), mainTabs(0), viewToolBar(0),
	isLeftRoster(false), psi(_psi), mainWin(_mainWin), hideTimer(0)
{

	statusGroup   = (IconActionGroup *)getAction("status_group");
	eventNotifier = (EventNotifierAction *)getAction("event_notifier");

	optionsButton = (PopupAction *)getAction("button_options");
	statusButton  = (PopupAction *)getAction("button_status");
	statusSmallerAlt = getAction("status_all");

	statusMapper = new QSignalMapper(mainWin);
	mainWin->connect(statusMapper, SIGNAL(mapped(int)), mainWin, SLOT(activatedStatusAction(int)));

	filterActive = false;
	prefilterShowOffline = false;
	prefilterShowAway = false;

	char* squishStr = getenv("SQUISH_ENABLED");
	squishEnabled = squishStr != 0;
}

MainWin::Private::~Private()
{
}

void MainWin::Private::registerActions()
{
	struct {
		const char* name;
		int id;
	} statuslist[] = {
		{ "status_chat",      STATUS_CHAT      },
		{ "status_online",    STATUS_ONLINE    },
		{ "status_away",      STATUS_AWAY      },
		{ "status_xa",        STATUS_XA        },
		{ "status_dnd",       STATUS_DND       },
		{ "status_invisible", STATUS_INVISIBLE },
		{ "status_offline",   STATUS_OFFLINE   },
		{ "", 0 }
	};

	int i;
	QString aName;
	for ( i = 0; !(aName = QString(statuslist[i].name)).isEmpty(); i++ ) {
		IconAction* action = getAction( aName );
		connect (action, SIGNAL(triggered()), statusMapper, SLOT(map()));

		statusMapper->setMapping(action, statuslist[i].id);
		statusActions[action] = statuslist[i].id;
	}

	// register all actions
	PsiActionList::ActionsType type = PsiActionList::ActionsType( PsiActionList::Actions_MainWin | PsiActionList::Actions_Common );
	ActionList actions = psi->actionList()->suitableActions( type );
	QStringList names = actions.actions();
	QStringList::Iterator it = names.begin();
	for ( ; it != names.end(); ++it ) {
		IconAction* action = actions.action( *it );
		if ( action ) {
			mainWin->registerAction( action );
		}
	}
}

IconAction* MainWin::Private::getAction( QString name )
{
	PsiActionList::ActionsType type = PsiActionList::ActionsType( PsiActionList::Actions_MainWin | PsiActionList::Actions_Common );
	ActionList actions = psi->actionList()->suitableActions( type );
	IconAction* action = actions.action( name );

	if ( !action ) {
		qWarning("MainWin::Private::getAction(): action %s not found!", qPrintable(name));
	}
	//else
	//	mainWin->registerAction( action );

	return action;
}

void MainWin::Private::updateMenu(QStringList actions, QMenu* menu)
{
	clearMenu(menu);

	IconAction* action;
	foreach (QString name, actions) {
		// workind around Qt/X11 bug, which displays
		// actions's text and the separator bar in Qt 4.1.1
		if ( name == "separator" ) {
			menu->addSeparator();
			continue;
		}

		if ( name == "diagnostics" ) {
			QMenu* diagMenu = new QMenu(tr("Diagnostics"), menu);
			getAction("help_diag_qcaplugin")->addTo(diagMenu);
			getAction("help_diag_qcakeystore")->addTo(diagMenu);
			menu->addMenu(diagMenu);
			continue;
		}

		if ( (action = getAction(name)) ) {
			action->addTo(menu);
		}
	}
}

//----------------------------------------------------------------------------
// MainWin
//----------------------------------------------------------------------------

const QString toolbarsStateOptionPath = "options.ui.save.toolbars-state";
const QString rosterGeometryPath      = "options.ui.save.roster-width";
const QString tabsGeometryPath        = "options.ui.save.log-width";

MainWin::MainWin(bool _onTop, bool _asTool, PsiCon* psi)
:AdvancedWidget<QMainWindow>(0, (_onTop ? Qt::WindowStaysOnTopHint : Qt::Widget) | (_asTool ? Qt::Tool : Qt::Widget))
{
	setObjectName("MainWin");
	setAttribute(Qt::WA_AlwaysShowToolTips);
	d = new Private(psi, this);

	setWindowIcon(PsiIconset::instance()->status(STATUS_OFFLINE).impix());

	d->onTop = _onTop;
	d->asTool = _asTool;

	// sbState:
	//   -1 : connect
	// >= 0 : STATUS_*
	d->sbState = STATUS_OFFLINE;
	d->lastStatus = -2;

	d->nextAmount = 0;
	d->nextAnim = 0;
	d->tray = 0;
	d->trayMenu = 0;
	d->statusTip = "";
	d->nickname = "";
	d->defaultAccount = 0;

	QWidget *rosterBar = new QWidget(this);
	bool allInOne = false;

	if ( PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool() && PsiOptions::instance()->getOption("options.ui.tabs.grouping").toString().contains('A')) {
		d->splitter = new QSplitter(this);
		d->splitter->setObjectName("onewindowsplitter");
		connect(d->splitter, SIGNAL(splitterMoved(int,int)), this, SLOT(splitterMoved()));
		setCentralWidget(d->splitter);
		allInOne = true;

		d->mainTabs = d->psi->tabManager()->newTabs(0);
		d->psi->tabManager()->setPreferredTabsForKind('C', d->mainTabs);
		d->psi->tabManager()->setPreferredTabsForKind('M', d->mainTabs);

		QList<int> sizes;
		d->rosterSize = PsiOptions::instance()->getOption(rosterGeometryPath).toInt();
		d->tabsSize = PsiOptions::instance()->getOption(tabsGeometryPath).toInt();
		d->isLeftRoster = PsiOptions::instance()->getOption("options.ui.contactlist.aio-left-roster").toBool();
		if (d->isLeftRoster) {
			d->splitter->addWidget(rosterBar);
			d->splitter->addWidget(d->mainTabs);
			sizes << d->rosterSize << d->tabsSize;
		} else {
			d->splitter->addWidget(d->mainTabs);
			d->splitter->addWidget(rosterBar);
			sizes << d->tabsSize << d->rosterSize;
		}

		d->splitter->setSizes(sizes);
	} else
		setCentralWidget(rosterBar);

	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));

	d->vb_roster = new QVBoxLayout(rosterBar);
	d->rosterWidget_ = new PsiRosterWidget(rosterBar);
	d->rosterWidget_->setContactList(psi->contactList());

	int layoutMargin = 2;
#ifdef Q_OS_MAC
	layoutMargin = 0;
	// FIXME
	// d->contactListView_->setFrameShape(QFrame::NoFrame);
#endif
	QMenu* viewMenu = new QMenu(tr("View"), this);


	d->vb_roster->setMargin(layoutMargin);
	d->vb_roster->setSpacing(layoutMargin);

	if (allInOne) {
		QString toolOpt = "options.ui.contactlist.toolbars";
		foreach(QString base, PsiOptions::instance()->getChildOptionNames(toolOpt, true, true)) {
			// toolbar "Show contacts" is fourth, so check m3
			if (base == toolOpt + ".m3") {
				d->viewToolBar = new PsiToolBar(base, rosterBar, d->psi->actionList());
				d->viewToolBar->initialize();
				connect(d->viewToolBar, SIGNAL(customize()), d->psi, SLOT(doToolbars()));
				d->vb_roster->addWidget(d->viewToolBar);
				break;
			}
		}
	}


	// create rosteravatarframe
	d->rosterAvatar = new RosterAvatarFrame(this);
	d->vb_roster->addWidget(d->rosterAvatar);
	d->rosterAvatar->setVisible(PsiOptions::instance()->getOption("options.ui.contactlist.show-avatar-frame").toBool());
	connect(d->rosterAvatar, SIGNAL(statusMessageChanged(QString)), this, SIGNAL(statusMessageChanged(QString)));
	connect(psiCon(), SIGNAL(statusMessageChanged(QString)), d->rosterAvatar, SLOT(setStatusMessage(QString)));
	connect(d->rosterAvatar, SIGNAL(setMood()), this, SLOT(actSetMoodActivated()));
	connect(d->rosterAvatar, SIGNAL(setActivity()), this, SLOT(actSetActivityActivated()));

	//add contact view
	d->vb_roster->addWidget(d->rosterWidget_);

	d->statusMenu = new GlobalStatusMenu((QWidget*)this, d->psi);
	d->statusMenu->setTitle(tr("Status"));
	d->statusMenu->setObjectName("statusMenu");
	connect(d->statusMenu, SIGNAL(statusSelected(XMPP::Status::Type, bool)), d->psi, SLOT(statusMenuChanged(XMPP::Status::Type, bool)));
	connect(d->statusMenu, SIGNAL(statusPresetSelected(XMPP::Status,bool,bool)), d->psi, SLOT(setGlobalStatus(XMPP::Status,bool,bool)));
	connect(d->statusMenu, SIGNAL(statusPresetDialogForced(const QString &)), d->psi, SLOT(showStatusDialog(const QString &)));

#ifdef Q_OS_LINUX
	d->statusMenuMB = new GlobalStatusMenu((QWidget*)this, d->psi);
	d->statusMenuMB->setTitle(tr("Status"));
	d->statusMenuMB->setObjectName("statusMenu");
	connect(d->statusMenuMB, SIGNAL(statusSelected(XMPP::Status::Type, bool)), d->psi, SLOT(statusMenuChanged(XMPP::Status::Type, bool)));
	connect(d->statusMenuMB, SIGNAL(statusPresetSelected(XMPP::Status,bool,bool)), d->psi, SLOT(setGlobalStatus(XMPP::Status,bool,bool)));
	connect(d->statusMenuMB, SIGNAL(statusPresetDialogForced(const QString &)), d->psi, SLOT(showStatusDialog(const QString &)));
#endif

	d->optionsMenu = new QMenu(tr("General"), this);
	d->optionsMenu->setObjectName("optionsMenu");
#ifdef Q_OS_MAC
	d->trayMenu = d->statusMenu;
	extern void qt_mac_set_dock_menu(QMenu *);
	qt_mac_set_dock_menu(d->statusMenu);
#else
	d->trayMenu = new QMenu(this);
	buildTrayMenu();
	connect(d->trayMenu, SIGNAL(aboutToShow()), SLOT(buildTrayMenu()));
#endif


	buildStatusMenu(d->statusMenu);
#ifdef Q_OS_LINUX
	buildStatusMenu(d->statusMenuMB);
#endif
	buildOptionsMenu();
	connect(d->optionsMenu, SIGNAL(aboutToShow()), SLOT(buildOptionsMenu()));


	X11WM_CLASS("main");

	connect(d->psi, SIGNAL(accountCountChanged()), SLOT(numAccountsChanged()));
	numAccountsChanged();

	updateCaption();

	d->registerActions();

	connect(d->psi->contactList(), SIGNAL(accountFeaturesChanged()), SLOT(accountFeaturesChanged()));
	accountFeaturesChanged();

	decorateButton(STATUS_OFFLINE);

	// Mac-only menus
#ifdef Q_OS_MAC
	QMenu* mainMenu = new QMenu(tr("Menu"), this);
	mainMenu->setObjectName("macMainMenu");
	mainMenuBar()->addMenu(mainMenu);
	d->getAction("menu_options")->addTo(mainMenu);
	d->getAction("menu_quit")->addTo(mainMenu);
	d->getAction("help_about")->addTo(mainMenu);
	d->getAction("help_about_qt")->addTo(mainMenu);

	d->mainMenu = new QMenu(tr("General"), this);
	d->mainMenu->setObjectName("mainMenu");
	mainMenuBar()->addMenu(d->mainMenu);
	connect(d->mainMenu, SIGNAL(aboutToShow()), SLOT(buildMainMenu()));
#else
	mainMenuBar()->addMenu(d->optionsMenu);
#endif

#ifdef Q_OS_LINUX
	mainMenuBar()->addMenu(d->statusMenuMB);
#else
	mainMenuBar()->addMenu(d->statusMenu);
#endif

	mainMenuBar()->addMenu(viewMenu);
	d->getAction("show_offline")->addTo(viewMenu);
	// if (PsiOptions::instance()->getOption("options.ui.menu.view.show-away").toBool()) {
	// 	d->getAction("show_away")->addTo(viewMenu);
	// }
	d->getAction("show_hidden")->addTo(viewMenu);
	d->getAction("show_agents")->addTo(viewMenu);
	d->getAction("show_self")->addTo(viewMenu);
	viewMenu->addSeparator();
	d->getAction("show_statusmsg")->addTo(viewMenu);

	// Mac-only menus
#ifdef Q_OS_MAC
	d->toolsMenu = new QMenu(tr("Tools"), this);
	mainMenuBar()->addMenu(d->toolsMenu);
	connect(d->toolsMenu, SIGNAL(aboutToShow()), SLOT(buildToolsMenu()));

	QMenu* helpMenu = new QMenu(tr("Help"), this);
	mainMenuBar()->addMenu(helpMenu);
	d->getAction("help_readme")->addTo (helpMenu);
	d->getAction("help_tip")->addTo (helpMenu);
	helpMenu->addSeparator();
	//d->getAction("help_online_help")->addTo (helpMenu);
	d->getAction("help_online_wiki")->addTo (helpMenu);
	d->getAction("help_online_home")->addTo (helpMenu);
	d->getAction("help_online_forum")->addTo (helpMenu);
	d->getAction("help_psi_muc")->addTo (helpMenu);
	d->getAction("help_report_bug")->addTo (helpMenu);
	QMenu* diagMenu = new QMenu(tr("Diagnostics"), this);
	helpMenu->addMenu(diagMenu);
	d->getAction("help_diag_qcaplugin")->addTo (diagMenu);
	d->getAction("help_diag_qcakeystore")->addTo (diagMenu);
	if(AvCallManager::isSupported()) {
		helpMenu->addSeparator();
		d->getAction("help_about_psimedia")->addTo (helpMenu);
	}
#else
	if (!PsiOptions::instance()->getOption("options.ui.contactlist.show-menubar").toBool())  {
		mainMenuBar()->hide();
	}
	//else
	//	mainMenuBar()->show();
#endif
	d->optionsButton->setMenu( d->optionsMenu );
	d->statusButton->setMenu( d->statusMenu );
	d->rosterAvatar->setStatusMenu( d->statusMenu );
	d->getAction("status_all")->setMenu(d->statusMenu);

	buildToolbars();
	// setUnifiedTitleAndToolBarOnMac(true);

	connect(qApp, SIGNAL(dockActivated()), SLOT(dockActivated()));
	qApp->installEventFilter(this);

	connect(psi, SIGNAL(emitOptionsUpdate()), SLOT(optionsUpdate()));
	optionsUpdate();

		/*QShortcut *sp_ss = new QShortcut(QKeySequence(tr("Ctrl+Shift+N")), this);
		connect(sp_ss, SIGNAL(triggered()), SLOT(avcallConfig()));*/
	optionChanged("options.ui.contactlist.css");

	reinitAutoHide();
}

MainWin::~MainWin()
{
	if(d->tray) {
		delete d->tray;
		d->tray = 0;
	}

	saveToolbarsState();

	if(d->splitter) {
		PsiOptions::instance()->setOption(rosterGeometryPath, d->rosterSize);
		PsiOptions::instance()->setOption(tabsGeometryPath, d->tabsSize);
	}

	delete d;
}

void MainWin::splitterMoved()
{
	QList<int> list = d->splitter->sizes();
	d->rosterSize = !d->isLeftRoster ? list.last() : list.first();
	d->tabsSize = d->isLeftRoster ? list.last() : list.first();
}

void MainWin::optionChanged(const QString& option)
{
	if (option == toolbarsStateOptionPath) {
		loadToolbarsState();
	}
	else if (option == "options.ui.contactlist.css") {
		const QString css = PsiOptions::instance()->getOption("options.ui.contactlist.css").toString();
		if (!css.isEmpty()) {
			setStyleSheet(css);
		}
	}
}

void MainWin::registerAction( IconAction* action )
{
	const char *activated  = SIGNAL( triggered() );
	const char *toggled    = SIGNAL( toggled(bool) );
	const char *setChecked = SLOT( setChecked(bool) );

	PsiContactList* contactList = psiCon()->contactList();

	struct {
		const char* name;
		const char* signal;
		QObject* receiver;
		const char* slot;
	} actionlist[] = {
		{ "choose_status", activated, this, SLOT( actChooseStatusActivated() ) },
		{ "reconnect_all", activated, this, SLOT( actReconnectActivated() ) },

		{ "active_contacts",activated, this, SLOT( actActiveContacts() ) },
		{ "show_offline",   toggled, contactList, SLOT( setShowOffline(bool) ) },
		// { "show_away",      toggled, contactList, SLOT( setShowAway(bool) ) },
		{ "show_hidden",    toggled, contactList, SLOT( setShowHidden(bool) ) },
		{ "show_agents",    toggled, contactList, SLOT( setShowAgents(bool) ) },
		{ "show_self",      toggled, contactList, SLOT( setShowSelf(bool) ) },
		{ "show_statusmsg", toggled, d->rosterWidget_, SLOT( setShowStatusMsg(bool) ) },
		{ "enable_groups",  toggled,   this, SLOT( actEnableGroupsActivated(bool) ) },

		{ "button_options", activated, this, SIGNAL( doOptions() ) },

		{ "menu_disco",       SIGNAL( activated(PsiAccount *, int) ), this, SLOT( activatedAccOption(PsiAccount*, int) ) },
		{ "menu_add_contact", SIGNAL( activated(PsiAccount *, int) ), this, SLOT( activatedAccOption(PsiAccount*, int) ) },
		{ "menu_xml_console", SIGNAL( activated(PsiAccount *, int) ), this, SLOT( activatedAccOption(PsiAccount*, int) ) },

		{ "menu_new_message",    activated, this, SIGNAL( blankMessage() ) },
#ifdef GROUPCHAT
		{ "menu_join_groupchat", activated, this, SIGNAL( doGroupChat() ) },
#endif
		{ "menu_account_setup",  activated, this, SIGNAL( doManageAccounts() ) },
		{ "menu_options",        activated, this, SIGNAL( doOptions() ) },
		{ "menu_file_transfer",  activated, this, SIGNAL( doFileTransDlg() ) },
		{ "menu_toolbars",       activated, this, SIGNAL( doToolbars() ) },
		{ "menu_change_profile", activated, this, SIGNAL( changeProfile() ) },
		{ "menu_quit",           activated, this, SLOT( try2tryCloseProgram() ) },
		{ "menu_play_sounds",    toggled,   this, SLOT( actPlaySoundsActivated(bool) ) },
#ifdef USE_PEP
		{ "publish_tune",        toggled,   this, SLOT( actPublishTuneActivated(bool) ) },
		{ "set_mood",		 activated,   this, SLOT( actSetMoodActivated() ) },
		{ "set_activity",        activated,   this, SLOT( actSetActivityActivated() ) },
		{ "set_geoloc",		 activated,   this, SLOT( actSetGeolocActivated() ) },
#endif

		{ "event_notifier", SIGNAL( clicked(int) ), this, SLOT( statusClicked(int) ) },
		{ "event_notifier", activated, this, SLOT( doRecvNextEvent() ) },

		{ "help_readme",      activated, this, SLOT( actReadmeActivated() ) },
		{ "help_tip",         activated, this, SLOT( actTipActivated() ) },
		{ "help_online_help", activated, this, SLOT( actOnlineHelpActivated() ) },
		{ "help_online_wiki", activated, this, SLOT( actOnlineWikiActivated() ) },
		{ "help_online_home", activated, this, SLOT( actOnlineHomeActivated() ) },
		{ "help_online_forum", activated, this, SLOT( actOnlineForumActivated() ) },
		{ "help_psi_muc",     activated, this, SLOT( actJoinPsiMUCActivated() ) },
		{ "help_report_bug",  activated, this, SLOT( actBugReportActivated() ) },
		{ "help_about",       activated, this, SLOT( actAboutActivated() ) },
		{ "help_about_qt",    activated, this, SLOT( actAboutQtActivated() ) },
		{ "help_about_psimedia",   activated, this, SLOT( actAboutPsiMediaActivated() ) },
		{ "help_diag_qcaplugin",   activated, this, SLOT( actDiagQCAPluginActivated() ) },
		{ "help_diag_qcakeystore", activated, this, SLOT( actDiagQCAKeyStoreActivated() ) },

		{ "", 0, 0, 0 }
	};

	int i;
	QString aName;
	for ( i = 0; !(aName = QString(actionlist[i].name)).isEmpty(); i++ ) {
		if ( aName == action->objectName() ) {
#ifdef USE_PEP
			// Check before connecting, otherwise we get a loop
			if ( aName == "publish_tune") {
				action->setChecked( PsiOptions::instance()->getOption("options.extended-presence.tune.publish").toBool() );
			}
#endif

			disconnect( action, actionlist[i].signal, actionlist[i].receiver, actionlist[i].slot ); // for safety
			connect( action, actionlist[i].signal, actionlist[i].receiver, actionlist[i].slot );

			// special cases
			if ( aName == "menu_play_sounds" ) {
				action->setChecked(PsiOptions::instance()->getOption("options.ui.notifications.sounds.enable").toBool());
			}
			else if ( aName == "enable_groups" ) {
				action->setChecked(PsiOptions::instance()->getOption("options.ui.contactlist.enable-groups").toBool());
			}
			//else if ( aName == "foobar" )
			//	;
		}
	}

	struct {
		const char* name;
		QObject* sender;
		const char* signal;
		const char* slot;
		bool checked;
	} reverseactionlist[] = {
		// { "show_away",      contactList, SIGNAL(showAwayChanged(bool)), setChecked, contactList->showAway()},
		{ "show_hidden",    contactList, SIGNAL(showHiddenChanged(bool)), setChecked, contactList->showHidden()},
		{ "show_offline",   contactList, SIGNAL(showOfflineChanged(bool)), setChecked, contactList->showOffline()},
		{ "show_self",      contactList, SIGNAL(showSelfChanged(bool)), setChecked, contactList->showSelf()},
		{ "show_agents",    contactList, SIGNAL(showAgentsChanged(bool)), setChecked, contactList->showAgents()},
		{ "show_statusmsg", 0, 0, 0, false},
		{ "", 0, 0, 0, false }
	};

	for ( i = 0; !(aName = QString(reverseactionlist[i].name)).isEmpty(); i++ ) {
		if ( aName == action->objectName() ) {
			if (reverseactionlist[i].sender) {
				disconnect( reverseactionlist[i].sender, reverseactionlist[i].signal, action, reverseactionlist[i].slot ); // for safety
				connect( reverseactionlist[i].sender, reverseactionlist[i].signal, action, reverseactionlist[i].slot );
			}

			if (aName == "show_statusmsg") {
				action->setChecked( PsiOptions::instance()->getOption(showStatusMessagesOptionPath).toBool() );
			}
			else
				action->setChecked( reverseactionlist[i].checked );
		}
	}
}

void MainWin::reinitAutoHide()
{
	int interval = PsiOptions::instance()->getOption("options.contactlist.autohide-interval").toInt();
	if (interval) {
		if (!d->hideTimer) {
			d->hideTimer = new QTimer(this);
			connect(d->hideTimer, SIGNAL(timeout()), SLOT(hideTimerTimeout()));
		}
		d->hideTimer->setInterval(interval*1000);
		if (isVisible()) {
			d->hideTimer->start();
		}
	} else {
		delete d->hideTimer;
		d->hideTimer = 0;
	}
}

void MainWin::hideTimerTimeout()
{
	d->hideTimer->stop();
	if(d->tray)
		trayHide();
	else
		setWindowState(Qt::WindowMinimized);
}

PsiCon* MainWin::psiCon() const
{
	return d->psi;
}

void MainWin::setWindowOpts(bool _onTop, bool _asTool)
{
	if(_onTop == d->onTop && _asTool == d->asTool) {
		return;
	}

	d->onTop = _onTop;
	d->asTool = _asTool;

	Qt::WindowFlags flags = windowFlags();
	if(d->onTop) {
		flags |= Qt::WindowStaysOnTopHint;
	}
	else {
		flags &= ~Qt::WindowStaysOnTopHint;
	}
	if(d->asTool) {
		flags |= Qt::Tool;
	}
	else {
		flags &= ~Qt::Tool;
#ifdef Q_OS_WIN
		flags |= Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint;
#endif
	}

	setWindowFlags(flags);
	show();
}

void MainWin::setUseDock(bool use)
{
	if (use == (d->tray != 0)) {
		return;
	}

	if (d->tray) {
		delete d->tray;
		d->tray = 0;
	}

	Q_ASSERT(!d->tray);
	if (use) {
		d->tray = new PsiTrayIcon(ApplicationInfo::name(), d->trayMenu);
		connect(d->tray, SIGNAL(clicked(const QPoint &, int)), SLOT(trayClicked(const QPoint &, int)));
		connect(d->tray, SIGNAL(doubleClicked(const QPoint &)), SLOT(trayDoubleClicked()));
		d->tray->setIcon(PsiIconset::instance()->statusPtr(STATUS_OFFLINE));
		setTrayToolTip();
		connect(d->tray, SIGNAL(doToolTip(QObject *, QPoint)), this, SLOT(doTrayToolTip(QObject *, QPoint)));

		updateReadNext(d->nextAnim, d->nextAmount);

		d->tray->show();
	}
}

void MainWin::setUseAvatarFrame(bool state)
{
	d->rosterAvatar->setVisible(state);
}

void MainWin::buildStatusMenu()
{
	GlobalStatusMenu *statusMenu = qobject_cast<GlobalStatusMenu*>(sender());
	Q_ASSERT(statusMenu);
	if (statusMenu) {
		buildStatusMenu(statusMenu);
	}
}

void MainWin::buildStatusMenu(GlobalStatusMenu *statusMenu)
{
	statusMenu->clear();
	statusMenu->fill();
#ifdef USE_PEP
	statusMenu->addSeparator();
	d->getAction("publish_tune")->addTo(statusMenu);
#endif
}

void MainWin::activatedStatusAction(int id)
{
	QList<IconAction*> l = d->statusGroup->findChildren<IconAction*>();
	foreach(IconAction* action, l) {
		action->setChecked ( d->statusActions.contains(action) && d->statusActions[action] == id );
	}

	statusChanged(static_cast<XMPP::Status::Type>(id));
}

QMenuBar* MainWin::mainMenuBar() const
{
#ifdef Q_OS_MAC
	if (!d->squishEnabled) {
		return psiCon()->defaultMenuBar();
	}
#endif
	return menuBar();
}

void MainWin::saveToolbarsState()
{
	PsiOptions::instance()->setOption(toolbarsStateOptionPath, saveState());
}

void MainWin::loadToolbarsState()
{
	restoreState(PsiOptions::instance()->getOption(toolbarsStateOptionPath).toByteArray());
}

void MainWin::buildToolbars()
{
	setUpdatesEnabled(false);
	if (toolbars_.count() > 0) {
		saveToolbarsState();
	}

	qDeleteAll(toolbars_);
	toolbars_.clear();

	PsiOptions *options = PsiOptions::instance();
	bool allInOne = options->getOption("options.ui.tabs.grouping").toString().contains('A');
	foreach(const QString &base, options->getChildOptionNames("options.ui.contactlist.toolbars", true, true)) {
		QString toolbarName = options->getOption(base + ".name").toString();
		if (toolbarName == "Chat" || toolbarName == "Groupchat") {
			continue;
		}

		PsiToolBar* tb;
		if (allInOne) {
			if (d && d->viewToolBar && (d->viewToolBar->base() == base))
				continue;
			tb = new PsiToolBar(base, this, d->psi->actionList());
			d->vb_roster->addWidget(tb);
		} else {
			if (d && d->viewToolBar) {
				delete d->viewToolBar;
				d->viewToolBar = 0;
			}
			tb = new PsiToolBar(base, this, d->psi->actionList());
		}
		tb->initialize();
		connect(tb, SIGNAL(customize()), d->psi, SLOT(doToolbars()));
		toolbars_ << tb;
	}

	loadToolbarsState();

	// loadToolbarsState also restores correct toolbar visibility,
	// we might want to override that
	foreach(PsiToolBar* tb, toolbars_) {
		tb->updateVisibility();
	}

	// d->eventNotifier->updateVisibility();
	setUpdatesEnabled(true);

	// in case we have floating toolbars, they have inherited the 'no updates enabled'
	// state. now we need to explicitly re-enable updates.
	foreach(PsiToolBar* tb, toolbars_) {
		tb->setUpdatesEnabled(true);
	}

	if (allInOne && d && d->viewToolBar) {
		d->viewToolBar->updateVisibility();
		d->viewToolBar->setUpdatesEnabled(true);
	}
}

bool MainWin::showDockMenu(const QPoint &)
{
	return false;
}

void MainWin::buildOptionsMenu()
{
	buildGeneralMenu( d->optionsMenu );
	d->optionsMenu->addSeparator();

	// help menu
	QMenu* helpMenu = new QMenu(tr("&Help"), d->optionsMenu);
	helpMenu->setIcon(IconsetFactory::icon("psi/help").icon());

	QStringList actions;
	actions << "help_readme"
			<< "help_tip"
			<< "separator"
			//<< "help_online_help"
			<< "help_online_wiki"
			<< "help_online_home"
			<< "help_online_forum"
			<< "help_psi_muc"
			<< "help_report_bug"
			<< "diagnostics"
			<< "separator"
			<< "help_about"
			<< "help_about_qt";

	if(AvCallManager::isSupported())
		actions << "help_about_psimedia";

	d->updateMenu(actions, helpMenu);
	d->optionsMenu->addMenu(helpMenu);
	d->getAction("menu_quit")->addTo( d->optionsMenu );

}

void MainWin::buildMainMenu()
{
	// main menu
	QStringList actions;
	actions << "menu_add_contact";
	if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool()) {
		actions << "menu_new_message";
	}
	actions << "menu_disco"
#ifdef GROUPCHAT
			<< "menu_join_groupchat"
#endif
			<< "separator"
			<< "menu_account_setup";
	if (PsiOptions::instance()->getOption("options.ui.menu.main.change-profile").toBool()) {
		actions << "menu_change_profile";
	}
	actions << "menu_play_sounds";

	d->updateMenu(actions, d->mainMenu);
}

void MainWin::buildToolsMenu()
{
	QStringList actions;
	actions << "menu_file_transfer"
			<< "separator"
			<< "menu_xml_console";

	d->updateMenu(actions, d->toolsMenu);
}

void MainWin::buildGeneralMenu(QMenu* menu)
{
	// options menu
	QStringList actions;
	actions << "menu_add_contact";
	if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool()) {
		actions << "menu_new_message";
	}
	actions << "menu_disco"
#ifdef GROUPCHAT
			<< "menu_join_groupchat"
#endif
			<< "menu_account_setup"
			<< "menu_options"
			<< "menu_file_transfer";
	if (PsiOptions::instance()->getOption("options.ui.menu.main.change-profile").toBool()) {
		actions << "menu_change_profile";
	}
	actions << "menu_play_sounds";

	d->updateMenu(actions, menu);
}

void MainWin::actReadmeActivated ()
{
	ShowTextDlg* w = new ShowTextDlg(":/README");
	w->setWindowTitle(CAP(tr("ReadMe")));
	w->show();
}

void MainWin::actOnlineHelpActivated ()
{
    DesktopUtil::openUrl("https://psi-im.org/wiki/User_Guide");
}

void MainWin::actOnlineWikiActivated ()
{
	DesktopUtil::openUrl("https://github.com/psi-im/psi/wiki");
}

void MainWin::actOnlineHomeActivated ()
{
    DesktopUtil::openUrl("https://psi-im.org");
}

void MainWin::actOnlineForumActivated ()
{
    DesktopUtil::openUrl("https://groups.google.com/g/psi-users?pli=1");
}

void MainWin::actJoinPsiMUCActivated()
{
	PsiAccount* account = d->psi->contactList()->defaultAccount();
	if(!account) {
		return;
	}

    account->actionJoin("psi-dev@conference.jabber.ru");
}

void MainWin::actBugReportActivated ()
{
	DesktopUtil::openUrl("https://github.com/psi-im/psi/issues");
}

void MainWin::actAboutActivated ()
{
	AboutDlg* about = new AboutDlg();
	about->show();
}

void MainWin::actTipActivated ()
{
	TipDlg::show(d->psi);
}

void MainWin::actAboutQtActivated ()
{
	QMessageBox::aboutQt(this);
}

void MainWin::actAboutPsiMediaActivated ()
{
	QString creditText = PsiMedia::creditText();
	QString gstVersion = extract_gst_version(creditText);

	QString str;
	QPixmap pix;
	if(!gstVersion.isEmpty())
	{
		str = tr(
			"This application uses GStreamer %1, a comprehensive "
			"open-source and cross-platform multimedia framework."
			"  For more information, see "
            "<a href=\"https://www.gstreamer.net/\">http://www.gstreamer.net/</a>").arg(gstVersion);
		pix = IconsetFactory::icon("psi/gst_logo").pixmap();
	}
	else
		str = creditText;

	QDialog aboutGst;
	QVBoxLayout *vb = new QVBoxLayout(&aboutGst);
	aboutGst.setWindowTitle(tr("About GStreamer"));
	QHBoxLayout *hb = new QHBoxLayout;
	vb->addLayout(hb);
	if(!pix.isNull())
	{
		QLabel *la = new QLabel(&aboutGst);
		la->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
		la->setPixmap(pix);
		hb->addWidget(la);
	}
	QLabel *lb = new QLabel(&aboutGst);
	lb->setText(str);
	lb->setTextFormat(Qt::RichText);
	lb->setWordWrap(true);
	lb->setOpenExternalLinks(true);
	hb->addWidget(lb);
	QDialogButtonBox *buttonBox = new QDialogButtonBox(&aboutGst);
	buttonBox->addButton(QDialogButtonBox::Ok);
	aboutGst.connect(buttonBox, SIGNAL(accepted()), SLOT(accept()));
	vb->addWidget(buttonBox);
	if(!pix.isNull())
	{
		int w = pix.width() * 4;
		aboutGst.resize(w, aboutGst.heightForWidth(w));
	}
	aboutGst.exec();
	//QMessageBox::about(this, tr("About GStreamer"), str);
}

void MainWin::actDiagQCAPluginActivated()
{
	QString dtext = QCA::pluginDiagnosticText();
	ShowTextDlg* w = new ShowTextDlg(dtext, true, false, this);
	w->setWindowTitle(CAP(tr("Security Plugins Diagnostic Text")));
	w->resize(560, 240);
	w->show();
}

void MainWin::actDiagQCAKeyStoreActivated()
{
	QString dtext = QCA::KeyStoreManager::diagnosticText();
	ShowTextDlg* w = new ShowTextDlg(dtext, true, false, this);
	w->setWindowTitle(CAP(tr("Key Storage Diagnostic Text")));
	w->resize(560, 240);
	w->show();
}

void MainWin::actChooseStatusActivated()
{
	PsiOptions* o = PsiOptions::instance();
	XMPP::Status::Type lastStatus = XMPP::Status::txt2type(PsiOptions::instance()->getOption("options.status.last-status").toString());
	StatusSetDlg *w = new StatusSetDlg(d->psi, makeLastStatus(lastStatus), lastPriorityNotEmpty());
	connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)), d->psi, SLOT(setGlobalStatus(const XMPP::Status &,bool,bool)));
	connect(w, SIGNAL(cancelled()), d->psi, SLOT(updateMainwinStatus()));
	if(o->getOption("options.ui.systemtray.enable").toBool() == true)
		connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)), SLOT(setTrayToolTip(const XMPP::Status &, bool, bool)));
	w->show();
}

void MainWin::actReconnectActivated()
{
	foreach (PsiAccount *pa, d->psi->contactList()->accounts()) {
		pa->reconnectOnce();
	}
}

void MainWin::actPlaySoundsActivated (bool state)
{
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.enable", state);
}

void MainWin::actPublishTuneActivated (bool state)
{
	PsiOptions::instance()->setOption("options.extended-presence.tune.publish",state);
}

void MainWin::actEnableGroupsActivated (bool state)
{
	PsiOptions::instance()->setOption("options.ui.contactlist.enable-groups", state);
}

void MainWin::actSetMoodActivated()
{
	QList<PsiAccount*> l;
	foreach(PsiAccount *pa, d->psi->contactList()->accounts()) {
		if(pa->isActive() && pa->serverInfoManager()->hasPEP() && !pa->accountOptions().ignore_global_actions)
			l.append(pa);
	}
	if(l.isEmpty())
		return;

	if(d->moodDlg)
		bringToFront(d->moodDlg);
	else {
		d->moodDlg = new MoodDlg(l);
		d->moodDlg->show();
	}
}

void MainWin::actSetActivityActivated()
{
	QList<PsiAccount*> l;
	foreach(PsiAccount *pa, d->psi->contactList()->accounts()) {
		if(pa->isActive() && pa->serverInfoManager()->hasPEP() && !pa->accountOptions().ignore_global_actions)
			l.append(pa);
	}
	if(l.isEmpty())
		return;

	if(d->activityDlg)
		bringToFront(d->activityDlg);
	else {
		d->activityDlg = new ActivityDlg(l);
		d->activityDlg->show();
	}
}

void MainWin::actSetGeolocActivated()
{
	QList<PsiAccount*> l;
	foreach(PsiAccount *pa, d->psi->contactList()->accounts()) {
		if(pa->isActive() && pa->serverInfoManager()->hasPEP() && !pa->accountOptions().ignore_global_actions)
			l.append(pa);
	}
	if(l.isEmpty())
		return;

	if(d->geolocationDlg)
		bringToFront(d->geolocationDlg);
	else {
		d->geolocationDlg = new GeoLocationDlg(l);
		d->geolocationDlg->show();
	}
}

void MainWin::actActiveContacts()
{
	ActiveContactsMenu* acm = new ActiveContactsMenu(d->psi, this);
	if(!acm->actions().isEmpty())
		acm->exec(QCursor::pos());
	delete acm;
}

void MainWin::activatedAccOption(PsiAccount* pa, int x)
{
	if(x == 0) {
		pa->openAddUserDlg();
	}
	else if(x == 2) {
		pa->showXmlConsole();
	}
	else if(x == 3) {
		pa->doDisco();
	}
}

void MainWin::buildTrayMenu()
{
#ifndef Q_OS_MAC
	d->trayMenu->clear();

	if(d->nextAmount > 0) {
		d->trayMenu->addAction(tr("Receive next event"), this, SLOT(doRecvNextEvent()));
		d->trayMenu->addSeparator();
	}

	if(isHidden()) {
		d->trayMenu->addAction(tr("Un&hide"), this, SLOT(trayShow()));
	}
	else {
		d->trayMenu->addAction(tr("&Hide"), this, SLOT(trayHide()));
	}
	d->optionsButton->addTo(d->trayMenu);
	d->trayMenu->addMenu(d->statusMenu);

	d->trayMenu->addSeparator();
	// TODO!
	d->getAction("menu_quit")->addTo(d->trayMenu);
#endif
}

void MainWin::setTrayToolTip()
{
	if (!d->tray) {
		return;
	}
	QString s = ApplicationInfo::name();
	QString str = "<qt>";
	QString imgTag = "icon name";
	str += QString("<div style='white-space:pre'><b>%1</b></div>").arg(TextUtil::escape(ApplicationInfo::name()));
	QString Tip = "";
	QString TipPlain = "";
	QString Events = "";
	QString EventsPlain = "";
	foreach(PsiAccount *pa, d->psi->contactList()->enabledAccounts()) {
		Status stat = pa->status();
		int status = makeSTATUS(stat);
		QString istr = "status/offline";
		if(status == STATUS_ONLINE)
			istr = "status/online";
		else if(status == STATUS_AWAY)
			istr = "status/away";
		else if(status == STATUS_XA)
			istr = "status/xa";
		else if(status == STATUS_DND)
			istr = "status/dnd";
		else if(status == STATUS_CHAT)
 			istr = "status/chat";
		else if(status == STATUS_INVISIBLE)
			istr = "status/invisible";
		Tip += QString("<div style='white-space:pre'>") + QString("<%1=\"%2\"> ").arg(imgTag).arg(istr) + QString("<b>%1</b>").arg(TextUtil::escape(pa->name())) + "</div>";
		TipPlain += "\n" + pa->name() + " (" + stat.typeString() + ")";
		QString text = stat.status();
		if(!text.isEmpty()) {
			text = clipStatus(text, 40, 1);
			Tip += QString("<div style='white-space:pre'>%1: %2</div>").arg(tr("Status Message")).arg(TextUtil::escape(text));
		}

		PsiEvent::Ptr e;
		e = pa->eventQueue()->peekNext();
		if(e) {
			Jid jid = e->jid();
			QString from;
			if(!jid.isEmpty()) {
				LiveRoster Roster = pa->client()->roster();
				while(!Roster.isEmpty()) {
					LiveRosterItem item = Roster.takeFirst();
					if(item.jid().compare(jid)) {
						from = item.name();
						break;
					}
				}
				if(from.isEmpty()) {
					from = jid.full();
				}
			}
			if(!from.isEmpty()) {
				Events += QString("<div style='white-space:pre'>%1</div>").arg(TextUtil::escape(from));
				EventsPlain += "\n" + from;
			}
		}
	}

	if(!Tip.isEmpty()) {
		str += QString("<div style='white-space:pre'><u><b>%1</b></u></div>").arg(TextUtil::escape(tr("Active accounts:"))) + Tip;
		s += tr("\nActive accounts:") + TipPlain;
	}

	if(!Events.isEmpty()) {
		str += QString("<div style='white-space:pre'><u><b>%1</b></u></div>").arg(TextUtil::escape(tr("Incoming event(s) from:"))) + Events;
		s += tr("\nIncoming event(s) from:") + EventsPlain;
	}

	str += "</qt>";

	d->ToolTipText = str;
	d->tray->setToolTip(s);
}

void MainWin::doTrayToolTip(QObject *, QPoint p)
{
	PsiToolTip::showText(p, d->ToolTipText);
}

void MainWin::decorateButton(int status)
{
	// update the 'change status' buttons
	QList<IconAction*> l = d->statusGroup->findChildren<IconAction*>();
	foreach(IconAction* action, l) {
		action->setChecked ( d->statusActions[action] == status );
	}

	setTrayToolTip();
	d->lastStatus = status;

	if(status == -1) {
		d->statusButton->setText(tr("Connecting"));
		if (PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString() != "no") {
			d->statusButton->setAlert(IconsetFactory::iconPtr("psi/connect"));
			d->statusSmallerAlt->setPsiIcon(IconsetFactory::iconPtr("psi/connect"));
		}
		else {
			d->statusButton->setIcon(PsiIconset::instance()->statusPtr(STATUS_OFFLINE));
			d->statusSmallerAlt->setPsiIcon(PsiIconset::instance()->statusPtr(STATUS_OFFLINE));
			d->rosterAvatar->setStatusIcon(PsiIconset::instance()->statusPtr(STATUS_OFFLINE)->icon());
		}

#ifdef Q_OS_LINUX
		d->statusMenuMB->statusChanged(makeStatus(STATUS_OFFLINE, ""));
#endif
		d->statusMenu->statusChanged(makeStatus(STATUS_OFFLINE, ""));

		setWindowIcon(PsiIconset::instance()->status(STATUS_OFFLINE).impix());
	}
	else {
		d->statusButton->setText(status2txt(status));
		d->statusButton->setIcon(PsiIconset::instance()->statusPtr(status));
		d->statusSmallerAlt->setPsiIcon(PsiIconset::instance()->statusPtr(status));
		d->rosterAvatar->setStatusIcon(PsiIconset::instance()->statusPtr(status)->icon());
#ifdef Q_OS_LINUX
		d->statusMenuMB->statusChanged(makeStatus(status, d->psi->currentStatusMessage()));
#endif
		d->statusMenu->statusChanged(makeStatus(status, d->psi->currentStatusMessage()));
		setWindowIcon(PsiIconset::instance()->status(status).impix());
	}

	updateTray();
}

bool MainWin::askQuit()
{
	return true;
}

void MainWin::try2tryCloseProgram()
{
	QTimer::singleShot(0, this, SLOT(tryCloseProgram()));
}

void MainWin::tryCloseProgram()
{
	if(askQuit()) {
		closeProgram();
	}
}

void MainWin::closeEvent(QCloseEvent* e)
{
#ifdef Q_OS_MAC
	trayHide();
	e->accept();
#else
	PsiOptions *o = PsiOptions::instance();
	bool quitOnClose = o->getOption("options.ui.contactlist.quit-on-close").toBool()
			&& o->getOption("options.contactlist.autohide-interval").toInt() == 0;

	if(d->tray && !quitOnClose) {
		trayHide();
		e->accept();
		return;
	}

	if(!askQuit()) {
		return;
	}

	closeProgram();

	e->accept();
#endif
}

void MainWin::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange ||
	    event->type() == QEvent::WindowStateChange)
	{
		if (d->mainTabs) {
			QCoreApplication::sendEvent(d->mainTabs, event);
		}
	}
}

void MainWin::keyPressEvent(QKeyEvent* e)
{
#ifdef Q_OS_MAC
	bool allowed = true;
#else
	bool allowed = d->tray ? true: false;
#endif

	bool closekey = false;
	if(e->key() == Qt::Key_Escape) {
		closekey = true;
	}
#ifdef Q_OS_MAC
	else if(e->key() == Qt::Key_W && e->modifiers() & Qt::ControlModifier) {
		closekey = true;
	}
#endif

	if(allowed && closekey) {
		close();
		e->accept();
		return;
	}
	QWidget::keyPressEvent(e);
}

void MainWin::enterEvent(QEvent *e)
{
	if(d->hideTimer)
		d->hideTimer->stop();
	QMainWindow::enterEvent(e);
}

void MainWin::leaveEvent(QEvent *e)
{
	if(d->hideTimer)
		d->hideTimer->start();
	QMainWindow::leaveEvent(e);
}

bool MainWin::eventFilter(QObject *o, QEvent *e)
{
	if(e->type() == QEvent::KeyPress
	   && o->isWidgetType()
	   && isAncestorOf( (QWidget*)o ) ) {
		if(d->hideTimer && d->hideTimer->isActive())
			d->hideTimer->start();
	}
	return false;
}

#ifdef Q_OS_WIN
#include <windows.h>
bool MainWin::winEvent(MSG* msg, long* result)
{
	if (d->asTool && msg->message == WM_SYSCOMMAND && msg->wParam == SC_MINIMIZE) {
		hide();	// minimized toolwindows look bad on Windows, so let's just hide it instead
			// plus we cannot do this in changeEvent(), because it's called too late
		*result = 0;
		return true;	// don't let Qt process this event
	}
	return false;
}
#endif

void MainWin::updateCaption()
{
	QString str = "";

	if(d->nextAmount > 0) {
		str += "* ";
	}

	if(d->nickname.isEmpty()) {
		str += ApplicationInfo::name();
	}
	else {
		str += d->nickname;
	}

	if(str == windowTitle()) {
		return;
	}

	setWindowTitle(str);
}

void MainWin::optionsUpdate()
{
	int status = d->lastStatus;
	d->lastStatus = -2;
	decorateButton(status);

#ifndef Q_OS_MAC
	if (!PsiOptions::instance()->getOption("options.ui.contactlist.show-menubar").toBool()) {
		mainMenuBar()->hide();
	}
	else {
		mainMenuBar()->show();
	}
#endif

	setWindowOpacity(double(qMax(MINIMUM_OPACITY,PsiOptions::instance()->getOption("options.ui.contactlist.opacity").toInt()))/100);

	buildStatusMenu(d->statusMenu);
#ifdef Q_OS_LINUX
	buildStatusMenu(d->statusMenuMB);
#endif
	updateTray();
}

void MainWin::toggleVisible()
{
	if(!isHidden()) {
		trayHide();
	}
	else {
		trayShow();
	}
}

void MainWin::setTrayToolTip(const Status& status, bool, bool)
{
	Q_UNUSED(status)

	if (!d->tray) {
		return;
	}
	setTrayToolTip();
}

void MainWin::trayClicked(const QPoint &, int button)
{
	if(PsiOptions::instance()->getOption("options.ui.systemtray.use-double-click").toBool()) {
		return;
	}

	if(button == Qt::MidButton) {
		doRecvNextEvent();
		return;
	}

	if(!isHidden()) {
		trayHide();
	}
	else {
		trayShow();
	}
}

void MainWin::trayDoubleClicked()
{
	if(!PsiOptions::instance()->getOption("options.ui.systemtray.use-double-click").toBool()) {
		return;
	}

	if(d->nextAmount > 0) {
		doRecvNextEvent();
		return;
	}


	if(!isHidden()) {
		trayHide();
	}
	else {
		trayShow();
	}
}

void MainWin::trayShow()
{
	bringToFront(this);
	if(d->hideTimer)
		d->hideTimer->start();
}

void MainWin::trayHide()
{
	hide();
}

void MainWin::updateReadNext(PsiIcon* anim, int amount)
{
	d->nextAnim = anim;
	if(anim == 0) {
		d->nextAmount = 0;
	}
	else {
		d->nextAmount = amount;
	}

	if(d->nextAmount <= 0) {
		d->eventNotifier->hide();
		d->eventNotifier->setMessage("");
	}
	else {
		d->eventNotifier->setMessage(QString("<b>") + numEventsString(d->nextAmount) + "</b>");
		d->eventNotifier->show();
		// make sure it shows
		//qApp->processEvents();
	}

	updateTray();
	updateCaption();
	setTrayToolTip();
}

QString MainWin::numEventsString(int x) const
{
	QString s;
	if(x <= 0) {
		s = "";
	}
	else if(x == 1) {
		s = tr("1 event received");
	}
	else {
		s = tr("%1 events received").arg(x);
	}

	return s;
}

void MainWin::updateTray()
{
	if(!d->tray) {
		return;
	}

	if ( d->nextAmount > 0 ) {
		d->tray->setAlert(d->nextAnim);
	}
	else if ( d->lastStatus == -1 ) {
		d->tray->setAlert(IconsetFactory::iconPtr("psi/connect"));
	}
	else {
		d->tray->setIcon(PsiIconset::instance()->statusPtr(d->lastStatus));
	}

	buildTrayMenu();
	d->tray->setContextMenu(d->trayMenu);
}

void MainWin::doRecvNextEvent()
{
	recvNextEvent();
}

void MainWin::statusClicked(int x)
{
	if(x == Qt::MidButton) {
		recvNextEvent();
	}
}

PsiTrayIcon *MainWin::psiTrayIcon()
{
	return d->tray;
}

void MainWin::numAccountsChanged()
{
	d->statusButton->setEnabled(d->psi->contactList()->haveEnabledAccounts());
	setTrayToolTip();
	PsiAccount *acc = d->psi->contactList()->defaultAccount();
	if(acc && acc != d->defaultAccount) {
		if(d->defaultAccount) {
			disconnect(d->defaultAccount, SIGNAL(nickChanged()), this, SLOT(nickChanged()));
//			disconnect(d->defaultAccount->avatarFactory(), SIGNAL(avatarChanged(Jid)), this, SLOT(avatarChanged()));
		}
		d->defaultAccount = acc;
		avatarChanged(acc->jid());
		nickChanged();
		d->rosterAvatar->setStatusMessage(acc->status().status());
		connect(acc->avatarFactory(), SIGNAL(avatarChanged(Jid)), this, SLOT(avatarChanged(Jid)));
		connect(acc, SIGNAL(nickChanged()), this, SLOT(nickChanged()));

	} if (!acc) { // no accounts left
		d->rosterAvatar->setAvatar(IconsetFactory::iconPixmap("psi/default_avatar"));
	}
}

void MainWin::nickChanged()
{
	if(d->defaultAccount)
		d->rosterAvatar->setNick(d->defaultAccount->nick());
}

void MainWin::avatarChanged(const Jid &jid)
{
	if(d->defaultAccount && d->defaultAccount->jid() == jid) {
		QPixmap pix = d->defaultAccount->avatarFactory()->getAvatar(d->defaultAccount->jid());
		if(pix.isNull())
			pix = IconsetFactory::iconPixmap("psi/default_avatar");
		d->rosterAvatar->setAvatar(pix);
	}
}

void MainWin::accountFeaturesChanged()
{
	bool have_pep = false;
	foreach(PsiAccount* account, d->psi->contactList()->enabledAccounts()) {
		if (account->serverInfoManager()->hasPEP()) {
			have_pep = true;
			break;
		}
	}

#ifdef USE_PEP
	d->getAction("publish_tune")->setEnabled(have_pep);
#endif
}

void MainWin::dockActivated()
{
	if(isHidden()) {
		show();
	}
}

/**
 * Called when the cancel is clicked or the search becomes empty.
 * Cancels the search.
 */
void MainWin::searchClearClicked()
{
	d->searchWidget->setVisible(false);
	d->searchText->clear();

	if (d->filterActive)
	{
		d->getAction("show_offline")->setChecked(d->prefilterShowOffline);
		d->getAction("show_away")->setChecked(d->prefilterShowAway);
	}
	d->filterActive=false;
}

/**
 * Called when the contactview has a keypress.
 * Starts the search/filter process
 */
void MainWin::searchTextStarted(QString const& text)
{
	d->searchWidget->setVisible(true);
	d->searchText->setText(d->searchText->text() + text);
	searchTextEntered(d->searchText->text());
	d->searchText->setFocus();
}

/**
 * Called when the search input is changed.
 * Updates the search.
 */
void MainWin::searchTextEntered(QString const& text)
{
	if (!d->filterActive)
	{
		d->filterActive = true;
		d->prefilterShowOffline = d->getAction("show_offline")->isChecked();
		d->prefilterShowAway = d->getAction("show_away")->isChecked();
		d->getAction("show_offline")->setChecked(true);
		d->getAction("show_away")->setChecked(true);
	}
	if (text.isEmpty()) {
		searchClearClicked();
	}
}

#ifdef Q_OS_MAC
void MainWin::setWindowIcon(const QPixmap&)
{
}
#else
void MainWin::setWindowIcon(const QPixmap& p)
{
	QMainWindow::setWindowIcon(p);
}
#endif

#if 0
#if defined(Q_OS_WIN)
#include <windows.h>
void MainWin::showNoFocus()
{
	clearWState( WState_ForceHide );

	if ( testWState(WState_Visible) ) {
		SetWindowPos(winId(),HWND_TOPMOST,0,0,0,0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
		if(!d->onTop)
			SetWindowPos(winId(),HWND_NOTOPMOST,0,0,0,0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
		return; // nothing to do
	}

	if ( isTopLevel() && !testWState( WState_Resized ) )  {
		// do this before sending the posted resize events. Otherwise
		// the layout would catch the resize event and may expand the
		// minimum size.
		QSize s = sizeHint();
		QSizePolicy::ExpandData exp;
#ifndef QT_NO_LAYOUT
		if ( layout() ) {
			if ( layout()->hasHeightForWidth() )
				s.setHeight( layout()->totalHeightForWidth( s.width() ) );
			exp =  layout()->expanding();
		} else
#endif
		{
			if ( sizePolicy().hasHeightForWidth() )
				s.setHeight( heightForWidth( s.width() ) );
			exp = sizePolicy().expanding();
		}
		if ( exp & QSizePolicy::Horizontally )
			s.setWidth( QMAX( s.width(), 200 ) );
		if ( exp & QSizePolicy::Vertically )
			s.setHeight( QMAX( s.height(), 150 ) );
		QRect screen = QApplication::desktop()->screenGeometry( QApplication::desktop()->screenNumber( pos() ) );
		s.setWidth( QMIN( s.width(), screen.width()*2/3 ) );
		s.setHeight( QMIN( s.height(), screen.height()*2/3 ) );
		if ( !s.isEmpty() )
			resize( s );
	}

	QApplication::sendPostedEvents( this, QEvent::Move );
	QApplication::sendPostedEvents( this, QEvent::Resize );

	setWState( WState_Visible );

	if ( testWFlags(Qt::WStyle_Tool) || isPopup() ) {
		raise();
	} else if ( testWFlags(Qt::WType_TopLevel) ) {
		while ( QApplication::activePopupWidget() )
		QApplication::activePopupWidget()->close();
	}

	if ( !testWState(WState_Polished) )
		polish();

	if ( children() ) {
		QObjectListIt it(*children());
		register QObject* object;
		QWidget* widget;
		while ( it ) {				// show all widget children
			object = it.current();		//   (except popups and other toplevels)
			++it;
			if ( object->isWidgetType() ) {
				widget = (QWidget*)object;
				if ( !widget->isHidden() && !widget->isTopLevel() )
				widget->show();
			}
		}
	}

#if defined(QT_ACCESSIBILITY_SUPPORT)
	QAccessible::updateAccessibility( this, 0, QAccessible::ObjectShow );
#endif

	SetWindowPos(winId(),HWND_TOP,0,0,0,0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
	UpdateWindow(winId());
}

#else
#endif
#endif

void MainWin::showNoFocus()
{
	bringToFront(this);
	if(d->hideTimer)
		d->hideTimer->start();
}

void MainWin::avcallConfig()
{
	if (AvCallManager::isSupported())
		AvCallManager::config();
}

void MainWin::resizeEvent(QResizeEvent *e)
{
	AdvancedWidget<QMainWindow>::resizeEvent(e);

	if(d->splitter && isVisible()) {
		QList<int> sizes = d->splitter->sizes();
		int tabsWidth = !d->isLeftRoster ? sizes.first() : sizes.last();
		int rosterWidth = d->isLeftRoster ? sizes.first() : sizes.last();
		int dw = rosterWidth - d->rosterSize;
		sizes.clear();
		sizes.append(tabsWidth + dw);
		if(d->isLeftRoster) {
			sizes.prepend(d->rosterSize);
		}
		else {
			sizes.append(d->rosterSize);
		}
		d->splitter->setSizes(sizes);
		d->tabsSize = tabsWidth + dw;
	}
}

//#endif

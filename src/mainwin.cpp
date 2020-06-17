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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "mainwin.h"
#include "pluginmanager.h"

#include "aboutdlg.h"
#include "activecontactsmenu.h"
#include "activitydlg.h"
#include "applicationinfo.h"
#include "avatars.h"
#include "avcall/avcall.h"
#include "common.h"
#include "desktoputil.h"
#include "geolocationdlg.h"
#include "globalstatusmenu.h"
#include "mainwin_p.h"
#include "mooddlg.h"
#include "mucjoindlg.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psicontactlist.h"
#include "psievent.h"
#include "psiiconset.h"
#include "psimedia/psimedia.h"
#include "psioptions.h"
#include "psirosterwidget.h"
#include "psitoolbar.h"
#include "psitooltip.h"
#include "psitrayicon.h"
#include "rosteravatarframe.h"
#include "showtextdlg.h"
#include "statusdlg.h"
#include "tabdlg.h"
#include "tabmanager.h"
#include "textutil.h"
#include "xmpp_serverinfomanager.h"

#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QObject>
#include <QPainter>
#include <QPixmap>
#include <QShortcut>
#include <QSignalMapper>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QtAlgorithms>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#ifdef HAVE_X11
#include <x11windowsystem.h>
#endif

using namespace XMPP;

static const QString showStatusMessagesOptionPath = "options.ui.contactlist.status-messages.show";

// FIXME: this is a really corny way of getting the GStreamer version...
QString extract_gst_version(const QString &in)
{
    int start = in.indexOf("GStreamer ");
    if (start == -1)
        return QString();
    start += 10;
    int end = in.indexOf(",", start);
    if (end == -1)
        return QString();
    return in.mid(start, end - start);
}

//----------------------------------------------------------------------------
// MainWin::Private
//----------------------------------------------------------------------------

class MainWin::Private {
public:
    Private(PsiCon *, MainWin *);
    ~Private();

    bool              onTop, asTool;
    QMenu *           mainMenu, *optionsMenu, *toolsMenu;
    GlobalStatusMenu *statusMenu;
#ifdef Q_OS_LINUX
    // Status menu for MenuBar.
    // Workaround a Unity bug.
    GlobalStatusMenu *statusMenuMB = nullptr;
#endif
    int          sbState;
    QString      nickname;
    PsiTrayIcon *tray;
    QMenu *      trayMenu;
#ifdef Q_OS_MAC
    QMenu *dockMenu;
#endif
    QVBoxLayout *vb_roster;
    QSplitter *  splitter;
    TabDlg *     mainTabs;
    QString      statusTip;
    PsiToolBar * viewToolBar;
    int          tabsSize;
    int          rosterSize;
    bool         isLeftRoster;

    PopupAction *        optionsButton, *statusButton;
    IconActionGroup *    statusGroup, *viewGroups;
    IconAction *         statusSmallerAlt;
    EventNotifierAction *eventNotifier;
    PsiCon *             psi;
    MainWin *            mainWin;
    RosterAvatarFrame *  rosterAvatar;
    QPointer<PsiAccount> defaultAccount;

    QLineEdit *  searchText;
    QToolButton *searchPb;
    QWidget *    searchWidget;

    QTimer *hideTimer;

    PsiIcon *nextAnim;
    int      nextAmount;

    QMap<QAction *, int> statusActions;

    int  lastStatus;
    bool filterActive, prefilterShowOffline, prefilterShowAway;
    bool squishEnabled;

    PsiRosterWidget *rosterWidget_;

#ifdef Q_OS_WIN
    DWORD deactivationTickCount;
#endif

    void        registerActions();
    IconAction *getAction(const QString &name);
    void        updateMenu(const QStringList &actions, QMenu *menu);

    QString ToolTipText;

    QPointer<MoodDlg>        moodDlg;
    QPointer<ActivityDlg>    activityDlg;
    QPointer<GeoLocationDlg> geolocationDlg;
};

MainWin::Private::Private(PsiCon *_psi, MainWin *_mainWin) :
    onTop(false), asTool(false), mainMenu(nullptr), optionsMenu(nullptr), toolsMenu(nullptr), statusMenu(nullptr),
    sbState(0), tray(nullptr), trayMenu(nullptr),
#ifdef Q_OS_MAC
    dockMenu(nullptr),
#endif
    vb_roster(nullptr), splitter(nullptr), mainTabs(nullptr), viewToolBar(nullptr), tabsSize(0), rosterSize(0),
    isLeftRoster(false), psi(_psi), mainWin(_mainWin), rosterAvatar(nullptr), searchText(nullptr), searchPb(nullptr),
    searchWidget(nullptr), hideTimer(nullptr), nextAnim(nullptr), nextAmount(0), lastStatus(0), rosterWidget_(nullptr)
{

    statusGroup   = static_cast<IconActionGroup *>(getAction("status_group"));
    viewGroups    = static_cast<IconActionGroup *>(getAction("view_groups"));
    eventNotifier = static_cast<EventNotifierAction *>(getAction("event_notifier"));

    optionsButton    = static_cast<PopupAction *>(getAction("button_options"));
    statusButton     = static_cast<PopupAction *>(getAction("button_status"));
    statusSmallerAlt = getAction("status_all");

    filterActive         = false;
    prefilterShowOffline = false;
    prefilterShowAway    = false;

    char *squishStr = getenv("SQUISH_ENABLED");
    squishEnabled   = squishStr != nullptr;
}

MainWin::Private::~Private() { }

void MainWin::Private::registerActions()
{
    struct {
        const char *name;
        int         id;
    } statuslist[] = { { "status_chat", STATUS_CHAT },       { "status_online", STATUS_ONLINE },
                       { "status_away", STATUS_AWAY },       { "status_xa", STATUS_XA },
                       { "status_dnd", STATUS_DND },         { "status_invisible", STATUS_INVISIBLE },
                       { "status_offline", STATUS_OFFLINE }, { "", 0 } };

    int     i;
    QString aName;
    for (i = 0; !(aName = QString::fromLatin1(statuslist[i].name)).isEmpty(); i++) {
        int         id     = statuslist[i].id;
        IconAction *action = getAction(aName);
        mainWin->connect(action, &IconAction::triggered, mainWin, [this, id](bool) {
            QList<IconAction *> l = statusGroup->findChildren<IconAction *>();
            for (IconAction *action : l) {
                auto it = statusActions.constFind(action);
                action->setChecked(it != statusActions.constEnd() && *it == id);
            }

            mainWin->statusChanged(static_cast<XMPP::Status::Type>(id));
        });

        statusActions[action] = id;
    }

    // register all actions
    PsiActionList::ActionsType type
        = PsiActionList::ActionsType(PsiActionList::Actions_MainWin | PsiActionList::Actions_Common);
    ActionList            actions = psi->actionList()->suitableActions(type);
    QStringList           names   = actions.actions();
    QStringList::Iterator it      = names.begin();
    for (; it != names.end(); ++it) {
        IconAction *action = actions.action(*it);
        if (action) {
            mainWin->registerAction(action);
        }
    }
}

IconAction *MainWin::Private::getAction(const QString &name)
{
    PsiActionList::ActionsType type
        = PsiActionList::ActionsType(PsiActionList::Actions_MainWin | PsiActionList::Actions_Common);
    ActionList  actions = psi->actionList()->suitableActions(type);
    IconAction *action  = actions.action(name);

    if (!action) {
        qWarning("MainWin::Private::getAction(): action %s not found!", qPrintable(name));
    }
    // else
    //    mainWin->registerAction( action );

    return action;
}

void MainWin::Private::updateMenu(const QStringList &actions, QMenu *menu)
{
    clearMenu(menu);

    IconAction *action;
    for (const QString &name : actions) {
        // workind around Qt/X11 bug, which displays
        // actions's text and the separator bar in Qt 4.1.1
        if (name == "separator") {
            menu->addSeparator();
            continue;
        }

        if (name == "diagnostics") {
            QMenu *diagMenu = new QMenu(tr("Diagnostics"), menu);
            getAction("help_diag_qcaplugin")->addTo(diagMenu);
            getAction("help_diag_qcakeystore")->addTo(diagMenu);
            menu->addMenu(diagMenu);
            continue;
        }

        if ((action = getAction(name))) {
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

MainWin::MainWin(bool _onTop, bool _asTool, PsiCon *psi) :
    AdvancedWidget<QMainWindow>(nullptr,
                                (_onTop ? Qt::WindowStaysOnTopHint : Qt::Widget) | (_asTool ? Qt::Tool : Qt::Widget))
{
    setObjectName("MainWin");
    setAttribute(Qt::WA_AlwaysShowToolTips);
    d = new Private(psi, this);

    setWindowIcon(PsiIconset::instance()->status(STATUS_OFFLINE).icon());

    d->onTop  = _onTop;
    d->asTool = _asTool;

    // sbState:
    //   -1 : connect
    // >= 0 : STATUS_*
    d->sbState    = STATUS_OFFLINE;
    d->lastStatus = -2;

    d->nextAmount     = 0;
    d->nextAnim       = nullptr;
    d->tray           = nullptr;
    d->trayMenu       = nullptr;
    d->statusTip      = "";
    d->nickname       = "";
    d->defaultAccount = nullptr;

    QWidget *rosterBar = new QWidget(this);
    bool     allInOne  = false;

    if (PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool()
        && PsiOptions::instance()->getOption("options.ui.tabs.grouping").toString().contains('A')) {
        d->splitter = new QSplitter(this);
        d->splitter->setObjectName("onewindowsplitter");
        connect(d->splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(splitterMoved()));
        setCentralWidget(d->splitter);
        allInOne = true;

        d->mainTabs = d->psi->tabManager()->newTabs(nullptr);
        d->psi->tabManager()->setPreferredTabsForKind('C', d->mainTabs);
        d->psi->tabManager()->setPreferredTabsForKind('M', d->mainTabs);

        QList<int> sizes;
        d->rosterSize   = PsiOptions::instance()->getOption(rosterGeometryPath).toInt();
        d->tabsSize     = PsiOptions::instance()->getOption(tabsGeometryPath).toInt();
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

    connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString &)), SLOT(optionChanged(const QString &)));

    d->vb_roster     = new QVBoxLayout(rosterBar);
    d->rosterWidget_ = new PsiRosterWidget(rosterBar);
    d->rosterWidget_->setContactList(psi->contactList());

    int layoutMargin = 2;
#ifdef Q_OS_MAC
    layoutMargin = 0;
    // FIXME
    // d->contactListView_->setFrameShape(QFrame::NoFrame);
#endif
    QMenu *viewMenu = new QMenu(tr("View"), this);

    d->vb_roster->setMargin(layoutMargin);
    d->vb_roster->setSpacing(layoutMargin);

    if (allInOne) {
        QString toolOpt = "options.ui.contactlist.toolbars";
        for (QString base : PsiOptions::instance()->getChildOptionNames(toolOpt, true, true)) {
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

    // add contact view
    d->vb_roster->addWidget(d->rosterWidget_);

    d->statusMenu = new GlobalStatusMenu(qobject_cast<QWidget *>(this), d->psi);
    d->statusMenu->setTitle(tr("Status"));
    d->statusMenu->setObjectName("statusMenu");
    connect(d->statusMenu, SIGNAL(statusSelected(XMPP::Status::Type, bool)), d->psi,
            SLOT(statusMenuChanged(XMPP::Status::Type, bool)));
    connect(d->statusMenu, SIGNAL(statusPresetSelected(XMPP::Status, bool, bool)), d->psi,
            SLOT(setGlobalStatus(XMPP::Status, bool, bool)));
    connect(d->statusMenu, SIGNAL(statusPresetDialogForced(const QString &)), d->psi,
            SLOT(showStatusDialog(const QString &)));

#ifdef Q_OS_LINUX
    d->statusMenuMB = new GlobalStatusMenu(qobject_cast<QWidget *>(this), d->psi);
    d->statusMenuMB->setTitle(tr("Status"));
    d->statusMenuMB->setObjectName("statusMenu");
    connect(d->statusMenuMB, SIGNAL(statusSelected(XMPP::Status::Type, bool)), d->psi,
            SLOT(statusMenuChanged(XMPP::Status::Type, bool)));
    connect(d->statusMenuMB, SIGNAL(statusPresetSelected(XMPP::Status, bool, bool)), d->psi,
            SLOT(setGlobalStatus(XMPP::Status, bool, bool)));
    connect(d->statusMenuMB, SIGNAL(statusPresetDialogForced(const QString &)), d->psi,
            SLOT(showStatusDialog(const QString &)));
#endif

    d->optionsMenu = new QMenu(tr("General"), this);
    d->optionsMenu->setObjectName("optionsMenu");

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
    QMenu *mainMenu = new QMenu(tr("Menu"), this);
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
    //     d->getAction("show_away")->addTo(viewMenu);
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

    QMenu *helpMenu = new QMenu(tr("Help"), this);
    mainMenuBar()->addMenu(helpMenu);
    d->getAction("help_readme")->addTo(helpMenu);
    helpMenu->addSeparator();
    d->getAction("help_online_wiki")->addTo(helpMenu);
    d->getAction("help_online_home")->addTo(helpMenu);
    d->getAction("help_online_forum")->addTo(helpMenu);
    d->getAction("help_psi_muc")->addTo(helpMenu);
    d->getAction("help_report_bug")->addTo(helpMenu);
    QMenu *diagMenu = new QMenu(tr("Diagnostics"), this);
    helpMenu->addMenu(diagMenu);
    d->getAction("help_diag_qcaplugin")->addTo(diagMenu);
    d->getAction("help_diag_qcakeystore")->addTo(diagMenu);
    if (AvCallManager::isSupported()) {
        helpMenu->addSeparator();
        d->getAction("help_about_psimedia")->addTo(helpMenu);
    }
#else
    if (!PsiOptions::instance()->getOption("options.ui.contactlist.show-menubar").toBool()) {
        mainMenuBar()->hide();
    }
    // else
    //    mainMenuBar()->show();
#endif
    d->optionsButton->setMenu(d->optionsMenu);
    d->statusButton->setMenu(d->statusMenu);
    d->rosterAvatar->setStatusMenu(d->statusMenu);
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
    if (d->tray) {
        delete d->tray;
        d->tray = nullptr;
    }

    saveToolbarsState();

    if (d->splitter) {
        PsiOptions::instance()->setOption(rosterGeometryPath, d->rosterSize);
        PsiOptions::instance()->setOption(tabsGeometryPath, d->tabsSize);
    }

    delete d;
}

void MainWin::splitterMoved()
{
    QList<int> list = d->splitter->sizes();
    d->rosterSize   = !d->isLeftRoster ? list.last() : list.first();
    d->tabsSize     = d->isLeftRoster ? list.last() : list.first();
}

void MainWin::optionChanged(const QString &option)
{
    if (option == toolbarsStateOptionPath) {
        loadToolbarsState();
    } else if (option == "options.ui.contactlist.css") {
        const QString css = PsiOptions::instance()->getOption("options.ui.contactlist.css").toString();
        if (!css.isEmpty()) {
            setStyleSheet(css);
        }
    }
}

void MainWin::registerAction(IconAction *action)
{
    const char *activated  = SIGNAL(triggered());
    const char *toggled    = SIGNAL(toggled(bool));
    const char *setChecked = SLOT(setChecked(bool));

    PsiContactList *contactList = psiCon()->contactList();

    struct MenuAction {
        const char *name;
        const char *signal;
        QObject *   receiver;
        const char *slot;
    };
    std::vector<MenuAction> actionlist = {
        { "choose_status", activated, this, SLOT(actChooseStatusActivated()) },
        { "reconnect_all", activated, this, SLOT(actReconnectActivated()) },

        { "active_contacts", activated, this, SLOT(actActiveContacts()) },
        { "show_offline", toggled, contactList, SLOT(setShowOffline(bool)) },
        // { "show_away",      toggled, contactList, SLOT( setShowAway(bool) ) },
        { "show_hidden", toggled, contactList, SLOT(setShowHidden(bool)) },
        { "show_agents", toggled, contactList, SLOT(setShowAgents(bool)) },
        { "show_self", toggled, contactList, SLOT(setShowSelf(bool)) },
        { "show_statusmsg", toggled, d->rosterWidget_, SLOT(setShowStatusMsg(bool)) },
        { "enable_groups", toggled, this, SLOT(actEnableGroupsActivated(bool)) },

        { "button_options", activated, this, SIGNAL(doOptions()) },

        { "menu_disco", SIGNAL(activated(PsiAccount *, int)), this, SLOT(activatedAccOption(PsiAccount *, int)) },
        { "menu_add_contact", SIGNAL(activated(PsiAccount *, int)), this, SLOT(activatedAccOption(PsiAccount *, int)) },
        { "menu_xml_console", SIGNAL(activated(PsiAccount *, int)), this, SLOT(activatedAccOption(PsiAccount *, int)) },

        { "menu_new_message", activated, this, SIGNAL(blankMessage()) },
#ifdef GROUPCHAT
        { "menu_join_groupchat", activated, this, SIGNAL(doGroupChat()) },
#endif
        { "menu_options", activated, this, SIGNAL(doOptions()) },
        { "menu_file_transfer", activated, this, SIGNAL(doFileTransDlg()) },
        { "menu_toolbars", activated, this, SIGNAL(doToolbars()) },
        { "menu_change_profile", activated, this, SIGNAL(changeProfile()) },
        { "menu_quit", activated, this, SLOT(try2tryCloseProgram()) },
        { "menu_play_sounds", toggled, this, SLOT(actPlaySoundsActivated(bool)) },
#ifdef USE_PEP
        { "publish_tune", toggled, this, SLOT(actPublishTuneActivated(bool)) },
        { "set_mood", activated, this, SLOT(actSetMoodActivated()) },
        { "set_activity", activated, this, SLOT(actSetActivityActivated()) },
        { "set_geoloc", activated, this, SLOT(actSetGeolocActivated()) },
#endif

        { "event_notifier", SIGNAL(clicked(int)), this, SLOT(statusClicked(int)) },
        { "event_notifier", activated, this, SLOT(doRecvNextEvent()) },

        { "help_readme", activated, this, SLOT(actReadmeActivated()) },
        { "help_online_wiki", activated, this, SLOT(actOnlineWikiActivated()) },
        { "help_online_home", activated, this, SLOT(actOnlineHomeActivated()) },
        { "help_online_forum", activated, this, SLOT(actOnlineForumActivated()) },
        { "help_psi_muc", activated, this, SLOT(actJoinPsiMUCActivated()) },
        { "help_report_bug", activated, this, SLOT(actBugReportActivated()) },
        { "help_about", activated, this, SLOT(actAboutActivated()) },
        { "help_about_qt", activated, this, SLOT(actAboutQtActivated()) },
        { "help_diag_qcaplugin", activated, this, SLOT(actDiagQCAPluginActivated()) },
        { "help_diag_qcakeystore", activated, this, SLOT(actDiagQCAKeyStoreActivated()) },
        { nullptr, nullptr, nullptr, nullptr }
    };

    int     i;
    QString aName;
    for (i = 0; !(aName = QLatin1String(actionlist[i].name)).isEmpty(); i++) {
        if (aName == action->objectName()) {
#ifdef USE_PEP
            // Check before connecting, otherwise we get a loop
            if (aName == "publish_tune") {
                action->setChecked(
                    PsiOptions::instance()->getOption("options.extended-presence.tune.publish").toBool());
                d->rosterAvatar->setTuneAction(action);
            }
#endif

            disconnect(action, actionlist[i].signal, actionlist[i].receiver, actionlist[i].slot); // for safety
            connect(action, actionlist[i].signal, actionlist[i].receiver, actionlist[i].slot);

            // special cases
            if (aName == "menu_play_sounds") {
                action->setChecked(
                    PsiOptions::instance()->getOption("options.ui.notifications.sounds.enable").toBool());
            } else if (aName == "enable_groups") {
                action->setChecked(PsiOptions::instance()->getOption("options.ui.contactlist.enable-groups").toBool());
            }
            // else if ( aName == "foobar" )
            //    ;
        }
    }

    struct {
        const char *name;
        QObject *   sender;
        const char *signal;
        const char *slot;
        bool        checked;
    } reverseactionlist[]
        = { // { "show_away",      contactList, SIGNAL(showAwayChanged(bool)), setChecked, contactList->showAway()},
            { "show_hidden", contactList, SIGNAL(showHiddenChanged(bool)), setChecked, contactList->showHidden() },
            { "show_offline", contactList, SIGNAL(showOfflineChanged(bool)), setChecked, contactList->showOffline() },
            { "show_self", contactList, SIGNAL(showSelfChanged(bool)), setChecked, contactList->showSelf() },
            { "show_agents", contactList, SIGNAL(showAgentsChanged(bool)), setChecked, contactList->showAgents() },
            { "show_statusmsg", nullptr, nullptr, nullptr, false },
            { "", nullptr, nullptr, nullptr, false }
          };

    for (i = 0; !(aName = QString(reverseactionlist[i].name)).isEmpty(); i++) {
        if (aName == action->objectName()) {
            if (reverseactionlist[i].sender) {
                disconnect(reverseactionlist[i].sender, reverseactionlist[i].signal, action,
                           reverseactionlist[i].slot); // for safety
                connect(reverseactionlist[i].sender, reverseactionlist[i].signal, action, reverseactionlist[i].slot);
            }

            if (aName == "show_statusmsg") {
                action->setChecked(PsiOptions::instance()->getOption(showStatusMessagesOptionPath).toBool());
            } else
                action->setChecked(reverseactionlist[i].checked);
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
        d->hideTimer->setInterval(interval * 1000);
        if (isVisible()) {
            d->hideTimer->start();
        }
    } else {
        delete d->hideTimer;
        d->hideTimer = nullptr;
    }
}

void MainWin::hideTimerTimeout()
{
    d->hideTimer->stop();
    if (d->tray)
        trayHide();
    else
        setWindowState(Qt::WindowMinimized);
}

PsiCon *MainWin::psiCon() const { return d->psi; }

void MainWin::setWindowOpts(bool _onTop, bool _asTool)
{
    if (_onTop == d->onTop && _asTool == d->asTool) {
        return;
    }

    d->onTop  = _onTop;
    d->asTool = _asTool;

    Qt::WindowFlags flags = windowFlags();
    if (d->onTop) {
        flags |= Qt::WindowStaysOnTopHint;
    } else {
        flags &= ~Qt::WindowStaysOnTopHint;
    }
    if (d->asTool) {
        flags |= Qt::Tool;
    } else {
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
    if (use == (d->tray != nullptr)) {
        return;
    }

    if (d->tray) {
        delete d->tray;
        d->tray = nullptr;
        if (d->trayMenu) {
            delete d->trayMenu;
            d->trayMenu = nullptr;
        }
#ifdef Q_OS_MAC
        if (d->dockMenu) {
            delete d->dockMenu;
            d->dockMenu = nullptr;
        }
#endif
        if (isHidden())
            trayShow();
    }

    Q_ASSERT(!d->tray);
    if (use) {
        buildTrayMenu();
        d->tray = new PsiTrayIcon(ApplicationInfo::name(), d->trayMenu, this);
        connect(d->tray, SIGNAL(clicked(const QPoint &, int)), SLOT(trayClicked(const QPoint &, int)));
#ifdef Q_OS_WIN
        connect(d->tray, SIGNAL(doubleClicked(const QPoint &)), SLOT(trayDoubleClicked()));
#endif
        d->tray->setIcon(PsiIconset::instance()->statusPtr(STATUS_OFFLINE));
        setTrayToolTip();
        connect(d->tray, SIGNAL(doToolTip(QObject *, QPoint)), this, SLOT(doTrayToolTip(QObject *, QPoint)));

        updateReadNext(d->nextAnim, d->nextAmount);

        d->tray->show();
    }
}

void MainWin::setUseAvatarFrame(bool state) { d->rosterAvatar->setVisible(state); }

void MainWin::buildStatusMenu()
{
    GlobalStatusMenu *statusMenu = qobject_cast<GlobalStatusMenu *>(sender());
    Q_ASSERT(statusMenu);
    if (statusMenu) {
        buildStatusMenu(statusMenu);
    }
}

void MainWin::buildStatusMenu(GlobalStatusMenu *statusMenu)
{
    statusMenu->clear();
    statusMenu->fill();
}

QMenuBar *MainWin::mainMenuBar() const
{
#ifdef Q_OS_MAC
    if (!d->squishEnabled) {
        return psiCon()->defaultMenuBar();
    }
#endif
    return menuBar();
}

void MainWin::saveToolbarsState() { PsiOptions::instance()->setOption(toolbarsStateOptionPath, saveState()); }

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

    PsiOptions *options  = PsiOptions::instance();
    bool        allInOne = options->getOption("options.ui.tabs.grouping").toString().contains('A');
    for (const QString &base : options->getChildOptionNames("options.ui.contactlist.toolbars", true, true)) {
        QString toolbarName = options->getOption(base + ".name").toString();
        if (toolbarName == "Chat" || toolbarName == "Groupchat") {
            continue;
        }

        PsiToolBar *tb;
        if (allInOne) {
            if (d && d->viewToolBar && (d->viewToolBar->base() == base))
                continue;
            tb = new PsiToolBar(base, this, d->psi->actionList());
            d->vb_roster->addWidget(tb);
        } else {
            if (d && d->viewToolBar) {
                delete d->viewToolBar;
                d->viewToolBar = nullptr;
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
    for (PsiToolBar *tb : toolbars_) {
        tb->updateVisibility();
    }

    // d->eventNotifier->updateVisibility();
    setUpdatesEnabled(true);

    // in case we have floating toolbars, they have inherited the 'no updates enabled'
    // state. now we need to explicitly re-enable updates.
    for (PsiToolBar *tb : toolbars_) {
        tb->setUpdatesEnabled(true);
    }

    if (allInOne && d && d->viewToolBar) {
        d->viewToolBar->updateVisibility();
        d->viewToolBar->setUpdatesEnabled(true);
    }
}

bool MainWin::showDockMenu(const QPoint &) { return false; }

void MainWin::buildOptionsMenu()
{
    buildGeneralMenu(d->optionsMenu);
    d->optionsMenu->addSeparator();
    d->optionsMenu->addAction(d->viewGroups);

    // help menu
    QMenu *helpMenu = new QMenu(tr("&Help"), d->optionsMenu);
    helpMenu->setIcon(IconsetFactory::icon("psi/help").icon());

    QStringList actions;
    actions << "help_readme"
            << "separator"
            << "help_online_wiki"
            << "help_online_home"
            << "help_online_forum"
            << "help_psi_muc"
            << "help_report_bug"
            << "diagnostics"
            << "separator"
            << "help_about"
            << "help_about_qt";

    d->updateMenu(actions, helpMenu);

    auto pluginActions = PluginManager::instance()->globalAboutMenuActions();
    for (auto a : pluginActions) {
        helpMenu->addAction(a);
    }

    d->optionsMenu->addMenu(helpMenu);
    d->getAction("menu_quit")->addTo(d->optionsMenu);
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
            << "separator";
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

void MainWin::buildGeneralMenu(QMenu *menu)
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
            << "menu_options"
            << "menu_file_transfer";
    if (PsiOptions::instance()->getOption("options.ui.menu.main.change-profile").toBool()) {
        actions << "menu_change_profile";
    }
    actions << "menu_play_sounds";

    d->updateMenu(actions, menu);
}

void MainWin::actReadmeActivated()
{
    ShowTextDlg *w = new ShowTextDlg(":/README.html", true);
    w->setWindowTitle(CAP(tr("ReadMe")));
    w->show();
}

void MainWin::actOnlineWikiActivated()
{
    DesktopUtil::openUrl(
#ifndef PSI_PLUS
        "https://github.com/psi-im/psi/wiki"
#else
        "https://psi-plus.com/wiki/en:main"
#endif
    );
}

void MainWin::actOnlineHomeActivated()
{
    DesktopUtil::openUrl(
#ifndef PSI_PLUS
        "https://psi-im.org"
#else
        "https://psi-plus.com"
#endif
    );
}

void MainWin::actOnlineForumActivated() { DesktopUtil::openUrl("https://groups.google.com/forum/#!forum/psi-users"); }

void MainWin::actJoinPsiMUCActivated()
{
    PsiAccount *account = d->psi->contactList()->defaultAccount();
    if (!account) {
        return;
    }

    account->actionJoin("psi-dev@conference.jabber.ru");
}

void MainWin::actBugReportActivated()
{
    DesktopUtil::openUrl(
#ifndef PSI_PLUS
        "https://github.com/psi-im/psi/issues"
#else
        "https://github.com/psi-plus/main/issues"
#endif
    );
}

void MainWin::actAboutActivated()
{
    AboutDlg *about = new AboutDlg();
    about->show();
}

void MainWin::actAboutQtActivated() { QMessageBox::aboutQt(this); }

void MainWin::actDiagQCAPluginActivated()
{
    QString      dtext = QCA::pluginDiagnosticText();
    ShowTextDlg *w     = new ShowTextDlg(dtext, true, false, this);
    w->setWindowTitle(CAP(tr("Security Plugins Diagnostic Text")));
    w->resize(560, 240);
    w->show();
}

void MainWin::actDiagQCAKeyStoreActivated()
{
    QString      dtext = QCA::KeyStoreManager::diagnosticText();
    ShowTextDlg *w     = new ShowTextDlg(dtext, true, false, this);
    w->setWindowTitle(CAP(tr("Key Storage Diagnostic Text")));
    w->resize(560, 240);
    w->show();
}

void MainWin::actChooseStatusActivated()
{
    PsiOptions *       o = PsiOptions::instance();
    XMPP::Status::Type lastStatus
        = XMPP::Status::txt2type(PsiOptions::instance()->getOption("options.status.last-status").toString());
    StatusSetDlg *w = new StatusSetDlg(d->psi, makeLastStatus(lastStatus), lastPriorityNotEmpty());
    connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)), d->psi,
            SLOT(setGlobalStatus(const XMPP::Status &, bool, bool)));
    connect(w, SIGNAL(cancelled()), d->psi, SLOT(updateMainwinStatus()));
    if (o->getOption("options.ui.systemtray.enable").toBool())
        connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)),
                SLOT(setTrayToolTip(const XMPP::Status &, bool, bool)));
    w->show();
}

void MainWin::actReconnectActivated()
{
    for (PsiAccount *pa : d->psi->contactList()->accounts()) {
        pa->reconnectOnce();
    }
}

void MainWin::actPlaySoundsActivated(bool state)
{
    PsiOptions::instance()->setOption("options.ui.notifications.sounds.enable", state);
}

void MainWin::actPublishTuneActivated(bool state)
{
    PsiOptions::instance()->setOption("options.extended-presence.tune.publish", state);
}

void MainWin::actEnableGroupsActivated(bool state)
{
    PsiOptions::instance()->setOption("options.ui.contactlist.enable-groups", state);
}

void MainWin::actSetMoodActivated()
{
    QList<PsiAccount *> l;
    for (PsiAccount *pa : d->psi->contactList()->accounts()) {
        if (pa->isActive() && pa->serverInfoManager()->hasPEP() && !pa->accountOptions().ignore_global_actions)
            l.append(pa);
    }
    if (l.isEmpty())
        return;

    if (d->moodDlg)
        bringToFront(d->moodDlg);
    else {
        d->moodDlg = new MoodDlg(l);
        d->moodDlg->show();
    }
}

void MainWin::actSetActivityActivated()
{
    QList<PsiAccount *> l;
    for (PsiAccount *pa : d->psi->contactList()->accounts()) {
        if (pa->isActive() && pa->serverInfoManager()->hasPEP() && !pa->accountOptions().ignore_global_actions)
            l.append(pa);
    }
    if (l.isEmpty())
        return;

    if (d->activityDlg)
        bringToFront(d->activityDlg);
    else {
        d->activityDlg = new ActivityDlg(l);
        d->activityDlg->show();
    }
}

void MainWin::actSetGeolocActivated()
{
    QList<PsiAccount *> l;
    for (PsiAccount *pa : d->psi->contactList()->accounts()) {
        if (pa->isActive() && pa->serverInfoManager()->hasPEP() && !pa->accountOptions().ignore_global_actions)
            l.append(pa);
    }
    if (l.isEmpty())
        return;

    if (d->geolocationDlg)
        bringToFront(d->geolocationDlg);
    else {
        d->geolocationDlg = new GeoLocationDlg(l);
        d->geolocationDlg->show();
    }
}

void MainWin::actActiveContacts()
{
    ActiveContactsMenu *acm = new ActiveContactsMenu(d->psi, this);
    if (!acm->actions().isEmpty())
        acm->exec(QCursor::pos());
    delete acm;
}

void MainWin::activatedAccOption(PsiAccount *pa, int x)
{
    if (x == 0) {
        pa->openAddUserDlg();
    } else if (x == 2) {
        pa->showXmlConsole();
    } else if (x == 3) {
        pa->doDisco();
    }
}

void MainWin::buildTrayMenu()
{
    if (!d->trayMenu) {
        auto iconSize = int(fontInfo().pixelSize() * BiggerTextIconK + .5);

        d->trayMenu          = new QMenu(this);
        QAction *nextEvent   = d->trayMenu->addAction(tr("Receive next event"), this, SLOT(doRecvNextEvent()));
        QAction *hideRestore = d->trayMenu->addAction(tr("Hide"), this, SLOT(trayHideShow()));
        connect(d->trayMenu, &QMenu::aboutToShow, this, [this, nextEvent, hideRestore]() {
            nextEvent->setEnabled(d->nextAmount > 0);
            hideRestore->setText(isHidden() ? tr("Show") : tr("Hide"));
        });
        d->trayMenu->addSeparator();

        auto addWrappedAction = [&](const QString &actionName) {
            auto action = d->getAction(actionName);
            if (actionName == QLatin1String("separator") || !action->psiIcon() || !action->psiIcon()->isScalable()) {
                action->addTo(d->trayMenu);
                return;
            }

            // Workaround for some unknow bug. likely this one https://bugreports.qt.io/browse/QTBUG-63187
            // replaces possible svg icon with QPixmap
            auto actionCopy
                = new QAction(QIcon(action->icon().pixmap(iconSize, iconSize)), action->text(), d->trayMenu);
            actionCopy->setCheckable(action->isCheckable());
            actionCopy->setChecked(action->isChecked());
            connect(actionCopy, &QAction::triggered, action, &QAction::trigger);
            // connect(actionCopy, &QAction::toggled, action, &QAction::setChecked);
            connect(
                action, &QAction::changed, actionCopy,
                [action, actionCopy]() {
                    if (actionCopy->isCheckable())
                        actionCopy->setChecked(action->isChecked());
                },
                Qt::QueuedConnection);
            d->trayMenu->addAction(actionCopy);
        };

        const QStringList _actions = { "status_online", "status_chat",    "status_away", "status_xa",
                                       "status_dnd",    "status_offline", "separator",   "menu_options" };
        for (const QString &actionName : _actions) {
            addWrappedAction(actionName);
        }
#ifndef Q_OS_MAC
        d->trayMenu->addSeparator();
        addWrappedAction(QLatin1String("menu_quit"));
    }
#else
    }
    if (!d->dockMenu) {
        d->dockMenu = new QMenu(this);
        d->dockMenu->addActions(d->statusMenu->actions());
        d->trayMenu->addSeparator();
        d->getAction("menu_options")->addTo(d->trayMenu);
        d->dockMenu->setAsDockMenu();
    }
#endif
}

void MainWin::setTrayToolTip()
{
    if (!d->tray) {
        return;
    }
    QString s      = ApplicationInfo::name();
    QString str    = "<qt>";
    QString imgTag = "icon name";
    str += QString("<div style='white-space:pre'><b>%1</b></div>").arg(TextUtil::escape(ApplicationInfo::name()));
    QString Tip         = "";
    QString TipPlain    = "";
    QString Events      = "";
    QString EventsPlain = "";
    for (PsiAccount *pa : d->psi->contactList()->enabledAccounts()) {
        Status  stat   = pa->status();
        int     status = makeSTATUS(stat);
        QString istr   = "status/offline";
        if (status == STATUS_ONLINE)
            istr = "status/online";
        else if (status == STATUS_AWAY)
            istr = "status/away";
        else if (status == STATUS_XA)
            istr = "status/xa";
        else if (status == STATUS_DND)
            istr = "status/dnd";
        else if (status == STATUS_CHAT)
            istr = "status/chat";
        else if (status == STATUS_INVISIBLE)
            istr = "status/invisible";
        Tip += QString("<div style='white-space:pre'>") + QString("<%1=\"%2\"> ").arg(imgTag).arg(istr)
            + QString("<b>%1</b>").arg(TextUtil::escape(pa->name())) + "</div>";
        TipPlain += "\n" + pa->name() + " (" + stat.typeString() + ")";
        QString text = stat.status();
        if (!text.isEmpty()) {
            text = clipStatus(text, 40, 1);
            Tip += QString("<div style='white-space:pre'>%1: %2</div>")
                       .arg(tr("Status Message"))
                       .arg(TextUtil::escape(text));
        }

        PsiEvent::Ptr e;
        e = pa->eventQueue()->peekNext();
        if (e) {
            Jid     jid = e->jid();
            QString from;
            if (!jid.isEmpty()) {
                LiveRoster Roster = pa->client()->roster();
                while (!Roster.isEmpty()) {
                    LiveRosterItem item = Roster.takeFirst();
                    if (item.jid().compare(jid)) {
                        from = item.name();
                        break;
                    }
                }
                if (from.isEmpty()) {
                    from = jid.full();
                }
            }
            if (!from.isEmpty()) {
                Events += QString("<div style='white-space:pre'>%1</div>").arg(TextUtil::escape(from));
                EventsPlain += "\n" + from;
            }
        }
    }

    if (!Tip.isEmpty()) {
        str += QString("<div style='white-space:pre'><u><b>%1</b></u></div>")
                   .arg(TextUtil::escape(tr("Active accounts:")))
            + Tip;
        s += tr("\nActive accounts:") + TipPlain;
    }

    if (!Events.isEmpty()) {
        str += QString("<div style='white-space:pre'><u><b>%1</b></u></div>")
                   .arg(TextUtil::escape(tr("Incoming event(s) from:")))
            + Events;
        s += tr("\nIncoming event(s) from:") + EventsPlain;
    }

    str += "</qt>";

    d->ToolTipText = str;
    d->tray->setToolTip(s);
}

void MainWin::doTrayToolTip(QObject *, QPoint p) { PsiToolTip::showText(p, d->ToolTipText); }

void MainWin::decorateButton(int status)
{
    // update the 'change status' buttons
    QList<IconAction *> l = d->statusGroup->findChildren<IconAction *>();
    for (IconAction *action : l) {
        action->setChecked(d->statusActions[action] == status);
    }

    setTrayToolTip();
    d->lastStatus = status;

    if (status == -1) {
        d->statusButton->setText(tr("Connecting"));
        if (PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString() != "no") {
            d->statusButton->setAlert(IconsetFactory::iconPtr("psi/connect"));
            d->statusSmallerAlt->setPsiIcon(IconsetFactory::iconPtr("psi/connect"));
        } else {
            d->statusButton->setIcon(PsiIconset::instance()->statusPtr(STATUS_OFFLINE));
            d->statusSmallerAlt->setPsiIcon(PsiIconset::instance()->statusPtr(STATUS_OFFLINE));
            d->rosterAvatar->setStatusIcon(PsiIconset::instance()->statusPtr(STATUS_OFFLINE)->icon());
        }

#ifdef Q_OS_LINUX
        d->statusMenuMB->statusChanged(makeStatus(STATUS_OFFLINE, ""));
#endif
        d->statusMenu->statusChanged(makeStatus(STATUS_OFFLINE, ""));

        setWindowIcon(PsiIconset::instance()->status(STATUS_OFFLINE).icon());
    } else {
        d->statusButton->setText(status2txt(status));
        d->statusButton->setIcon(PsiIconset::instance()->statusPtr(status));
        d->statusSmallerAlt->setPsiIcon(PsiIconset::instance()->statusPtr(status));
        d->rosterAvatar->setStatusIcon(PsiIconset::instance()->statusPtr(status)->icon());
#ifdef Q_OS_LINUX
        d->statusMenuMB->statusChanged(makeStatus(status, d->psi->currentStatusMessage()));
#endif
        d->statusMenu->statusChanged(makeStatus(status, d->psi->currentStatusMessage()));
        setWindowIcon(PsiIconset::instance()->status(status).icon());
    }

    updateTray();
}

bool MainWin::askQuit() { return true; }

void MainWin::try2tryCloseProgram() { QTimer::singleShot(0, this, SLOT(tryCloseProgram())); }

void MainWin::tryCloseProgram()
{
    if (askQuit()) {
        closeProgram();
    }
}

void MainWin::closeEvent(QCloseEvent *e)
{
#ifdef Q_OS_MAC
    trayHide();
    e->accept();
#else
    PsiOptions *o           = PsiOptions::instance();
    bool        quitOnClose = o->getOption("options.ui.contactlist.quit-on-close").toBool()
        && o->getOption("options.contactlist.autohide-interval").toInt() == 0;

    if (d->tray && !quitOnClose) {
        trayHide();
        e->ignore();
        return;
    }

    if (!askQuit()) {
        e->ignore();
        return;
    }

    closeProgram();

    e->accept();
#endif
}

void MainWin::changeEvent(QEvent *event)
{
#ifdef Q_OS_WIN
    if (event->type() == QEvent::ActivationChange
        && PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool()
        && PsiOptions::instance()->getOption("options.ui.contactlist.raise-inactive").toBool()) {
        // On Windows app window loose active state when you
        //  click on tray icon. Workaround is to use timer:
        //  we'll keep activated == true within 300 msec
        //  (+ doubleClickInterval, if double click is enabled)
        //  after deactivation.
        if (!isActiveWindow()) {
            d->deactivationTickCount = GetTickCount();
        }
    }
#endif

    if (event->type() == QEvent::ActivationChange || event->type() == QEvent::WindowStateChange) {
        if (d->mainTabs) {
            QCoreApplication::sendEvent(d->mainTabs, event);
        }
    }
}

void MainWin::keyPressEvent(QKeyEvent *e)
{
#ifdef Q_OS_MAC
    bool allowed = true;
#else
    bool allowed = d->tray != nullptr;
#endif

    bool closekey = false;
    if (e->key() == Qt::Key_Escape) {
        closekey = true;
    }
#ifdef Q_OS_MAC
    else if (e->key() == Qt::Key_W && e->modifiers() & Qt::ControlModifier) {
        closekey = true;
    }
#endif

    if (allowed && closekey) {
        close();
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}

void MainWin::enterEvent(QEvent *e)
{
    if (d->hideTimer)
        d->hideTimer->stop();
    QMainWindow::enterEvent(e);
}

void MainWin::leaveEvent(QEvent *e)
{
    if (d->hideTimer)
        d->hideTimer->start();
    QMainWindow::leaveEvent(e);
}

bool MainWin::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::KeyPress && o->isWidgetType() && isAncestorOf(qobject_cast<QWidget *>(o))) {
        if (d->hideTimer && d->hideTimer->isActive())
            d->hideTimer->start();
    }
    return false;
}

#ifdef Q_OS_WIN
bool MainWin::nativeEvent(const QByteArray &eventType, MSG *msg, long *result)
{
    Q_UNUSED(eventType);
    if (d->asTool && msg->message == WM_SYSCOMMAND && msg->wParam == SC_MINIMIZE) {
        hide(); // minimized toolwindows look bad on Windows, so let's just hide it instead
                // plus we cannot do this in changeEvent(), because it's called too late
        *result = 0;
        return true; // don't let Qt process this event
    }
    return false;
}
#endif

void MainWin::updateCaption()
{
    QString str = "";

    if (d->nextAmount > 0) {
        str += "* ";
    }

    if (d->nickname.isEmpty()) {
        str += ApplicationInfo::name();
    } else {
        str += d->nickname;
    }

    if (str == windowTitle()) {
        return;
    }

    setWindowTitle(str);
}

void MainWin::optionsUpdate()
{
    int status    = d->lastStatus;
    d->lastStatus = -2;
    decorateButton(status);

#ifndef Q_OS_MAC
    if (!PsiOptions::instance()->getOption("options.ui.contactlist.show-menubar").toBool()) {
        mainMenuBar()->hide();
    } else {
        mainMenuBar()->show();
    }
#endif

    setWindowOpacity(
        double(qMax(MINIMUM_OPACITY, PsiOptions::instance()->getOption("options.ui.contactlist.opacity").toInt()))
        / 100);

    buildStatusMenu(d->statusMenu);
#ifdef Q_OS_LINUX
    buildStatusMenu(d->statusMenuMB);
#endif
    updateTray();
}

void MainWin::toggleVisible(bool fromTray)
{
    if (PsiOptions::instance()->getOption("options.ui.contactlist.raise-inactive").toBool()) {
        bool hidden = false;
#ifdef Q_OS_WIN
        if (fromTray) {
            unsigned timeout = 300;
            if (PsiOptions::instance()->getOption("options.ui.systemtray.use-double-click").toBool())
                timeout += qApp->doubleClickInterval();
            hidden = isHidden() || (GetTickCount() - d->deactivationTickCount > timeout);
        } else {
            hidden = isHidden() || !isActiveWindow();
        }
#elif defined(HAVE_X11)
        Q_UNUSED(fromTray);
        if (QX11Info::isPlatformX11()) {
            hidden = isHidden()
                || X11WindowSystem::instance()->isWindowObscured(
                    this, PsiOptions::instance()->getOption("options.ui.contactlist.always-on-top").toBool());
        } else {
            hidden = isHidden() || !isActiveWindow();
        }
#else
            Q_UNUSED(fromTray);
            hidden = isHidden() || !isActiveWindow();
#endif
        hidden ? trayShow() : trayHide();
    } else
        trayHideShow();
}

void MainWin::setTrayToolTip(const Status &status, bool, bool)
{
    Q_UNUSED(status)

    if (!d->tray) {
        return;
    }
    setTrayToolTip();
}

void MainWin::trayClicked(const QPoint &, int button)
{
#ifdef Q_OS_WIN
    if (PsiOptions::instance()->getOption("options.ui.systemtray.use-double-click").toBool()) {
        return;
    }
#endif

    if (button == Qt::MidButton) {
        doRecvNextEvent();
        return;
    }

    // if we widget is obscured by other widgets then hiding is not that expected
    // some interesting info about the problem
    // http://www.qtcentre.org/threads/56573-Get-the-Window-Visible-State-when-it-is-partially-visible
    // But currently now really good cross-platform solution.
    toggleVisible(true);
}

void MainWin::trayDoubleClicked()
{
#ifdef Q_OS_WIN
    // Double click works like second single click now if "double-click" style is disabled

    if (PsiOptions::instance()->getOption("options.ui.systemtray.use-double-click").toBool()) {
        if (d->nextAmount > 0) {
            doRecvNextEvent();
            return;
        }
    }
    toggleVisible(true);
#endif
}

void MainWin::trayShow()
{
    bringToFront(this);
    if (d->hideTimer)
        d->hideTimer->start();
}

void MainWin::trayHide() { hide(); }

void MainWin::trayHideShow()
{
    if (isHidden()) {
        trayShow();
    } else {
        trayHide();
    }
}

void MainWin::updateReadNext(PsiIcon *anim, int amount)
{
    d->nextAnim = anim;
    if (anim == nullptr) {
        d->nextAmount = 0;
    } else {
        d->nextAmount = amount;
    }

    if (d->nextAmount <= 0) {
        d->eventNotifier->hide();
        d->eventNotifier->setMessage("");
    } else {
        d->eventNotifier->setMessage(QString("<b>") + numEventsString(d->nextAmount) + "</b>");
        d->eventNotifier->show();
        // make sure it shows
        // qApp->processEvents();
    }

    updateTray();
    updateCaption();
    setTrayToolTip();
}

QString MainWin::numEventsString(int x) const
{
    QString s;
    if (x <= 0) {
        s = "";
    } else if (x == 1) {
        s = tr("1 event received");
    } else {
        s = tr("%1 events received").arg(x);
    }

    return s;
}

void MainWin::updateTray()
{
    if (!d->tray) {
        return;
    }

    if (d->nextAmount > 0) {
        d->tray->setAlert(d->nextAnim);
    } else if (d->lastStatus == -1) {
        d->tray->setAlert(IconsetFactory::iconPtr("psi/connect"));
    } else {
        d->tray->setIcon(PsiIconset::instance()->statusPtr(d->lastStatus));
    }

    buildTrayMenu();
    d->tray->setContextMenu(d->trayMenu);
}

void MainWin::doRecvNextEvent() { recvNextEvent(); }

void MainWin::statusClicked(int x)
{
    if (x == Qt::MidButton) {
        recvNextEvent();
    }
}

PsiTrayIcon *MainWin::psiTrayIcon() { return d->tray; }

void MainWin::numAccountsChanged()
{
    d->statusButton->setEnabled(d->psi->contactList()->haveEnabledAccounts());
    setTrayToolTip();
    PsiAccount *acc = d->psi->contactList()->defaultAccount();
    if (acc && acc != d->defaultAccount) {
        if (d->defaultAccount) {
            disconnect(d->defaultAccount, &PsiAccount::nickChanged, this, &MainWin::nickChanged);
            //            disconnect(d->defaultAccount->avatarFactory(), SIGNAL(avatarChanged(Jid)), this,
            //            SLOT(avatarChanged()));
        }
        d->defaultAccount = acc;
        avatarChanged(acc->jid());
        nickChanged();
        d->rosterAvatar->setStatusMessage(acc->status().status());
        connect(acc->avatarFactory(), &AvatarFactory::avatarChanged, this, &MainWin::avatarChanged);
        connect(acc, &PsiAccount::nickChanged, this, &MainWin::nickChanged);
    }
    if (!acc) { // no accounts left
        avatarChanged(Jid());
    }
}

void MainWin::nickChanged()
{
    if (d->defaultAccount)
        d->rosterAvatar->setNick(d->defaultAccount->nick());
}

void MainWin::avatarChanged(const Jid &jid)
{
    // we come here all the contacts
    if (d->defaultAccount && d->defaultAccount->jid() != jid)
        return; // we don't care about other contacts

    QPixmap pix;
    if (d->defaultAccount && d->defaultAccount->jid() == jid) {
        pix = d->defaultAccount->avatarFactory()->getAvatar(d->defaultAccount->jid());
    }
    if (pix.isNull()) {
        int avSize
            = PsiOptions::instance()->getOption("options.ui.contactlist.roster-avatar-frame.avatar.size").toInt();
        pix = IconsetFactory::iconPixmap("psi/default_avatar", avSize);
    }
    d->rosterAvatar->setAvatar(pix);
}

void MainWin::accountFeaturesChanged()
{
    bool have_pep = false;
    for (PsiAccount *account : d->psi->contactList()->enabledAccounts()) {
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
    if (isHidden()) {
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

    if (d->filterActive) {
        d->getAction("show_offline")->setChecked(d->prefilterShowOffline);
        d->getAction("show_away")->setChecked(d->prefilterShowAway);
    }
    d->filterActive = false;
}

/**
 * Called when the contactview has a keypress.
 * Starts the search/filter process
 */
void MainWin::searchTextStarted(QString const &text)
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
void MainWin::searchTextEntered(QString const &text)
{
    if (!d->filterActive) {
        d->filterActive         = true;
        d->prefilterShowOffline = d->getAction("show_offline")->isChecked();
        d->prefilterShowAway    = d->getAction("show_away")->isChecked();
        d->getAction("show_offline")->setChecked(true);
        d->getAction("show_away")->setChecked(true);
    }
    if (text.isEmpty()) {
        searchClearClicked();
    }
}

#ifdef Q_OS_MAC
void MainWin::setWindowIcon(const QIcon &) { }
#else
void MainWin::setWindowIcon(const QIcon &p) { QMainWindow::setWindowIcon(p); }
#endif

#if 0
#if defined(Q_OS_WIN)
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
        while ( it ) {                // show all widget children
            object = it.current();        //   (except popups and other toplevels)
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
    if (d->hideTimer)
        d->hideTimer->start();
}

void MainWin::resizeEvent(QResizeEvent *e)
{
    AdvancedWidget<QMainWindow>::resizeEvent(e);

    if (d->splitter && isVisible()) {
        QList<int> sizes       = d->splitter->sizes();
        int        tabsWidth   = !d->isLeftRoster ? sizes.first() : sizes.last();
        int        rosterWidth = d->isLeftRoster ? sizes.first() : sizes.last();
        int        dw          = rosterWidth - d->rosterSize;
        sizes.clear();
        sizes.append(tabsWidth + dw);
        if (d->isLeftRoster) {
            sizes.prepend(d->rosterSize);
        } else {
            sizes.append(d->rosterSize);
        }
        d->splitter->setSizes(sizes);
        d->tabsSize = tabsWidth + dw;
    }
}

//#endif

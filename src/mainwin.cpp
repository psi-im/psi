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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "mainwin_b.h"

#include <QDesktopServices>
#include <qmessagebox.h>
#include <qicon.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qobject.h>
#include <qpainter.h>
#include <qsignalmapper.h>
#include <qmenubar.h>
#include <QPixmap>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QEvent>
#include <QVBoxLayout>
#include <QMenu>
#include <QMenuItem>
#include <QImage>

#ifdef Q_WS_WIN
#include <windows.h>
#endif

#include "common.h"
#include "showtextdlg.h"
#include "psicon.h"
#include "contactview_b.h"
#include "psiiconset.h"
#include "serverinfomanager.h"
#include "applicationinfo.h"
#include "psiaccount_b.h"
#include "psitrayicon.h"
#include "psitoolbar.h"
#include "aboutdlg.h"
#include "psitoolbar.h"
#include "psipopup.h"
#include "psioptions.h"
#include "tipdlg.h"
#include "mucjoindlg.h"
#include "psicontactlist.h"
#include "avatars.h"
#include "xmpp_vcard.h"
#include "vcardfactory.h"
//#include "transportwizard.h"
#include "transportsetupdlg.h"
#include "profiles.h"
#include "userlist.h"
#include "psievent.h"

#include "mainwin_p_b.h"

#ifdef QUICKVOIP
#include "quickvoip/jinglertp.h"
#endif

using namespace XMPP;

QImage makeAvatarImage(const QImage &_in)
{
	QImage out(50, 50, QImage::Format_ARGB32);
	QImage in;
	if(!_in.isNull())
		in = _in.convertToFormat(QImage::Format_ARGB32).scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);

	out.fill(qRgba(255, 255, 255, 255));
	for(int ny = 1; ny < out.height() - 1; ++ny)
	{
		for(int nx = 1; nx < out.width() - 1; ++nx)
			out.setPixel(nx, ny, qRgba(0, 0, 0, 255));
	}

	if(in.isNull())
		in = cuda_defaultAvatar();

	if(!in.isNull())
	{
		for(int ny = 1; ny < out.height() - 1; ++ny)
		{
			for(int nx = 1; nx < out.width() - 1; ++nx)
			{
				out.setPixel(nx, ny, in.pixel(nx - 1, ny - 1));
			}
		}
	}

	return out;
}

class PopupActionButton : public QPushButton
{
	Q_OBJECT
public:
	PopupActionButton(QWidget *parent = 0, const char *name = 0);
	~PopupActionButton();

	void setIcon(PsiIcon *, bool showText);
	void setLabel(QString);
	QSize sizeHint() const;

private slots:
	void pixmapUpdated();

private:
	void update();
	void paintEvent(QPaintEvent *);
	bool hasToolTip;
	PsiIcon *icon;
	bool showText;
	QString label;
};

//----------------------------------------------------------------------------
// ClickableLabel
//----------------------------------------------------------------------------
ClickableLabel::ClickableLabel(QWidget *parent)
:QLabel(parent)
{
}

ClickableLabel::~ClickableLabel()
{
}

void ClickableLabel::mousePressEvent(QMouseEvent *event)
{
	event->accept();
}

void ClickableLabel::mouseReleaseEvent(QMouseEvent *event)
{
	event->accept();

	int w = width();
	int h = height();
	int x = event->x();
	int y = event->y();
	if(x >= 0 && x < w && y >= 0 && y < h)
		emit clicked();
}

//----------------------------------------------------------------------------
// SearchLineEdit
//----------------------------------------------------------------------------
SearchLineEdit::SearchLineEdit(QWidget *parent)
:CudaLineEdit(parent)
{
	cuda_applyTheme(this);

	connect(this, SIGNAL(textEdited(const QString &)), SLOT(le_textEdited(const QString &)));
	defaultMode = true;
	QPalette pal = QPalette();
	pal.setColor(QPalette::Text, get_frameCol1());
	setPalette(pal);
	setText("Search contacts/address book");
}

QString SearchLineEdit::search() const
{
	if(defaultMode)
		return QString();
	else
		return text();
}

void SearchLineEdit::clearSearch()
{
	defaultMode = true;
	QPalette pal = palette();
	pal.setColor(QPalette::Text, get_frameCol1());
	setPalette(pal);
	setText("Search contacts/address book");
}

void SearchLineEdit::focusInEvent(QFocusEvent *event)
{
	QLineEdit::focusInEvent(event);

	if(defaultMode) {
		defaultMode = false;
		setText("");
		QPalette pal = palette();
		pal.setColor(QPalette::Text, Qt::black);
		setPalette(pal);
	}
}

void SearchLineEdit::focusOutEvent(QFocusEvent *event)
{
	QLineEdit::focusOutEvent(event);

	if(text().isEmpty()) {
		defaultMode = true;
		QPalette pal = palette();
		pal.setColor(QPalette::Text, get_frameCol1());
		setPalette(pal);
		setText("Search contacts/address book");

		// null string = remove the 'x' button
		emit searchChanged(QString());
	}
}

void SearchLineEdit::le_textEdited(const QString &text)
{
	emit searchChanged(text);
}

//----------------------------------------------------------------------------
// MainWinB::Private
//----------------------------------------------------------------------------

class MainWinB::Private
{
public:
	Private(PsiCon *, MainWinB *);
	~Private();

	QVBoxLayout *vb_main;
	bool onTop, asTool;
	QMenu *mainMenu, *statusMenu, *optionsMenu, *toolsMenu;
	int sbState;
	QString nickname;
	PsiTrayIcon *tray;
	QMenu *trayMenu;
	QString statusTip;

	PopupAction *optionsButton, *statusButton;
	IconActionGroup *statusGroup;
	EventNotifierAction *eventNotifier;
	PsiCon *psi;
	MainWinB *mainWin;

	QSignalMapper *statusMapper;

	PsiIcon *nextAnim;
	int nextAmount;

	QMap<QAction *, int> statusActions;

	int lastStatus;
	bool old_trayicon;
	bool filterActive, prefilterShowOffline, prefilterShowAway;
	QStringList prefilterClosedGroups;

	void registerActions();
	IconAction *getAction( QString name );
	void updateMenu(QStringList actions, QMenu *menu);

	ClickableLabel *lb_avatar;
	QLabel *lb_name;
	PopupActionButton *pb_status;
	SearchLineEdit *le_search;
	QPushButton *pb_view, *pb_searchcancel;
	SimpleSearch *serv_search;
	QTimer searchTimeout;

	void savePreSearchState()
	{
		prefilterShowOffline = getAction("show_offline")->isChecked();
		prefilterShowAway = getAction("show_away")->isChecked();
		getAction("show_offline")->setChecked(true);
		getAction("show_away")->setChecked(true);

		prefilterClosedGroups.clear();
		const QMap<QString, UserAccount::GroupData> &groupState = psi->contactList()->defaultAccount()->userAccount().groupState;
		QMapIterator<QString, UserAccount::GroupData> it(groupState);
		while(it.hasNext())
		{
			it.next();
			if(!it.value().open && !prefilterClosedGroups.contains(it.key()))
				prefilterClosedGroups += it.key();
		}
	}

	void restorePreSearchState()
	{
		getAction("show_offline")->setChecked(prefilterShowOffline);
		getAction("show_away")->setChecked(prefilterShowAway);

		for(ContactViewItemB *i = (ContactViewItemB *)mainWin->cvlist->firstChild(); i; i = (ContactViewItemB *)i->nextSibling())
		{
			if(i->type() == ContactViewItemB::Group && prefilterClosedGroups.contains(i->groupName()) && i->isOpen())
			{
				i->setOpen(false);
			}
		}
	}
};

MainWinB::Private::Private(PsiCon *_psi, MainWinB *_mainWin)
{
	psi = _psi;
	mainWin = _mainWin;

	statusGroup   = (IconActionGroup *)getAction("status_all");
	eventNotifier = (EventNotifierAction *)getAction("event_notifier");

	optionsButton = (PopupAction *)getAction("button_options");
	statusButton  = (PopupAction *)getAction("button_status");

	statusMapper = new QSignalMapper(mainWin, "statusMapper");
	mainWin->connect(statusMapper, SIGNAL(mapped(int)), mainWin, SLOT(activatedStatusAction(int)));

	filterActive = false;
	prefilterShowOffline = false;
	prefilterShowAway = false;
}

MainWinB::Private::~Private()
{
}

void MainWinB::Private::registerActions()
{
	struct {
		const char *name;
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
		IconAction *action = getAction( aName );
		connect (action, SIGNAL(activated()), statusMapper, SLOT(map()));

		statusMapper->setMapping(action, statuslist[i].id);
		statusActions[action] = statuslist[i].id;
	}

	// register all actions
	PsiActionList::ActionsType type = PsiActionList::ActionsType( PsiActionList::Actions_MainWin | PsiActionList::Actions_Common );
	ActionList actions = psi->actionList()->suitableActions( type );
	QStringList names = actions.actions();
	QStringList::Iterator it = names.begin();
	for ( ; it != names.end(); ++it ) {
		IconAction *action = actions.action( *it );
		if ( action )
			mainWin->registerAction( action );
	}
}

IconAction *MainWinB::Private::getAction( QString name )
{
	PsiActionList::ActionsType type = PsiActionList::ActionsType( PsiActionList::Actions_MainWin | PsiActionList::Actions_Common );
	ActionList actions = psi->actionList()->suitableActions( type );
	IconAction *action = actions.action( name );

	if ( !action )
		qWarning("MainWinB::Private::getAction(): action %s not found!", name.latin1());
	//else
	//	mainWin->registerAction( action );

	return action;
}

void MainWinB::Private::updateMenu(QStringList actions, QMenu *menu)
{
	menu->clear();

	IconAction *action;
	foreach (QString name, actions) {
		// workind around Qt/X11 bug, which displays
		// actions's text and the separator bar in Qt 4.1.1
		if ( name == "separator" ) {
			menu->insertSeparator();
			continue;
		}

		if ( name == "cuda_transports") {
			/*QMenu *tmenu = menu->addMenu("&Public IM Accounts");
			getAction("reg_aim")->addTo(tmenu);
			getAction("reg_msn")->addTo(tmenu);
			getAction("reg_yahoo")->addTo(tmenu);
			getAction("reg_icq")->addTo(tmenu);*/
			getAction("reg_trans")->addTo(menu);
		}

		if ( (action = getAction(name)) )
			action->addTo(menu);
	}
}

//----------------------------------------------------------------------------
// MainWinB
//----------------------------------------------------------------------------

//#ifdef Q_WS_X11
//#define TOOLW_FLAGS WStyle_Customize
//#else
//#define TOOLW_FLAGS ((Qt::WFlags) 0)
//#endif

#ifdef Q_WS_WIN
#define TOOLW_FLAGS (Qt::WindowMinimizeButtonHint)
#else
#define TOOLW_FLAGS ((Qt::WFlags) 0)
#endif

MainWinB::MainWinB(bool _onTop, bool _asTool, PsiCon *psi, const char *name)
:AdvancedWidget<CudaFrame>(0, (_onTop ? Qt::WStyle_StaysOnTop : Qt::Widget) | (_asTool ? (Qt::WStyle_Tool |TOOLW_FLAGS) : Qt::Widget))
//: QMainWindow(0,name,(_onTop ? Qt::WStyle_StaysOnTop : Qt::Widget) | (_asTool ? (Qt::WStyle_Tool |TOOLW_FLAGS) : Qt::Widget))
{
	{
		QVBoxLayout *vb = new QVBoxLayout(this);
		vb->setMargin(0);
		mainWindow = new QMainWindow(this, 0);
		Qt::WindowFlags wflags = mainWindow->windowFlags();
		wflags &= ~Qt::Window;
		wflags |= Qt::Widget;
		mainWindow->setWindowFlags(wflags);
		mainWindow->show();
		vb->addWidget(mainWindow);
#ifndef Q_OS_MAC
		mainWindow->menuBar()->setAutoFillBackground(false);
		mainWindow->menuBar()->setFixedHeight(24);
		cuda_applyTheme(mainWindow->menuBar());
#endif
	}

	setObjectName(name);
	setAttribute(Qt::WA_AlwaysShowToolTips);
	if ( PsiOptions::instance()->getOption("options.ui.mac.use-brushed-metal-windows").toBool() ) {
		setAttribute(Qt::WA_MacMetalStyle);
	}
	d = new Private(psi, this);

	//setWindowIcon(PsiIconset::instance()->status(STATUS_OFFLINE).impix());
	setWindowIcon(IconsetFactory::icon("psi/logo_16").pixmap());

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
#ifdef Q_WS_MAC
	d->old_trayicon = false;
#else
	d->old_trayicon = PsiOptions::instance()->getOption("options.ui.systemtray.use-old").toBool();
#endif

	d->serv_search = 0;

	QWidget *center = new QWidget (mainWindow, "Central widget");
	mainWindow->setCentralWidget ( center );

	d->vb_main = new QVBoxLayout(center);

	CudaSubFrame *topFrame = new CudaSubFrame(center);
	QHBoxLayout *hb_tf = new QHBoxLayout(topFrame);
	hb_tf->setSpacing(12);
	QVBoxLayout *vb_tf = new QVBoxLayout;
	hb_tf->addLayout(vb_tf);
	d->lb_avatar = new ClickableLabel(topFrame);
	d->lb_avatar->setAlignment(Qt::AlignTop);
	connect(d->lb_avatar, SIGNAL(clicked()), SLOT(avatar_clicked()));
	vb_tf->addWidget(d->lb_avatar);

	vb_tf = new QVBoxLayout;
	hb_tf->addLayout(vb_tf);
	d->lb_name = new QLabel(topFrame);
	cuda_applyTheme(d->lb_name);
	vb_tf->addWidget(d->lb_name);
	hb_tf = new QHBoxLayout;
	vb_tf->addLayout(hb_tf);
	d->pb_status = new PopupActionButton(topFrame);
	d->pb_status->setLabel(status2txt(STATUS_OFFLINE));
	//d->pb_status->setIcon(PsiIconset::instance()->statusPtr(STATUS_OFFLINE), true);
	cuda_applyTheme(d->pb_status);
	d->pb_status->setFixedHeight(20);
	d->pb_status->setMinimumWidth(120);
	hb_tf->addWidget(d->pb_status);
	hb_tf->addStretch(1);

	d->vb_main->addWidget(topFrame);

	CudaSubFrame *botFrame = new CudaSubFrame(center);
	QVBoxLayout *vb_bf = new QVBoxLayout(botFrame);

	QHBoxLayout *search_layout = new QHBoxLayout;
	vb_bf->addLayout(search_layout);
	d->le_search = new SearchLineEdit(botFrame);
	d->le_search->setFixedHeight(20);
	connect(d->le_search, SIGNAL(searchChanged(const QString &)), SLOT(searchTextChanged(const QString &)));
	search_layout->addWidget(d->le_search);
	d->pb_searchcancel = new QPushButton("X", botFrame);
	connect(d->pb_searchcancel, SIGNAL(clicked()), SLOT(searchCancel()));
	cuda_applyTheme(d->pb_searchcancel);
	d->pb_searchcancel->setFixedWidth(20);
	d->pb_searchcancel->setFixedHeight(20);
	search_layout->addWidget(d->pb_searchcancel);
	d->pb_searchcancel->hide();

	connect(&d->searchTimeout, SIGNAL(timeout()), SLOT(searchCancel()));
	d->searchTimeout.setInterval(1000 * 60 * 30); // 30 mins
	d->searchTimeout.setSingleShot(true);

	QHBoxLayout *hb_bf = new QHBoxLayout;
	vb_bf->addLayout(hb_bf);

	d->pb_view = new QPushButton("&View", botFrame);
	cuda_applyTheme(d->pb_view);
	d->pb_view->setFixedHeight(20);
	hb_bf->addWidget(d->pb_view);
	hb_bf->addStretch(1);
	QPushButton *pb = new QPushButton("&Add Contact", botFrame);
	connect(pb, SIGNAL(clicked()), SLOT(addUser()));
	cuda_applyTheme(pb);
	pb->setFixedHeight(20);
	hb_bf->addWidget(pb);

	cvlist = new ContactView(botFrame);
	cuda_applyTheme(cvlist);
	vb_bf->addWidget(cvlist);
	d->eventNotifier->addTo(botFrame);

	int layoutMargin = 2;
#ifdef Q_WS_MAC
	layoutMargin = 0;
	cvlist->setFrameShape(QFrame::NoFrame);
#endif
	//d->vb_main->setMargin(layoutMargin);
	d->vb_main->setMargin(0);
	d->vb_main->setSpacing(layoutMargin);

	d->vb_main->addWidget(botFrame);
	//d->vb_main->addWidget(cvlist);

#ifdef Q_WS_MAC
	// Disable the empty vertical scrollbar:
	// it's here because of weird code in q3scrollview.cpp
	// Q3ScrollView::updateScrollBars() around line 877
	d->vb_main->addSpacing(4);
#endif

	d->statusMenu = new QMenu(this);
	d->optionsMenu = new QMenu(this);
#ifdef Q_WS_MAC
	d->trayMenu = d->statusMenu;
#else
	d->trayMenu = new QMenu(mainWindow);
	buildTrayMenu();
	connect(d->trayMenu, SIGNAL(aboutToShow()), SLOT(buildTrayMenu()));
#endif


	buildStatusMenu();
	buildOptionsMenu();
	connect(d->optionsMenu, SIGNAL(aboutToShow()), SLOT(buildOptionsMenu()));


	X11WM_CLASS("main");

	//connect(VCardFactory::instance(), SIGNAL(vcardChanged(const Jid &)), SLOT(vcardChanged(const Jid &)));

	connect(d->psi, SIGNAL(accountCountChanged()), SLOT(numAccountsChanged()));
	numAccountsChanged();

	updateCaption();

	d->registerActions();
	
	connect(d->psi->contactList(), SIGNAL(accountFeaturesChanged()), SLOT(accountFeaturesChanged()));
	accountFeaturesChanged();

	decorateButton(STATUS_OFFLINE);

	// Mac-only menus
#ifdef Q_WS_MAC
	QMenu *mainMenu = new QMenu(mainWindow);
	mainMenuBar()->insertItem(tr("Menu"), mainMenu);
	mainMenu->insertItem(tr("Preferences"), this, SIGNAL(doOptions()));
	mainMenu->insertItem(tr("Quit"), this, SLOT(try2tryCloseProgram()));
	d->getAction("help_about")->addTo(mainMenu);
	//d->getAction("help_about_qt")->addTo(mainMenu);

	d->mainMenu = new QMenu(mainWindow);
	mainMenuBar()->insertItem(tr("File"), d->mainMenu);
	connect(d->mainMenu, SIGNAL(aboutToShow()), SLOT(buildMainMenu()));
#else
	mainMenuBar()->insertItem(tr("File"), d->optionsMenu);
#endif

	//mainMenuBar()->insertItem(tr("Status"), d->statusMenu);

	QMenu *actionsMenu = new QMenu(mainWindow);
	mainMenuBar()->insertItem(tr("Actions"), actionsMenu);
	d->getAction("menu_new_message")->addTo(actionsMenu);
	d->getAction("menu_join_groupchat")->addTo(actionsMenu);

	QMenu *settingsMenu = new QMenu(mainWindow);
	mainMenuBar()->insertItem(tr("Settings"), settingsMenu);
	d->getAction("menu_account_modify")->addTo(settingsMenu);
	d->getAction("link_password")->addTo(settingsMenu);
	d->getAction("menu_edit_vcard")->addTo(settingsMenu);
	/*QMenu *tmenu = settingsMenu->addMenu("&Public IM Accounts");
	d->getAction("reg_aim")->addTo(tmenu);
	d->getAction("reg_msn")->addTo(tmenu);
	d->getAction("reg_yahoo")->addTo(tmenu);
	d->getAction("reg_icq")->addTo(tmenu);*/
	d->getAction("reg_trans")->addTo(settingsMenu);
	settingsMenu->insertSeparator();
	d->getAction("menu_options")->addTo(settingsMenu);
	d->getAction("menu_play_sounds")->addTo(settingsMenu);

	QMenu *helpMenu = new QMenu(mainWindow);
	mainMenuBar()->insertItem(tr("Help"), helpMenu);
	d->getAction("help_about")->addTo (helpMenu);

	/*QMenu *viewMenu = new QMenu(mainWindow);
	mainMenuBar()->insertItem(tr("View"), viewMenu);
	d->getAction("show_offline")->addTo(viewMenu);
	if (PsiOptions::instance()->getOption("options.ui.menu.view.show-away").toBool())
		d->getAction("show_away")->addTo(viewMenu);
	d->getAction("show_hidden")->addTo(viewMenu);
	d->getAction("show_agents")->addTo(viewMenu);
	d->getAction("show_self")->addTo(viewMenu);
	viewMenu->insertSeparator();
	d->getAction("show_statusmsg")->addTo(viewMenu);*/

	// Mac-only menus
#ifdef Q_WS_MAC
	/*d->toolsMenu = new QMenu(mainWindow);
	mainMenuBar()->insertItem(tr("Tools"), d->toolsMenu);
	connect(d->toolsMenu, SIGNAL(aboutToShow()), SLOT(buildToolsMenu()));*/

	//QMenu *helpMenu = new QMenu(mainWindow);
	//mainMenuBar()->insertItem(tr("Help"), helpMenu);
	//d->getAction("help_readme")->addTo (helpMenu);
	//d->getAction("help_tip")->addTo (helpMenu);
	//helpMenu->insertSeparator();
	//d->getAction("help_online_help")->addTo (helpMenu);
	//d->getAction("help_online_wiki")->addTo (helpMenu);
	//d->getAction("help_online_home")->addTo (helpMenu);
	//d->getAction("help_psi_muc")->addTo (helpMenu);
	//d->getAction("help_report_bug")->addTo (helpMenu);
#else
	if (!PsiOptions::instance()->getOption("options.ui.contactlist.show-menubar").toBool())
		mainMenuBar()->hide();
	//else 
	//	mainMenuBar()->show();
#endif

	// view menu button thing
	QMenu *viewMenu = new QMenu(mainWindow);
	d->getAction("show_offline")->addTo(viewMenu);
	//if (PsiOptions::instance()->getOption("options.ui.menu.view.show-away").toBool())
	d->getAction("show_away")->addTo(viewMenu);
	d->getAction("show_hidden")->addTo(viewMenu);
	d->getAction("show_agents")->addTo(viewMenu);
	d->getAction("show_self")->addTo(viewMenu);
	viewMenu->insertSeparator();
	d->getAction("show_statusmsg")->addTo(viewMenu);
	d->pb_view->setMenu(viewMenu);

	QMenu *oldMenu = d->statusMenu;
	d->statusMenu = new QMenu(mainWindow);
	buildStatusMenu();
	d->pb_status->setMenu(d->statusMenu);
	d->statusMenu = oldMenu;

	/*d->optionsButton->setMenu( d->optionsMenu );
	d->statusButton->setMenu( d->statusMenu );
	
	buildToolbars();*/

	setWindowOpacity(double(qMax(MINIMUM_OPACITY,PsiOptions::instance()->getOption("options.ui.contactlist.opacity").toInt()))/100);

	connect(qApp, SIGNAL(dockActivated()), SLOT(dockActivated()));

	connect(psi, SIGNAL(emitOptionsUpdate()), SLOT(optionsUpdate()));
	optionsUpdate();

	cvlist->setFocus();

	// special shortcuts
	QShortcut *sp_ss = new QShortcut(QKeySequence(tr("Ctrl+Shift+X")), this);
	connect(sp_ss, SIGNAL(activated()), SLOT(showXmlConsole()));
	sp_ss = new QShortcut(QKeySequence(tr("Ctrl+Shift+U")), this);
	connect(sp_ss, SIGNAL(activated()), SLOT(checkUpgrade()));

	sp_ss = new QShortcut(QKeySequence(tr("Ctrl+Shift+N")), this);
	connect(sp_ss, SIGNAL(activated()), SLOT(voipConfig()));
}
	

MainWinB::~MainWinB()
{
	PsiPopup::deleteAll();

	if(d->tray) {
		delete d->tray;
		d->tray = 0;
	}

	//saveToolbarsState();
	delete d;
}

void MainWinB::registerAction( IconAction *action )
{
	char activated[] = SIGNAL( activated() );
	char toggled[]   = SIGNAL( toggled(bool) );
	char setChecked[]     = SLOT( setChecked(bool) );

	struct {
		const char *name;
		const char *signal;
		QObject *receiver;
		const char *slot;
	} actionlist[] = {
		{ "show_offline", toggled, cvlist, SLOT( setShowOffline(bool) ) },
		{ "show_away",    toggled, cvlist, SLOT( setShowAway(bool) ) },
		{ "show_hidden",  toggled, cvlist, SLOT( setShowHidden(bool) ) },
		{ "show_agents",  toggled, cvlist, SLOT( setShowAgents(bool) ) },
		{ "show_self",    toggled, cvlist, SLOT( setShowSelf(bool) ) },
		{ "show_statusmsg", toggled, cvlist, SLOT( setShowStatusMsg(bool) ) },

		{ "button_options", activated, this, SIGNAL( doOptions() ) },

		{ "menu_disco",       SIGNAL( activated(PsiAccount *, int) ), this, SLOT( activatedAccOption(PsiAccount*, int) ) },
		{ "menu_add_contact", SIGNAL( activated(PsiAccount *, int) ), this, SLOT( activatedAccOption(PsiAccount*, int) ) },
		{ "menu_xml_console", SIGNAL( activated(PsiAccount *, int) ), this, SLOT( activatedAccOption(PsiAccount*, int) ) },

		{ "menu_new_message",    activated, this, SIGNAL( blankMessage() ) },
		{ "menu_join_groupchat", activated, this, SIGNAL( doGroupChat() ) },
		{ "menu_account_setup",  activated, this, SIGNAL( doManageAccounts() ) },
		{ "menu_options",        activated, this, SIGNAL( doOptions() ) },
		{ "menu_file_transfer",  activated, this, SIGNAL( doFileTransDlg() ) },
		{ "menu_toolbars",       activated, this, SIGNAL( doToolbars() ) },
		{ "menu_change_profile", activated, this, SIGNAL( changeProfile() ) },
		{ "menu_quit",           activated, this, SLOT( try2tryCloseProgram() ) },
		{ "menu_play_sounds",    toggled,   this, SLOT( actPlaySoundsActivated(bool) ) },
#ifdef USE_PEP
		{ "publish_tune",        toggled,   this, SLOT( actPublishTuneActivated(bool) ) },
#endif

		{ "event_notifier", SIGNAL( clicked(int) ), this, SLOT( statusClicked(int) ) },
		{ "event_notifier", activated, this, SLOT( doRecvNextEvent() ) },

		{ "help_readme",      activated, this, SLOT( actReadmeActivated() ) },
		{ "help_tip",         activated, this, SLOT( actTipActivated() ) },
		{ "help_online_help", activated, this, SLOT( actOnlineHelpActivated() ) },
		{ "help_online_wiki", activated, this, SLOT( actOnlineWikiActivated() ) },
		{ "help_online_home", activated, this, SLOT( actOnlineHomeActivated() ) },
		{ "help_psi_muc",     activated, this, SLOT( actJoinPsiMUCActivated() ) },
		{ "help_report_bug",  activated, this, SLOT( actBugReportActivated() ) },
		{ "help_about",       activated, this, SLOT( actAboutActivated() ) },
		{ "help_about_qt",    activated, this, SLOT( actAboutQtActivated() ) },
		{ "log_on",               activated, this, SLOT( actDoLogOn() ) },
		{ "log_off",              activated, this, SLOT( actDoLogOff() ) },
		{ "menu_account_modify",  activated, this, SLOT( actDoAccountModify() ) },
		{ "menu_edit_vcard",      activated, this, SLOT( actDoEditVCard() ) },
		{ "link_history",         activated, this, SLOT( actDoLinkHistory() ) },
		{ "link_password",        activated, this, SLOT( actDoLinkPassword() ) },
		{ "reg_aim",          activated, this, SLOT( actDoRegAim() ) },
		{ "reg_msn",          activated, this, SLOT( actDoRegMsn() ) },
		{ "reg_yahoo",        activated, this, SLOT( actDoRegYahoo() ) },
		{ "reg_icq",          activated, this, SLOT( actDoRegIcq() ) },
		{ "reg_trans",          activated, this, SLOT( actDoRegTrans() ) },

		{ "", 0, 0, 0 }
	};

	int i;
	QString aName;
	for ( i = 0; !(aName = QString(actionlist[i].name)).isEmpty(); i++ ) {
		if ( aName == action->name() ) {
#ifdef USE_PEP
			// Check before connecting, otherwise we get a loop
			if ( aName == "publish_tune")
				action->setChecked( PsiOptions::instance()->getOption("options.extended-presence.tune.publish").toBool() );
#endif

			disconnect( action, actionlist[i].signal, actionlist[i].receiver, actionlist[i].slot ); // for safety
			connect( action, actionlist[i].signal, actionlist[i].receiver, actionlist[i].slot );

			// special cases
			if ( aName == "menu_play_sounds" )
				action->setChecked(PsiOptions::instance()->getOption("options.ui.notifications.sounds.enable").toBool());
			//else if ( aName == "foobar" )
			//	;
		}
	}

	struct {
		const char *name;
		QObject *sender;
		const char *signal;
		const char *slot;
		bool checked;
	} reverseactionlist[] = {
		{ "show_away",    cvlist, SIGNAL( showAway(bool) ), setChecked, cvlist->isShowAway()},
		{ "show_hidden",  cvlist, SIGNAL( showHidden(bool) ), setChecked, cvlist->isShowHidden()},
		{ "show_offline", cvlist, SIGNAL( showOffline(bool) ), setChecked, cvlist->isShowOffline()},
		{ "show_self",    cvlist, SIGNAL( showSelf(bool) ), setChecked, cvlist->isShowSelf()},
		{ "show_agents",  cvlist, SIGNAL( showAgents(bool) ), setChecked, cvlist->isShowAgents()},
		{ "show_statusmsg", cvlist, SIGNAL( showStatusMsg(bool) ), setChecked, cvlist->isShowStatusMsg()},
		{ "", 0, 0, 0, false }
	};

	for ( i = 0; !(aName = QString(reverseactionlist[i].name)).isEmpty(); i++ ) {
		if ( aName == action->name() ) {
			disconnect( reverseactionlist[i].sender, reverseactionlist[i].signal, action, reverseactionlist[i].slot ); // for safety
			connect( reverseactionlist[i].sender, reverseactionlist[i].signal, action, reverseactionlist[i].slot );

			if (aName == "show_statusmsg") {
				action->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.show").toBool() );
			}
			else
				action->setChecked( reverseactionlist[i].checked );
		}
	}
}

PsiCon *MainWinB::psiCon() const
{
	return d->psi;
}

void MainWinB::setWindowOpts(bool _onTop, bool _asTool)
{
	if(_onTop == d->onTop && _asTool == d->asTool)
		return;

	d->onTop = _onTop;
	d->asTool = _asTool;

	Qt::WFlags flags = 0;
	if(d->onTop)
		flags |= Qt::WStyle_StaysOnTop;
	if(d->asTool)
		flags |= Qt::WStyle_Tool | TOOLW_FLAGS;

	QPoint p = pos();
	reparent(parentWidget(), flags, p, FALSE);
	move(p);
	show();
}

void MainWinB::setUseDock(bool use)
{
	if(use == false || d->tray) {
		if(d->tray) {
			delete d->tray;
			d->tray = 0;
		}

		if (use == false)
			return;
	}

	if(d->tray)
		return;

	d->tray = new PsiTrayIcon("Barracuda", d->trayMenu, d->old_trayicon);
	if (d->old_trayicon) {
		connect(d->tray, SIGNAL(closed()), SLOT(dockActivated()));
		connect(qApp, SIGNAL(trayOwnerDied()), SLOT(dockActivated()));
	}
	connect(d->tray, SIGNAL(clicked(const QPoint &, int)), SLOT(trayClicked(const QPoint &, int)));
	connect(d->tray, SIGNAL(doubleClicked(const QPoint &)), SLOT(trayDoubleClicked()));

	// ###cuda
	//d->tray->setIcon( PsiIconset::instance()->statusPtr( STATUS_OFFLINE ));
	d->tray->setIcon(IconsetFactory::iconPtr("psi/logo_16"));

	d->tray->setToolTip(ApplicationInfo::name());

	updateReadNext(d->nextAnim, d->nextAmount);

	d->tray->show();
}

void MainWinB::buildStatusMenu()
{
	d->statusMenu->clear();
	d->getAction("status_online")->addTo(d->statusMenu);
	if (PsiOptions::instance()->getOption("options.ui.menu.status.chat").toBool())
		d->getAction("status_chat")->addTo(d->statusMenu);
	d->statusMenu->insertSeparator();
	d->getAction("status_away")->addTo(d->statusMenu);
	if (PsiOptions::instance()->getOption("options.ui.menu.status.xa").toBool())
		d->getAction("status_xa")->addTo(d->statusMenu);
	d->getAction("status_dnd")->addTo(d->statusMenu);
	if (PsiOptions::instance()->getOption("options.ui.menu.status.invisible").toBool()) {
		d->statusMenu->insertSeparator();
		d->getAction("status_invisible")->addTo(d->statusMenu);
	}
	d->statusMenu->insertSeparator();
	d->getAction("status_offline")->addTo(d->statusMenu);
//#ifdef USE_PEP
//	d->statusMenu->insertSeparator();
//	d->getAction("publish_tune")->addTo(d->statusMenu);
//#endif
}

void MainWinB::activatedStatusAction(int id)
{
	QObjectList l = d->statusGroup->queryList( "IconAction" );
	for (QObjectList::Iterator it = l.begin() ; it != l.end(); ++it) {
		IconAction *action = (IconAction *)(*it);
		action->setChecked ( d->statusActions[action] == id );
	}

	statusChanged(id);
}

QMenuBar* MainWinB::mainMenuBar() const
{
#ifdef Q_WS_MAC
	return psiCon()->defaultMenuBar();
#else
	return mainWindow->menuBar();
#endif
}

//void MainWinB::buildToolbars()
//{
	/*while ( option.toolbars["mainWin"].count() < toolbars.count() && toolbars.count() ) {
		PsiToolBar *tb = toolbars.last();
		toolbars.removeLast();
		delete tb;
	}

	for (int i = 0; i < option.toolbars["mainWin"].count(); i++) {
		PsiToolBar *tb = 0;
		if ( i < toolbars.count() )
			tb = toolbars.at(i);

		Options::ToolbarPrefs &tbPref = option.toolbars["mainWin"][i];
		if ( tb && !tbPref.dirty )
			continue;

		if ( tb )
			delete tb;

		tb = new PsiToolBar(tbPref.name, mainWindow, d->psi);
		mainWindow->moveDockWindow ( tb, tbPref.dock, tbPref.nl, tbPref.index, tbPref. extraOffset );

		tb->setGroup( "mainWin", i );
		tb->setType( PsiActionList::Actions_MainWin );
		//connect( tb, SIGNAL( registerAction( IconAction * ) ), SLOT( registerAction( IconAction * ) ) );
		tb->initialize( tbPref, false );

		if ( i < toolbars.count() )
			toolbars.removeAt(i);
		toolbars.insert(i, tb);
	}*/
//}

void MainWinB::saveToolbarsState()
{
}

void MainWinB::loadToolbarsState()
{
}

void MainWinB::buildToolbars()
{
	/*QStringList bases = PsiOptions::instance()->getChildOptionNames("options.ui.contactlist.toolbars", true, true);
	foreach(QString base, bases) {
		addToolbar(base);
	}*/
}

bool MainWinB::showDockMenu(const QPoint &)
{
	return false;
}

void MainWinB::buildOptionsMenu()
{
	// help menu
	QMenu *helpMenu = new QMenu(d->optionsMenu);

	QStringList actions;
	actions // << "help_readme"
	        // << "help_tip"
	        // << "separator"
	        // << "help_online_help"
	        // << "help_online_wiki"
	        // << "help_online_home"
	        // << "help_psi_muc"
	        // << "help_report_bug"
	        // << "separator"
	        << "help_about";
	        // << "help_about_qt";

	d->updateMenu(actions, helpMenu);

	buildGeneralMenu( d->optionsMenu );

	d->optionsMenu->insertSeparator();
	//d->optionsMenu->insertItem(IconsetFactory::icon("psi/help").icon(), tr("&Help"), helpMenu);
	d->getAction("menu_quit")->addTo( d->optionsMenu );

}

void MainWinB::buildMainMenu()
{
	// main menu
	QStringList actions;
	//actions << "menu_add_contact";
	//if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool())
	//	actions << "menu_new_message";
	actions << "log_on"
		<< "log_off"
		<< "separator"
		<< "link_history"
		<< "menu_file_transfer";
	        /*<< "menu_join_groupchat"
	        << "separator"
	        << "menu_account_setup";
	if (PsiOptions::instance()->getOption("options.ui.menu.main.change-profile").toBool())
	        actions << "menu_change_profile";
	actions << "menu_play_sounds";*/

	d->updateMenu(actions, d->mainMenu);
}

void MainWinB::buildToolsMenu()
{
	/*QStringList actions;
	actions << "menu_file_transfer"
	        << "separator"
	        << "menu_xml_console";
	
	d->updateMenu(actions, d->toolsMenu);*/
}
	
void MainWinB::buildGeneralMenu(QMenu *menu)
{
	// options menu
	QStringList actions;
	//actions << "menu_add_contact";
	//if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool())
	//	actions << "menu_new_message";
	actions << "log_on"
		<< "log_off"
		<< "separator"
		<< "link_history"
	        << "menu_file_transfer";
	        /*<< "menu_join_groupchat"
	        << "menu_account_setup"
		<< "cuda_transports"
	        << "menu_options"*/
	//if (PsiOptions::instance()->getOption("options.ui.menu.main.change-profile").toBool())
	//	actions << "menu_change_profile";
	//actions << "menu_play_sounds";

	d->updateMenu(actions, menu);
}

void MainWinB::actReadmeActivated ()
{
	ShowTextDlg *w = new ShowTextDlg(":/README");
	w->setWindowTitle(CAP(tr("ReadMe")));
	w->show();
}

void MainWinB::actOnlineHelpActivated ()
{
	QDesktopServices::openUrl(QUrl("http://psi-im.org/wiki/User_Guide"));
}

void MainWinB::actOnlineWikiActivated ()
{
	QDesktopServices::openUrl(QUrl("http://psi-im.org/wiki"));
}

void MainWinB::actOnlineHomeActivated ()
{
	QDesktopServices::openUrl(QUrl("http://psi-im.org"));
}

void MainWinB::actJoinPsiMUCActivated()
{
	PsiAccount *account = d->psi->contactList()->defaultAccount();
	if(!account)
		return;

	account->actionJoin("psi@conference.psi-im.org");
}

void MainWinB::actBugReportActivated ()
{
	QDesktopServices::openUrl(QUrl("http://psi-im.org/forum/forum/2"));
}

void MainWinB::actAboutActivated ()
{
	AboutDlg *about = new AboutDlg();
	about->show();
}

void MainWinB::actTipActivated ()
{
	TipDlg::show(d->psi);
}

void MainWinB::actAboutQtActivated ()
{
	QMessageBox::aboutQt(this);
}

void MainWinB::actDiagQCAPluginActivated()
{
	QString dtext = QCA::pluginDiagnosticText();
	ShowTextDlg *w = new ShowTextDlg(dtext, true, false, this);
	w->setWindowTitle(CAP(tr("Security Plugins Diagnostic Text")));
	w->resize(560, 240);
	w->show();
}

void MainWinB::actDiagQCAKeyStoreActivated()
{
	QString dtext = QCA::KeyStoreManager::diagnosticText();
	ShowTextDlg *w = new ShowTextDlg(dtext, true, false, this);
	w->setWindowTitle(CAP(tr("Key Storage Diagnostic Text")));
	w->resize(560, 240);
	w->show();
}

void MainWinB::actPlaySoundsActivated (bool state)
{
	PsiOptions::instance()->setOption("options.ui.notifications.sounds.enable", state);
}

void MainWinB::actPublishTuneActivated (bool state)
{
	PsiOptions::instance()->setOption("options.extended-presence.tune.publish",state);
}

void MainWinB::addUser()
{
	PsiAccount *account = d->psi->contactList()->defaultAccount();
	if(!account)
		return;

	account->openAddUserDlg();
}

void MainWinB::activatedAccOption(PsiAccount *pa, int x)
{
	if(x == 0)
		pa->openAddUserDlg();
	else if(x == 2)
		pa->showXmlConsole();
	else if(x == 3)
		pa->doDisco();
}

void MainWinB::buildTrayMenu()
{
#ifndef Q_WS_MAC
	d->trayMenu->clear();

	if(d->nextAmount > 0) {
		d->trayMenu->insertItem(tr("Receive next event"), this, SLOT(doRecvNextEvent()));
		d->trayMenu->insertSeparator();
	}

	if(isHidden())
		d->trayMenu->insertItem(tr("Un&hide"), this, SLOT(trayShow()));
	else
		d->trayMenu->insertItem(tr("&Hide"), this, SLOT(trayHide()));
	//d->optionsButton->addTo(d->trayMenu);
	d->getAction("log_on")->addTo(d->trayMenu);
	d->getAction("log_off")->addTo(d->trayMenu);
	d->trayMenu->insertItem(tr("Status"), d->statusMenu);
	
	d->trayMenu->insertSeparator();
	// TODO!
	d->getAction("menu_quit")->addTo(d->trayMenu);
#endif
}

void MainWinB::setTrayToolTip(int status)
{
	if (!d->tray)
		return;
	d->tray->setToolTip(QString("Barracuda - " + status2txt(status)));
}

void MainWinB::decorateButton(int status)
{
	// update the 'change status' buttons
	QObjectList l = d->statusGroup->queryList( "IconAction" );
	for (QObjectList::Iterator it = l.begin() ; it != l.end(); ++it) {
		IconAction *action = (IconAction *)(*it);
		action->setChecked ( d->statusActions[action] == status );
	}

	if(d->lastStatus == status)
		return;
	d->lastStatus = status;

	if(status == -1) {
		d->statusButton->setText(tr("Connecting"));
		d->pb_status->setLabel(tr("Connecting"));
		if (PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString() != "no") {
			d->statusButton->setAlert(IconsetFactory::iconPtr("psi/connect"));
			//d->pb_status->setIcon(IconsetFactory::iconPtr("psi/connect"));
			d->statusGroup->setPsiIcon(IconsetFactory::iconPtr("psi/connect"));
		}
		else {
			d->statusButton->setIcon(PsiIconset::instance()->statusPtr(STATUS_OFFLINE));
			d->statusGroup->setPsiIcon(PsiIconset::instance()->statusPtr(STATUS_OFFLINE));
		}

		// ###cuda
		//setWindowIcon(PsiIconset::instance()->status(STATUS_OFFLINE).impix());
	}
	else {
		d->statusButton->setText(status2txt(status));
		d->pb_status->setLabel(status2txt(status));
		d->statusButton->setIcon(PsiIconset::instance()->statusPtr(status));
		//d->pb_status->setIcon(PsiIconset::instance()->statusPtr(status));
		d->statusGroup->setPsiIcon(PsiIconset::instance()->statusPtr(status));

		// ###cuda
		//setWindowIcon(PsiIconset::instance()->status(status).impix());
	}

	updateTray();
}

bool MainWinB::askQuit()
{
	return TRUE;
}

void MainWinB::try2tryCloseProgram()
{
	QTimer::singleShot(0, this, SLOT(tryCloseProgram()));
}

void MainWinB::tryCloseProgram()
{
	if(askQuit())
		closeProgram();
}

void MainWinB::closeEvent(QCloseEvent *e)
{
#ifdef Q_WS_MAC
	trayHide();
	e->accept();
#else
	if(d->tray) {
		trayHide();
		e->accept();
		return;
	}

	if(!askQuit())
		return;

        closeProgram();
	e->accept();
#endif
}

void MainWinB::keyPressEvent(QKeyEvent *e)
{
#ifdef Q_WS_MAC
	bool allowed = true;
#else
	bool allowed = d->tray ? true: false;
#endif

	bool closekey = false;
	if(e->key() == Qt::Key_Escape)
	{
		if(d->le_search->hasFocus())
			searchCancel();
		else
			closekey = true;
	}
#ifdef Q_WS_MAC
	else if(e->key() == Qt::Key_W && e->modifiers() & Qt::ControlModifier)
		closekey = true;
#endif

	if(allowed && closekey) {
		close();
		e->accept();
		return;
	}

	QWidget::keyPressEvent(e);
}

#ifdef Q_WS_WIN
#include <windows.h>
bool MainWinB::winEvent(MSG *msg, long *result)
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

void MainWinB::updateCaption()
{
	QString str = "";

	if(d->nextAmount > 0)
		str += "* ";

	if(d->nickname.isEmpty())
		str += ApplicationInfo::name();
	else
		str += d->nickname;

	if(str == caption())
		return;

	setWindowTitle(str);
}

void MainWinB::optionsUpdate()
{
	int status = d->lastStatus;
	d->lastStatus = -2;
	decorateButton(status);

#ifndef Q_WS_MAC
	if (!PsiOptions::instance()->getOption("options.ui.contactlist.show-menubar").toBool()) 
		mainMenuBar()->hide();
	else 
		mainMenuBar()->show();
#endif
	
	setWindowOpacity(double(qMax(MINIMUM_OPACITY,PsiOptions::instance()->getOption("options.ui.contactlist.opacity").toInt()))/100);

	buildStatusMenu();
	
	updateTray();
}

void MainWinB::toggleVisible()
{
	if(!isHidden())
		trayHide();
	else
		trayShow();
}

void MainWinB::setTrayToolTip(const Status &status, bool)
{
	if (!d->tray)
		return;
	QString s = "Barracuda";

 	QString show = status.show();
	if(!show.isEmpty()) {
		show[0] = show[0].upper();
		s += " - "+show;
	}

	QString text = status.status();
	if(!text.isEmpty())
		s += ": "+text;

	d->tray->setToolTip(s);
}

void MainWinB::trayClicked(const QPoint &, int button)
{
	if(PsiOptions::instance()->getOption("options.ui.systemtray.use-double-click").toBool())
		return;

	if(button == Qt::MidButton) {
		doRecvNextEvent();
		return;
	}

	if(!isHidden())
		trayHide();
	else
		trayShow();
}

void MainWinB::trayDoubleClicked()
{
	if(!PsiOptions::instance()->getOption("options.ui.systemtray.use-double-click").toBool())
		return;

	if(d->nextAmount > 0) {
		doRecvNextEvent();
		return;
	}

	// for windows, always bring to front.  for other platforms, toggle.
#ifndef Q_WS_WIN
	if(!isHidden())
		trayHide();
	else
#endif
		trayShow();
}

void MainWinB::trayShow()
{
	bringToFront(this);
}

void MainWinB::trayHide()
{
	hide();
}

void MainWinB::updateReadNext(PsiIcon *anim, int amount)
{
	d->nextAnim = anim;
	if(anim == 0)
		d->nextAmount = 0;
	else
		d->nextAmount = amount;

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
}

QString MainWinB::numEventsString(int x) const
{
	QString s;
	if(x <= 0)
		s = "";
	else if(x == 1)
		s = tr("1 event received");
	else
		s = tr("%1 events received").arg(x);

	return s;
}

void MainWinB::updateTray()
{
	if(!d->tray)
		return;

	if ( d->nextAmount > 0 )
		d->tray->setAlert(d->nextAnim);
	else if ( d->lastStatus == -1 )
	{
		// ###cuda
		//d->tray->setAlert(IconsetFactory::iconPtr("psi/connect"));
	}
	else
	{
		// ###cuda
		//d->tray->setIcon(PsiIconset::instance()->statusPtr(d->lastStatus));
		d->tray->setIcon(IconsetFactory::iconPtr("psi/logo_16"));
	}

	buildTrayMenu();
	d->tray->setContextMenu(d->trayMenu);
}

void MainWinB::doRecvNextEvent()
{
	recvNextEvent();
}

void MainWinB::statusClicked(int x)
{
	if(x == Qt::MidButton)
		recvNextEvent();
}

void MainWinB::numAccountsChanged()
{
	if(!d->psi->contactList()->haveEnabledAccounts())
		return;

	QList<PsiAccount*> accountList = d->psi->contactList()->accounts();
	foreach(PsiAccount *acc, accountList)
	{
		disconnect(acc->avatarFactory(), SIGNAL(avatarChanged(const Jid &)), this, SLOT(vcardChanged(const Jid &)));
		connect(acc->avatarFactory(), SIGNAL(avatarChanged(const Jid &)), SLOT(vcardChanged(const Jid &)));
	}

	PsiAccount *account = d->psi->contactList()->defaultAccount();
	QString jid, name;
	QImage img;
	if(account)
	{
		jid = account->jid().bare();

		QPixmap avatarPixmap = account->avatarFactory()->getAvatar(account->jid());
		if(!avatarPixmap.isNull())
			img = avatarPixmap.toImage();

		const VCard *vcard = VCardFactory::instance()->vcard(account->jid());
		if(vcard)
			name = vcard->fullName();
	}

	QString str;
	if(!name.isEmpty())
		str += QString("<b>%1</b><br>").arg(name);
	str += jid;
	if(str.isEmpty())
		str = "&lt;Unknown&gt;";
	d->lb_name->setText(str);
	d->lb_avatar->setPixmap(QPixmap::fromImage(makeAvatarImage(img)));

	d->statusButton->setEnabled(d->psi->contactList()->haveEnabledAccounts());

	if(!d->serv_search && !accountList.isEmpty())
	{
		d->serv_search = new SimpleSearch(d->psi->contactList()->defaultAccount());
		connect(d->serv_search, SIGNAL(results(const QList<SimpleSearch::Result> &)), SLOT(serv_search_results(const QList<SimpleSearch::Result> &)));
	}
	else if(d->serv_search && accountList.isEmpty())
		delete d->serv_search;
}

void MainWinB::accountFeaturesChanged()
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

void MainWinB::dockActivated()
{
	if(isHidden())
		show();
}


#ifdef Q_WS_MAC
void MainWinB::setWindowIcon(const QPixmap&)
{
}
#else
void MainWinB::setWindowIcon(const QPixmap& p)
{
	AdvancedWidget<CudaFrame>::setWindowIcon(p);
}
#endif

#if 0
#if defined(Q_WS_WIN)
#include <windows.h>
void MainWinB::showNoFocus()
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
		register QObject *object;
		QWidget *widget;
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

void MainWinB::showNoFocus()
{
	bringToFront(this);
}

//#endif

void MainWinB::searchClearClicked()
{
}

void MainWinB::searchTextEntered(QString const &text)
{
	Q_UNUSED(text);
}

void MainWinB::searchTextStarted(QString const &text)
{
	Q_UNUSED(text);
}

// ###cuda
void MainWinB::vcardChanged(const Jid &jid)
{
	PsiAccount *account = d->psi->contactList()->defaultAccount();
	if(account && account->jid().compare(jid, false))
		numAccountsChanged();
}

void MainWinB::searchTextChanged(const QString &text)
{
	// search cancelled
	if(text.isNull())
	{
		d->pb_searchcancel->hide();
		d->restorePreSearchState();
		d->filterActive = false;

		d->serv_search->stop();
		return;
	}

	if(!d->filterActive)
	{
		d->filterActive = true;
		d->pb_searchcancel->show();
		d->savePreSearchState();
	}

	if(text.length() < 2)
	{
		cvlist->clearFilter();
		d->serv_search->stop();
	}
	else
	{
		cvlist->setFilter(text);
		d->serv_search->search(text);
		d->searchTimeout.start();
	}
}

void MainWinB::searchCancel()
{
	d->le_search->clearSearch();
	d->pb_searchcancel->hide();
	cvlist->setFocus();
	cvlist->clearFilter();
	d->restorePreSearchState();
	d->filterActive = false;

	d->serv_search->stop();
}

void MainWinB::actDoLogOn()
{
	d->psi->setGlobalStatus(makeStatus(STATUS_ONLINE, QString()));
}

void MainWinB::actDoLogOff()
{
	d->psi->setGlobalStatus(makeStatus(STATUS_OFFLINE, QString()));
}

void MainWinB::actDoAccountModify()
{
	PsiAccount *account = d->psi->contactList()->defaultAccount();
	account->modify();
}

void MainWinB::actDoEditVCard()
{
	PsiAccount *account = d->psi->contactList()->defaultAccount();
	account->changeVCard();
}

void MainWinB::actDoLinkHistory()
{
	PsiAccount *account = d->psi->contactList()->defaultAccount();
	account->doGotoWebHistory();
}

void MainWinB::actDoLinkPassword()
{
	PsiAccount *account = d->psi->contactList()->defaultAccount();
	account->doGotoWebPass();
}

void MainWinB::actDoRegAim()
{
	/*PsiAccount *account = d->psi->contactList()->defaultAccount();
	if(!account)
		return;
	transportWizard *w = new transportWizard(0, "", account, "aim");
	w->show();*/
}

void MainWinB::actDoRegMsn()
{
	/*PsiAccount *account = d->psi->contactList()->defaultAccount();
	if(!account)
		return;
	transportWizard *w = new transportWizard(0, "", account, "msn");
	w->show();*/
}

void MainWinB::actDoRegYahoo()
{
	/*PsiAccount *account = d->psi->contactList()->defaultAccount();
	if(!account)
		return;
	transportWizard *w = new transportWizard(0, "", account, "yahoo");
	w->show();*/
}

void MainWinB::actDoRegIcq()
{
	/*PsiAccount *account = d->psi->contactList()->defaultAccount();
	if(!account)
		return;
	transportWizard *w = new transportWizard(0, "", account, "icq");
	w->show();*/
}

void MainWinB::actDoRegTrans()
{
	PsiAccount *account = d->psi->contactList()->defaultAccount();
	if(!account)
		return;
	TransportSetupDlg *w = new TransportSetupDlg(account, 0);
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->show();
}

void MainWinB::avatar_clicked()
{
	actDoEditVCard();
}

void MainWinB::serv_search_results(const QList<SimpleSearch::Result> &list)
{
	PsiAccount *account = d->psi->contactList()->defaultAccount();
	UserList *userList = account->userList();
	AvatarFactory *avatarFactory = account->avatarFactory();
	ContactProfile *cp = account->contactProfile();

	// remove results that are no longer in the result set
	UserListIt it(*userList);
	for(UserListItem *u; (u = it.current());)
	{
		if(u->inList())
		{
			++it;
			continue;
		}

		bool found = false;
		foreach(const SimpleSearch::Result &r, list)
		{
			if(r.jid.compare(u->jid()))
			{
				found = true;
				break;
			}
		}

		if(!found)
		{
			account->eventQueue()->clear(u->jid());
			cp->removeEntry(u->jid());
			userList->removeRef(u);
		}
		else
			++it;
	}

	// add results from the result set that we don't yet have
	foreach(const SimpleSearch::Result &r, list)
	{
		bool found = false;
		UserListIt it(*userList);
		for(UserListItem *u; (u = it.current()); ++it)
		{
			if(u->jid().compare(r.jid))
			{
				found = true;
				break;
			}
		}

		if(!found)
		{
			UserListItem *u = new UserListItem;
			u->setJid(r.jid);
			u->setInList(false);
			u->setAvatarFactory(avatarFactory);
			u->setName(r.name);
			userList->append(u);
			account->cpUpdate_pub(*u);
		}
	}
}

void MainWinB::showXmlConsole()
{
	PsiAccount *account = d->psi->contactList()->defaultAccount();
	account->showXmlConsole();
}

void MainWinB::checkUpgrade()
{
	PsiAccount *account = d->psi->contactList()->defaultAccount();
	account->checkUpgrade();
}

void MainWinB::voipConfig()
{
#ifdef QUICKVOIP
	JingleRtpManager::instance()->config();
#endif
}

/*
 * psicon.cpp - core of Psi
 * Copyright (C) 2001, 2002  Justin Karneges
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

#include "psicon.h"

#include <q3ptrlist.h>
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <QMenuBar>
#include <qpointer.h>
#include <qicon.h>
#include <qcolor.h>
#include <qimage.h>
#include <qpixmapcache.h>
#include <QPixmap>
#include <QList>
#include <QImageReader>
#include <QMessageBox>

#include "s5b.h"
#include "psiaccount.h"
#include "accountadddlg.h"
#include "psiiconset.h"
#include "contactview.h"
#include "psievent.h"
#include "passphrasedlg.h"
#include "common.h"
#include "mainwin.h"
#include "idle.h"
#include "accountmanagedlg.h"
#include "statusdlg.h"
#include "options/optionsdlg.h"
#include "options/opt_toolbars.h"
#include "accountregdlg.h"
#include "combinedtunecontroller.h"
#include "mucjoindlg.h"
#include "userlist.h"
#include "eventdlg.h"
#include "pgputil.h"
#include "eventdb.h"
#include "proxy.h"
#ifdef PSIMNG
#include "psimng.h"
#endif
#include "alerticon.h"
#include "iconselect.h"
#include "psitoolbar.h"
#include "filetransfer.h"
#include "filetransdlg.h"
#include "accountmodifydlg.h"
#include "psiactionlist.h"
#include "applicationinfo.h"
#include "jidutil.h"
#include "systemwatch.h"
#include "accountscombobox.h"
#include "tabdlg.h"
#include "chatdlg.h"
#include "capsregistry.h"
#include "urlobject.h"
#include "anim.h"
#include "psioptions.h"
#ifdef PSI_PLUGINS
#include "pluginmanager.h"
#endif
#include "psicontactlist.h"
#include "tipdlg.h"
#include "shortcutmanager.h"
#include "globalshortcutmanager.h"
#include "desktoputil.h"


#ifdef Q_WS_MAC
#include "mac_dock.h"
#endif

//----------------------------------------------------------------------------
// PsiConObject
//----------------------------------------------------------------------------
class PsiConObject : public QObject
{
	Q_OBJECT
public:
	PsiConObject(QObject *parent)
	: QObject(parent, "PsiConObject")
	{
		QDir p(ApplicationInfo::homeDir());
		QDir v(ApplicationInfo::homeDir() + "/tmp-sounds");
		if(!v.exists())
			p.mkdir("tmp-sounds");
		Iconset::setSoundPrefs(v.absPath(), this, SLOT(playSound(QString)));
		connect(URLObject::getInstance(), SIGNAL(openURL(QString)), SLOT(openURL(QString)));
	}

	~PsiConObject()
	{
		// removing temp dirs
		QDir p(ApplicationInfo::homeDir());
		QDir v(ApplicationInfo::homeDir() + "/tmp-sounds");
		folderRemove(v);
	}

public slots:
	void playSound(QString file)
	{
		if ( file.isEmpty() || !useSound )
			return;

		soundPlay(file);
	}

	void openURL(QString url)
	{
		DesktopUtil::openUrl(url);
	}

private:
	// ripped from profiles.cpp
	bool folderRemove(const QDir &_d)
	{
		QDir d = _d;

		QStringList entries = d.entryList();
		for(QStringList::Iterator it = entries.begin(); it != entries.end(); ++it) {
			if(*it == "." || *it == "..")
				continue;
			QFileInfo info(d, *it);
			if(info.isDir()) {
				if(!folderRemove(QDir(info.filePath())))
					return FALSE;
			}
			else {
				//printf("deleting [%s]\n", info.filePath().latin1());
				d.remove(info.fileName());
			}
		}
		QString name = d.dirName();
		if(!d.cdUp())
			return FALSE;
		//printf("removing folder [%s]\n", d.filePath(name).latin1());
		d.rmdir(name);

		return TRUE;
	}
};

//----------------------------------------------------------------------------
// PsiCon::Private
//----------------------------------------------------------------------------

struct item_dialog
{
	QWidget *widget;
	QString className;
};

class PsiCon::Private
{
public:
	Private(PsiCon *parent)
		: contactList(0), iconSelect(0)
	{
		psi = parent;
		//the list 'owns' the tabs
		tabs.setAutoDelete( true );
		tabControlledChats.setAutoDelete( false );
	}

	~Private()
	{
		if ( iconSelect )
			delete iconSelect;
	}

	void saveProfile(UserAccountList acc = UserAccountList())
	{
		pro.recentGCList = recentGCList;
		pro.recentBrowseList = recentBrowseList;
		pro.lastStatusString = lastStatusString;
		pro.useSound = useSound;
		pro.prefs = option;
		if ( proxy )
			pro.proxyList = proxy->itemList();

		if ( acc.count() )
			pro.acc = acc;

		pro.toFile(pathToProfileConfig(activeProfile));
	}

	void updateIconSelect()
	{
		Iconset iss;
		Q3PtrListIterator<Iconset> iconsets(PsiIconset::instance()->emoticons);
		Iconset *iconset;
		while ( (iconset = iconsets.current()) != 0 ) {
			iss += *iconset;

			++iconsets;
		}

		iconSelect->setIconset(iss);
	}

	PsiCon* psi;
	PsiContactList* contactList;
	UserProfile pro;
	QString lastStatusString;
	MainWin *mainwin;
	Idle idle;
	QList<item_dialog*> dialogList;
	Q3PtrList<TabDlg> tabs;
	Q3PtrList<ChatDlg> tabControlledChats;
	int eventId;
	QStringList recentGCList, recentBrowseList, recentNodeList;
	EDB *edb;
	S5BServer *s5bServer;
	ProxyManager *proxy;
	IconSelectPopup *iconSelect;
	QRect mwgeom;
	FileTransDlg *ftwin;
	PsiActionList *actionList;
	QCA::EventHandler *qcaEventHandler;
	QCA::KeyStoreManager qcaKeyStoreManager;
	//GlobalAccelManager *globalAccelManager;
	TuneController* tuneController;
	QMenuBar* defaultMenuBar;
};

//----------------------------------------------------------------------------
// PsiCon
//----------------------------------------------------------------------------

PsiCon::PsiCon()
:QObject(0)
{
	//pdb(DEBUG_JABCON, QString("%1 v%2\n By Justin Karneges\n    infiniti@affinix.com\n\n").arg(PROG_NAME).arg(PROG_VERSION));

	d = new Private(this);

	d->lastStatusString = "";
	useSound = true;
	d->mainwin = 0;
	d->ftwin = 0;
	d->qcaEventHandler = 0;

	d->eventId = 0;
	d->edb = new EDBFlatFile;

	d->s5bServer = 0;
	d->proxy = 0;
	d->tuneController = 0;

	d->actionList = 0;
	d->defaultMenuBar = new QMenuBar(0);
}

PsiCon::~PsiCon()
{
	deinit();

	delete d->actionList;
	delete d->edb;
	delete d->defaultMenuBar;
	delete d;
}

bool PsiCon::init()
{
	// QCA (needs to be before any gpg usage!)
	d->qcaEventHandler = new QCA::EventHandler(this);
	PassphraseDlg::setEventHandler(d->qcaEventHandler);
	connect(d->qcaEventHandler,SIGNAL(eventReady(int,const QCA::Event&)),SLOT(qcaEvent(int,const QCA::Event&)));
	d->qcaEventHandler->start();
	d->qcaKeyStoreManager.waitForBusyFinished(); // FIXME get rid of this
	connect(&d->qcaKeyStoreManager, SIGNAL(keyStoreAvailable(const QString&)), SLOT(keyStoreAvailable(const QString&)));
	foreach(QString k, d->qcaKeyStoreManager.keyStores()) {
		QCA::KeyStore* ks = new QCA::KeyStore(k, &d->qcaKeyStoreManager);
		connect(ks, SIGNAL(updated()), SLOT(pgp_keysUpdated()));
		PGPUtil::keystores += ks;
	}

	d->contactList = new PsiContactList(this);


	connect(d->contactList, SIGNAL(accountAdded(PsiAccount*)), SIGNAL(accountAdded(PsiAccount*)));
	connect(d->contactList, SIGNAL(accountRemoved(PsiAccount*)), SIGNAL(accountRemoved(PsiAccount*)));
	connect(d->contactList, SIGNAL(accountCountChanged()), SIGNAL(accountCountChanged()));
	connect(d->contactList, SIGNAL(accountActivityChanged()), SIGNAL(accountActivityChanged()));
	connect(d->contactList, SIGNAL(saveAccounts()), SLOT(saveAccounts()));

	// To allow us to upgrade from old hardcoded options gracefully, be careful about the order here
	PsiOptions *options=PsiOptions::instance();
	//load the system-wide defaults, if they exist
	QString systemDefaults=ApplicationInfo::resourcesDir();
	systemDefaults += "/options-default.xml";
	//qWarning(qPrintable(QString("Loading system defaults from %1").arg(systemDefaults)));
	options->load(systemDefaults);

#ifdef USE_PEP
	// Create the tune controller
	d->tuneController = new CombinedTuneController();
#endif

	// load the old profile
	d->pro.reset();
	d->pro.fromFile(pathToProfileConfig(activeProfile));
	
	//load the new profile
	QString optionsFile=pathToProfile( activeProfile );
	optionsFile += "/options.xml";
	options->load(optionsFile);
	//Save every time an option is changed
	options->autoSave(true, optionsFile);
	
	//just set a dummy option to trigger saving
	options->setOption("trigger-save",false);
	options->setOption("trigger-save",true);
	
	connect(options, SIGNAL(optionChanged(const QString&)), SLOT(optionsUpdate()));

	QDir profileDir( pathToProfile( activeProfile ) );
	profileDir.rmdir( "info" ); // remove unused dir

	d->recentGCList = d->pro.recentGCList;
	d->recentBrowseList = d->pro.recentBrowseList;
	d->lastStatusString = d->pro.lastStatusString;
	useSound = d->pro.useSound;

	option = d->pro.prefs;

	// first thing, try to load the iconset
	if( !PsiIconset::instance()->loadAll() ) {
		//option.iconset = "stellar";
		//if(!is.load(option.iconset)) {
			QMessageBox::critical(0, tr("Error"), tr("Unable to load iconset!  Please make sure Psi is properly installed."));
			return false;
		//}
	}

	if ( !d->actionList )
		d->actionList = new PsiActionList( this );

	new PsiConObject(this);
		
	Anim::setMainThread(QThread::currentThread());

	d->iconSelect = new IconSelectPopup(0);
	d->updateIconSelect();

	// setup the main window
	d->mainwin = new MainWin(option.alwaysOnTop, (option.useDock && option.dockToolMW), this, "psimain"); 
	d->mainwin->setUseDock(option.useDock);

	connect(d->mainwin, SIGNAL(closeProgram()), SLOT(closeProgram()));
	connect(d->mainwin, SIGNAL(changeProfile()), SLOT(changeProfile()));
	connect(d->mainwin, SIGNAL(doManageAccounts()), SLOT(doManageAccounts()));
	connect(d->mainwin, SIGNAL(doGroupChat()), SLOT(doGroupChat()));
	connect(d->mainwin, SIGNAL(blankMessage()), SLOT(doNewBlankMessage()));
	connect(d->mainwin, SIGNAL(statusChanged(int)), SLOT(statusMenuChanged(int)));
	connect(d->mainwin, SIGNAL(doOptions()), SLOT(doOptions()));
	connect(d->mainwin, SIGNAL(doToolbars()), SLOT(doToolbars()));
	connect(d->mainwin, SIGNAL(doFileTransDlg()), SLOT(doFileTransDlg()));
	connect(d->mainwin, SIGNAL(recvNextEvent()), SLOT(recvNextEvent()));
	connect(d->mainwin, SIGNAL(geomChanged(QRect)), SLOT(mainWinGeomChanged(QRect)));
	connect(this, SIGNAL(emitOptionsUpdate()), d->mainwin, SLOT(optionsUpdate()));

	connect(this, SIGNAL(emitOptionsUpdate()), d->mainwin->cvlist, SLOT(optionsUpdate()));

	d->mainwin->restoreSavedGeometry(d->pro.mwgeom);
	
	if(!(option.useDock && option.dockHideMW))
		d->mainwin->show();

	d->ftwin = new FileTransDlg(this);

	d->idle.start();

	// S5B
	d->s5bServer = new S5BServer;
	s5b_init();

	// proxy
	d->proxy = new ProxyManager(this);
	d->proxy->setItemList(d->pro.proxyList);
	connect(d->proxy, SIGNAL(settingsChanged()), SLOT(proxy_settingsChanged()));

	// Disable accounts if necessary, and overwrite locked properties
	if (PsiOptions::instance()->getOption("options.ui.account.single").toBool() || !PsiOptions::instance()->getOption("options.account.domain").toString().isEmpty()) {
		bool haveEnabled = false;
		for(UserAccountList::Iterator it = d->pro.acc.begin(); it != d->pro.acc.end(); ++it) {
			// With single accounts, only modify the first account
			if (PsiOptions::instance()->getOption("options.ui.account.single").toBool()) {
				if (!haveEnabled) {
					haveEnabled = it->opt_enabled;
					if (it->opt_enabled) {
						if (!PsiOptions::instance()->getOption("options.account.domain").toString().isEmpty())
							it->jid = JIDUtil::accountFromString(Jid(it->jid).user()).bare();
					}
				}
				else
					it->opt_enabled = false;
			}
			else {
				// Overwirte locked properties
				if (!PsiOptions::instance()->getOption("options.account.domain").toString().isEmpty())
					it->jid = JIDUtil::accountFromString(Jid(it->jid).user()).bare();
			}
		}
	}
	
	// Connect to the system monitor
	SystemWatch* sw = SystemWatch::instance();
	connect(sw, SIGNAL(sleep()), this, SLOT(doSleep()));
	connect(sw, SIGNAL(idleSleep()), this, SLOT(doSleep()));
	connect(sw, SIGNAL(wakeup()), this, SLOT(doWakeup()));

#ifdef PSI_PLUGINS
	// Plugin Manager
	PluginManager::instance();
#endif

	// Global shortcuts
	setShortcuts();

	// Entity capabilities
	CapsRegistry::instance()->setFile(ApplicationInfo::homeDir() + "/caps.xml");


	// load accounts
	d->contactList->loadAccounts(d->pro.acc);
	checkAccountsEmpty();
	// try autologin if needed
	foreach(PsiAccount* account, d->contactList->accounts()) {
		account->autoLogin();
	}

	// show tip of the day
	if ( PsiOptions::instance()->getOption("options.ui.tip.show").toBool() )
		TipDlg::show(this);

	return true;
}

void PsiCon::deinit()
{
	// this deletes all dialogs except for mainwin
	deleteAllDialogs();

	// QCA Keystores
	foreach(QCA::KeyStore* ks,PGPUtil::keystores)  {
		delete ks;
	}
	PGPUtil::keystores.clear();

	d->idle.stop();

	// shut down all accounts
	UserAccountList acc = d->contactList->getUserAccountList();
	delete d->contactList;

	// delete s5b server
	delete d->s5bServer;

	delete d->ftwin;
	delete d->qcaEventHandler;

	if(d->mainwin) {
		// shut down mainwin
		QRect mwgeom;
		if ( !d->mainwin->isHidden() && !d->mainwin->isMinimized() )
			mwgeom = d->mainwin->saveableGeometry();
		else
			mwgeom = d->mwgeom;

		delete d->mainwin;
		d->mainwin = 0;

		d->pro.mwgeom = mwgeom;
	}

	// TuneController
	delete d->tuneController;

	// save profile
	d->saveProfile(acc);
}

void PsiCon::optionsUpdate()
{
	// Global shortcuts
	setShortcuts();
}

void PsiCon::setShortcuts()
{
	// FIX-ME: GlobalShortcutManager::clear() is one big hack,
	// but people wanted to change global hotkeys without restarting in 0.11
	GlobalShortcutManager::clear();
	ShortcutManager::connect("global.event", this, SLOT(recvNextEvent()));
	ShortcutManager::connect("global.toggle-visibility", d->mainwin, SLOT(toggleVisible()));
	ShortcutManager::connect("global.bring-to-front", d->mainwin, SLOT(trayShow()));
	ShortcutManager::connect("global.new-blank-message", this, SLOT(doNewBlankMessage()));

}

ContactView *PsiCon::contactView() const
{
	if(d->mainwin)
		return d->mainwin->cvlist;
	else
		return 0;
}

PsiContactList* PsiCon::contactList() const
{
	return d->contactList;
}

EDB *PsiCon::edb() const
{
	return d->edb;
}

ProxyManager *PsiCon::proxy() const
{
	return d->proxy;
}

FileTransDlg *PsiCon::ftdlg() const
{
	return d->ftwin;
}

TuneController *PsiCon::tuneController() const
{
	return d->tuneController;
}

void PsiCon::closeProgram()
{
	quit(QuitProgram);
}

void PsiCon::changeProfile()
{
	if(d->contactList->haveActiveAccounts()) {
		QMessageBox::information(0, CAP(tr("Error")), tr("Please disconnect before changing the profile."));
		return;
	}

	quit(QuitProfile);
}

void PsiCon::qcaEvent(int id, const QCA::Event& event)
{
	if (event.type() == QCA::Event::Password) {
		if(PGPUtil::passphrases.contains(event.keyStoreEntry().id())) {
			d->qcaEventHandler->submitPassword(id,QCA::SecureArray(PGPUtil::passphrases[event.keyStoreEntry().id()].utf8()));
		}
		else {
			QString name;
			QCA::KeyStore ks(event.keyStoreInfo().id(), &d->qcaKeyStoreManager);
			foreach(QCA::KeyStoreEntry e, ks.entryList()) {
				if (e.id() == event.keyStoreEntry().id()) {
					name = e.name();
				}
			}
			PassphraseDlg::promptPassphrase(name,event.keyStoreEntry().id(),id);
		}
	}
}
void PsiCon::keyStoreAvailable(const QString& k)
{
	QCA::KeyStore* ks = new QCA::KeyStore(k, &d->qcaKeyStoreManager);
	connect(ks, SIGNAL(updated()), SLOT(pgp_keysUpdated()));
	PGPUtil::keystores += ks;
}


void PsiCon::doManageAccounts()
{
	if (!PsiOptions::instance()->getOption("options.ui.account.single").toBool()) {
		AccountManageDlg *w = (AccountManageDlg *)dialogFind("AccountManageDlg");
		if(w)
			bringToFront(w);
		else {
			w = new AccountManageDlg(this);
			w->show();
		}
	}
	else {
		PsiAccount *account = d->contactList->defaultAccount();
		if(account) {
			account->modify();
		}
		else {
			promptUserToCreateAccount();
		}
	}
}

void PsiCon::doGroupChat()
{
	PsiAccount *account = d->contactList->defaultAccount();
	if(!account)
		return;

	MUCJoinDlg *w = new MUCJoinDlg(this, account);
	w->show();
}

void PsiCon::doNewBlankMessage()
{
	PsiAccount *account = d->contactList->defaultAccount();
	if(!account)
		return;

	EventDlg *w = createEventDlg("", account);
	w->show();
}

// FIXME: smells fishy. Refactor! Probably create a common class for all dialogs and 
// call optionsUpdate() automatically.
EventDlg *PsiCon::createEventDlg(const QString &to, PsiAccount *pa)
{
	EventDlg *w = new EventDlg(to, this, pa);
	connect(this, SIGNAL(emitOptionsUpdate()), w, SLOT(optionsUpdate()));
	return w;
}

// FIXME: WTF? Refactor! Refactor!
void PsiCon::updateContactGlobal(PsiAccount *pa, const Jid &j)
{
	foreach(item_dialog* i, d->dialogList) {
		if(i->className == "EventDlg") {
			EventDlg *e = (EventDlg *)i->widget;
			if(e->psiAccount() == pa)
				e->updateContact(j);
		}
	}
}

// FIXME: make it work like QObject::findChildren<ChildName>()
QWidget *PsiCon::dialogFind(const char *className)
{
	foreach(item_dialog *i, d->dialogList) {
		// does the classname and jid match?
		if(i->className == className) {
			return i->widget;
		}
	}
	return 0;
}

QMenuBar* PsiCon::defaultMenuBar() const
{
	return d->defaultMenuBar;
}

TabDlg* PsiCon::newTabs()
{
	TabDlg *tab;
	tab=new TabDlg(this);
	d->tabs.append(tab);
	connect (tab, SIGNAL ( isDying(TabDlg*) ), SLOT ( tabDying(TabDlg*) ) );
	connect(this, SIGNAL(emitOptionsUpdate()), tab, SLOT(optionsUpdate()));
	return tab;
}

TabDlg* PsiCon::getTabs()
{
	if (!d->tabs.isEmpty())
	{
		return d->tabs.getFirst();
	}
	else
	{
		return newTabs();
	}
}

void PsiCon::tabDying(TabDlg* tab)
{
	d->tabs.remove(tab);
}

bool PsiCon::isChatTabbed(ChatDlg* chat)
{
	for (uint i = 0; i < d->tabs.count(); ++i)
	{
		if ( d->tabs.at(i)->managesChat(chat) )
				return true;
	}
	return false;
}

ChatDlg* PsiCon::getChatInTabs(QString jid){
	for (uint i = 0; i < d->tabs.count(); ++i)
	{
		if ( d->tabs.at(i)->getChatPointer(jid) )
				return d->tabs.at(i)->getChatPointer(jid);
	}
	return NULL;

}

TabDlg* PsiCon::getManagingTabs(ChatDlg* chat)
{
	for (uint i = 0; i < d->tabs.count(); ++i)
	{
		if ( d->tabs.at(i)->managesChat(chat) )
				return d->tabs.at(i);
	}
	return NULL;

}

Q3PtrList<TabDlg>* PsiCon::getTabSets()
{
	return &d->tabs;
}

bool PsiCon::isChatActiveWindow(ChatDlg* chat)
{
	//returns true if chat is on top of a tab pile
	if ( chat->isHidden() )
	{
		return false;
	}
	if (!option.useTabs)
	{
		return chat->isActiveWindow();
	}
	for (uint i = 0; i < d->tabs.count(); ++i)
	{
		if ( d->tabs.at(i)->isActiveWindow() )
		{
			if ( d->tabs.at(i)->chatOnTop( chat ) )
			{
				return true;
			}
		}
	}
	return false;
}

void PsiCon::dialogRegister(QWidget *w)
{
	item_dialog *i = new item_dialog;
	i->widget = w;
	i->className = w->className();
	d->dialogList.append(i);
}

void PsiCon::dialogUnregister(QWidget *w)
{
	for (QList<item_dialog*>::Iterator it = d->dialogList.begin(); it != d->dialogList.end(); ) {
		item_dialog* i = *it;
		if(i->widget == w) {
			it = d->dialogList.erase(it);
			delete i;
		}
		else
			++it;
	}
}

void PsiCon::deleteAllDialogs()
{
	while(!d->dialogList.isEmpty()) {
		item_dialog* i = d->dialogList.takeFirst();
		delete i->widget;
		delete i;
	}
	d->tabs.clear();
}

AccountsComboBox *PsiCon::accountsComboBox(QWidget *parent, bool online_only)
{
	AccountsComboBox *acb = new AccountsComboBox(this, parent, online_only);
	return acb;
}

void PsiCon::createAccount(const QString &name, const Jid &j, const QString &pass, bool opt_host, const QString &host, int port, bool legacy_ssl_probe, UserAccount::SSLFlag ssl, int proxy)
{
	d->contactList->createAccount(name, j, pass, opt_host, host, port, legacy_ssl_probe, ssl, proxy);
}

PsiAccount *PsiCon::createAccount(const UserAccount& acc)
{
	PsiAccount *pa = new PsiAccount(acc, d->contactList);
	connect(&d->idle, SIGNAL(secondsIdle(int)), pa, SLOT(secondsIdle(int)));
	connect(pa, SIGNAL(updatedActivity()), SLOT(pa_updatedActivity()));
	connect(pa, SIGNAL(updatedAccount()), SLOT(pa_updatedAccount()));
	connect(pa, SIGNAL(queueChanged()), SLOT(queueChanged()));
	if(d->s5bServer)
		pa->client()->s5bManager()->setServer(d->s5bServer);
	return pa;
}

void PsiCon::removeAccount(PsiAccount *pa)
{
	d->contactList->removeAccount(pa);
}

void PsiCon::statusMenuChanged(int x)
{
	if(x == STATUS_OFFLINE && !option.askOffline) {
		setGlobalStatus(Status("","Logged out",0,false));
		if(option.useDock == true)
			d->mainwin->setTrayToolTip(Status("","",0,false));
	}
	else {
		if(x == STATUS_ONLINE && !option.askOnline) {
			setGlobalStatus(Status());
			if(option.useDock == true)
				d->mainwin->setTrayToolTip(Status());
		}
		else if(x == STATUS_INVISIBLE){
			Status s("","",0,true);
			s.setIsInvisible(true);
			setGlobalStatus(s);
			if(option.useDock == true)
				d->mainwin->setTrayToolTip(s);
		}
		else {
			// Create a dialog with the last status message
			StatusSetDlg *w = new StatusSetDlg(this, makeStatus(x, d->lastStatusString));
			connect(w, SIGNAL(set(const XMPP::Status &, bool)), SLOT(setStatusFromDialog(const XMPP::Status &, bool)));
			connect(w, SIGNAL(cancelled()), SLOT(updateMainwinStatus()));
			if(option.useDock == true)
				connect(w, SIGNAL(set(const XMPP::Status &, bool)), d->mainwin, SLOT(setTrayToolTip(const XMPP::Status &, bool)));
			w->show();
		}
	}
}

void PsiCon::setStatusFromDialog(const Status &s, bool withPriority)
{
	d->lastStatusString = s.status();
	setGlobalStatus(s, withPriority);
}

void PsiCon::setGlobalStatus(const Status &s,  bool withPriority)
{
	// Check whether all accounts are logged off
	bool allOffline = true;
	foreach(PsiAccount* account, d->contactList->enabledAccounts()) {
		if ( account->isActive() ) {
			allOffline = false;
			break;
		}
	}

	// globally set each account which is logged in
	foreach(PsiAccount* account, d->contactList->enabledAccounts())
		if (allOffline || account->isActive())
			account->setStatus(s, withPriority);
}

void PsiCon::pa_updatedActivity()
{
	PsiAccount *pa = (PsiAccount *)sender();
	emit accountUpdated(pa);

	// update s5b server
	updateS5BServerAddresses();

	updateMainwinStatus();
}

void PsiCon::pa_updatedAccount()
{
	PsiAccount *pa = (PsiAccount *)sender();
	emit accountUpdated(pa);

	saveAccounts();
}

void PsiCon::saveAccounts()
{
	UserAccountList acc = d->contactList->getUserAccountList();

	d->pro.proxyList = d->proxy->itemList();
	//d->pro.acc = acc;
	//d->pro.toFile(pathToProfileConfig(activeProfile));
	d->saveProfile(acc);
}

void PsiCon::updateMainwinStatus()
{
	bool active = false;
	bool loggedIn = false;
	int state = STATUS_ONLINE;
	foreach(PsiAccount* account, d->contactList->enabledAccounts()) {
		if(account->isActive())
			active = true;
		if(account->loggedIn()) {
			loggedIn = true;
			state = makeSTATUS(account->status());
		}
	}
	if(loggedIn)
		d->mainwin->decorateButton(state);
	else {
		if(active)
			d->mainwin->decorateButton(-1);
		else
			d->mainwin->decorateButton(STATUS_OFFLINE);
	}
}

void PsiCon::setToggles(bool tog_offline, bool tog_away, bool tog_agents, bool tog_hidden, bool tog_self)
{
	if(d->contactList->enabledAccounts().count() > 1)
		return;

	d->mainwin->cvlist->setShowOffline(tog_offline);
	d->mainwin->cvlist->setShowAway(tog_away);
	d->mainwin->cvlist->setShowAgents(tog_agents);
	d->mainwin->cvlist->setShowHidden(tog_hidden);
	d->mainwin->cvlist->setShowSelf(tog_self);
}

void PsiCon::getToggles(bool *tog_offline, bool *tog_away, bool *tog_agents, bool *tog_hidden, bool *tog_self)
{
	*tog_offline = d->mainwin->cvlist->isShowOffline();
	*tog_away = d->mainwin->cvlist->isShowAway();
	*tog_agents = d->mainwin->cvlist->isShowAgents();
	*tog_hidden = d->mainwin->cvlist->isShowHidden();
	*tog_self = d->mainwin->cvlist->isShowSelf();
}

void PsiCon::doOptions()
{
	OptionsDlg *w = (OptionsDlg *)dialogFind("OptionsDlg");
	if(w)
		bringToFront(w);
	else {
		w = new OptionsDlg(this, option);
		connect(w, SIGNAL(applyOptions(const Options &)), SLOT(slotApplyOptions(const Options &)));
		w->show();
	}
}

void PsiCon::doFileTransDlg()
{
	bringToFront(d->ftwin);
}

void PsiCon::checkAccountsEmpty()
{
	if (d->pro.acc.count() == 0) {
		promptUserToCreateAccount();
	}
}

void PsiCon::doToolbars()
{
	OptionsDlg *w = (OptionsDlg *)dialogFind("OptionsDlg");
	if (w) {
		w->openTab("toolbars");
		bringToFront(w);
	}
	else {
		w = new OptionsDlg(this, option);
		connect(w, SIGNAL(applyOptions(const Options &)), SLOT(slotApplyOptions(const Options &)));
		w->openTab("toolbars");
		w->show();
	}
}

void PsiCon::slotApplyOptions(const Options &opt)
{
	Options oldOpt = option;
	bool notifyRestart = true;

	option = opt;

#ifndef Q_WS_MAC
	if (option.hideMenubar) {
		// check if all toolbars are disabled
		bool toolbarsVisible = false;
		QList<Options::ToolbarPrefs>::ConstIterator it = option.toolbars["mainWin"].begin();
		for ( ; it != option.toolbars["mainWin"].end() && !toolbarsVisible; ++it) {
			toolbarsVisible = toolbarsVisible || (*it).on;
		}

		// Check whether it is legal to disable the menubar
		if ( !toolbarsVisible ) {
			QMessageBox::warning(0, tr("Warning"),
				tr("You can not disable <i>all</i> toolbars <i>and</i> the menubar. If you do so, you will be unable to enable them back, when you'll change your mind.\n"
					"<br><br>\n"
					"If you really-really want to disable all toolbars and the menubar, you need to edit the config.xml file by hand."),
				tr("I understand"));
			option.hideMenubar = false;
		}
	}
#endif

	if ( option.useTabs != oldOpt.useTabs ) {
		QMessageBox::information(0, tr("Information"), tr("Some of the options you changed will only have full effect upon restart."));
		notifyRestart = false;
	}

	// change icon set
	if ( option.systemIconset		!= oldOpt.systemIconset		||
	     option.emoticons			!= oldOpt.emoticons		||
	     option.defaultRosterIconset	!= oldOpt.defaultRosterIconset	||
	     operator!=(option.serviceRosterIconset,oldOpt.serviceRosterIconset)	||
	     operator!=(option.customRosterIconset,oldOpt.customRosterIconset) )
	{
		if ( notifyRestart && PsiIconset::instance()->optionsChanged(&oldOpt) )
			QMessageBox::information(0, tr("Information"), tr("The complete iconset update will happen on next Psi start."));

		// update icon selector
		d->updateIconSelect();

		// flush the QPixmapCache
		QPixmapCache::clear();
	}

	if ( oldOpt.alertStyle != option.alertStyle )
		alertIconUpdateAlertStyle();

	d->mainwin->buildToolbars();

	/*// change pgp engine
	if(option.pgp != oldpgp) {
		if(d->pgp) {
			delete d->pgp;
			d->pgp = 0;
			pgpToggled(false);
		}
		pgp_init(option.pgp);
	}*/

	// update s5b
	if(oldOpt.dtPort != option.dtPort)
		s5b_init();
	updateS5BServerAddresses();

	// mainwin stuff
	d->mainwin->setWindowOpts(option.alwaysOnTop, (option.useDock && option.dockToolMW));
	d->mainwin->setUseDock(option.useDock);

	// notify about options change
	emitOptionsUpdate();

	// save just the options
	//d->pro.prefs = option;
	//d->pro.toFile(pathToProfileConfig(activeProfile));
	d->saveProfile();
}

int PsiCon::getId()
{
	return d->eventId++;
}

void PsiCon::queueChanged()
{
	PsiIcon *nextAnim = 0;
	int nextAmount = d->contactList->queueCount();
	PsiAccount *pa = d->contactList->queueLowestEventId();
	if(pa)
		nextAnim = PsiIconset::instance()->event2icon(pa->eventQueue()->peekNext());

#ifdef Q_WS_MAC
	{
		// The Mac Dock icon

		// Update the event count
		if (nextAmount) 
			MacDock::overlay(QString::number(nextAmount));
		else 
			MacDock::overlay(QString::null);

		// Check if bouncing is necessary
		bool doBounce = false;
		foreach(PsiAccount* account, d->contactList->enabledAccounts()) {
			if (account->eventQueue()->count() && pa->status().type() != XMPP::Status::DND) {
				doBounce = true;
				break;
			}
		}

		// Bouncy bouncy
		if (doBounce) {
			if (option.bounceDock != Options::NoBounce) {
				MacDock::startBounce();
				if (option.bounceDock == Options::BounceOnce) 
					MacDock::stopBounce();
			}
		}
		else 
			MacDock::stopBounce();
	}
#endif

	d->mainwin->updateReadNext(nextAnim, nextAmount);
}

void PsiCon::recvNextEvent()
{
	/*printf("--- Queue Content: ---\n");
	PsiAccountListIt it(d->list);
	for(PsiAccount *pa; (pa = it.current()); ++it) {
		printf(" Account: [%s]\n", pa->name().latin1());
		pa->eventQueue()->printContent();
	}*/
	PsiAccount *pa = d->contactList->queueLowestEventId();
	if(pa)
		pa->openNextEvent();
}

void PsiCon::playSound(const QString &str)
{
	if(str.isEmpty() || !useSound)
		return;

	soundPlay(str);
}

void PsiCon::raiseMainwin()
{
	d->mainwin->showNoFocus();
}

const QStringList & PsiCon::recentGCList() const
{
	return d->recentGCList;
}

void PsiCon::recentGCAdd(const QString &str)
{
	// remove it if we have it
	for(QStringList::Iterator it = d->recentGCList.begin(); it != d->recentGCList.end(); ++it) {
		if(*it == str) {
			d->recentGCList.remove(it);
			break;
		}
	}

	// put it in the front
	d->recentGCList.prepend(str);

	// trim the list if bigger than 10
	while(d->recentGCList.count() > PsiOptions::instance()->getOption("options.muc.recent-joins.maximum").toInt())
		d->recentGCList.remove(d->recentGCList.fromLast());
}

const QStringList & PsiCon::recentBrowseList() const
{
	return d->recentBrowseList;
}

void PsiCon::recentBrowseAdd(const QString &str)
{
	// remove it if we have it
	for(QStringList::Iterator it = d->recentBrowseList.begin(); it != d->recentBrowseList.end(); ++it) {
		if(*it == str) {
			d->recentBrowseList.remove(it);
			break;
		}
	}

	// put it in the front
	d->recentBrowseList.prepend(str);

	// trim the list if bigger than 10
	while(d->recentBrowseList.count() > 10)
		d->recentBrowseList.remove(d->recentBrowseList.fromLast());
}

const QStringList & PsiCon::recentNodeList() const
{
	return d->recentNodeList;
}

void PsiCon::recentNodeAdd(const QString &str)
{
	// remove it if we have it
	for(QStringList::Iterator it = d->recentNodeList.begin(); it != d->recentNodeList.end(); ++it) {
		if(*it == str) {
			d->recentNodeList.remove(it);
			break;
		}
	}

	// put it in the front
	d->recentNodeList.prepend(str);

	// trim the list if bigger than 10
	while(d->recentNodeList.count() > 10)
		d->recentNodeList.remove(d->recentNodeList.fromLast());
}

void PsiCon::pgp_keysUpdated()
{
	emit pgpKeysUpdated();
	//QMessageBox::information(0, CAP(tr("OpenPGP")), tr("Psi has detected that you have modified your keyring.  That is all."));
}

void PsiCon::proxy_settingsChanged()
{
	// properly index accounts
	foreach(PsiAccount* account, d->contactList->accounts()) {
		UserAccount acc = account->userAccount();
		if(acc.proxy_index > 0) {
			int x = d->proxy->findOldIndex(acc.proxy_index-1);
			if(x == -1)
				acc.proxy_index = 0;
			else
				acc.proxy_index = x+1;
			account->setUserAccount(acc);
		}
	}

	saveAccounts();
}

IconSelectPopup *PsiCon::iconSelectPopup() const
{
	return d->iconSelect;
}

void PsiCon::processEvent(PsiEvent *e)
{
	if ( e->type() == PsiEvent::PGP ) {
		e->account()->eventQueue()->dequeue(e);
		e->account()->queueChanged();
		return;
	}

	if ( !e->account() )
		return;

	UserListItem *u = e->account()->find(e->jid());
	if ( !u ) {
		qWarning("SYSTEM MESSAGE: Bug #1. Contact the developers and tell them what you did to make this message appear. Thank you.");
		e->account()->eventQueue()->dequeue(e);
		e->account()->queueChanged();
		return;
	}

	if( e->type() == PsiEvent::File ) {
		FileEvent *fe = (FileEvent *)e;
		FileTransfer *ft = fe->takeFileTransfer();
		e->account()->eventQueue()->dequeue(e);
		e->account()->queueChanged();
		e->account()->cpUpdate(*u);
		if(ft) {
			FileRequestDlg *w = new FileRequestDlg(fe->timeStamp(), ft, e->account());
			bringToFront(w);
		}
		return;
	}

	bool isChat = false;
	bool sentToChatWindow = false;
	if ( e->type() == PsiEvent::Message ) {
		MessageEvent *me = (MessageEvent *)e;
		const Message &m = me->message();
		bool emptyForm = m.getForm().fields().empty();
		if ( m.type() == "chat" && emptyForm ) {
			isChat = true;
			sentToChatWindow = me->sentToChatWindow();
		}
	}

	if ( isChat ) {
		if ( option.alertOpenChats && sentToChatWindow ) {
			// Message already displayed, need only to pop up chat dialog, so that
			// it will be read (or marked as read)
			ChatDlg *c = e->account()->findDialog<ChatDlg*>(e->from());
			if(!c)
				c = e->account()->findDialog<ChatDlg*>(e->jid());
			if(!c)
				return; // should never happen

			e->account()->processChats(e->from()); // this will delete all events, corresponding to that chat dialog
			//KIS: I changed the following line with the one following that to attempt to get tabs to behave correctly. Lots of this stuff isn't great.
			bringToFront((QWidget *)c);
			if ( option.useTabs)
			{
				TabDlg* tabSet = getManagingTabs(c);
				tabSet->selectTab(c);
			}
		}
		else
			e->account()->openChat(e->from());
	}
	else {
		// search for an already opened eventdlg
		EventDlg *w = e->account()->findDialog<EventDlg*>(u->jid());

		if ( !w ) {
			// create the eventdlg
			w = e->account()->ensureEventDlg(u->jid());

			// load next message
			e->account()->processReadNext(*u);
		}

		bringToFront(w);
	}
}

void PsiCon::mainWinGeomChanged(QRect saveableGeometry)
{
	d->mwgeom = saveableGeometry;
}

void PsiCon::updateS5BServerAddresses()
{
	if(!d->s5bServer)
		return;

	QList<QHostAddress> list;

	// grab all IP addresses
	foreach(PsiAccount* account, d->contactList->accounts()) {
		QHostAddress *a = account->localAddress();
		if(!a)
			continue;

		// don't take dups
		bool found = false;
		for(QList<QHostAddress>::ConstIterator hit = list.begin(); hit != list.end(); ++hit) {
			const QHostAddress &ha = *hit;
			if(ha == (*a)) {
				found = true;
				break;
			}
		}
		if(!found)
			list += (*a);
	}

	// convert to stringlist
	QStringList slist;
	for(QList<QHostAddress>::ConstIterator hit = list.begin(); hit != list.end(); ++hit)
		slist += (*hit).toString();

	// add external
	if(!option.dtExternal.isEmpty()) {
		bool found = false;
		for(QStringList::ConstIterator sit = slist.begin(); sit != slist.end(); ++sit) {
			const QString &s = *sit;
			if(s == option.dtExternal) {
				found = true;
				break;
			}
		}
		if(!found)
			slist += option.dtExternal;
	}

	// set up the server
	d->s5bServer->setHostList(slist);
}

void PsiCon::s5b_init()
{
	if(d->s5bServer->isActive())
		d->s5bServer->stop();

	if (option.dtPort) {
		if(!d->s5bServer->start(option.dtPort)) {
			QMessageBox::warning(0, tr("Warning"), tr("Unable to bind to port %1 for Data Transfer.\nThis may mean you are already running another instance of Psi. You may experience problems sending and/or receiving files.").arg(option.dtPort));
		}
	}
}

void PsiCon::doSleep()
{
	setGlobalStatus(Status("",tr("Computer went to sleep"),0,false));
}

void PsiCon::doWakeup()
{
	// TODO: Restore the status from before the log out. Make it an (hidden) option for people with a bad wireless connection.
	//setGlobalStatus(Status());

	foreach(PsiAccount* account, d->contactList->enabledAccounts()) {
		if (account->userAccount().opt_reconn) {
			// Should we do this when the network comes up ?
			account->setStatus(Status("", "", account->userAccount().priority));
		}
	}
}


QList<PsiToolBar*> PsiCon::toolbarList() const
{
	return d->mainwin->toolbars;
}

PsiToolBar *PsiCon::findToolBar(QString group, int index)
{
	PsiToolBar *toolBar = 0;

	if (( group == "mainWin" ) && (index < d->mainwin->toolbars.size()))
		toolBar = d->mainwin->toolbars.at(index);

	return toolBar;
}

void PsiCon::buildToolbars()
{
	d->mainwin->buildToolbars();
}

bool PsiCon::getToolbarLocation(Q3DockWindow* dw, Qt::Dock& dock, int& index, bool& nl, int& extraOffset) const
{
	return d->mainwin->getLocation(dw, dock, index, nl, extraOffset);
}

PsiActionList *PsiCon::actionList() const
{
	return d->actionList;
}

/**
 * Prompts user to create new account, if none are currently present in system.
 */
void PsiCon::promptUserToCreateAccount()
{
	QMessageBox msgBox(QMessageBox::Question,tr("Account setup"),tr("You need to set up an account to start. Would you like to register a new account, or use an existing account?"));
	QPushButton *registerButton = msgBox.addButton(tr("Register new account"), QMessageBox::AcceptRole);
	QPushButton *existingButton = msgBox.addButton(tr("Use existing account"),QMessageBox::AcceptRole);
	msgBox.addButton(QMessageBox::Cancel);
	msgBox.exec();
	if (msgBox.clickedButton() ==  existingButton) {
		AccountModifyDlg w(this);
		w.exec();
	}
	else if (msgBox.clickedButton() ==  registerButton) {
		AccountRegDlg w(proxy());
		int n = w.exec();
		if (n == QDialog::Accepted) {
			contactList()->createAccount(w.jid().node(),w.jid(),w.pass(),w.useHost(),w.host(),w.port(),w.legacySSLProbe(),w.ssl(),w.proxy(),false);
		}
	}
}


#include "psicon.moc"

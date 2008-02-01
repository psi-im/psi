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
#include <QFile>
#include <QPixmap>
#include <QList>
#include <QImageReader>
#include <QMessageBox>

#include "s5b.h"
#include "psiaccount.h"
#include "activeprofiles.h"
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
#include "dbus.h"
#include "tipdlg.h"
#include "shortcutmanager.h"
#include "globalshortcutmanager.h"
#include "desktoputil.h"
#include "tabmanager.h"


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
		if ( file.isEmpty() || !PsiOptions::instance()->getOption("options.ui.notifications.sounds.enable").toBool() )
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
	}

	~Private()
	{
		if ( iconSelect )
			delete iconSelect;
	}

	void saveProfile(UserAccountList acc)
	{
		// clear it
		accountTree.removeOption("accounts", true);
		// save accounts with known base
		QSet<QString> cbases;
		foreach(UserAccount ua, acc) {
			if (!ua.optionsBase.isEmpty()) {
				ua.toOptions(&accountTree);
				cbases += ua.optionsBase;
			}
		}
		// save new accounts
		int idx = 0;
		foreach(UserAccount ua, acc) {
			if (ua.optionsBase.isEmpty()) {
				QString base;
				do {
					base = "accounts.a"+QString::number(idx++);
				} while (cbases.contains(base));
				cbases += base;
				ua.toOptions(&accountTree, base);
			}
		}
		QFile accountsFile(pathToProfile( activeProfile ) + "/accounts.xml");	
		accountTree.saveOptions(accountsFile.fileName(), "accounts", ApplicationInfo::optionsNS(), ApplicationInfo::version());;
		
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
	OptionsMigration optionsMigration;
	OptionsTree accountTree;
	MainWin *mainwin;
	Idle idle;
	QList<item_dialog*> dialogList;
	int eventId;
	QStringList recentNodeList; // FIXME move this to options system?
	EDB *edb;
	S5BServer *s5bServer;
	ProxyManager *proxy;
	IconSelectPopup *iconSelect;
	FileTransDlg *ftwin;
	PsiActionList *actionList;
	//GlobalAccelManager *globalAccelManager;
	TuneController* tuneController;
	QMenuBar* defaultMenuBar;
	CapsRegistry* capsRegistry;
	TabManager *tabManager;
};

//----------------------------------------------------------------------------
// PsiCon
//----------------------------------------------------------------------------

PsiCon::PsiCon()
:QObject(0)
{
	//pdb(DEBUG_JABCON, QString("%1 v%2\n By Justin Karneges\n    infiniti@affinix.com\n\n").arg(PROG_NAME).arg(PROG_VERSION));

	d = new Private(this);
	d->tabManager = new TabManager(this);

	d->mainwin = 0;
	d->ftwin = 0;

	d->eventId = 0;
	d->edb = new EDBFlatFile;

	d->s5bServer = 0;
	d->proxy = 0;
	d->tuneController = 0;

	d->actionList = 0;
	d->defaultMenuBar = new QMenuBar(0);
	d->capsRegistry = new CapsRegistry();
	connect(d->capsRegistry, SIGNAL(registered(const CapsSpec&)), SLOT(saveCapabilities()));
}

PsiCon::~PsiCon()
{
	deinit();

	saveCapabilities();
	delete d->capsRegistry;

	delete d->actionList;
	delete d->edb;
	delete d->defaultMenuBar;
	delete d->tabManager;
	delete d;
}

bool PsiCon::init()
{
	// check active profiles
	if (!ActiveProfiles::instance()->setThisProfile(activeProfile))
		return false;

	connect(qApp, SIGNAL(forceSavePreferences()), SLOT(forceSavePreferences()));

	// PGP initialization (needs to be before any gpg usage!)
	PGPUtil::instance();

	d->contactList = new PsiContactList(this);

	connect(d->contactList, SIGNAL(accountAdded(PsiAccount*)), SIGNAL(accountAdded(PsiAccount*)));
	connect(d->contactList, SIGNAL(accountRemoved(PsiAccount*)), SIGNAL(accountRemoved(PsiAccount*)));
	connect(d->contactList, SIGNAL(accountCountChanged()), SIGNAL(accountCountChanged()));
	connect(d->contactList, SIGNAL(accountActivityChanged()), SIGNAL(accountActivityChanged()));
	connect(d->contactList, SIGNAL(saveAccounts()), SLOT(saveAccounts()));

	// do some backuping in case we are about to start migration from config.xml+options.xml
	// to options.xml only.
	QString backupfile = optionsFile() + "-preOptionsMigration";
	if (QFile::exists(pathToProfileConfig(activeProfile))
		&& QFile::exists(optionsFile())
		&& !QFile::exists(backupfile)) {
		QFile::copy(optionsFile(), backupfile);
	}
	
	// advanced widget
	GAdvancedWidget::setStickEnabled( false ); //until this is bugless
	GAdvancedWidget::setStickToWindows( false ); //again
	GAdvancedWidget::setStickAt( 5 );
	
	
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

	// calculate the small font size
	const int minimumFontSize = 7;
	common_smallFontSize = qApp->font().pointSize();
	common_smallFontSize -= 2;
	if ( common_smallFontSize < minimumFontSize )
		common_smallFontSize = minimumFontSize;
	FancyLabel::setSmallFontSize( common_smallFontSize );
	
	
	if (!QFile::exists(optionsFile()) && !QFile::exists(pathToProfileConfig(activeProfile))) {
		if (!options->load(":/options/newprofile.xml")) {
			qWarning("ERROR: Failed to new profile default options");
		}
	}
	
	// load the old profile
	d->optionsMigration.fromFile(pathToProfileConfig(activeProfile));
	
	//load the new profile
	//Save every time an option is changed
	options->load(optionsFile());
	options->autoSave(true, optionsFile());

	//just set a dummy option to trigger saving
	options->setOption("trigger-save",false);
	options->setOption("trigger-save",true);
	
	// do some late migration work
	d->optionsMigration.lateMigration();
	
	QFile accountsFile(pathToProfile( activeProfile ) + "/accounts.xml");
	bool accountMigration = false;	
	if (!accountsFile.exists()) {
		accountMigration = true;
		int idx = 0;
		foreach(UserAccount a, d->optionsMigration.accMigration) {
			QString base = "accounts.a"+QString::number(idx++);
			a.toOptions(&d->accountTree, base);
		}
	} else {
		d->accountTree.loadOptions(accountsFile.fileName(), "accounts", ApplicationInfo::optionsNS());
	}

	// proxy
	d->proxy = new ProxyManager(&d->accountTree, this);
	if (accountMigration) d->proxy->migrateItemList(d->optionsMigration.proxyMigration);
	connect(d->proxy, SIGNAL(settingsChanged()), SLOT(proxy_settingsChanged()));
	
	connect(options, SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
	
	QDir profileDir( pathToProfile( activeProfile ) );
	profileDir.rmdir( "info" ); // remove unused dir



	// first thing, try to load the iconset
	if( !PsiIconset::instance()->loadAll() ) {
		//LEGOPTS.iconset = "stellar";
		//if(!is.load(LEGOPTS.iconset)) {
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
	d->mainwin = new MainWin(PsiOptions::instance()->getOption("options.ui.contactlist.always-on-top").toBool(), (PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool() && PsiOptions::instance()->getOption("options.contactlist.use-toolwindow").toBool()), this, "psimain"); 
	d->mainwin->setUseDock(PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool());

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

	QRect geom = PsiOptions::instance()->getOption("options.ui.contactlist.saved-window-geometry").toRect();
	if (geom.isValid()) d->mainwin->restoreSavedGeometry(geom);
	
	if(!(PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool() && PsiOptions::instance()->getOption("options.contactlist.hide-on-start").toBool()))
		d->mainwin->show();

	d->ftwin = new FileTransDlg(this);

	d->idle.start();

	// S5B
	d->s5bServer = new S5BServer;
	s5b_init();

	// Connect to the system monitor
	SystemWatch* sw = SystemWatch::instance();
	connect(sw, SIGNAL(sleep()), this, SLOT(doSleep()));
	connect(sw, SIGNAL(wakeup()), this, SLOT(doWakeup()));

#ifdef PSI_PLUGINS
	// Plugin Manager
	PluginManager::instance();
#endif

	// Global shortcuts
	setShortcuts();
	
	// FIXME
#ifdef __GNUC__
#warning "Temporary hard-coding caps registration of own version"
#endif
	// client()->identity()

	registerCaps(ApplicationInfo::capsVersion(), QStringList()
	             << "http://jabber.org/protocol/bytestreams"
	             << "http://jabber.org/protocol/si"
	             << "http://jabber.org/protocol/si/profile/file-transfer"
	             << "http://jabber.org/protocol/disco#info"
	             << "http://jabber.org/protocol/commands"
	             << "http://jabber.org/protocol/rosterx"
	             << "http://jabber.org/protocol/muc"
	             << "jabber:x:data"
	            );

	registerCaps("ep", QStringList()
	             << "http://jabber.org/protocol/mood"
	             << "http://jabber.org/protocol/tune"
	             << "http://jabber.org/protocol/physloc"
	             << "http://jabber.org/protocol/geoloc"
	             << "http://www.xmpp.org/extensions/xep-0084.html#ns-data"
	             << "http://www.xmpp.org/extensions/xep-0084.html#ns-metadata"
	            );

	registerCaps("ep-notify", QStringList()
	             << "http://jabber.org/protocol/mood+notify"
	             << "http://jabber.org/protocol/tune+notify"
	             << "http://jabber.org/protocol/physloc+notify"
	             << "http://jabber.org/protocol/geoloc+notify"
	             << "http://www.xmpp.org/extensions/xep-0084.html#ns-metadata+notify"
	            );

	registerCaps("html", QStringList("http://jabber.org/protocol/xhtml-im"));
	registerCaps("cs", QStringList("http://jabber.org/protocol/chatstates"));
	//I've commented out the automatic replies, so commenting out support as well - KIS
	registerCaps("mr", QStringList("urn:xmpp:receipts"));

	// load accounts
	{
		QList<UserAccount> accs;
		QStringList bases = d->accountTree.getChildOptionNames("accounts", true, true);
		foreach (QString base, bases) {
			UserAccount ua;
			ua.fromOptions(&d->accountTree, base);
			accs += ua;
		}
		
		// Disable accounts if necessary, and overwrite locked properties
		if (PsiOptions::instance()->getOption("options.ui.account.single").toBool() || !PsiOptions::instance()->getOption("options.account.domain").toString().isEmpty()) {
			bool haveEnabled = false;
			for(UserAccountList::Iterator it = accs.begin(); it != accs.end(); ++it) {
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
		
		d->contactList->loadAccounts(accs);
	}	
	
	checkAccountsEmpty();
	
	
	// try autologin if needed
	foreach(PsiAccount* account, d->contactList->accounts()) {
		account->autoLogin();
	}
	
	// show tip of the day
	if ( PsiOptions::instance()->getOption("options.ui.tip.show").toBool() ) {
		TipDlg::show(this);
	}

#ifdef USE_DBUS
	addPsiConAdapter(this);
#endif

	connect(ActiveProfiles::instance(), SIGNAL(raiseMainWindow()), SLOT(raiseMainwin()));

	return true;
}

void PsiCon::registerCaps(const QString& ext, const QStringList& features)
{
	DiscoItem::Identity identity = { "client", ApplicationInfo::name(), "pc" };
	DiscoItem::Identities identities;
	identities += identity;

	d->capsRegistry->registerCaps(CapsSpec(ApplicationInfo::capsNode(),
	                                       ApplicationInfo::capsVersion(), ext),
	                              identities,
	                              Features(features));
}

void PsiCon::deinit()
{
	// this deletes all dialogs except for mainwin
	deleteAllDialogs();

	d->idle.stop();

	// shut down all accounts
	UserAccountList acc = d->contactList->getUserAccountList();
	delete d->contactList;

	// delete s5b server
	delete d->s5bServer;

	delete d->ftwin;

	if(d->mainwin) {
		// shut down mainwin
		if (!d->mainwin->isHidden() && !d->mainwin->isMinimized()) {
			PsiOptions::instance()->setOption("options.ui.contactlist.saved-window-geometry", d->mainwin->saveableGeometry());
		}

		delete d->mainwin;
		d->mainwin = 0;
	}

	// TuneController
	delete d->tuneController;

	// save profile
	d->saveProfile(acc);
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
	ActiveProfiles::instance()->unsetThisProfile();
	if(d->contactList->haveActiveAccounts()) {
		QMessageBox::information(0, CAP(tr("Error")), tr("Please disconnect before changing the profile."));
		return;
	}

	quit(QuitProfile);
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
	d->tabManager->deleteAll();
}

AccountsComboBox *PsiCon::accountsComboBox(QWidget *parent, bool online_only)
{
	AccountsComboBox* acb = new AccountsComboBox(parent);
	acb->setController(this);
	acb->setOnlineOnly(online_only);
	return acb;
}

void PsiCon::createAccount(const QString &name, const Jid &j, const QString &pass, bool opt_host, const QString &host, int port, bool legacy_ssl_probe, UserAccount::SSLFlag ssl, QString proxy)
{
	d->contactList->createAccount(name, j, pass, opt_host, host, port, legacy_ssl_probe, ssl, proxy);
}

PsiAccount *PsiCon::createAccount(const UserAccount& acc)
{
	PsiAccount *pa = new PsiAccount(acc, d->contactList, d->capsRegistry, d->tabManager);
	connect(&d->idle, SIGNAL(secondsIdle(int)), pa, SLOT(secondsIdle(int)));
	connect(pa, SIGNAL(updatedActivity()), SLOT(pa_updatedActivity()));
	connect(pa, SIGNAL(updatedAccount()), SLOT(pa_updatedAccount()));
	connect(pa, SIGNAL(queueChanged()), SLOT(queueChanged()));
	connect(pa, SIGNAL(startBounce()), SLOT(startBounce()));
	if (d->s5bServer) {
		pa->client()->s5bManager()->setServer(d->s5bServer);
	}
	return pa;
}

void PsiCon::removeAccount(PsiAccount *pa)
{
	d->contactList->removeAccount(pa);
}

void PsiCon::statusMenuChanged(int x)
{
	if(x == STATUS_OFFLINE && !PsiOptions::instance()->getOption("options.status.ask-for-message-on-offline").toBool()) {
		setGlobalStatus(Status(Status::Offline, "Logged out", 0));
		if(PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool() == true)
			d->mainwin->setTrayToolTip(Status(Status::Offline, "", 0));
	}
	else {
		if(x == STATUS_ONLINE && !PsiOptions::instance()->getOption("options.status.ask-for-message-on-online").toBool()) {
			setGlobalStatus(Status());
			if(PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool() == true)
				d->mainwin->setTrayToolTip(Status());
		}
		else if(x == STATUS_INVISIBLE){
			Status s("","",0,true);
			s.setIsInvisible(true);
			setGlobalStatus(s);
			if(PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool() == true)
				d->mainwin->setTrayToolTip(s);
		}
		else {
			// Create a dialog with the last status message
			StatusSetDlg *w = new StatusSetDlg(this, makeStatus(x, PsiOptions::instance()->getOption("options.status.last-message").toString()));
			connect(w, SIGNAL(set(const XMPP::Status &, bool)), SLOT(setStatusFromDialog(const XMPP::Status &, bool)));
			connect(w, SIGNAL(cancelled()), SLOT(updateMainwinStatus()));
			if(PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool() == true)
				connect(w, SIGNAL(set(const XMPP::Status &, bool)), d->mainwin, SLOT(setTrayToolTip(const XMPP::Status &, bool)));
			w->show();
		}
	}
}

void PsiCon::setStatusFromDialog(const Status &s, bool withPriority)
{
	PsiOptions::instance()->setOption("options.status.last-message", s.status());
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
	d->saveProfile(acc);
}

void PsiCon::saveCapabilities()
{
	QFile file(ApplicationInfo::homeDir() + "/caps.xml");
	d->capsRegistry->save(file);
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

void PsiCon::doOptions()
{
	OptionsDlg *w = (OptionsDlg *)dialogFind("OptionsDlg");
	if(w)
		bringToFront(w);
	else {
		w = new OptionsDlg(this);
		connect(w, SIGNAL(applyOptions()), SLOT(slotApplyOptions()));
		w->show();
	}
}

void PsiCon::doFileTransDlg()
{
	bringToFront(d->ftwin);
}

void PsiCon::checkAccountsEmpty()
{
	if (d->contactList->accounts().count() == 0) {
		promptUserToCreateAccount();
	}
}

void PsiCon::doOpenUri(const QUrl &uriToOpen)
{
	Q_UNUSED(uriToOpen);	
	
/*
	QUrl uri(uriToOpen);	// got to copy, because setQueryDelimiters() is not const

	qWarning("uri:  " + uri.toString());

	// scheme

	if (uri.scheme() != "xmpp") {	// try handling legacy URIs
		QMessageBox::warning(0, tr("Warning"), QString("URI (link) type \"%1\" is unsupported.").arg(uri.scheme()));
	}

	// authority

	PsiAccount *pa = 0;
	if (uri.authority().isEmpty()) {
		pa = d->contactList->defaultAccount();
		if (!pa) {
			QMessageBox::warning(0, tr("Warning"), QString("You don't have any account enabled."));
		}
	}
	else {
		qWarning("uri auth: [" + uri.authority() + "]");

		Jid authJid = JIDUtil::fromString(uri.authority());
		foreach(PsiAccount* acc, d->contactList->enabledAccounts()) {
			if (acc->jid().compare(authJid, false)) {
				pa = acc;
			}
		}

		if (!pa) {
			foreach(PsiAccount* acc, d->contactList->accounts()) {
				if (acc->jid().compare(authJid, false)) {
					QMessageBox::warning(0, tr("Warning"), QString("The account for %1 JID is disabled right now.").arg(authJid.bare()));
					return;	// FIX-ME: Should suggest enabling it now
				}
			}
		}
		if (!pa) {
			QMessageBox::warning(0, tr("Warning"), QString("You don't have an account for %1.").arg(authJid.bare()));
			return;
		}
	}

	// entity

	QString path = uri.path();
	if (path.startsWith('/'))	// this happens when authority part is present
		path = path.mid(1);
	Jid entity = JIDUtil::fromString(path);

	// query

	uri.setQueryDelimiters('=', ';');

	QString querytype = uri.queryItems().value(0).first;	// defaults to empty string

	if (querytype == "message") {
		if (uri.queryItemValue("type") == "chat")
			pa->actionOpenChat(entity);
		else {
			pa->dj_newMessage(entity, uri.queryItemValue("body"), uri.queryItemValue("subject"), uri.queryItemValue("thread"));
		}
	}
	else if (querytype == "roster") {
		pa->openAddUserDlg(entity, uri.queryItemValue("name"), uri.queryItemValue("group"));
	}
	else if (querytype == "join") {
		pa->actionJoin(entity, uri.queryItemValue("password"));
	}
	else if (querytype == "vcard") {
		pa->actionInfo(entity);
	}
	else if (querytype == "disco") {
		pa->actionDisco(entity, uri.queryItemValue("node"));
	}
	else {
		pa->actionSendMessage(entity);
	}
*/}

void PsiCon::doToolbars()
{
	OptionsDlg *w = (OptionsDlg *)dialogFind("OptionsDlg");
	if (w) {
		w->openTab("toolbars");
		bringToFront(w);
	}
	else {
		w = new OptionsDlg(this);
		connect(w, SIGNAL(applyOptions(const Options &)), SLOT(slotApplyOptions(const Options &)));
		w->openTab("toolbars");
		w->show();
	}
}

void PsiCon::optionChanged(const QString& option)
{
	// Global shortcuts
	setShortcuts();
	
	if (option == "options.ui.notifications.alert-style") {
		alertIconUpdateAlertStyle();		
	}
	
	if ( option == "options.ui.tabs.use-tabs") {
		QMessageBox::information(0, tr("Information"), tr("Some of the options you changed will only have full effect upon restart."));
		//notifyRestart = false;
	}
	
	// update s5b
	if(option == "options.p2p.bytestreams.listen-port") {
		s5b_init();
	}

/* LEGOPTFIXME
	// change icon set
	if ( "options.iconsets.system" "options.iconsets.emoticons" "options.iconsets.status"	||
	     operator!=(LEGOPTS.serviceRosterIconset,oldOpt.serviceRosterIconset)	||
	     operator!=(LEGOPTS.customRosterIconset,oldOpt.customRosterIconset) )
	{
		if ( notifyRestart && PsiIconset::instance()->optionsChanged(&oldOpt) )
			QMessageBox::information(0, tr("Information"), tr("The complete iconset update will happen on next Psi start."));

		// update icon selector
		d->updateIconSelect();

		// flush the QPixmapCache
		QPixmapCache::clear();
	}
*/
}

void PsiCon::slotApplyOptions()
{
	PsiOptions *o = PsiOptions::instance();

#ifndef Q_WS_MAC
	if (!PsiOptions::instance()->getOption("options.ui.contactlist.show-menubar").toBool()) {
		// check if all toolbars are disabled
		bool toolbarsVisible = false;
		QStringList bases = o->getChildOptionNames("options.ui.contactlist.toolbars", true, true);
		foreach(QString base, bases) {
			toolbarsVisible = toolbarsVisible || o->getOption( base + ".visible").toBool();
		}

		// Check whether it is legal to disable the menubar
		if ( !toolbarsVisible ) {
			QMessageBox::warning(0, tr("Warning"),
				tr("You can not disable <i>all</i> toolbars <i>and</i> the menubar. If you do so, you will be unable to enable them back, when you'll change your mind."),
				tr("I understand"));
			PsiOptions::instance()->setOption("options.ui.contactlist.show-menubar", true);
		}
	}
#endif

	updateS5BServerAddresses();

	// mainwin stuff
	d->mainwin->setWindowOpts(PsiOptions::instance()->getOption("options.ui.contactlist.always-on-top").toBool(), (PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool() && PsiOptions::instance()->getOption("options.contactlist.use-toolwindow").toBool()));
	d->mainwin->setUseDock(PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool());


	// notify about options change
	emitOptionsUpdate();
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
		// Update the event count
		MacDock::overlay(nextAmount ? QString::number(nextAmount) : QString());

		if (!nextAmount) {
			MacDock::stopBounce();
		}
	}
#endif

	d->mainwin->updateReadNext(nextAnim, nextAmount);
}

void PsiCon::startBounce()
{
#ifdef Q_WS_MAC
	if (PsiOptions::instance()->getOption("options.ui.notifications.bounce-dock").toString() != "never") {
		MacDock::startBounce();
		if (PsiOptions::instance()->getOption("options.ui.notifications.bounce-dock").toString() == "once") {
			MacDock::stopBounce();
		}
	}
#endif
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
		pa->openNextEvent(UserAction);
}

void PsiCon::playSound(const QString &str)
{
	if(str.isEmpty() || !PsiOptions::instance()->getOption("options.ui.notifications.sounds.enable").toBool())
		return;

	soundPlay(str);
}

void PsiCon::raiseMainwin()
{
	d->mainwin->showNoFocus();
}

QStringList PsiCon::recentGCList() const
{
	return PsiOptions::instance()->getOption("options.muc.recent-joins.jids").toStringList();
}

void PsiCon::recentGCAdd(const QString &str)
{
	QStringList recentList = recentGCList();
	
	// remove it if we have it
	for(QStringList::Iterator it = recentList.begin(); it != recentList.end(); ++it) {
		if(*it == str) {
			recentList.remove(it);
			break;
		}
	}

	// put it in the front
	recentList.prepend(str);

	// trim the list if bigger than 10
	while(recentList.count() > PsiOptions::instance()->getOption("options.muc.recent-joins.maximum").toInt()) {
		recentList.remove(recentList.fromLast());
	}
	
	PsiOptions::instance()->setOption("options.muc.recent-joins.jids", recentList);
}

QStringList PsiCon::recentBrowseList() const
{
	return PsiOptions::instance()->getOption("options.ui.service-discovery.recent-jids").toStringList();
}

void PsiCon::recentBrowseAdd(const QString &str)
{
	QStringList recentList = recentBrowseList();
	// remove it if we have it
	for(QStringList::Iterator it = recentList.begin(); it != recentList.end(); ++it) {
		if(*it == str) {
			recentList.remove(it);
			break;
		}
	}

	// put it in the front
	recentList.prepend(str);

	// trim the list if bigger than 10
	while(recentList.count() > 10) {
		recentList.remove(recentList.fromLast());
	}
	
	PsiOptions::instance()->setOption("options.ui.service-discovery.recent-jids", recentList);
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

void PsiCon::proxy_settingsChanged()
{
	saveAccounts();
}

IconSelectPopup *PsiCon::iconSelectPopup() const
{
	return d->iconSelect;
}

void PsiCon::processEvent(PsiEvent *e, ActivationType activationType)
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
		PsiAccount* account = e->account();
		XMPP::Jid from = e->from();

		if ( PsiOptions::instance()->getOption("options.ui.chat.alert-for-already-open-chats").toBool() && sentToChatWindow ) {
			// Message already displayed, need only to pop up chat dialog, so that
			// it will be read (or marked as read)
			ChatDlg *c = account->findChatDialog(from);
			if(!c)
				c = account->findChatDialog(e->jid());
			if(!c)
				return; // should never happen

			account->processChats(from); // this will delete all events, corresponding to that chat dialog
		}

		// as the event could be deleted just above, we're using cached account and from values
		account->openChat(from, activationType);
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
	if (!saveableGeometry.isNull())
		PsiOptions::instance()->setOption("options.ui.contactlist.saved-window-geometry", saveableGeometry);
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
	if(!PsiOptions::instance()->getOption("options.p2p.bytestreams.external-address").toString().isEmpty()) {
		bool found = false;
		for(QStringList::ConstIterator sit = slist.begin(); sit != slist.end(); ++sit) {
			const QString &s = *sit;
			if(s == PsiOptions::instance()->getOption("options.p2p.bytestreams.external-address").toString()) {
				found = true;
				break;
			}
		}
		if(!found)
			slist += PsiOptions::instance()->getOption("options.p2p.bytestreams.external-address").toString();
	}

	// set up the server
	d->s5bServer->setHostList(slist);
}

void PsiCon::s5b_init()
{
	if(d->s5bServer->isActive())
		d->s5bServer->stop();

	if (PsiOptions::instance()->getOption("options.p2p.bytestreams.listen-port").toInt()) {
		if(!d->s5bServer->start(PsiOptions::instance()->getOption("options.p2p.bytestreams.listen-port").toInt())) {
			QMessageBox::warning(0, tr("Warning"), tr("Unable to bind to port %1 for Data Transfer.\nThis may mean you are already running another instance of Psi. You may experience problems sending and/or receiving files.").arg(PsiOptions::instance()->getOption("options.p2p.bytestreams.listen-port").toInt()));
		}
	}
}

void PsiCon::doSleep()
{
	setGlobalStatus(Status(Status::Offline, tr("Computer went to sleep"), 0));
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

void PsiCon::addToolbar(const QString &base)
{
	d->mainwin->addToolbar(base);
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

QString PsiCon::optionsFile() const
{
	return pathToProfile(activeProfile) + "/options.xml";
}

void PsiCon::forceSavePreferences()
{
	PsiOptions::instance()->save(optionsFile());
}
 
#include "psicon.moc"

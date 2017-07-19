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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psicon.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QMenuBar>
#include <QPointer>
#include <QIcon>
#include <QColor>
#include <QImage>
#include <QPixmapCache>
#include <QFile>
#include <QPixmap>
#include <QList>
#include <QImageReader>
#include <QMessageBox>
#include <QDir>

#include "s5b.h"
#include "xmpp_caps.h"
#include "psiaccount.h"
#include "activeprofiles.h"
#include "accountadddlg.h"
#include "psiiconset.h"
#include "psithememanager.h"
#include "psievent.h"
#include "passphrasedlg.h"
#include "common.h"
#include "mainwin.h"
#include "idle/idle.h"
#include "accountmanagedlg.h"
#include "statusdlg.h"
#include "options/optionsdlg.h"
#include "options/opt_toolbars.h"
#include "accountregdlg.h"
#include "tunecontrollermanager.h"
#include "mucjoindlg.h"
#include "userlist.h"
#include "eventdlg.h"
#ifdef HAVE_PGPUTIL
#include "pgputil.h"
#endif
#include "edbflatfile.h"
#include "proxy.h"
#ifdef PSIMNG
#include "psimng.h"
#endif
#include "alerticon.h"
#include "iconselect.h"
#include "psitoolbar.h"
#ifdef FILETRANSFER
#include "filetransfer.h"
#include "filetransdlg.h"
#endif
#include "accountmodifydlg.h"
#include "psiactionlist.h"
#include "applicationinfo.h"
#include "jidutil.h"
#include "systemwatch/systemwatch.h"
#include "accountscombobox.h"
#include "tabdlg.h"
#include "chatdlg.h"
#ifdef GROUPCHAT
#include "groupchatdlg.h"
#endif
#include "spellchecker/aspellchecker.h"
#include "networkaccessmanager.h"
#ifdef WEBKIT
#include "avatars.h"
#include "chatviewthemeprovider.h"
#include "webview.h"
#endif
#include "urlobject.h"
#include "anim.h"
#include "psioptions.h"
#include "psirichtext.h"
#ifdef PSI_PLUGINS
#include "pluginmanager.h"
#endif
#include "psicontactlist.h"
#include "dbus.h"
#include "tipdlg.h"
#include "shortcutmanager.h"
#include "globalshortcut/globalshortcutmanager.h"
#include "desktoputil.h"
#include "tabmanager.h"
#include "xmpp_xmlcommon.h"
#include "psicapsregsitry.h"
#include "psicontact.h"
#include "contactupdatesmanager.h"
#include "avcall/avcall.h"
#include "avcall/calldlg.h"
#include "alertmanager.h"
#include "bosskey.h"
#include "popupmanager.h"
#ifdef WHITEBOARDING
#include "whiteboarding/wbmanager.h"
#endif

#include "AutoUpdater/AutoUpdater.h"
#ifdef HAVE_SPARKLE
#include "AutoUpdater/SparkleAutoUpdater.h"
#endif

#ifdef Q_OS_MAC
#include "mac_dock/mac_dock.h"
#endif

// from opt_avcall.cpp
extern void options_avcall_update();

static const char *tunePublishOptionPath = "options.extended-presence.tune.publish";
static const char *tuneUrlFilterOptionPath = "options.extended-presence.tune.url-filter";
static const char *tuneTitleFilterOptionPath = "options.extended-presence.tune.title-filter";
static const char *tuneControllerFilterOptionPath = "options.extended-presence.tune.controller-filter";

//----------------------------------------------------------------------------
// PsiConObject
//----------------------------------------------------------------------------
class PsiConObject : public QObject
{
	Q_OBJECT
public:
	PsiConObject(QObject *parent)
	: QObject(parent)
	{
		QDir p(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation));
		QDir v(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation) + "/tmp-sounds");
		if(!v.exists())
			p.mkdir("tmp-sounds");
		Iconset::setSoundPrefs(v.absolutePath(), this, SLOT(playSound(QString)));
		connect(URLObject::getInstance(), SIGNAL(openURL(QString)), SLOT(openURL(QString)));
	}

	~PsiConObject()
	{
		// removing temp dirs
		QDir p(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation));
		QDir v(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation) + "/tmp-sounds");
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
					return false;
			}
			else {
				//printf("deleting [%s]\n", info.filePath().latin1());
				d.remove(info.fileName());
			}
		}
		QString name = d.dirName();
		if(!d.cdUp())
			return false;
		//printf("removing folder [%s]\n", d.filePath(name).latin1());
		d.rmdir(name);

		return true;
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

class PsiCon::Private : public QObject
{
	Q_OBJECT
public:
	Private(PsiCon *parent)
		: QObject(parent)
		, contactList(0)
		, iconSelect(0)
		, quitting(false)
		, alertManager(parent)
		, bossKey(0)
		, popupManager(0)
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
		QStringList order;
		foreach(UserAccount ua, acc) {
			if (!ua.optionsBase.isEmpty()) {
				ua.toOptions(&accountTree);
				cbases += ua.optionsBase;
			}
			order.append(ua.id);
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
		accountTree.setOption("order", order);
		QFile accountsFile(pathToProfile(activeProfile, ApplicationInfo::ConfigLocation) + "/accounts.xml");
		accountTree.saveOptions(accountsFile.fileName(), "accounts", ApplicationInfo::optionsNS(), ApplicationInfo::version());;

	}

private slots:
	void updateIconSelect()
	{
		Iconset iss;
		foreach(Iconset* iconset, PsiIconset::instance()->emoticons) {
			iss += *iconset;
		}

		iconSelect->setIconset(iss);
		QPixmapCache::clear();
	}

public:
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
	IconSelectPopup *iconSelect;
	NetworkAccessManager *nam;
#ifdef FILETRANSFER
	FileTransDlg *ftwin;
#endif
	PsiActionList *actionList;
	//GlobalAccelManager *globalAccelManager;
	TuneControllerManager* tuneManager;
	QMenuBar* defaultMenuBar;
	TabManager *tabManager;
	bool quitting;
	QTimer* updatedAccountTimer_;
	AutoUpdater *autoUpdater;
	AlertManager alertManager;
	BossKey *bossKey;
	PopupManager * popupManager;

	struct IdleSettings
	{
		IdleSettings() : secondsIdle(0)
		{}

		void update()
		{
			PsiOptions *o = PsiOptions::instance();
			useOffline = o->getOption("options.status.auto-away.use-offline").toBool();
			useNotAvailable = o->getOption("options.status.auto-away.use-not-availible").toBool();
			useAway = o->getOption("options.status.auto-away.use-away").toBool();
			offlineAfter = o->getOption("options.status.auto-away.offline-after").toInt();
			notAvailableAfter = o->getOption("options.status.auto-away.not-availible-after").toInt();
			awayAfter = o->getOption("options.status.auto-away.away-after").toInt();
			menuXA = o->getOption("options.ui.menu.status.xa").toBool();
			useIdleServer = o->getOption("options.service-discovery.last-activity").toBool();
		}

		bool useOffline, useNotAvailable, useAway, menuXA;
		int offlineAfter, notAvailableAfter, awayAfter;
		int secondsIdle;
		bool useIdleServer;
	};

	IdleSettings idleSettings_;
};

//----------------------------------------------------------------------------
// PsiCon
//----------------------------------------------------------------------------

PsiCon::PsiCon()
	: QObject(0)
{
	//pdb(DEBUG_JABCON, QString("%1 v%2\n By Justin Karneges\n    infiniti@affinix.com\n\n").arg(PROG_NAME).arg(PROG_VERSION));
	d = new Private(this);
	d->tabManager = new TabManager(this);
	connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), SLOT(aboutToQuit()));


	d->mainwin = 0;
#ifdef FILETRANSFER
	d->ftwin = 0;
#endif

	d->edb = new EDBFlatFile(this);

	d->s5bServer = 0;
	d->tuneManager = 0;
	d->autoUpdater = 0;

	d->actionList = 0;
	d->defaultMenuBar = new QMenuBar(0);

	XMPP::CapsRegistry::setInstance(new PsiCapsRegistry(this));
	XMPP::CapsRegistry *pcr = XMPP::CapsRegistry::instance();
	connect(pcr, SIGNAL(destroyed(QObject*)), pcr, SLOT(save()));
	connect(pcr, SIGNAL(registered(const XMPP::CapsSpec&)), pcr, SLOT(save()));
	pcr->load();
}

PsiCon::~PsiCon()
{
	deinit();

	delete d->autoUpdater;
	delete d->actionList;
	delete d->edb;
	delete d->defaultMenuBar;
	delete d->tabManager;
	delete d->popupManager;
	delete d;
}

bool PsiCon::init()
{
	// check active profiles
	if (!ActiveProfiles::instance()->setThisProfile(activeProfile))
		return false;

	connect(qApp, SIGNAL(forceSavePreferences()), SLOT(forceSavePreferences()));

#ifdef HAVE_PGPUTIL
	// PGP initialization (needs to be before any gpg usage!)
	PGPUtil::instance();
#endif

	d->contactList = new PsiContactList(this);

	connect(d->contactList, SIGNAL(accountAdded(PsiAccount*)), SIGNAL(accountAdded(PsiAccount*)));
	connect(d->contactList, SIGNAL(accountRemoved(PsiAccount*)), SIGNAL(accountRemoved(PsiAccount*)));
	connect(d->contactList, SIGNAL(accountCountChanged()), SIGNAL(accountCountChanged()));
	connect(d->contactList, SIGNAL(accountActivityChanged()), SIGNAL(accountActivityChanged()));
	connect(d->contactList, SIGNAL(saveAccounts()), SLOT(saveAccounts()));
	connect(d->contactList, SIGNAL(queueChanged()), SLOT(queueChanged()));

	d->updatedAccountTimer_ = new QTimer(this);
	d->updatedAccountTimer_->setSingleShot(true);
	d->updatedAccountTimer_->setInterval(1000);
	connect(d->updatedAccountTimer_, SIGNAL(timeout()), SLOT(saveAccounts()));


	QString oldConfig = pathToProfileConfig(activeProfile);
	if(QFile::exists(oldConfig)) {
		QMessageBox::warning(d->mainwin, tr("Migration is impossible"),
		                     tr("Found no more supported configuration file from some very old version:\n%1\n\n"
		                        "Migration is possible with Psi-0.15")
		                     .arg(oldConfig));
	}

	// advanced widget
	GAdvancedWidget::setStickEnabled( false ); //until this is bugless
	GAdvancedWidget::setStickToWindows( false ); //again
	GAdvancedWidget::setStickAt( 5 );

	PsiRichText::setAllowedImageDirs(QStringList()
									 << ApplicationInfo::resourcesDir()
									 << ApplicationInfo::homeDir(ApplicationInfo::CacheLocation));

	// To allow us to upgrade from old hardcoded options gracefully, be careful about the order here
	PsiOptions *options=PsiOptions::instance();
	//load the system-wide defaults, if they exist
	QString systemDefaults=ApplicationInfo::resourcesDir();
	systemDefaults += "/options-default.xml";
	//qWarning(qPrintable(QString("Loading system defaults from %1").arg(systemDefaults)));
	options->load(systemDefaults);

	if (!PsiOptions::exists(optionsFile())) {
		if (!options->newProfile()) {
			qWarning("ERROR: Failed to new profile default options");
		}
	}

	//load the new profile
	options->load(optionsFile());
	//Save every time an option is changed
	options->autoSave(true, optionsFile());

	//just set a dummy option to trigger saving
	options->setOption("trigger-save",false);
	options->setOption("trigger-save",true);

	// do some late migration work
	d->optionsMigration.lateMigration();

#ifdef USE_PEP
	// Create the tune controller
	d->tuneManager = new TuneControllerManager();
#endif

	// Auto updater initialization
#ifdef HAVE_SPARKLE
	d->autoUpdater = new SparkleAutoUpdater(ApplicationInfo::getAppCastURL());
#endif
	if (d->autoUpdater && options->getOption("options.auto-update.check-on-startup").toBool()) {
		d->autoUpdater->checkForUpdates();
	}

	// calculate the small font size
	const int minimumFontSize = 7;
	common_smallFontSize = qApp->font().pointSize();
	common_smallFontSize -= 2;
	if ( common_smallFontSize < minimumFontSize )
		common_smallFontSize = minimumFontSize;
	FancyLabel::setSmallFontSize( common_smallFontSize );

	QFile accountsFile(pathToProfile(activeProfile, ApplicationInfo::ConfigLocation) + "/accounts.xml");
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
	ProxyManager *proxy = ProxyManager::instance();
	proxy->init(&d->accountTree);
	if (accountMigration) proxy->migrateItemList(d->optionsMigration.proxyMigration);
	connect(proxy, SIGNAL(settingsChanged()), SLOT(proxy_settingsChanged()));

	connect(options, SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));


	contactUpdatesManager_ = new ContactUpdatesManager(this);

	QDir profileDir( pathToProfile(activeProfile, ApplicationInfo::DataLocation) );
	profileDir.rmdir( "info" ); // remove unused dir

	d->iconSelect = new IconSelectPopup(0);
	connect(PsiIconset::instance(), SIGNAL(emoticonsChanged()), d, SLOT(updateIconSelect()));

	const QString css = options->getOption("options.ui.chat.css").toString();
	if (!css.isEmpty())
		d->iconSelect->setStyleSheet(css);

	// first thing, try to load the iconset
	bool result = true;;
	if( !PsiIconset::instance()->loadAll() ) {
		//LEGOPTS.iconset = "stellar";
		//if(!is.load(LEGOPTS.iconset)) {
			QMessageBox::critical(0, tr("Error"), tr("Unable to load iconset!  Please make sure Psi is properly installed."));
			result = false;
		//}
	}

	d->nam = new NetworkAccessManager(this);
#ifdef WEBKIT
	PsiThemeManager::instance()->registerProvider(
			new ChatViewThemeProvider(this), true);
	PsiThemeManager::instance()->registerProvider(
			new GroupChatViewThemeProvider(this), true);
#endif

	if( !PsiThemeManager::instance()->loadAll() ) {
		QMessageBox::critical(0, tr("Error"), tr("Unable to load theme!  Please make sure Psi is properly installed."));
		result = false;
	}

	if ( !d->actionList )
		d->actionList = new PsiActionList( this );

	PsiConObject* psiConObject = new PsiConObject(this);

	Anim::setMainThread(QThread::currentThread());

	// setup the main window
	d->mainwin = new MainWin(options->getOption("options.ui.contactlist.always-on-top").toBool(), (options->getOption("options.ui.systemtray.enable").toBool() && options->getOption("options.contactlist.use-toolwindow").toBool()), this);
	d->mainwin->setUseDock(options->getOption("options.ui.systemtray.enable").toBool());
	d->bossKey = new BossKey(d->mainwin);

	Q_UNUSED(psiConObject);

	connect(d->mainwin, SIGNAL(closeProgram()), SLOT(closeProgram()));
	connect(d->mainwin, SIGNAL(changeProfile()), SLOT(changeProfile()));
	connect(d->mainwin, SIGNAL(doManageAccounts()), SLOT(doManageAccounts()));
	connect(d->mainwin, SIGNAL(doGroupChat()), SLOT(doGroupChat()));
	connect(d->mainwin, SIGNAL(blankMessage()), SLOT(doNewBlankMessage()));
	connect(d->mainwin, SIGNAL(statusChanged(XMPP::Status::Type)), SLOT(statusMenuChanged(XMPP::Status::Type)));
	connect(d->mainwin, SIGNAL(statusMessageChanged(QString)), SLOT(setStatusMessage(QString)));
	connect(d->mainwin, SIGNAL(doOptions()), SLOT(doOptions()));
	connect(d->mainwin, SIGNAL(doToolbars()), SLOT(doToolbars()));
	connect(d->mainwin, SIGNAL(doFileTransDlg()), SLOT(doFileTransDlg()));
	connect(d->mainwin, SIGNAL(recvNextEvent()), SLOT(recvNextEvent()));
	connect(this, SIGNAL(emitOptionsUpdate()), d->mainwin, SLOT(optionsUpdate()));

	d->mainwin->setGeometryOptionPath("options.ui.contactlist.saved-window-geometry");

	if (result &&
	    !(options->getOption("options.ui.systemtray.enable").toBool() &&
	      options->getOption("options.contactlist.hide-on-start").toBool()))
	{
		d->mainwin->show();
	}

	connect(&d->idle, SIGNAL(secondsIdle(int)), SLOT(secondsIdle(int)));

	//PopupDurationsManager
	d->popupManager = new PopupManager(this);

	// S5B
	d->s5bServer = new S5BServer;
	s5b_init();

	// Connect to the system monitor
	SystemWatch* sw = SystemWatch::instance();
	connect(sw, SIGNAL(sleep()), this, SLOT(doSleep()));
	connect(sw, SIGNAL(wakeup()), this, SLOT(doWakeup()));

#ifdef PSI_PLUGINS
	// Plugin Manager
	PluginManager::instance()->initNewSession(this);
#endif

	// Global shortcuts
	setShortcuts();

	// load accounts
	{
		QList<UserAccount> accs;
		QStringList bases = d->accountTree.getChildOptionNames("accounts", true, true);
		foreach (QString base, bases) {
			UserAccount ua;
			ua.fromOptions(&d->accountTree, base);
			accs += ua;
		}
		QStringList order = d->accountTree.getOption("order").toStringList();
		int start = 0;
		foreach (const QString &id, order) {
			for (int i = start; i < accs.size(); ++i) {
				if (accs[i].id == id) {
					accs.move(i, start);
					start++;
					break;
				}
			}
		}

		// Disable accounts if necessary, and overwrite locked properties
		bool single = options->getOption("options.ui.account.single").toBool();
		QString domain = options->getOption("options.account.domain").toString();
		if (single || !domain.isEmpty()) {
			bool haveEnabled = false;
			for(UserAccountList::Iterator it = accs.begin(); it != accs.end(); ++it) {
				// With single accounts, only modify the first account
				if (single) {
					if (!haveEnabled) {
						haveEnabled = it->opt_enabled;
						if (it->opt_enabled) {
							if (!domain.isEmpty())
								it->jid = JIDUtil::accountFromString(Jid(it->jid).node()).bare();
						}
					}
					else
						it->opt_enabled = false;
				}
				else {
					// Overwirte locked properties
					if (!domain.isEmpty())
						it->jid = JIDUtil::accountFromString(Jid(it->jid).node()).bare();
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
	if(d->contactList->defaultAccount())
		emit statusMessageChanged(d->contactList->defaultAccount()->status().status());

	// show tip of the day
	if ( options->getOption("options.ui.tip.show").toBool() ) {
		TipDlg::show(this);
	}

#ifdef USE_DBUS
	addPsiConAdapter(this);
#endif

	connect(ActiveProfiles::instance(), SIGNAL(setStatusRequested(const QString &, const QString &)), SLOT(setStatusFromCommandline(const QString &, const QString &)));
	connect(ActiveProfiles::instance(), SIGNAL(openUriRequested(const QString &)), SLOT(openUri(const QString &)));
	connect(ActiveProfiles::instance(), SIGNAL(raiseRequested()), SLOT(raiseMainwin()));


	DesktopUtil::setUrlHandler("xmpp", this, "openUri");
	DesktopUtil::setUrlHandler("x-psi-atstyle", this, "openAtStyleUri");

	if(AvCallManager::isSupported()) {
		options_avcall_update();
		AvCallManager::setAudioOutDevice(options->getOption("options.media.devices.audio-output").toString());
		AvCallManager::setAudioInDevice(options->getOption("options.media.devices.audio-input").toString());
		AvCallManager::setVideoInDevice(options->getOption("options.media.devices.video-input").toString());
		AvCallManager::setBasePort(options->getOption("options.p2p.bytestreams.listen-port").toInt());
		AvCallManager::setExternalAddress(options->getOption("options.p2p.bytestreams.external-address").toString());
	}


#ifdef USE_PEP
	optionChanged(tuneControllerFilterOptionPath);
	optionChanged(tuneUrlFilterOptionPath);
#endif

	//init spellchecker
	optionChanged("options.ui.spell-check.langs");

	return result;
}

bool PsiCon::haveAutoUpdater() const
{
	return d->autoUpdater != 0;
}

void PsiCon::updateStatusPresets()
{
	emit statusPresetsChanged();
}

void PsiCon::deinit()
{
	// this deletes all dialogs except for mainwin
	deleteAllDialogs();

	d->idle.stop();

	// shut down all accounts
	UserAccountList acc;
	if(d->contactList) {
		acc = d->contactList->getUserAccountList();
		delete d->contactList;
	}
	d->nam->releaseHandlers();

	// delete s5b server
	delete d->s5bServer;

#ifdef FILETRANSFER
	delete d->ftwin;
	d->ftwin = NULL;
#endif

	if(d->mainwin) {
		delete d->mainwin;
		d->mainwin = 0;
	}

	// TuneController
	delete d->tuneManager;

	// save profile
	if(d->contactList)
		d->saveProfile(acc);
#ifdef PSI_PLUGINS
	PluginManager::instance()->unloadAllPlugins();
#endif
	GlobalShortcutManager::clear();

	DesktopUtil::unsetUrlHandler("xmpp");
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
	ShortcutManager::connect("global.boss-key", d->bossKey, SLOT(shortCutActivated()));
#ifdef PSI_PLUGINS
	PluginManager::instance()->setShortcuts();
#endif
}

PsiContactList* PsiCon::contactList() const
{
	return d->contactList;
}

EDB *PsiCon::edb() const
{
	return d->edb;
}

FileTransDlg *PsiCon::ftdlg()
{
#ifdef FILETRANSFER
	if (!d->ftwin)
		d->ftwin = new FileTransDlg(this);
	return d->ftwin;
#else
	return 0;
#endif
}

PopupManager* PsiCon::popupManager() const
{
	return d->popupManager;
}

struct OptFeatureMap {
	OptFeatureMap(const QString &option, const QStringList &feature) :
	    option(option), feature(feature) {}
	QString option;
	QStringList feature;
};

QStringList PsiCon::xmppFatures() const
{
	QStringList features = QStringList() << "http://jabber.org/protocol/commands"
		<< "http://jabber.org/protocol/rosterx"
#ifdef GROUPCHAT
		<< "http://jabber.org/protocol/muc"
#endif
		<< "http://jabber.org/protocol/mood"
		<< "http://jabber.org/protocol/activity"
		<< "http://jabber.org/protocol/tune"
		<< "http://jabber.org/protocol/geoloc"
		<< "urn:xmpp:avatar:data"
		<< "urn:xmpp:avatar:metadata";

	if(AvCallManager::isSupported()) {
		features << "urn:xmpp:jingle:1";
		features << "urn:xmpp:jingle:transports:ice-udp:1";
		features << "urn:xmpp:jingle:apps:rtp:1";
		features << "urn:xmpp:jingle:apps:rtp:audio";

		if(AvCallManager::isVideoSupported()) {
			features << "urn:xmpp:jingle:apps:rtp:video";
		}
	}

	static QList<OptFeatureMap> fmap = QList<OptFeatureMap>()
	        << OptFeatureMap("options.service-discovery.last-activity", QStringList() << "jabber:iq:last")
	        << OptFeatureMap("options.html.chat.render", QStringList() << "http://jabber.org/protocol/xhtml-im")
	        << OptFeatureMap("options.extended-presence.notify", QStringList() << "http://jabber.org/protocol/mood+notify"
	                                                                  << "http://jabber.org/protocol/activity+notify"
	                                                                  << "http://jabber.org/protocol/tune+notify"
	                                                                  << "http://jabber.org/protocol/geoloc+notify"
			                                                          << "urn:xmpp:avatar:metadata+notify")
	        << OptFeatureMap("options.messages.send-composing-events", QStringList() << "http://jabber.org/protocol/chatstates")
	        << OptFeatureMap("options.ui.notifications.send-receipts", QStringList() << "urn:xmpp:receipts");

	foreach (const OptFeatureMap &f, fmap) {
		if(PsiOptions::instance()->getOption(f.option).toBool()) {
			features << f.feature;
		}
	}

	return features;
}

TabManager *PsiCon::tabManager() const
{
	return d->tabManager;
}

NetworkAccessManager *PsiCon::networkAccessManager() const
{
	return d->nam;
}

TuneControllerManager *PsiCon::tuneManager() const
{
	return d->tuneManager;
}

AlertManager *PsiCon::alertManager() const {
	return &(d->alertManager);
}


void PsiCon::closeProgram()
{
	doQuit(QuitProgram);
}

void PsiCon::changeProfile()
{
	ActiveProfiles::instance()->unsetThisProfile();
	if(d->contactList->haveActiveAccounts()) {
		QMessageBox messageBox(QMessageBox::Information, CAP(tr("Error")), tr("Please disconnect before changing the profile."));
		QPushButton* cancel = messageBox.addButton(QMessageBox::Cancel);
		QPushButton* disconnect = messageBox.addButton(tr("&Disconnect"), QMessageBox::AcceptRole);
		messageBox.setDefaultButton(disconnect);
		messageBox.exec();
		if (messageBox.clickedButton() == cancel)
			return;

		setGlobalStatus(XMPP::Status::Offline, false, true);
	}

	doQuit(QuitProfile);
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
#ifdef GROUPCHAT
	PsiAccount *account = d->contactList->defaultAccount();
	if(!account)
		return;

	MUCJoinDlg *w = new MUCJoinDlg(this, account);
	w->show();
#endif
}

void PsiCon::doNewBlankMessage()
{
	PsiAccount *account = d->contactList->defaultAccount();
	if(!account)
		return;

	EventDlg *w = createMessageDlg("", account);
	if (!w)
		return;

	w->show();
}

EventDlg *PsiCon::createMessageDlg(const QString &to, PsiAccount *pa)
{
	if (!EventDlg::messagingEnabled())
		return 0;

	return createEventDlg(to, pa);
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
	i->className = w->metaObject()->className();
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

PsiAccount* PsiCon::createAccount(const QString &name, const Jid &j, const QString &pass, bool opt_host, const QString &host, int port, bool legacy_ssl_probe, UserAccount::SSLFlag ssl, QString proxy, const QString &tlsOverrideDomain, const QByteArray &tlsOverrideCert)
{
	return d->contactList->createAccount(name, j, pass, opt_host, host, port, legacy_ssl_probe, ssl, proxy, tlsOverrideDomain, tlsOverrideCert);
}

PsiAccount *PsiCon::createAccount(const UserAccount& _acc)
{
	UserAccount acc = _acc;
	PsiAccount *pa = new PsiAccount(acc, d->contactList, d->tabManager);
//	connect(&d->idle, SIGNAL(secondsIdle(int)), pa, SLOT(secondsIdle(int)));
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

void PsiCon::setAccountsOrder(QList<PsiAccount*> accounts)
{
	d->contactList->setAccountsOrder(accounts);
}

void PsiCon::statusMenuChanged(XMPP::Status::Type x, bool forceDialog)
{
	QString optionName;
	if (!forceDialog)
	{
		switch (x) {
		case STATUS_OFFLINE:
			optionName = "offline";
			break;
		case STATUS_ONLINE:
			optionName = "online";
			break;
		case STATUS_CHAT:
			optionName = "chat";
			break;
		case STATUS_AWAY:
			optionName = "away";
			break;
		case STATUS_XA:
			optionName = "xa";
			break;
		case STATUS_DND:
			optionName = "dnd";
			break;
		default:
			break;
		}
	}
	PsiOptions* o = PsiOptions::instance();
	//If option name is not empty (it is empty for Invisible) and option is set to ask for message, show dialog
	if (forceDialog || (!optionName.isEmpty() && o->getOption("options.status.ask-for-message-on-" + optionName).toBool())) {
		StatusSetDlg *w = new StatusSetDlg(this, makeLastStatus(x), lastPriorityNotEmpty());
		connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)), SLOT(setGlobalStatus(const XMPP::Status &, bool, bool)));
		connect(w, SIGNAL(cancelled()), SLOT(updateMainwinStatus()));
		if(o->getOption("options.ui.systemtray.enable").toBool() == true)
			connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)), d->mainwin, SLOT(setTrayToolTip(const XMPP::Status &, bool, bool)));
		w->show();
	}
	else {
		Status status;
		switch (x) {
		case STATUS_OFFLINE:
			status = PsiAccount::loggedOutStatus();
			break;
		case STATUS_INVISIBLE:
			status = Status("","",0,true);
			status.setIsInvisible(true);
			break;
		default:
			status = Status((XMPP::Status::Type)x, "", 0);
			break;
		}
		if (o->getOption("options.status.last-overwrite.by-status").toBool()) {
			o->setOption("options.status.last-priority", "");
			o->setOption("options.status.last-message", "");
			o->setOption("options.status.last-status", status.typeString());
		}
		setGlobalStatus(status, false, true);
	}
}

XMPP::Status::Type PsiCon::currentStatusType() const
{
	//bool active = false;
	bool loggedIn = false;
	XMPP::Status::Type state = XMPP::Status::Online;
	foreach(PsiAccount* account, d->contactList->enabledAccounts()) {
//		if(account->isActive())
//			active = true;
		if(account->loggedIn()) {
			loggedIn = true;
			state = account->status().type();
		}
	}

	if (!loggedIn) {
		state = XMPP::Status::Offline;
	}

	return state;
}

QString PsiCon::currentStatusMessage() const
{
	QString message = "";
	foreach(PsiAccount* account, d->contactList->enabledAccounts()) {
		if(account->loggedIn()) {
			message = account->status().status();
			break;
		}
	}
	return message;
}

void PsiCon::setStatusFromCommandline(const QString &status, const QString &message)
{
	bool isManualStatus = true; // presume that this will always be a manual user action
	if (isManualStatus) {
		PsiOptions::instance()->setOption("options.status.last-message", message);
	}
	XMPP::Status s;
	s.setType(status);
	s.setStatus(message);	// yes, a bit different naming convention..
	setGlobalStatus(s, false, isManualStatus);
}

void PsiCon::setGlobalStatus(const Status &s, bool withPriority, bool isManualStatus)
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
		if ((allOffline || account->isActive()) && (!account->accountOptions().ignore_global_actions || s.type() == Status::Offline))
			account->setStatus(s, withPriority, isManualStatus);

	emit statusMessageChanged(s.status());
}

void PsiCon::showStatusDialog(const QString& presetName)
{
	StatusPreset preset;
	preset.fromOptions(PsiOptions::instance(), presetName);
	int priority = preset.priority().hasValue() ? preset.priority().value() : 0;
	Status status(preset.status(), preset.message(), priority);
	StatusSetDlg *w = new StatusSetDlg(this, status, preset.priority().hasValue());
	connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)), SLOT(setGlobalStatus(const XMPP::Status &, bool, bool)));
	connect(w, SIGNAL(cancelled()), SLOT(updateMainwinStatus()));
	if(PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool() == true)
		connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)), d->mainwin, SLOT(setTrayToolTip(const XMPP::Status &, bool, bool)));
	w->show();
}

void PsiCon::setStatusMessage(QString message)
{
	XMPP::Status s;
	s.setType(currentStatusType());
	s.setStatus(message);
	setGlobalStatus(s, false, true);
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

	d->updatedAccountTimer_->start();
}

void PsiCon::saveAccounts()
{
	d->updatedAccountTimer_->stop();

	UserAccountList acc = d->contactList->getUserAccountList();
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
#ifdef FILETRANSFER
	bringToFront(ftdlg());
#endif
}

void PsiCon::checkAccountsEmpty()
{
	if (d->contactList->accounts().count() == 0) {
		promptUserToCreateAccount();
	}
}

void PsiCon::openUri(const QString &uri)
{
	QUrl url;
#ifdef HAVE_QT5
	url.setUrl(uri, QUrl::StrictMode);
#else
	url.setEncodedUrl(uri.toLatin1());
#endif
	openUri(url);
}

void PsiCon::openUri(const QUrl &uri)
{
	//qDebug() << "uri:  " << uri.toString();

	// scheme
	if (uri.scheme() != "xmpp") {
		QMessageBox::critical(0, tr("Unsupported URI type"), QString("URI (link) type \"%1\" is not supported.").arg(uri.scheme()));
		return;
	}

	// authority
	PsiAccount *pa = 0;
	//if (uri.authority().isEmpty()) {
		TabbableWidget* tw = findActiveTab();
		if (tw) {
			pa = tw->account();
		}
		if (!pa) {
			pa = d->contactList->defaultAccount();
		}

		if (!pa) {
			QMessageBox::critical(0, tr("Error"), QString("You need to have an account configured and enabled to open URIs (links)."));
			return;
		}
	//
	// TODO: finish authority component handling
	//
	//} else {
	//	qDebug() << "uri auth: [" << uri.authority() << "]");

	//	// is there such account ready to use?
	//	Jid authJid = JIDUtil::fromString(uri.authority());
	//	foreach (PsiAccount* acc, d->contactList->enabledAccounts()) {
	//		if (acc->jid().compare(authJid, false)) {
	//			pa = acc;
	//		}
	//	}

	//	// or maybe it is configured but not enabled?
	//	if (!pa) {
	//		foreach (PsiAccount* acc, d->contactList->accounts()) {
	//			if (acc->jid().compare(authJid, false)) {
	//				QMessageBox::error(0, tr("Error"), QString("The account for %1 JID is disabled right now.").arg(authJid.bare()));
	//				return;	// TODO: Should suggest enabling it now
	//			}
	//		}
	//	}

	//	// nope..
	//	if (!pa) {
	//		QMessageBox::error(0, tr("Error"), QString("You don't have an account for %1.").arg(authJid.bare()));
	//		return;
	//	}
	//}

	pa->openUri(uri);


}

void PsiCon::openAtStyleUri(const QUrl &uri)
{
	QUrl u(uri);
	u.setScheme("mailto");
	DesktopUtil::openUrl(u);
}

void PsiCon::doToolbars()
{
	// TODO try to remember source toolbar to open correct settings in the toolbars dialog
	OptionsDlg *w = (OptionsDlg *)dialogFind("OptionsDlg");
	if (w) {
		w->openTab("toolbars");
		bringToFront(w);
	}
	else {
		w = new OptionsDlg(this);
		connect(w, SIGNAL(applyOptions()), SLOT(slotApplyOptions()));
		w->openTab("toolbars");
		w->show();
	}
}

void PsiCon::doStatusPresets()
{
	OptionsDlg *w = (OptionsDlg *)dialogFind("OptionsDlg");
	if (w) {
		w->openTab("status");
		bringToFront(w);
	}
	else {
		w = new OptionsDlg(this);
		connect(w, SIGNAL(applyOptions()), SLOT(slotApplyOptions()));
		w->openTab("status");
		w->show();
	}
}

void PsiCon::optionChanged(const QString& option)
{
	//bool notifyRestart = true;

	// Global shortcuts
	setShortcuts();

	//Idle server
	d->idleSettings_.update();
	if(d->idleSettings_.useAway || d->idleSettings_.useNotAvailable || d->idleSettings_.useOffline || d->idleSettings_.useIdleServer)
		d->idle.start();
	else {
		d->idle.stop();
		d->idleSettings_.secondsIdle = 0;
	}

	if (option == "options.ui.notifications.alert-style") {
		alertIconUpdateAlertStyle();
	}

	if (option == "options.ui.tabs.use-tabs" ||
		option == "options.ui.tabs.grouping") {
		QMessageBox::information(0, tr("Information"), tr("Some of the options you changed will only have full effect upon restart."));
		//notifyRestart = false;
	}

	// update s5b
	if (option == "options.p2p.bytestreams.listen-port") {
		s5b_init();
	}

	if (option == "options.ui.chat.css") {
		QString css = PsiOptions::instance()->getOption(option).toString();
		if (!css.isEmpty())
			d->iconSelect->setStyleSheet(css);
		return;
	}

	if (option == "options.ui.spell-check.langs") {
		QStringList langs = PsiOptions::instance()->getOption(option).toString().split(QRegExp("\\s+"), QString::SkipEmptyParts);
		if(langs.isEmpty()) {
			langs = SpellChecker::instance()->getAllLanguages();
			QString lang_env = getenv("LANG");
			if(!lang_env.isEmpty()) {
				lang_env = lang_env.split("_").first();
				if(langs.contains(lang_env, Qt::CaseInsensitive))
					langs = QStringList(lang_env);
			}
		}
		SpellChecker::instance()->setActiveLanguages(langs);
		return;
	}

#ifdef USE_PEP
	if (option == tuneUrlFilterOptionPath || option == tuneTitleFilterOptionPath) {
		d->tuneManager->setTuneFilters(PsiOptions::instance()->getOption(tuneUrlFilterOptionPath).toString().split(QRegExp("\\W+")),
							 PsiOptions::instance()->getOption(tuneTitleFilterOptionPath).toString());
	}
	if (option == tuneControllerFilterOptionPath || option == tunePublishOptionPath) {
		if (PsiOptions::instance()->getOption(tunePublishOptionPath).toBool()) {
			d->tuneManager->updateControllers(PsiOptions::instance()->getOption(tuneControllerFilterOptionPath).toString().split(QRegExp("[,]\\s*")));
		}
		else {
			d->tuneManager->updateControllers(d->tuneManager->controllerNames());
		}
	}
#endif
}

void PsiCon::slotApplyOptions()
{
	PsiIconset::instance()->reloadRoster();
	PsiOptions *o = PsiOptions::instance();

#ifndef Q_OS_MAC
	if (!o->getOption("options.ui.contactlist.show-menubar").toBool()) {
		// check if all toolbars are disabled
		bool toolbarsVisible = false;
		foreach(QString base, o->getChildOptionNames("options.ui.contactlist.toolbars", true, true)) {
			if (o->getOption( base + ".visible").toBool()) {
				toolbarsVisible = true;
				break;
			}
		}

		// Check whether it is legal to disable the menubar
		if ( !toolbarsVisible ) {
			QMessageBox::warning(0, tr("Warning"),
				tr("You can not disable <i>all</i> toolbars <i>and</i> the menubar. If you do so, you will be unable to enable them back, when you'll change your mind."),
				tr("I understand"));
			o->setOption("options.ui.contactlist.show-menubar", true);
		}
	}
#endif

	updateS5BServerAddresses();

	if(AvCallManager::isSupported()) {
		AvCallManager::setAudioOutDevice(o->getOption("options.media.devices.audio-output").toString());
		AvCallManager::setAudioInDevice(o->getOption("options.media.devices.audio-input").toString());
		AvCallManager::setVideoInDevice(o->getOption("options.media.devices.video-input").toString());
		AvCallManager::setBasePort(o->getOption("options.p2p.bytestreams.listen-port").toInt());
		AvCallManager::setExternalAddress(o->getOption("options.p2p.bytestreams.external-address").toString());
	}

	// mainwin stuff
	d->mainwin->setWindowOpts(o->getOption("options.ui.contactlist.always-on-top").toBool(), (o->getOption("options.ui.systemtray.enable").toBool() && o->getOption("options.contactlist.use-toolwindow").toBool()));
	d->mainwin->setUseDock(o->getOption("options.ui.systemtray.enable").toBool());
	d->mainwin->buildToolbars();
	d->mainwin->setUseAvatarFrame(o->getOption("options.ui.contactlist.show-avatar-frame").toBool());
	d->mainwin->reinitAutoHide();

	// notify about options change
	emit emitOptionsUpdate();
}

void PsiCon::queueChanged()
{
	PsiIcon *nextAnim = 0;
	int nextAmount = d->contactList->queueCount();
	PsiAccount *pa = d->contactList->queueLowestEventId();
	if(pa)
		nextAnim = PsiIconset::instance()->event2icon(pa->eventQueue()->peekNext());

#ifdef Q_OS_MAC
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
#ifdef Q_OS_MAC
	QString bounce = PsiOptions::instance()->getOption("options.ui.notifications.bounce-dock").toString();
	if (bounce != "never") {
		MacDock::startBounce();
		if (bounce == "once") {
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

bool PsiCon::mainWinVisible() const
{
	return d->mainwin->isVisible();
}

QStringList PsiCon::recentGCList() const
{
	return PsiOptions::instance()->getOption("options.muc.recent-joins.jids").toStringList();
}

void PsiCon::recentGCAdd(const QString &str)
{
	QStringList recentList = recentGCList();
	// remove it if we have it
	foreach(const QString& s, recentList) {
		if(s == str) {
			recentList.removeAll(s);
			break;
		}
	}

	// put it in the front
	recentList.prepend(str);

	// trim the list if bigger than 10
	int max = PsiOptions::instance()->getOption("options.muc.recent-joins.maximum").toInt();
	while(recentList.count() > max) {
		recentList.takeLast();
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
	foreach(const QString& s, recentList) {
		if(s == str) {
			recentList.removeAll(s);
			break;
		}
	}

	// put it in the front
	recentList.prepend(str);

	// trim the list if bigger than 10
	while(recentList.count() > 10) {
		recentList.takeLast();
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
	foreach(const QString& s, d->recentNodeList) {
		if(s == str) {
			d->recentNodeList.removeAll(s);
			break;
		}
	}

	// put it in the front
	d->recentNodeList.prepend(str);

	// trim the list if bigger than 10
	while(d->recentNodeList.count() > 10)
		d->recentNodeList.takeLast();
}

void PsiCon::proxy_settingsChanged()
{
	saveAccounts();
}

IconSelectPopup *PsiCon::iconSelectPopup() const
{
	return d->iconSelect;
}

bool PsiCon::filterEvent(const PsiAccount* acc, const PsiEvent::Ptr &e) const
{
	Q_UNUSED(acc);
	Q_UNUSED(e);
	return false;
}

void PsiCon::processEvent(const PsiEvent::Ptr &e, ActivationType activationType)
{
	if ( !e->account() ) {
		return;
	}

	if ( e->type() == PsiEvent::PGP ) {
		e->account()->eventQueue()->dequeue(e);
		e->account()->queueChanged();
		return;
	}

	UserListItem *u = e->account()->find(e->jid());
	if ( !u ) {
		qWarning("SYSTEM MESSAGE: Bug #1. Contact the developers and tell them what you did to make this message appear. Thank you.");
		e->account()->eventQueue()->dequeue(e);
		e->account()->queueChanged();
		return;
	}

#ifdef FILETRANSFER
	if( e->type() == PsiEvent::File ) {
		FileEvent::Ptr fe = e.staticCast<FileEvent>();
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
#endif

	if(e->type() == PsiEvent::AvCallType) {
		AvCallEvent::Ptr ae = e.staticCast<AvCallEvent>();
		AvCall *sess = ae->takeAvCall();
		e->account()->eventQueue()->dequeue(e);
		e->account()->queueChanged();
		e->account()->cpUpdate(*u);
		if(sess) {
			if(!sess->jid().isEmpty()) {
				CallDlg *w = new CallDlg(e->account(), 0);
				w->setAttribute(Qt::WA_DeleteOnClose);
				w->setIncoming(sess);
				bringToFront(w);
			}
			else {
				QMessageBox::information(0, tr("Call ended"), tr("Other party canceled call."));
				delete sess;
			}
		}
		return;
	}

	bool isChat = false;
#ifdef GROUPCHAT
	bool isMuc = false;
#endif
	bool sentToChatWindow = false;
	if ( e->type() == PsiEvent::Message ) {
		MessageEvent::Ptr me = e.staticCast<MessageEvent>();
		const Message &m = me->message();
#ifdef GROUPCHAT
		if (m.type() == "groupchat") {
			isMuc = true;
		}
		else {
#endif
			bool emptyForm = m.getForm().fields().empty();
			// FIXME: Refactor this, PsiAccount and PsiEvent out
			if (m.type() == "chat" && emptyForm) {
				isChat = true;
				sentToChatWindow = me->sentToChatWindow();
			}
#ifdef GROUPCHAT
		}
#endif
	}

	if ( isChat ) {
		PsiAccount* account = e->account();
		XMPP::Jid from = e->from();

		if ( PsiOptions::instance()->getOption("options.ui.chat.alert-for-already-open-chats").toBool() && sentToChatWindow ) {
			// Message already displayed, need only to pop up chat dialog, so that
			// it will be read (or marked as read)
			ChatDlg *c = account->findChatDialogEx(from);
			if(!c)
				c = account->findChatDialogEx(e->jid());
			if(!c)
				return; // should never happen

			account->processChats(from); // this will delete all events, corresponding to that chat dialog
		}

		// as the event could be deleted just above, we're using cached account and from values
		account->openChat(from, activationType);
	}
#ifdef WHITEBOARDING
	else if (e->type() == PsiEvent::Sxe) {
		e->account()->eventQueue()->dequeue(e);
		e->account()->queueChanged();
		e->account()->cpUpdate(*u);
		e->account()->wbManager()->requestActivated((e.staticCast<SxeEvent>())->id());
		return;
	}
#endif
	else {
#ifdef GROUPCHAT
		if (isMuc) {
			PsiAccount *account = e->account();
			GCMainDlg *c = account->findDialog<GCMainDlg*>(e->from());
			if (c) {
				c->ensureTabbedCorrectly();
				c->bringToFront(true);
				return;
			}
		}
#endif
		// search for an already opened eventdlg
		EventDlg *w = e->account()->findDialog<EventDlg*>(u->jid());

		if ( !w ) {
			// create the eventdlg
			w = e->account()->ensureEventDlg(u->jid());

			// load next message
			e->account()->processReadNext(*u);
		}

		if (w)
			bringToFront(w);
	}
}

void PsiCon::removeEvent(const PsiEvent::Ptr &e)
{
	PsiAccount* account = e->account();
	if (!account)
		return;
	UserListItem *u = account->find(e->jid());
	account->eventQueue()->dequeue(e);
	account->queueChanged();
	if(u)
		account->cpUpdate(*u);
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
	QString extAddr = PsiOptions::instance()->getOption("options.p2p.bytestreams.external-address").toString();
	if(!extAddr.isEmpty()) {
		bool found = false;
		for(QStringList::ConstIterator sit = slist.begin(); sit != slist.end(); ++sit) {
			const QString &s = *sit;
			if(s == extAddr) {
				found = true;
				break;
			}
		}
		if(!found)
			slist += extAddr;
	}

	// set up the server
	d->s5bServer->setHostList(slist);
}

void PsiCon::s5b_init()
{
	if(d->s5bServer->isActive())
		d->s5bServer->stop();

	int port = PsiOptions::instance()->getOption("options.p2p.bytestreams.listen-port").toInt();
	if (port) {
		if(!d->s5bServer->start(port)) {
			QMessageBox::warning(0, tr("Warning"), tr("Unable to bind to port %1 for Data Transfer.\nThis may mean you are already running another instance of Psi. You may experience problems sending and/or receiving files.").arg(port));
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
		if (account->userAccount().opt_connectAfterSleep) {
			// Should we do this when the network comes up ?
			if (account->accountOptions().opt_autoSameStatus) {
				Status s = account->accountOptions().lastStatus;
				account->setStatus(s, account->accountOptions().lastStatusWithPriority, true);
				emit statusMessageChanged(s.status());
			}
			else {
				account->setStatus(makeStatus(XMPP::Status::Online, ""), false, true);
			}
		}
	}
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
		AccountRegDlg w(this);
		int n = w.exec();
		if (n == QDialog::Accepted) {
			contactList()->createAccount(w.jid().node(),w.jid(),w.pass(),w.useHost(),w.host(),w.port(),w.legacySSLProbe(),w.ssl(),w.proxy(),w.tlsOverrideDomain(), w.tlsOverrideCert());
		}
	}
}

QString PsiCon::optionsFile() const
{
	return pathToProfile(activeProfile, ApplicationInfo::ConfigLocation) + "/options.xml";
}

void PsiCon::forceSavePreferences()
{
	PsiOptions::instance()->save(optionsFile());
}

void PsiCon::doQuit(int quitCode)
{
	d->quitting = true;
	emit quit(quitCode);
}

void PsiCon::aboutToQuit()
{
	doQuit(QuitProgram);
}

void PsiCon::secondsIdle(int sec)
{
	d->idleSettings_.secondsIdle = sec;
	int minutes = sec / 60;
	PsiAccount::AutoAway aa;

	if(d->idleSettings_.useOffline && d->idleSettings_.offlineAfter > 0 && minutes >= d->idleSettings_.offlineAfter)
		aa = PsiAccount::AutoAway_Offline;
	else if(d->idleSettings_.useNotAvailable && d->idleSettings_.menuXA && d->idleSettings_.notAvailableAfter > 0 && minutes >= d->idleSettings_.notAvailableAfter)
		aa = PsiAccount::AutoAway_XA;
	else if(d->idleSettings_.useAway && d->idleSettings_.awayAfter > 0 && minutes >= d->idleSettings_.awayAfter)
		aa = PsiAccount::AutoAway_Away;
	else
		aa = PsiAccount::AutoAway_None;

	foreach(PsiAccount* pa, d->contactList->enabledAccounts()) {
		if(pa->accountOptions().ignore_global_actions)
			continue;

		pa->setAutoAwayStatus(aa);
	}
}

int PsiCon::idle() const
{
	return d->idleSettings_.secondsIdle;
}

ContactUpdatesManager* PsiCon::contactUpdatesManager() const
{
	return contactUpdatesManager_;
}

#include "psicon.moc"

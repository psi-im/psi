/*
 * psicon.cpp - core of Psi
 * Copyright (C) 2001-2002  Justin Karneges
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

#include "psicon.h"

#include "AutoUpdater/AutoUpdater.h"
#include "accountadddlg.h"
#include "accountmanagedlg.h"
#include "accountmodifydlg.h"
#include "accountregdlg.h"
#include "accountscombobox.h"
#include "activeprofiles.h"
#include "alerticon.h"
#include "alertmanager.h"
#include "anim.h"
#include "applicationinfo.h"
#include "avcall/avcall.h"
#include "avcall/calldlg.h"
#include "avcall/mediadevicewatcher.h"
#include "bosskey.h"
#include "chatdlg.h"
#include "common.h"
#include "contactupdatesmanager.h"
#include "dbus.h"
#include "desktoputil.h"
#include "edbsqlite.h"
#include "eventdlg.h"
#include "globalshortcut/globalshortcutmanager.h"
#ifdef GROUPCHAT
#include "groupchatdlg.h"
#endif
#include "iconselect.h"
#include "idle/idle.h"
#include "iris/processquit.h"
#include "iris/tcpportreserver.h"
#include "jidutil.h"
#include "jingle-s5b.h"
#include "mainwin.h"
#include "mucjoindlg.h"
#include "networkaccessmanager.h"
#include "options/opt_toolbars.h"
#include "options/optionsdlg.h"
#ifdef HAVE_PGPUTIL
#include "pgputil.h"
#endif
#include "popupmanager.h"
#include "proxy.h"
#include "psiaccount.h"
#include "psiactionlist.h"
#include "psicapsregsitry.h"
#include "psicontact.h"
#include "psicontactlist.h"
#include "psievent.h"
#include "psiiconset.h"
#ifdef PSIMNG
#include "psimng.h"
#endif
#include "psioptions.h"
#include "psirichtext.h"
#include "psithememanager.h"
#include "psitoolbar.h"
#include "s5b.h"
#include "shortcutmanager.h"
#include "spellchecker/aspellchecker.h"
#include "statusdlg.h"
#include "systemwatch/systemwatch.h"
#include "tabdlg.h"
#include "tabmanager.h"
#include "tunecontrollermanager.h"
#include "urlobject.h"
#include "userlist.h"
#ifdef HAVE_WEBSERVER
#include "webserver.h"
#endif
#include "xmpp_caps.h"
#include "xmpp_xmlcommon.h"
#ifdef WHITEBOARDING
#include "whiteboarding/wbmanager.h"
#endif
#ifdef FILETRANSFER
#include "filetransdlg.h"
#include "filetransfer.h"
#include "multifiletransferdlg.h"
#endif
#ifdef PSI_PLUGINS
#include "filesharingmanager.h"
#include "filesharingnamproxy.h"
#include "pluginmanager.h"
#endif
#ifdef WEBKIT
#include "avatars.h"
#include "chatviewthemeprovider.h"
#include "webview.h"
#endif
#ifdef HAVE_SPARKLE
#include "AutoUpdater/SparkleAutoUpdater.h"
#endif
#ifdef Q_OS_MAC
#include "mac_dock/mac_dock.h"
#endif

#include <QApplication>
#include <QColor>
#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QList>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkConfigurationManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QPixmapCache>
#include <QPointer>
#include <QSessionManager>

static const char *tunePublishOptionPath          = "options.extended-presence.tune.publish";
static const char *tuneUrlFilterOptionPath        = "options.extended-presence.tune.url-filter";
static const char *tuneTitleFilterOptionPath      = "options.extended-presence.tune.title-filter";
static const char *tuneControllerFilterOptionPath = "options.extended-presence.tune.controller-filter";

//----------------------------------------------------------------------------
// PsiConObject
//----------------------------------------------------------------------------
class PsiConObject : public QObject {
    Q_OBJECT
public:
    PsiConObject(QObject *parent) : QObject(parent)
    {
        QDir p(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation));
        QDir v(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation) + "/tmp-sounds");
        if (!v.exists())
            p.mkdir("tmp-sounds");
        Iconset::setSoundPrefs(v.absolutePath(), this, SLOT(playSound(QString)));
        connect(URLObject::getInstance(), SIGNAL(openURL(QString)), SLOT(openURL(QString)));
    }

    ~PsiConObject()
    {
        // removing temp dirs
        [[maybe_unused]] QDir p(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation));
        QDir                  v(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation) + "/tmp-sounds");
        folderRemove(v);
    }

public slots:
    void playSound(QString file)
    {
        if (file.isEmpty() || !PsiOptions::instance()->getOption("options.ui.notifications.sounds.enable").toBool())
            return;

        soundPlay(file);
    }

    void openURL(QString url) { DesktopUtil::openUrl(url); }

private:
    // ripped from profiles.cpp
    bool folderRemove(const QDir &_d)
    {
        QDir d = _d;

        QStringList entries = d.entryList();
        for (QStringList::Iterator it = entries.begin(); it != entries.end(); ++it) {
            if (*it == "." || *it == "..")
                continue;
            QFileInfo info(d, *it);
            if (info.isDir()) {
                if (!folderRemove(QDir(info.filePath())))
                    return false;
            } else {
                // printf("deleting [%s]\n", info.filePath().latin1());
                d.remove(info.fileName());
            }
        }
        QString name = d.dirName();
        if (!d.cdUp())
            return false;
        // printf("removing folder [%s]\n", d.filePath(name).latin1());
        d.rmdir(name);

        return true;
    }
};

//----------------------------------------------------------------------------
// PsiCon::Private
//----------------------------------------------------------------------------

struct item_dialog {
    QWidget *widget;
    QString  className;
};

class PsiCon::Private : public QObject {
    Q_OBJECT
public:
    Private(PsiCon *parent) : QObject(parent), psi(parent), alertManager(parent) { }

    ~Private()
    {
        if (iconSelect)
            delete iconSelect;
    }

    void saveProfile(UserAccountList acc)
    {
        // clear it
        accountTree.removeOption("accounts", true);
        // save accounts with known base
        QSet<QString> cbases;
        QStringList   order;
        for (UserAccount ua : acc) {
            if (!ua.optionsBase.isEmpty()) {
                ua.toOptions(&accountTree);
                cbases += ua.optionsBase;
            }
            order.append(ua.id);
        }
        // save new accounts
        int idx = 0;
        for (UserAccount ua : acc) {
            if (ua.optionsBase.isEmpty()) {
                QString base;
                do {
                    base = "accounts.a" + QString::number(idx++);
                } while (cbases.contains(base));
                cbases += base;
                ua.toOptions(&accountTree, base);
            }
        }
        accountTree.setOption("order", order);
        QFile accountsFile(pathToProfile(activeProfile, ApplicationInfo::ConfigLocation) + "/accounts.xml");
        accountTree.saveOptions(accountsFile.fileName(), "accounts", ApplicationInfo::optionsNS(),
                                ApplicationInfo::version());
        ;
    }

    std::pair<PsiAccount *, QString> uriToShareSource(const QString &path)
    {
        auto pathParts = path.midRef(sizeof("/psi/account")).split('/');
        if (pathParts.size() < 3 || pathParts[1] != QLatin1String("sharedfile")
            || pathParts[2].isEmpty()) // <acoount_uuid>/sharedfile/<file_hash>
            return { nullptr, QString() };

        for (PsiAccount *account : contactList->enabledAccounts()) {
            if (!account->isActive() || account->id() != pathParts[0])
                continue;

            return { account, pathParts[2].toString() };
        }
        return { nullptr, QString() };
    }

private slots:
    void updateIconSelect()
    {
        Iconset iss;
        for (Iconset *iconset : qAsConst(PsiIconset::instance()->emoticons)) {
            iss += *iconset;
        }

        iconSelect->setIconset(iss);
        QPixmapCache::clear();
    }

public:
    PsiCon *              psi         = nullptr;
    PsiContactList *      contactList = nullptr;
    OptionsMigration      optionsMigration;
    OptionsTree           accountTree;
    MainWin *             mainwin = nullptr;
    Idle                  idle;
    QList<item_dialog *>  dialogList;
    int                   eventId = 0;
    QStringList           recentNodeList; // FIXME move this to options system?
    EDB *                 edb                = nullptr;
    TcpPortReserver *     tcpPortReserver    = nullptr;
    IconSelectPopup *     iconSelect         = nullptr;
    NetworkAccessManager *nam                = nullptr;
    FileSharingManager *  fileSharingManager = nullptr;
    PsiThemeManager *     themeManager       = nullptr;
#ifdef HAVE_WEBSERVER
    WebServer *webServer = nullptr;
#endif
#ifdef FILETRANSFER
    FileTransDlg *ftwin = nullptr;
#endif
    PsiActionList *actionList = nullptr;
    // GlobalAccelManager *globalAccelManager;
    TuneControllerManager *tuneManager          = nullptr;
    QMenuBar *             defaultMenuBar       = nullptr;
    TabManager *           tabManager           = nullptr;
    bool                   quitting             = false;
    bool                   wakeupPending        = false;
    QTimer *               updatedAccountTimer_ = nullptr;
    AutoUpdater *          autoUpdater          = nullptr;
    AlertManager           alertManager;
    BossKey *              bossKey         = nullptr;
    PopupManager *         popupManager    = nullptr;
    quint16                byteStreamsPort = 0;
    QString                externalByteStreamsAddress;

    struct IdleSettings {
        IdleSettings() = default;

        void update()
        {
            PsiOptions *o     = PsiOptions::instance();
            useOffline        = o->getOption("options.status.auto-away.use-offline").toBool();
            useNotAvailable   = o->getOption("options.status.auto-away.use-not-availible").toBool();
            useAway           = o->getOption("options.status.auto-away.use-away").toBool();
            offlineAfter      = o->getOption("options.status.auto-away.offline-after").toInt();
            notAvailableAfter = o->getOption("options.status.auto-away.not-availible-after").toInt();
            awayAfter         = o->getOption("options.status.auto-away.away-after").toInt();
            menuXA            = o->getOption("options.ui.menu.status.xa").toBool();
            useIdleServer     = o->getOption("options.service-discovery.last-activity").toBool();
        }

        bool useOffline = false, useNotAvailable = false, useAway = false, menuXA = false;
        int  offlineAfter = 0, notAvailableAfter = 0, awayAfter = 0;
        int  secondsIdle   = 0;
        bool useIdleServer = false;
    };

    IdleSettings idleSettings_;
};

//----------------------------------------------------------------------------
// PsiCon
//----------------------------------------------------------------------------

PsiCon::PsiCon() : QObject(nullptr)
{
    // pdb(DEBUG_JABCON, QString("%1 v%2\n By Justin Karneges\n
    // justin@karneges.com\n\n").arg(PROG_NAME).arg(PROG_VERSION));
    d             = new Private(this);
    d->tabManager = new TabManager(this);
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), SLOT(aboutToQuit()));
    connect(ProcessQuit::instance(), SIGNAL(quit()), SLOT(aboutToQuit()));

    d->defaultMenuBar = new QMenuBar(nullptr);

    XMPP::CapsRegistry::setInstance(new PsiCapsRegistry(this));
    XMPP::CapsRegistry *pcr = XMPP::CapsRegistry::instance();
    connect(pcr, SIGNAL(destroyed(QObject *)), pcr, SLOT(save()));
    connect(pcr, SIGNAL(registered(const XMPP::CapsSpec &)), pcr, SLOT(save()));
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

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    qApp->setFallbackSessionManagementEnabled(false);
#endif
    connect(qApp, SIGNAL(commitDataRequest(QSessionManager &)), SLOT(forceSavePreferences(QSessionManager &)));

#ifdef HAVE_PGPUTIL
    // PGP initialization (needs to be before any gpg usage!)
    PGPUtil::instance();
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    if (QGuiApplication::desktopFileName().isEmpty()) {
        QGuiApplication::setDesktopFileName(ApplicationInfo::desktopFileBaseName());
    }
#endif

    d->contactList = new PsiContactList(this);

    connect(d->contactList, SIGNAL(accountAdded(PsiAccount *)), SIGNAL(accountAdded(PsiAccount *)));
    connect(d->contactList, SIGNAL(accountRemoved(PsiAccount *)), SIGNAL(accountRemoved(PsiAccount *)));
    connect(d->contactList, SIGNAL(accountCountChanged()), SIGNAL(accountCountChanged()));
    connect(d->contactList, SIGNAL(accountActivityChanged()), SIGNAL(accountActivityChanged()));
    connect(d->contactList, SIGNAL(saveAccounts()), SLOT(saveAccounts()));
    connect(d->contactList, SIGNAL(queueChanged()), SLOT(queueChanged()));

    d->updatedAccountTimer_ = new QTimer(this);
    d->updatedAccountTimer_->setSingleShot(true);
    d->updatedAccountTimer_->setInterval(1000);
    connect(d->updatedAccountTimer_, SIGNAL(timeout()), SLOT(saveAccounts()));

    QString oldConfig = pathToProfileConfig(activeProfile);
    if (QFile::exists(oldConfig)) {
        QMessageBox::warning(d->mainwin, tr("Migration is impossible"),
                             tr("Found no more supported configuration file from some very old version:\n%1\n\n"
                                "Migration is possible with Psi-0.15")
                                 .arg(oldConfig));
    }

    // advanced widget
    GAdvancedWidget::setStickEnabled(false);   // until this is bugless
    GAdvancedWidget::setStickToWindows(false); // again
    GAdvancedWidget::setStickAt(5);

    PsiRichText::setAllowedImageDirs(QStringList() << ApplicationInfo::resourcesDir()
                                                   << ApplicationInfo::homeDir(ApplicationInfo::CacheLocation));

    // To allow us to upgrade from old hardcoded options gracefully, be careful about the order here
    PsiOptions *options = PsiOptions::instance();
    // load the system-wide defaults, if they exist
    QString systemDefaults = ApplicationInfo::resourcesDir();
    systemDefaults += "/options-default.xml";
    // qWarning(qPrintable(QString("Loading system defaults from %1").arg(systemDefaults)));
    options->load(systemDefaults);

    if (!PsiOptions::exists(optionsFile())) {
        if (!options->newProfile()) {
            qWarning("ERROR: Failed to new profile default options");
        }
    }

    // load the new profile
    options->load(optionsFile());
    // Save every time an option is changed
    options->autoSave(true, optionsFile());

    // just set a dummy option to trigger saving
    options->setOption("trigger-save", false);
    options->setOption("trigger-save", true);

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
    common_smallFontSize      = qApp->font().pointSize();
    common_smallFontSize -= 2;
    if (common_smallFontSize < minimumFontSize)
        common_smallFontSize = minimumFontSize;
    FancyLabel::setSmallFontSize(common_smallFontSize);

    QFile accountsFile(pathToProfile(activeProfile, ApplicationInfo::ConfigLocation) + "/accounts.xml");
    bool  accountMigration = false;
    if (!accountsFile.exists()) {
        accountMigration = true;
        int idx          = 0;
        for (UserAccount a : qAsConst(d->optionsMigration.accMigration)) {
            QString base = "accounts.a" + QString::number(idx++);
            a.toOptions(&d->accountTree, base);
        }
    } else {
        d->accountTree.loadOptions(accountsFile.fileName(), "accounts", ApplicationInfo::optionsNS());
    }

    // proxy
    QNetworkProxyFactory::setUseSystemConfiguration(false); // we have qca-based own implementation
    ProxyManager *proxy = ProxyManager::instance();
    proxy->init(&d->accountTree);
    if (accountMigration)
        proxy->migrateItemList(d->optionsMigration.proxyMigration);
    connect(proxy, SIGNAL(settingsChanged()), SLOT(proxy_settingsChanged()));

    connect(options, SIGNAL(optionChanged(const QString &)), SLOT(optionChanged(const QString &)));

    contactUpdatesManager_ = new ContactUpdatesManager(this);

    QDir profileDir(pathToProfile(activeProfile, ApplicationInfo::DataLocation));
    profileDir.rmdir("info"); // remove unused dir

    d->iconSelect = new IconSelectPopup(nullptr);
    d->iconSelect->setEmojiSortingEnabled(true);
    connect(PsiIconset::instance(), SIGNAL(emoticonsChanged()), d, SLOT(updateIconSelect()));

    const QString css = options->getOption("options.ui.chat.css").toString();
    if (!css.isEmpty())
        d->iconSelect->setStyleSheet(css);

    // first thing, try to load the iconset
    bool result = true;
    if (!PsiIconset::instance()->loadAll()) {
        // LEGOPTS.iconset = "stellar";
        // if(!is.load(LEGOPTS.iconset)) {
        QMessageBox::critical(nullptr, tr("Error"),
                              tr("Unable to load iconset!  Please make sure Psi is properly installed."));
        result = false;
        //}
    }

    d->nam                = new NetworkAccessManager(this);
    d->fileSharingManager = new FileSharingManager(this);
#ifdef HAVE_WEBSERVER
    d->webServer = new WebServer(this);
    d->webServer->route("/psi/account",
                        [this](qhttp::server::QHttpRequest *req, qhttp::server::QHttpResponse *res) -> bool {
                            if (req->method() != qhttp::EHTTP_GET)
                                return false;
                            PsiAccount *acc;
                            QString     id;
                            std::tie(acc, id) = d->uriToShareSource(req->url().path());
                            if (!acc)
                                return false;
                            return d->fileSharingManager->downloadHttpRequest(acc, id, req, res);
                        });
#endif
    d->nam->route("/psi/account/", [this](const QNetworkRequest &req) -> QNetworkReply * {
        PsiAccount *acc;
        QString     id;
        std::tie(acc, id) = d->uriToShareSource(req.url().path());
        if (id.isNull() || !acc)
            return nullptr;
        return new FileSharingNAMReply(acc, id, req);
    });

    d->themeManager = new PsiThemeManager(this);
#ifdef WEBKIT
    d->themeManager->registerProvider(new ChatViewThemeProvider(this), true);
    d->themeManager->registerProvider(new GroupChatViewThemeProvider(this), true);
#endif

    if (!d->themeManager->loadAll()) {
        QMessageBox::critical(nullptr, tr("Error"),
                              tr("Unable to load theme!  Please make sure Psi is properly installed."));
        result = false;
    }

    if (!d->actionList)
        d->actionList = new PsiActionList(this);

    PsiConObject *psiConObject = new PsiConObject(this);

    Anim::setMainThread(QThread::currentThread());

    // setup the main window
    d->mainwin = new MainWin(options->getOption("options.ui.contactlist.always-on-top").toBool(),
                             (options->getOption("options.ui.systemtray.enable").toBool()
                              && options->getOption("options.contactlist.use-toolwindow").toBool()),
                             this);
    d->mainwin->setUseDock(options->getOption("options.ui.systemtray.enable").toBool());
    d->bossKey = new BossKey(d->mainwin);

    Q_UNUSED(psiConObject);

    connect(d->mainwin, SIGNAL(closeProgram()), SLOT(closeProgram()));
    connect(d->mainwin, SIGNAL(changeProfile()), SLOT(changeProfile()));
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

    if (result
        && !(options->getOption("options.ui.systemtray.enable").toBool()
             && options->getOption("options.contactlist.hide-on-start").toBool())) {
        d->mainwin->show();
    }

    connect(&d->idle, SIGNAL(secondsIdle(int)), SLOT(secondsIdle(int)));

    // PopupDurationsManager
    d->popupManager = new PopupManager(this);

    // incoming bytestreams address
    int bsPort = PsiOptions::instance()->getOption(QString::fromLatin1("options.p2p.bytestreams.listen-port")).toInt();
    if (bsPort > 0 && bsPort < 65536) {
        d->byteStreamsPort = quint16(bsPort);
    }
    d->externalByteStreamsAddress
        = PsiOptions::instance()->getOption("options.p2p.bytestreams.external-address").toString();

    // general purpose tcp port reserver
    d->tcpPortReserver = new TcpPortReserver;
    connect(d->tcpPortReserver, &TcpPortReserver::newDiscoverer, this, [this](TcpPortDiscoverer *tpd) {
        // add external
        if (d->externalByteStreamsAddress.isEmpty() || d->byteStreamsPort) {
            return;
        }
        if (!tpd->setExternalHost(d->externalByteStreamsAddress, d->byteStreamsPort, QHostAddress(QHostAddress::Any),
                                  d->byteStreamsPort)) {
            QMessageBox::warning(nullptr, tr("Warning"),
                                 tr("Unable to bind to port %1 for Data Transfer.\n"
                                    "This may mean you are already running another instance of Psi. "
                                    "You may experience problems sending and/or receiving files.")
                                     .arg(d->byteStreamsPort));
        }
    });

    d->tcpPortReserver->registerScope(QString::fromLatin1("s5b"), new S5BServersProducer);

    // Connect to the system monitor
    SystemWatch *sw = SystemWatch::instance();
    connect(sw, SIGNAL(sleep()), this, SLOT(doSleep()));
    connect(sw, SIGNAL(wakeup()), this, SLOT(doWakeup()));

#ifdef PSI_PLUGINS
    // Plugin Manager
    connect(PluginManager::instance(), &PluginManager::pluginEnabled, this,
            [this](const QString &) { slotApplyOptions(); });
    PluginManager::instance()->initNewSession(this);
#endif

    // Global shortcuts
    setShortcuts();

    // load accounts
    {
        QList<UserAccount> accs;
        QStringList        bases = d->accountTree.getChildOptionNames("accounts", true, true);
        for (const QString &base : bases) {
            UserAccount ua;
            ua.fromOptions(&d->accountTree, base);
            accs += ua;
        }
        QStringList order = d->accountTree.getOption("order", QStringList()).toStringList();
        int         start = 0;
        for (const QString &id : order) {
            for (int i = start; i < accs.size(); ++i) {
                if (accs[i].id == id) {
                    accs.move(i, start);
                    start++;
                    break;
                }
            }
        }

        // Disable accounts if necessary, and overwrite locked properties
        bool    single = options->getOption("options.ui.account.single").toBool();
        QString domain = options->getOption("options.account.domain").toString();
        if (single || !domain.isEmpty()) {
            bool haveEnabled = false;
            for (UserAccountList::Iterator it = accs.begin(); it != accs.end(); ++it) {
                // With single accounts, only modify the first account
                if (single) {
                    if (!haveEnabled) {
                        haveEnabled = it->opt_enabled;
                        if (it->opt_enabled) {
                            if (!domain.isEmpty())
                                it->jid = JIDUtil::accountFromString(Jid(it->jid).node()).bare();
                        }
                    } else
                        it->opt_enabled = false;
                } else {
                    // Overwirte locked properties
                    if (!domain.isEmpty())
                        it->jid = JIDUtil::accountFromString(Jid(it->jid).node()).bare();
                }
            }
        }

        d->contactList->loadAccounts(accs);
    }

    checkAccountsEmpty();

    // Import for SQLite history
    if (d->contactList->defaultAccount()) {
        EDBSqLite *edb = new EDBSqLite(this);
        d->edb         = edb;
        if (!edb->init())
            return false;
    }

    if (d->contactList->defaultAccount())
        emit statusMessageChanged(d->contactList->defaultAccount()->status().status());

#ifdef USE_DBUS
    addPsiConAdapter(this);
#endif

    connect(ActiveProfiles::instance(), SIGNAL(setStatusRequested(const QString &, const QString &)),
            SLOT(setStatusFromCommandline(const QString &, const QString &)));
    connect(ActiveProfiles::instance(), SIGNAL(openUriRequested(const QString &)), SLOT(openUri(const QString &)));
    connect(ActiveProfiles::instance(), SIGNAL(raiseRequested()), SLOT(raiseMainwin()));

    DesktopUtil::setUrlHandler("xmpp", this, "openUri");
    DesktopUtil::setUrlHandler("x-psi-atstyle", this, "openAtStyleUri");

#ifdef USE_PEP
    optionChanged(tuneControllerFilterOptionPath);
    optionChanged(tuneUrlFilterOptionPath);
#endif

    // init spellchecker
    optionChanged("options.ui.spell-check.langs");

    // try autologin if needed
    for (PsiAccount *account : d->contactList->accounts()) {
        account->autoLogin();
    }

    return result;
}

bool PsiCon::haveAutoUpdater() const { return d->autoUpdater != nullptr; }

void PsiCon::updateStatusPresets() { emit statusPresetsChanged(); }

void PsiCon::deinit()
{
    // this deletes all dialogs except for mainwin
    deleteAllDialogs();

#ifdef WEBKIT
    // unload webkit themes early (before realease of webengine profile)
    delete d->themeManager->unregisterProvider(QString::fromLatin1("groupchatview"));
    delete d->themeManager->unregisterProvider(QString::fromLatin1("chatview"));
#endif
    delete d->themeManager;
    d->themeManager = nullptr;
#ifdef HAVE_WEBSERVER
    delete d->webServer;
    d->webServer = nullptr;
#endif

    d->idle.stop();

    // shut down all accounts
    UserAccountList acc;
    if (d->contactList) {
        acc = d->contactList->getUserAccountList();
        delete d->contactList; // also deletes all accounts
    }
    d->nam->releaseHandlers();

    delete d->tcpPortReserver;

#ifdef FILETRANSFER
    delete d->ftwin;
    d->ftwin = nullptr;
#endif

    if (d->mainwin) {
        delete d->mainwin;
        d->mainwin = nullptr;
    }

    // TuneController
    delete d->tuneManager;

    // save profile
    if (d->contactList)
        d->saveProfile(acc);
#ifdef PSI_PLUGINS
    PluginManager::instance()->unloadAllPlugins();
#endif
    GlobalShortcutManager::clear();

    DesktopUtil::unsetUrlHandler("xmpp");
}

// will gracefully finish all network activity and other async stuff
void PsiCon::gracefulDeinit(std::function<void()> callback)
{
    if (d->contactList) {
        connect(d->contactList, &PsiContactList::gracefulDeinitFinished, this, callback);
        d->contactList->gracefulDeinit();
    } else {
        callback();
    }
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

PsiContactList *PsiCon::contactList() const { return d->contactList; }

EDB *PsiCon::edb() const { return d->edb; }

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

PopupManager *PsiCon::popupManager() const { return d->popupManager; }

struct OptFeatureMap {
    OptFeatureMap(const QString &option, const QStringList &feature) : option(option), feature(feature) { }
    QString     option;
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

    static QList<OptFeatureMap> fmap = QList<OptFeatureMap>()
        << OptFeatureMap("options.service-discovery.last-activity", QStringList() << "jabber:iq:last")
        << OptFeatureMap("options.html.chat.render", QStringList() << "http://jabber.org/protocol/xhtml-im")
        << OptFeatureMap("options.extended-presence.notify",
                         QStringList() << "http://jabber.org/protocol/mood+notify"
                                       << "http://jabber.org/protocol/activity+notify"
                                       << "http://jabber.org/protocol/tune+notify"
                                       << "http://jabber.org/protocol/geoloc+notify"
                                       << "urn:xmpp:avatar:metadata+notify")
        << OptFeatureMap("options.messages.send-composing-events",
                         QStringList() << "http://jabber.org/protocol/chatstates")
        << OptFeatureMap("options.ui.notifications.send-receipts", QStringList() << "urn:xmpp:receipts");

    for (const OptFeatureMap &f : qAsConst(fmap)) {
        if (PsiOptions::instance()->getOption(f.option).toBool()) {
            features << f.feature;
        }
    }

    return features;
}

TabManager *PsiCon::tabManager() const { return d->tabManager; }

NetworkAccessManager *PsiCon::networkAccessManager() const { return d->nam; }

FileSharingManager *PsiCon::fileSharingManager() const { return d->fileSharingManager; }

PsiThemeManager *PsiCon::themeManager() const { return d->themeManager; }

WebServer *PsiCon::webServer() const
{
#ifdef HAVE_WEBSERVER
    return d->webServer;
#else
    return nullptr;
#endif
}

TuneControllerManager *PsiCon::tuneManager() const { return d->tuneManager; }

AlertManager *PsiCon::alertManager() const { return &(d->alertManager); }

void PsiCon::closeProgram() { doQuit(QuitProgram); }

void PsiCon::changeProfile()
{
    ActiveProfiles::instance()->unsetThisProfile();
    if (d->contactList->haveActiveAccounts()) {
        QMessageBox  messageBox(QMessageBox::Information, CAP(tr("Error")),
                               tr("Please disconnect before changing the profile."));
        QPushButton *cancel     = messageBox.addButton(QMessageBox::Cancel);
        QPushButton *disconnect = messageBox.addButton(tr("&Disconnect"), QMessageBox::AcceptRole);
        messageBox.setDefaultButton(disconnect);
        messageBox.exec();
        if (messageBox.clickedButton() == cancel)
            return;

        setGlobalStatus(XMPP::Status::Offline, false, true);
    }

    doQuit(QuitProfile);
}

void PsiCon::doGroupChat()
{
#ifdef GROUPCHAT
    PsiAccount *account = d->contactList->defaultAccount();
    if (!account)
        return;

    MUCJoinDlg *w = new MUCJoinDlg(this, account);
    w->show();
#endif
}

void PsiCon::doNewBlankMessage()
{
    PsiAccount *account = d->contactList->defaultAccount();
    if (!account)
        return;

    EventDlg *w = createMessageDlg("", account);
    if (!w)
        return;

    w->show();
}

EventDlg *PsiCon::createMessageDlg(const QString &to, PsiAccount *pa)
{
    if (!EventDlg::messagingEnabled())
        return nullptr;

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
    for (item_dialog *i : qAsConst(d->dialogList)) {
        if (i->className == "EventDlg") {
            EventDlg *e = qobject_cast<EventDlg *>(i->widget);
            if (e->psiAccount() == pa)
                e->updateContact(j);
        }
    }
}

// FIXME: make it work like QObject::findChildren<ChildName>()
QWidget *PsiCon::dialogFind(const char *className)
{
    for (item_dialog *i : qAsConst(d->dialogList)) {
        // does the classname and jid match?
        if (i->className == className) {
            return i->widget;
        }
    }
    return nullptr;
}

QMenuBar *PsiCon::defaultMenuBar() const { return d->defaultMenuBar; }

void PsiCon::dialogRegister(QWidget *w)
{
    item_dialog *i = new item_dialog;
    i->widget      = w;
    i->className   = w->metaObject()->className();
    d->dialogList.append(i);
}

void PsiCon::dialogUnregister(QWidget *w)
{
    for (QList<item_dialog *>::Iterator it = d->dialogList.begin(); it != d->dialogList.end();) {
        item_dialog *i = *it;
        if (i->widget == w) {
            it = d->dialogList.erase(it);
            delete i;
        } else
            ++it;
    }
}

void PsiCon::deleteAllDialogs()
{
    while (!d->dialogList.isEmpty()) {
        item_dialog *i = d->dialogList.takeFirst();
        delete i->widget;
        delete i;
    }
    d->tabManager->deleteAll();
}

AccountsComboBox *PsiCon::accountsComboBox(QWidget *parent, bool online_only)
{
    AccountsComboBox *acb = new AccountsComboBox(parent);
    acb->setController(this);
    acb->setOnlineOnly(online_only);
    return acb;
}

PsiAccount *PsiCon::createAccount(const QString &name, const Jid &j, const QString &pass, bool opt_host,
                                  const QString &host, int port, bool legacy_ssl_probe, UserAccount::SSLFlag ssl,
                                  QString proxy, const QString &tlsOverrideDomain, const QByteArray &tlsOverrideCert)
{
    return d->contactList->createAccount(name, j, pass, opt_host, host, port, legacy_ssl_probe, ssl, proxy,
                                         tlsOverrideDomain, tlsOverrideCert);
}

PsiAccount *PsiCon::createAccount(const UserAccount &_acc)
{
    UserAccount acc = _acc;
    PsiAccount *pa  = new PsiAccount(acc, d->contactList, d->tabManager);
    //    connect(&d->idle, SIGNAL(secondsIdle(int)), pa, SLOT(secondsIdle(int)));
    connect(pa, SIGNAL(updatedActivity()), SLOT(pa_updatedActivity()));
    connect(pa, SIGNAL(updatedAccount()), SLOT(pa_updatedAccount()));
    connect(pa, SIGNAL(queueChanged()), SLOT(queueChanged()));
    connect(pa, SIGNAL(startBounce()), SLOT(startBounce()));
    connect(pa, SIGNAL(disconnected()), SLOT(proceedWithSleep()));
    pa->client()->setTcpPortReserver(d->tcpPortReserver);
    return pa;
}

void PsiCon::removeAccount(PsiAccount *pa) { d->contactList->removeAccount(pa); }

void PsiCon::setAccountsOrder(QList<PsiAccount *> accounts) { d->contactList->setAccountsOrder(accounts); }

void PsiCon::statusMenuChanged(XMPP::Status::Type x, bool forceDialog)
{
    QString optionName;
    if (!forceDialog) {
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
    PsiOptions *o = PsiOptions::instance();
    // If option name is not empty (it is empty for Invisible) and option is set to ask for message, show dialog
    if (forceDialog
        || (!optionName.isEmpty() && o->getOption("options.status.ask-for-message-on-" + optionName).toBool())) {
        StatusSetDlg *w = new StatusSetDlg(this, makeLastStatus(x), lastPriorityNotEmpty());
        connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)),
                SLOT(setGlobalStatus(const XMPP::Status &, bool, bool)));
        connect(w, SIGNAL(cancelled()), SLOT(updateMainwinStatus()));
        if (o->getOption("options.ui.systemtray.enable").toBool())
            connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)), d->mainwin,
                    SLOT(setTrayToolTip(const XMPP::Status &, bool, bool)));
        w->show();
    } else {
        Status status;
        switch (x) {
        case STATUS_OFFLINE:
            status = PsiAccount::loggedOutStatus();
            break;
        case STATUS_INVISIBLE:
            status = Status("", "", 0, true);
            status.setIsInvisible(true);
            break;
        default:
            status = Status(XMPP::Status::Type(x), "", 0);
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
    // bool active = false;
    bool               loggedIn = false;
    XMPP::Status::Type state    = XMPP::Status::Online;
    for (PsiAccount *account : d->contactList->enabledAccounts()) {
        //        if(account->isActive())
        //            active = true;
        if (account->loggedIn()) {
            loggedIn = true;
            state    = account->status().type();
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
    for (PsiAccount *account : d->contactList->enabledAccounts()) {
        if (account->loggedIn()) {
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
    s.setStatus(message); // yes, a bit different naming convention..
    setGlobalStatus(s, false, isManualStatus);
}

void PsiCon::setGlobalStatus(const Status &s, bool withPriority, bool isManualStatus)
{
    // Check whether all accounts are logged off
    bool allOffline = true;

    for (PsiAccount *account : d->contactList->enabledAccounts()) {
        if (account->isActive()) {
            allOffline = false;
            break;
        }
    }

    // globally set each account which is logged in
    for (PsiAccount *account : d->contactList->enabledAccounts())
        if ((allOffline || account->isActive())
            && (!account->accountOptions().ignore_global_actions || s.type() == Status::Offline))
            account->setStatus(s, withPriority, isManualStatus);

    emit statusMessageChanged(s.status());
}

void PsiCon::showStatusDialog(const QString &presetName)
{
    StatusPreset preset;
    preset.fromOptions(PsiOptions::instance(), presetName);
    int           priority = preset.priority().hasValue() ? preset.priority().value() : 0;
    Status        status(preset.status(), preset.message(), priority);
    StatusSetDlg *w = new StatusSetDlg(this, status, preset.priority().hasValue());
    connect(w, &StatusSetDlg::set, this, &PsiCon::setGlobalStatus);
    connect(w, &StatusSetDlg::cancelled, this, &PsiCon::updateMainwinStatus);
    if (PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool())
        connect(w, &StatusSetDlg::set, d->mainwin,
                [this](const XMPP::Status &s, bool withPriority, bool isManualStatus) {
                    d->mainwin->setTrayToolTip(s, withPriority, isManualStatus);
                });
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
    PsiAccount *pa = static_cast<PsiAccount *>(sender());
    emit        accountUpdated(pa);

    updateMainwinStatus();
}

void PsiCon::pa_updatedAccount()
{
    PsiAccount *pa = static_cast<PsiAccount *>(sender());
    emit        accountUpdated(pa);

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
    bool active   = false;
    bool loggedIn = false;
    int  state    = STATUS_ONLINE;
    for (PsiAccount *account : d->contactList->enabledAccounts()) {
        if (account->isActive())
            active = true;
        if (account->loggedIn()) {
            loggedIn = true;
            state    = makeSTATUS(account->status());
        }
    }
    if (loggedIn)
        d->mainwin->decorateButton(state);
    else {
        if (active)
            d->mainwin->decorateButton(-1);
        else
            d->mainwin->decorateButton(STATUS_OFFLINE);
    }
}

void PsiCon::doOptions()
{
    OptionsDlg *w = qobject_cast<OptionsDlg *>(dialogFind("OptionsDlg"));
    if (w)
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
    QUrl url = QUrl::fromUserInput(uri);
    openUri(url);
}

void PsiCon::openUri(const QUrl &uri)
{
    // qDebug() << "uri:  " << uri.toString();

    // scheme
    if (uri.scheme() != "xmpp") {
        QMessageBox::critical(nullptr, tr("Unsupported URI type"),
                              QString("URI (link) type \"%1\" is not supported.").arg(uri.scheme()));
        return;
    }

    // authority
    PsiAccount *pa = nullptr;
    // if (uri.authority().isEmpty()) {
    TabbableWidget *tw = findActiveTab();
    if (tw) {
        pa = tw->account();
    }
    if (!pa) {
        pa = d->contactList->defaultAccount();
    }

    if (!pa) {
        QMessageBox::critical(nullptr, tr("Error"),
                              QString("You need to have an account configured and enabled to open URIs (links)."));
        return;
    }
    //
    // TODO: finish authority component handling
    //
    //} else {
    //    qDebug() << "uri auth: [" << uri.authority() << "]");

    //    // is there such account ready to use?
    //    Jid authJid = JIDUtil::fromString(uri.authority());
    //    for (PsiAccount* acc: d->contactList->enabledAccounts()) {
    //        if (acc->jid().compare(authJid, false)) {
    //            pa = acc;
    //        }
    //    }

    //    // or maybe it is configured but not enabled?
    //    if (!pa) {
    //        for (PsiAccount* acc: d->contactList->accounts()) {
    //            if (acc->jid().compare(authJid, false)) {
    //                QMessageBox::error(0, tr("Error"), QString("The account for %1 JID is disabled right
    //                now.").arg(authJid.bare())); return;    // TODO: Should suggest enabling it now
    //            }
    //        }
    //    }

    //    // nope..
    //    if (!pa) {
    //        QMessageBox::error(0, tr("Error"), QString("You don't have an account for %1.").arg(authJid.bare()));
    //        return;
    //    }
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
    OptionsDlg *w = qobject_cast<OptionsDlg *>(dialogFind("OptionsDlg"));
    if (w) {
        w->openTab("toolbars");
        bringToFront(w);
    } else {
        w = new OptionsDlg(this);
        connect(w, SIGNAL(applyOptions()), SLOT(slotApplyOptions()));
        w->openTab("toolbars");
        w->show();
    }
}

void PsiCon::doStatusPresets()
{
    OptionsDlg *w = qobject_cast<OptionsDlg *>(dialogFind("OptionsDlg"));
    if (w) {
        w->openTab("status");
        bringToFront(w);
    } else {
        w = new OptionsDlg(this);
        connect(w, SIGNAL(applyOptions()), SLOT(slotApplyOptions()));
        w->openTab("status");
        w->show();
    }
}

void PsiCon::optionChanged(const QString &option)
{
    // bool notifyRestart = true;

    // Global shortcuts
    setShortcuts();

    // Idle server
    d->idleSettings_.update();
    if (d->idleSettings_.useAway || d->idleSettings_.useNotAvailable || d->idleSettings_.useOffline
        || d->idleSettings_.useIdleServer)
        d->idle.start();
    else {
        d->idle.stop();
        d->idleSettings_.secondsIdle = 0;
    }

    if (option == QString::fromLatin1("options.ui.notifications.alert-style")) {
        alertIconUpdateAlertStyle();
    }

    if (option == QString::fromLatin1("options.ui.tabs.use-tabs")
        || option == QString::fromLatin1("options.ui.tabs.grouping")
        || option == QString::fromLatin1("options.ui.tabs.show-tab-buttons")) {
        QMessageBox::information(nullptr, tr("Information"),
                                 tr("Some of the options you changed will only have full effect upon restart."));
        // notifyRestart = false;
    }

    // update s5b
    QString checkOpt = QString::fromLatin1("options.p2p.bytestreams.listen-port");
    if (option == checkOpt) {
        int port = PsiOptions::instance()->getOption(checkOpt).toInt();
        if (port > 0 && port < 65536) {
            d->byteStreamsPort = quint16(port);
        }
    }

    checkOpt = QString::fromLatin1("options.p2p.bytestreams.external-address");
    if (option == checkOpt) {
        d->externalByteStreamsAddress = PsiOptions::instance()->getOption(checkOpt).toString();
    }

    if (option == "options.ui.chat.css") {
        QString css = PsiOptions::instance()->getOption(option).toString();
        if (!css.isEmpty())
            d->iconSelect->setStyleSheet(css);
        return;
    }

    if (option == QString::fromLatin1("options.ui.spell-check.langs")) {
        if (PsiOptions::instance()->getOption("options.ui.spell-check.enabled").toBool()) {
            auto langs = LanguageManager::deserializeLanguageSet(PsiOptions::instance()->getOption(option).toString());
            if (langs.isEmpty()) {
                langs = SpellChecker::instance()->getAllLanguages();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                auto list = LanguageManager::bestUiMatch(langs);
                langs     = QSet<LanguageManager::LangId>(list.begin(), list.end());
#else
                langs = LanguageManager::bestUiMatch(langs).toSet();
#endif
            }
            SpellChecker::instance()->setActiveLanguages(langs);
        }
        return;
    }

#ifdef USE_PEP
    if (option == tuneUrlFilterOptionPath || option == tuneTitleFilterOptionPath) {
        d->tuneManager->setTuneFilters(
            PsiOptions::instance()->getOption(tuneUrlFilterOptionPath).toString().split(QRegExp("\\W+")),
            PsiOptions::instance()->getOption(tuneTitleFilterOptionPath).toString());
    }
    if (option == tuneControllerFilterOptionPath || option == tunePublishOptionPath) {
        if (PsiOptions::instance()->getOption(tunePublishOptionPath).toBool()) {
            d->tuneManager->updateControllers(
                PsiOptions::instance()->getOption(tuneControllerFilterOptionPath).toString().split(QRegExp("[,]\\s*")));
        } else {
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
        bool        toolbarsVisible = false;
        auto const &directChildren  = o->getChildOptionNames("options.ui.contactlist.toolbars", true, true);
        for (const QString &base : directChildren) {
            if (o->getOption(base + QLatin1String(".visible")).toBool()) {
                toolbarsVisible = true;
                break;
            }
        }

        // Check whether it is legal to disable the menubar
        if (!toolbarsVisible) {
            QMessageBox::warning(nullptr, tr("Warning"),
                                 tr("You can not disable <i>all</i> toolbars <i>and</i> the menubar. If you do so, you "
                                    "will be unable to enable them back, when you'll change your mind."),
                                 tr("I understand"));
            o->setOption("options.ui.contactlist.show-menubar", true);
        }
    }
#endif

    // mainwin stuff
    d->mainwin->setWindowOpts(o->getOption("options.ui.contactlist.always-on-top").toBool(),
                              (o->getOption("options.ui.systemtray.enable").toBool()
                               && o->getOption("options.contactlist.use-toolwindow").toBool()));
    d->mainwin->setUseDock(o->getOption("options.ui.systemtray.enable").toBool());
    d->mainwin->buildToolbars();
    d->mainwin->setUseAvatarFrame(o->getOption("options.ui.contactlist.show-avatar-frame").toBool());
    d->mainwin->reinitAutoHide();

    // notify about options change
    emit emitOptionsUpdate();
}

void PsiCon::queueChanged()
{
    PsiIcon *   nextAnim   = nullptr;
    int         nextAmount = d->contactList->queueCount();
    PsiAccount *pa         = d->contactList->queueLowestEventId();
    if (pa)
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

void PsiCon::proceedWithSleep()
{
    for (PsiAccount *account : d->contactList->enabledAccounts()) {
        if (account->loggedIn()) {
            return; // we need all disconnedted to proceed with sleep
        }
    }
    SystemWatch::instance()->proceedWithSleep();
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
    if (pa)
        pa->openNextEvent(UserAction);
}

void PsiCon::playSound(const QString &str)
{
    if (str.isEmpty() || !PsiOptions::instance()->getOption("options.ui.notifications.sounds.enable").toBool())
        return;

    soundPlay(str);
}

void PsiCon::raiseMainwin() { d->mainwin->showNoFocus(); }

bool PsiCon::mainWinVisible() const { return d->mainwin->isVisible(); }

QStringList PsiCon::recentGCList() const
{
    return PsiOptions::instance()->getOption("options.muc.recent-joins.jids").toStringList();
}

void PsiCon::recentGCAdd(const QString &str)
{
    QStringList recentList = recentGCList();
    // remove it if we have it
    for (const QString &s : recentList) {
        if (s == str) {
            recentList.removeAll(s);
            break;
        }
    }

    // put it in the front
    recentList.prepend(str);

    // trim the list if bigger than 10
    int max = PsiOptions::instance()->getOption("options.muc.recent-joins.maximum").toInt();
    while (recentList.count() > max) {
        recentList.takeLast();
    }

    PsiOptions::instance()->setOption("options.muc.recent-joins.jids", recentList);
}

void PsiCon::recentGCRemove(const QString &str)
{
    QStringList recentList = recentGCList();
    // remove it if we have it
    for (const QString &s : recentList) {
        if (s == str) {
            recentList.removeAll(s);
            break;
        }
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
    for (const QString &s : recentList) {
        if (s == str) {
            recentList.removeAll(s);
            break;
        }
    }

    // put it in the front
    recentList.prepend(str);

    // trim the list if bigger than 10
    while (recentList.count() > 10) {
        recentList.takeLast();
    }

    PsiOptions::instance()->setOption("options.ui.service-discovery.recent-jids", recentList);
}

const QStringList &PsiCon::recentNodeList() const { return d->recentNodeList; }

void PsiCon::recentNodeAdd(const QString &str)
{
    // remove it if we have it
    for (const QString &s : qAsConst(d->recentNodeList)) {
        if (s == str) {
            d->recentNodeList.removeAll(s);
            break;
        }
    }

    // put it in the front
    d->recentNodeList.prepend(str);

    // trim the list if bigger than 10
    while (d->recentNodeList.count() > 10)
        d->recentNodeList.takeLast();
}

void PsiCon::proxy_settingsChanged() { saveAccounts(); }

IconSelectPopup *PsiCon::iconSelectPopup() const { return d->iconSelect; }

bool PsiCon::filterEvent(const PsiAccount *acc, const PsiEvent::Ptr &e) const
{
    Q_UNUSED(acc);
    Q_UNUSED(e);
    return false;
}

void PsiCon::processEvent(const PsiEvent::Ptr &e, ActivationType activationType)
{
    if (!e->account()) {
        return;
    }

    if (e->type() == PsiEvent::PGP) {
        e->account()->eventQueue()->dequeue(e);
        emit e->account()->queueChanged();
        return;
    }

    UserListItem *u = e->account()->find(e->jid());
    if (!u) {
        qWarning("SYSTEM MESSAGE: Bug #1. Contact the developers and tell them what you did to make this message "
                 "appear. Thank you.");
        e->account()->eventQueue()->dequeue(e);
        emit e->account()->queueChanged();
        return;
    }

#ifdef FILETRANSFER
    if (e->type() == PsiEvent::File) {
        FileEvent::Ptr fe   = e.staticCast<FileEvent>();
        FileTransfer * ft   = fe->takeFileTransfer();
        auto *         sess = fe->takeJingleSession();
        e->account()->eventQueue()->dequeue(e);
        emit e->account()->queueChanged();
        e->account()->cpUpdate(*u);
        if (ft) {
            FileRequestDlg *w = new FileRequestDlg(fe->timeStamp(), ft, e->account());
            bringToFront(w);
        }
        if (sess) {
            // TODO design dialog for Jingle file transfer
            auto w = new MultiFileTransferDlg(e->account());
            w->initIncoming(sess);
            bringToFront(w);
        }
        return;
    }
#endif

    if (e->type() == PsiEvent::AvCallType) {
        AvCallEvent::Ptr ae   = e.staticCast<AvCallEvent>();
        AvCall *         sess = ae->takeAvCall();
        e->account()->eventQueue()->dequeue(e);
        emit e->account()->queueChanged();
        e->account()->cpUpdate(*u);
        if (sess) {
            if (!sess->jid().isEmpty()) {
                CallDlg *w = new CallDlg(e->account(), nullptr);
                w->setAttribute(Qt::WA_DeleteOnClose);
                w->setIncoming(sess);
                bringToFront(w);
            } else {
                QMessageBox::information(nullptr, tr("Call ended"), tr("Other party canceled call."));
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
    if (e->type() == PsiEvent::Message) {
        MessageEvent::Ptr me = e.staticCast<MessageEvent>();
        const Message &   m  = me->message();
#ifdef GROUPCHAT
        if (m.type() == "groupchat") {
            isMuc = true;
        } else {
#endif
            bool emptyForm = m.getForm().fields().empty();
            // FIXME: Refactor this, PsiAccount and PsiEvent out
            if (m.type() == "chat" && emptyForm) {
                isChat           = true;
                sentToChatWindow = me->sentToChatWindow();
            }
#ifdef GROUPCHAT
        }
#endif
    }

    if (isChat) {
        PsiAccount *account = e->account();
        XMPP::Jid   from    = e->from();

        if (PsiOptions::instance()->getOption("options.ui.chat.alert-for-already-open-chats").toBool()
            && sentToChatWindow) {
            // Message already displayed, need only to pop up chat dialog, so that
            // it will be read (or marked as read)
            ChatDlg *c = account->findChatDialogEx(from);
            if (!c)
                c = account->findChatDialogEx(e->jid());
            if (!c)
                return; // should never happen

            account->processChats(from); // this will delete all events, corresponding to that chat dialog
        }

        // as the event could be deleted just above, we're using cached account and from values
        account->openChat(from, activationType);
    }
#ifdef WHITEBOARDING
    else if (e->type() == PsiEvent::Sxe) {
        e->account()->eventQueue()->dequeue(e);
        emit e->account()->queueChanged();
        e->account()->cpUpdate(*u);
        e->account()->wbManager()->requestActivated((e.staticCast<SxeEvent>())->id());
        return;
    }
#endif
    else {
#ifdef GROUPCHAT
        if (isMuc) {
            PsiAccount *account = e->account();
            GCMainDlg * c       = account->findDialog<GCMainDlg *>(e->from());
            if (c) {
                c->ensureTabbedCorrectly();
                c->bringToFront(true);
                return;
            }
        }
#endif
        // search for an already opened eventdlg
        EventDlg *w = e->account()->findDialog<EventDlg *>(u->jid());

        if (!w) {
            // create the eventdlg
            w = e->account()->createEventDlg(u->jid());

            // load next message
            e->account()->processReadNext(*u);
        }

        if (w)
            bringToFront(w);
    }
}

void PsiCon::removeEvent(const PsiEvent::Ptr &e)
{
    PsiAccount *account = e->account();
    if (!account)
        return;
    UserListItem *u = account->find(e->jid());
    account->eventQueue()->dequeue(e);
    emit account->queueChanged();
    if (u)
        account->cpUpdate(*u);
}

void PsiCon::doSleep() { setGlobalStatus(Status(Status::Offline, tr("Computer went to sleep"), 0)); }

void PsiCon::doWakeup()
{
    // TODO: Restore the status from before the log out. Make it an (hidden) option for people with a bad wireless
    // connection.
    // setGlobalStatus(Status());

    d->wakeupPending = true; // and wait for signal till network session is opened (proved to work on gentoo+nm+xfce)

    // TODO QNetworkSession was deprecated and removed. Think again how we can live with it
    QTimer::singleShot(3000, this, SLOT(networkSessionOpened())); // a fallback if QNetworkSession doesn't signal
    // if 5secs is not enough to connect to the network then it isn't considered a wakeup anymore but a regular
    // reconnect.
}

void PsiCon::networkSessionOpened()
{
    if (d->wakeupPending) {
        d->wakeupPending = false;
        for (PsiAccount *account : d->contactList->enabledAccounts()) {
            account->doWakeup();
        }

        if (d->contactList && d->contactList->defaultAccount())
            emit statusMessageChanged(d->contactList->defaultAccount()->status().status());
    }
}

PsiActionList *PsiCon::actionList() const { return d->actionList; }

/**
 * Prompts user to create new account, if none are currently present in system.
 */
void PsiCon::promptUserToCreateAccount()
{
    QMessageBox  msgBox(QMessageBox::Question, tr("Account setup"),
                       tr("You need to set up an account to start. Would you like to register a new account, or use an "
                          "existing account?"));
    QPushButton *registerButton = msgBox.addButton(tr("Register new account"), QMessageBox::AcceptRole);
    QPushButton *existingButton = msgBox.addButton(tr("Use existing account"), QMessageBox::AcceptRole);
    msgBox.addButton(QMessageBox::Cancel);
    msgBox.exec();
    if (msgBox.clickedButton() == existingButton) {
        AccountModifyDlg w(this);
        w.exec();
    } else if (msgBox.clickedButton() == registerButton) {
        AccountRegDlg w(this);
        int           n = w.exec();
        if (n == QDialog::Accepted) {
            contactList()->createAccount(w.jid().node(), w.jid(), w.pass(), w.useHost(), w.host(), w.port(), false,
                                         w.ssl(), w.proxy(), w.tlsOverrideDomain(), w.tlsOverrideCert());
        }
    }
}

QString PsiCon::optionsFile() const
{
    return pathToProfile(activeProfile, ApplicationInfo::ConfigLocation) + "/options.xml";
}

void PsiCon::forceSavePreferences(QSessionManager &session)
{
    session.setRestartHint(QSessionManager::RestartIfRunning);
    PsiOptions::instance()->save(optionsFile());
    // TODO save any other options
    // TODO warn about unfinished stuff like file transfer
}

void PsiCon::doQuit(int quitCode)
{
    d->quitting = true;
    emit quit(quitCode);
}

void PsiCon::aboutToQuit() { doQuit(QuitProgram); }

void PsiCon::secondsIdle(int sec)
{
    d->idleSettings_.secondsIdle = sec;
    int                  minutes = sec / 60;
    PsiAccount::AutoAway aa;

    if (d->idleSettings_.useOffline && d->idleSettings_.offlineAfter > 0 && minutes >= d->idleSettings_.offlineAfter)
        aa = PsiAccount::AutoAway_Offline;
    else if (d->idleSettings_.useNotAvailable && d->idleSettings_.menuXA && d->idleSettings_.notAvailableAfter > 0
             && minutes >= d->idleSettings_.notAvailableAfter)
        aa = PsiAccount::AutoAway_XA;
    else if (d->idleSettings_.useAway && d->idleSettings_.awayAfter > 0 && minutes >= d->idleSettings_.awayAfter)
        aa = PsiAccount::AutoAway_Away;
    else
        aa = PsiAccount::AutoAway_None;

    for (PsiAccount *pa : d->contactList->enabledAccounts()) {
        if (pa->accountOptions().ignore_global_actions)
            continue;

        pa->setAutoAwayStatus(aa);
    }
}

int PsiCon::idle() const { return d->idleSettings_.secondsIdle; }

ContactUpdatesManager *PsiCon::contactUpdatesManager() const { return contactUpdatesManager_; }

#include "psicon.moc"

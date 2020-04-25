/*
 * profiles.cpp - deal with profiles
 * Copyright (C) 2001-2003  Justin Karneges
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

#include "profiles.h"

#include "advwidget.h"
#include "applicationinfo.h"
#include "atomicxmlfile/atomicxmlfile.h"
#include "chatdlg.h"
#include "common.h"
#include "eventdlg.h"
#include "fancylabel.h"
#include "optionstree.h"
#ifdef HAVE_PGPUTIL
#include "pgputil.h"
#endif
#ifdef PSI_PLUGINS
#include "pluginmanager.h"
#endif
#include "psioptions.h"
#include "psitoolbar.h"
#include "shortcutmanager.h"
#include "varlist.h"
#include "xmpp_xmlcommon.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDir>
#include <QDomElement>
#include <QFileInfo>
#include <QList>
#include <QTextStream>
#include <QUuid>
#include <QtCrypto>

#define PROXY_NONE 0
#define PROXY_HTTPS 1
#define PROXY_SOCKS4 2
#define PROXY_SOCKS5 3

using namespace XMLHelper;
using namespace XMPP;

UserAccount::UserAccount() : lastStatus(XMPP::Status::Online) { reset(); }

void UserAccount::reset()
{
    id                        = QUuid::createUuid().toString();
    name                      = "Default";
    opt_enabled               = true;
    opt_auto                  = false;
    tog_offline               = true;
    tog_away                  = true;
    tog_hidden                = false;
    tog_agents                = true;
    tog_self                  = false;
    customAuth                = false;
    storeSaltedHashedPassword = false;
    req_mutual_auth           = false;
    legacy_ssl_probe          = false;
    security_level            = QCA::SL_None;
    ssl                       = SSL_Auto;
    jid                       = "";
    pass                      = "";
    scramSaltedHashPassword   = "";
    opt_pass                  = false;
    port                      = 5222;
    opt_host                  = false;
    host                      = "";
    opt_automatic_resource    = true;
    priority_dep_on_status    = true;
    ignore_global_actions     = false;
    resource                  = ApplicationInfo::name();
    priority                  = 55;
    ibbOnly                   = false;
    opt_keepAlive             = true;
    opt_sm                    = true;
    allow_plain               = XMPP::ClientStream::AllowPlainOverTLS;
    opt_compress              = true;
    opt_log                   = true;
    opt_reconn                = true;
    opt_connectAfterSleep     = false;
    opt_autoSameStatus        = true;
    lastStatusWithPriority    = false;
    opt_ignoreSSLWarnings     = false;

    proxy_index = 0;
    proxy_type  = PROXY_NONE;
    proxy_host  = "";
    proxy_port  = 8080;
    proxy_user  = "";
    proxy_pass  = "";

    stunHosts.clear();
    stunHosts << "stun.jabber.ru:5249"
              << "stun.habahaba.im"
              << "stun.ekiga.net"
              << "provserver.televolution.net"
              << "stun1.voiceeclipse.net"
              << "stun.callwithus.com"
              << "stun.counterpath.net"
              << "stun.endigovoip.com"
              << "stun.ideasip.com"
              << "stun.internetcalls.com"
              << "stun.noc.ams-ix.net"
              << "stun.phonepower.com"
              << "stun.phoneserve.com"
              << "stun.rnktel.com"
              << "stun.softjoys.com"
              << "stun.sipgate.net"
              << "stun.sipgate.net:10000"
              << "stun.stunprotocol.org"
              << "stun.voipbuster.com"
              << "stun.voxgratia.org";

    stunHost = stunHosts[0];

    keybind.clear();

    roster.clear();
}

UserAccount::~UserAccount() { }

void UserAccount::fromOptions(OptionsTree *o, QString base)
{
    // WARNING: If you add any new option here, only read the option if
    // allSetOptions (defined below) contains the new option. If not
    // the code should just leave the default value from the reset()
    // call in place.
    optionsBase = base;

    reset();

    QStringList allSetOptions = o->getChildOptionNames(base, true, true);

    opt_enabled            = o->getOption(base + ".enabled").toBool();
    opt_auto               = o->getOption(base + ".auto").toBool();
    opt_keepAlive          = o->getOption(base + ".keep-alive").toBool();
    opt_sm                 = o->getOption(base + ".enable-sm", true).toBool();
    opt_compress           = o->getOption(base + ".compress").toBool();
    req_mutual_auth        = o->getOption(base + ".require-mutual-auth").toBool();
    legacy_ssl_probe       = o->getOption(base + ".legacy-ssl-probe").toBool();
    opt_automatic_resource = o->getOption(base + ".automatic-resource").toBool();
    priority_dep_on_status = o->getOption(base + ".priority-depends-on-status", false).toBool();
    ignore_global_actions  = o->getOption(base + ".ignore-global-actions").toBool();
    opt_log                = o->getOption(base + ".log").toBool();
    opt_reconn             = o->getOption(base + ".reconn").toBool();
    opt_ignoreSSLWarnings  = o->getOption(base + ".ignore-SSL-warnings").toBool();

    // FIX-ME: See FS#771
    if (o->getChildOptionNames().contains(base + ".connect-after-sleep")) {
        opt_connectAfterSleep = o->getOption(base + ".connect-after-sleep").toBool();
    } else {
        o->setOption(base + ".connect-after-sleep", opt_connectAfterSleep);
    }

    QString tmpId = o->getOption(base + ".id").toString();
    if (!tmpId.isEmpty()) {
        id = tmpId;
    }
    name = o->getOption(base + ".name").toString();
    jid  = o->getOption(base + ".jid").toString();

    customAuth = o->getOption(base + ".custom-auth.use").toBool();
    authid     = o->getOption(base + ".custom-auth.authid").toString();
    realm      = o->getOption(base + ".custom-auth.realm").toString();

    // read scram salted password options
    storeSaltedHashedPassword = o->getOption(base + ".scram.store-salted-password").toBool();
    scramSaltedHashPassword   = o->getOption(base + ".scram.salted-password").toString();

    // read password (we must do this after reading the jid, to decode properly)
    QString tmp = o->getOption(base + ".password", QString()).toString();
    if (!tmp.isEmpty()) {
        opt_pass = true;
        pass     = decodePassword(tmp, jid);
    }

    opt_host       = o->getOption(base + ".use-host").toBool();
    security_level = o->getOption(base + ".security-level").toInt();

    tmp = o->getOption(base + ".ssl").toString();
    if (tmp == "no") {
        ssl = SSL_No;
    } else if (tmp == "yes") {
        ssl = SSL_Yes;
    } else if (tmp == "auto") {
        ssl = SSL_Auto;
    } else if (tmp == "legacy") {
        ssl = SSL_Legacy;
    } else {
        ssl = SSL_Yes;
    }

    host = o->getOption(base + ".host").toString();
    port = quint16(o->getOption(base + ".port").toUInt());

    resource = o->getOption(base + ".resource").toString();
    priority = o->getOption(base + ".priority").toInt();

    if (allSetOptions.contains(base + ".auto-same-status")) {
        opt_autoSameStatus = o->getOption(base + ".auto-same-status").toBool();
        lastStatus.setType(o->getOption(base + ".last-status").toString());
        lastStatus.setStatus(o->getOption(base + ".last-status-message").toString());
        lastStatusWithPriority = o->getOption(base + ".last-with-priority").toBool();
        if (lastStatusWithPriority) {
            lastStatus.setPriority(o->getOption(base + ".last-priority").toInt());
        } else {
            lastStatus.setPriority(defaultPriority(lastStatus));
        }
    }

#ifdef HAVE_PGPUTIL
    QString pgpSecretKeyID = o->getOption(base + ".pgp-secret-key-id").toString();
    if (!pgpSecretKeyID.isEmpty()) {
        QCA::KeyStoreEntry e = PGPUtil::instance().getSecretKeyStoreEntry(pgpSecretKeyID);
        if (!e.isNull())
            pgpSecretKey = e.pgpSecretKey();

        pgpPassPhrase = o->getOption(base + ".pgp-pass-phrase").toString();
        if (!pgpPassPhrase.isEmpty()) {
            pgpPassPhrase = decodePassword(pgpPassPhrase, pgpSecretKeyID);
        }
    }
#endif

    tmp = o->getOption(base + ".allow-plain").toString();
    if (tmp == "never") {
        allow_plain = XMPP::ClientStream::NoAllowPlain;
    } else if (tmp == "always") {
        allow_plain = XMPP::ClientStream::AllowPlain;
    } else if (tmp == "over encryped") {
        allow_plain = XMPP::ClientStream::AllowPlainOverTLS;
    } else {
        allow_plain = XMPP::ClientStream::NoAllowPlain;
    }

    QStringList rosterCache = o->getChildOptionNames(base + ".roster-cache", true, true);
    for (const QString &rbase : rosterCache) {
        RosterItem ri;
        ri.setJid(Jid(o->getOption(rbase + ".jid").toString()));
        ri.setName(o->getOption(rbase + ".name").toString());
        Subscription s;
        s.fromString(o->getOption(rbase + ".subscription").toString());
        ri.setSubscription(s);
        ri.setAsk(o->getOption(rbase + ".ask").toString());
        ri.setGroups(o->getOption(rbase + ".groups").toStringList());
        roster += ri;
    }

    groupState.clear();
    QVariantList states = o->mapKeyList(base + ".group-state");
    for (QVariant k : states) {
        GroupData gd;
        QString   sbase = o->mapLookup(base + ".group-state", k);
        gd.open         = o->getOption(sbase + ".open").toBool();
        gd.rank         = o->getOption(sbase + ".rank").toInt();
        groupState.insert(k.toString(), gd);
    }

    proxyID = o->getOption(base + ".proxy-id").toString();

    keybind.fromOptions(o, base + ".pgp-key-bindings");

    dtProxy = o->getOption(base + ".bytestreams-proxy").toString();
    ibbOnly = o->getOption(base + ".ibb-only").toBool();

    if (allSetOptions.contains(base + ".stun-hosts")) {
        stunHosts = o->getOption(base + ".stun-hosts").toStringList();
        if (allSetOptions.contains(base + ".stun-host")) {
            stunHost = o->getOption(base + ".stun-host").toString();
        }
    } else if (!o->getOption(base + ".stun-host").toString().isEmpty()) {
        stunHost = o->getOption(base + ".stun-host").toString();
    }
    if (allSetOptions.contains(base + ".stun-username")) {
        stunUser = o->getOption(base + ".stun-username").toString();
    }
    if (allSetOptions.contains(base + ".stun-password")) {
        stunPass = o->getOption(base + ".stun-password").toString();
    }

    if (allSetOptions.contains(base + ".tls")) {
        tlsOverrideCert   = o->getOption(base + ".tls.override-certificate").toByteArray();
        tlsOverrideDomain = o->getOption(base + ".tls.override-domain").toString();
    }

    alwaysVisibleContacts = o->getOption(base + ".always-visible-contacts").toStringList();
    localMucBookmarks     = o->getOption(base + ".muc-bookmarks").toStringList();
    ignoreMucBookmarks    = o->getOption(base + ".muc-bookmarks-ignore").toStringList();
}

void UserAccount::toOptions(OptionsTree *o, QString base)
{
    if (base.isEmpty()) {
        base = optionsBase;
    }
    // clear old data away
    o->removeOption(base, true);

    o->setOption(base + ".enabled", opt_enabled);
    o->setOption(base + ".auto", opt_auto);
    o->setOption(base + ".keep-alive", opt_keepAlive);
    o->setOption(base + ".enable-sm", opt_sm);
    o->setOption(base + ".compress", opt_compress);
    o->setOption(base + ".require-mutual-auth", req_mutual_auth);
    o->setOption(base + ".legacy-ssl-probe", legacy_ssl_probe);
    o->setOption(base + ".automatic-resource", opt_automatic_resource);
    o->setOption(base + ".priority-depends-on-status", priority_dep_on_status);
    o->setOption(base + ".ignore-global-actions", ignore_global_actions);
    o->setOption(base + ".log", opt_log);
    o->setOption(base + ".reconn", opt_reconn);
    o->setOption(base + ".connect-after-sleep", opt_connectAfterSleep);
    o->setOption(base + ".auto-same-status", opt_autoSameStatus);
    o->setOption(base + ".ignore-SSL-warnings", opt_ignoreSSLWarnings);

    o->setOption(base + ".id", id);
    o->setOption(base + ".name", name);
    o->setOption(base + ".jid", jid);

    o->setOption(base + ".custom-auth.use", customAuth);
    o->setOption(base + ".custom-auth.authid", authid);
    o->setOption(base + ".custom-auth.realm", realm);

    o->setOption(base + ".scram.store-salted-password", storeSaltedHashedPassword);
    o->setOption(base + ".scram.salted-password", scramSaltedHashPassword);

#ifdef HAVE_KEYCHAIN
    if (!isKeychainEnabled()) {
#endif
        if (opt_pass) {
            o->setOption(base + ".password", encodePassword(pass, jid));
        } else {
            o->setOption(base + ".password", "");
        }
#ifdef HAVE_KEYCHAIN
    }
#endif
    o->setOption(base + ".use-host", opt_host);
    o->setOption(base + ".security-level", security_level);
    switch (ssl) {
    case SSL_No:
        o->setOption(base + ".ssl", "no");
        break;
    case SSL_Yes:
        o->setOption(base + ".ssl", "yes");
        break;
    case SSL_Auto:
        o->setOption(base + ".ssl", "auto");
        break;
    case SSL_Legacy:
        o->setOption(base + ".ssl", "legacy");
        break;
    default:
        qFatal("unknown ssl enum value in UserAccount::toOptions");
    }
    o->setOption(base + ".host", host);
    o->setOption(base + ".port", port);
    o->setOption(base + ".resource", resource);
    o->setOption(base + ".priority", priority);
    if (!pgpSecretKey.isNull()) {
        o->setOption(base + ".pgp-secret-key-id", pgpSecretKey.keyId());
        o->setOption(base + ".pgp-pass-phrase", encodePassword(pgpPassPhrase, pgpSecretKey.keyId()));
    } else {
        o->setOption(base + ".pgp-secret-key-id", "");
        o->setOption(base + ".pgp-pass-phrase", "");
    }
    switch (allow_plain) {
    case XMPP::ClientStream::NoAllowPlain:
        o->setOption(base + ".allow-plain", "never");
        break;
    case XMPP::ClientStream::AllowPlain:
        o->setOption(base + ".allow-plain", "always");
        break;
    case XMPP::ClientStream::AllowPlainOverTLS:
        o->setOption(base + ".allow-plain", "over encryped");
        break;
    default:
        qFatal("unknown allow_plain enum value in UserAccount::toOptions");
    }

    int idx = 0;
    for (RosterItem ri : roster) {
        QString rbase = base + ".roster-cache.a" + QString::number(idx++);
        o->setOption(rbase + ".jid", ri.jid().full());
        o->setOption(rbase + ".name", ri.name());
        o->setOption(rbase + ".subscription", ri.subscription().toString());
        o->setOption(rbase + ".ask", ri.ask());
        o->setOption(rbase + ".groups", ri.groups());
    }

    // now we check for redundant entries
    QStringList   groupList;
    QSet<QString> removeList;
    groupList << "/\\/" + name + "\\/\\"; // account name is a very 'special' group

    // special groups that should also have their state remembered
    groupList << qApp->translate("ContactProfile", "General");
    groupList << qApp->translate("ContactProfile", "Agents/Transports");

    // first, add all groups' names to groupList
    for (RosterItem i : roster) {
        groupList += i.groups();
    }

    // now, check if there's groupState name entry in groupList
    for (const QString &group : groupState.keys()) {
        if (!groupList.contains(group)) {
            removeList << group;
        }
    }

    // remove redundant groups
    for (const QString &group : removeList) {
        groupState.remove(group);
    }

    // and finally, save the data
    for (const QString &group : groupState.keys()) {
        QString groupBase = o->mapPut(base + ".group-state", group);
        o->setOption(groupBase + ".open", groupState[group].open);
        o->setOption(groupBase + ".rank", groupState[group].rank);
    }

    o->setOption(base + ".proxy-id", proxyID);

    keybind.toOptions(o, base + ".pgp-key-bindings");
    o->setOption(base + ".bytestreams-proxy", dtProxy.full());
    o->setOption(base + ".ibb-only", ibbOnly);

    o->setOption(base + ".stun-hosts", stunHosts);
    o->setOption(base + ".stun-host", stunHost);
    o->setOption(base + ".stun-username", stunUser);
    o->setOption(base + ".stun-password", stunPass);

    o->setOption(base + ".tls.override-certificate", tlsOverrideCert);
    o->setOption(base + ".tls.override-domain", tlsOverrideDomain);
    saveLastStatus(o, base);

    o->setOption(base + ".always-visible-contacts", alwaysVisibleContacts);
    o->setOption(base + ".muc-bookmarks", localMucBookmarks);
    o->setOption(base + ".muc-bookmarks-ignore", ignoreMucBookmarks);
}

int UserAccount::defaultPriority(const XMPP::Status &s)
{
    if (priority_dep_on_status) {
        if (s.isAvailable()) {
            return PsiOptions::instance()->getOption("options.status.default-priority." + s.typeString()).toInt();
        } else {
            return 0; // Priority for Offline status, it is not used
        }
    } else {
        return priority;
    }
}

void UserAccount::saveLastStatus(OptionsTree *o, QString base = QString())
{
    if (base.isEmpty()) {
        base = optionsBase;
    }

    o->setOption(base + ".last-status", lastStatus.typeString());
    o->setOption(base + ".last-status-message", lastStatus.status());
    o->setOption(base + ".last-with-priority", lastStatusWithPriority);
    if (lastStatusWithPriority) {
        o->setOption(base + ".last-priority", lastStatus.priority());
    } else {
        o->removeOption(base + ".last-priority");
    }
}

void OptionsMigration::lateMigration()
{
    // Add default chat and groupchat toolbars
    if (PsiOptions::instance()->getOption("options.ui.contactlist.toolbars.m0.name").toString() != "Chat") {
        QStringList pluginsKeys;
#ifdef PSI_PLUGINS
        PluginManager *pm      = PluginManager::instance();
        QStringList    plugins = pm->availablePlugins();
        for (const QString &plugin : plugins) {
            pluginsKeys << pm->shortName(plugin) + "-plugin";
        }
#endif
        ToolbarPrefs chatToolbar;
        chatToolbar.on = PsiOptions::instance()->getOption("options.ui.chat.central-toolbar").toBool();
        PsiOptions::instance()->removeOption("options.ui.chat.central-toolbar");
        chatToolbar.name = "Chat";
        chatToolbar.keys << "chat_clear"
                         << "chat_find"
                         << "chat_html_text"
                         << "chat_add_contact"
                         << "chat_share_files";
        chatToolbar.keys += pluginsKeys;
        chatToolbar.keys << "spacer"
                         << "chat_icon"
                         << "chat_file"
                         << "chat_pgp"
                         << "chat_info"
                         << "chat_history"
                         << "chat_voice"
                         << "chat_active_contacts"
                         << "chat_templates";

        if (PsiOptions::instance()->getOption("options.ui.chat.disable-paste-send").toBool()) {
            chatToolbar.keys.removeAt(chatToolbar.keys.indexOf("chat_ps"));
        }

        ToolbarPrefs groupchatToolbar;
        groupchatToolbar.on = chatToolbar.on;

        groupchatToolbar.name = "Groupchat";
        groupchatToolbar.keys << "gchat_info"
                              << "gchat_clear"
                              << "gchat_find"
                              << "gchat_html_text"
                              << "gchat_configure"
                              << "gchat_share_files"
                              << "gchat_templates";
        groupchatToolbar.keys += pluginsKeys;
        groupchatToolbar.keys << "spacer"
                              << "gchat_icon";

        if (PsiOptions::instance()->getOption("options.ui.chat.disable-paste-send").toBool()) {
            groupchatToolbar.keys.removeAt(groupchatToolbar.keys.indexOf("gchat_ps"));
        }
        PsiOptions::instance()->removeOption("options.ui.chat.disable-paste-send");

        QList<ToolbarPrefs> toolbars;
        toolbars << chatToolbar << groupchatToolbar;

        QStringList toolbarBases
            = PsiOptions::instance()->getChildOptionNames("options.ui.contactlist.toolbars", true, true);
        for (const QString &base : toolbarBases) {
            ToolbarPrefs tb;
            tb.id   = PsiOptions::instance()->getOption(base + ".key").toString();
            tb.name = PsiOptions::instance()->getOption(base + ".name").toString();
            if (tb.id.isEmpty() || tb.name.isEmpty()) {
                qDebug("Does not look like a toolbar");
                continue;
            }

            tb.on     = PsiOptions::instance()->getOption(base + ".visible").toBool();
            tb.locked = PsiOptions::instance()->getOption(base + ".locked").toBool();
            tb.dock   = Qt3Dock(PsiOptions::instance()->getOption(base + ".dock.position").toInt()); // FIXME
            tb.nl     = PsiOptions::instance()->getOption(base + ".dock.nl").toBool();
            tb.keys   = PsiOptions::instance()->getOption(base + ".actions").toStringList();

            toolbars << tb;
        }

        PsiOptions::instance()->removeOption("options.ui.contactlist.toolbars", true);

        for (ToolbarPrefs tb : toolbars) {
            tb.locked = true;
            PsiToolBar::structToOptions(PsiOptions::instance(), tb);
        }
    }

    // 2016-02-09 touches Psi+ users. but let it be here for awhile
    if (PsiOptions::instance()->getOption("options.contactlist.use-autohide", false).toBool()) {
        PsiOptions::instance()->setOption("options.contactlist.autohide-interval", 0);
        PsiOptions::instance()->removeOption("options.contactlist.use-autohide");
    }

    // avoid duplicating Esc for hide and close
    auto         hideKS  = ShortcutManager::readShortcutsFromOptions("common.hide", PsiOptions::instance());
    auto         closeKS = ShortcutManager::readShortcutsFromOptions("common.close", PsiOptions::instance());
    QKeySequence escKS(Qt::Key_Escape);
    if (hideKS.contains(escKS) && closeKS.contains(escKS)) {
        closeKS.removeAll(escKS);
        QVariantList vl;
        for (auto &ks : closeKS) {
            vl.append(QVariant::fromValue(ks));
        }

        PsiOptions::instance()->setOption("options.shortcuts.common.close", vl);
    }
}

QString pathToProfile(const QString &name, ApplicationInfo::HomedirType type)
{
    return ApplicationInfo::profilesDir(type) + "/" + name;
}

QString pathToProfileConfig(const QString &name)
{
    return pathToProfile(name, ApplicationInfo::ConfigLocation) + "/config.xml";
}

QStringList getProfilesList()
{
    QStringList list;

    QDir d(ApplicationInfo::profilesDir(ApplicationInfo::ConfigLocation));
    if (!d.exists())
        return list;

    QStringList entries = d.entryList();
    for (QStringList::Iterator it = entries.begin(); it != entries.end(); ++it) {
        if (*it == "." || *it == "..")
            continue;
        QFileInfo info(d, *it);
        if (!info.isDir())
            continue;

        list.append(*it);
    }

    list.sort();

    return list;
}

bool profileExists(const QString &_name)
{
    QString name = _name.toLower();

    QStringList list = getProfilesList();
    for (QStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {
        if ((*it).toLower() == name)
            return true;
    }
    return false;
}

bool profileNew(const QString &name)
{
    if (name.isEmpty())
        return false;

    // verify the string is sane
    for (int n = 0; n < int(name.length()); ++n) {
        if (!name.at(n).isLetterOrNumber())
            return false;
    }

    // make it
    QDir configProfilesDir(ApplicationInfo::profilesDir(ApplicationInfo::ConfigLocation));
    if (!configProfilesDir.exists())
        return false;
    QDir configCurrentProfileDir(configProfilesDir.path() + "/" + name);
    if (!configCurrentProfileDir.exists()) {
        if (!configProfilesDir.mkdir(name))
            return false;
    }

    QDir dataProfilesDir(ApplicationInfo::profilesDir(ApplicationInfo::DataLocation));
    if (!dataProfilesDir.exists())
        return false;
    QDir dataCurrentProfileDir(dataProfilesDir.path() + "/" + name);
    if (!dataCurrentProfileDir.exists()) {
        if (!dataProfilesDir.mkdir(name))
            return false;
    }
    dataCurrentProfileDir.mkdir("history");

    QDir cacheProfilesDir(ApplicationInfo::profilesDir(ApplicationInfo::CacheLocation));
    if (!cacheProfilesDir.exists())
        return false;
    QDir cacheCurrentProfileDir(cacheProfilesDir.path() + "/" + name);
    if (!cacheCurrentProfileDir.exists()) {
        if (!cacheProfilesDir.mkdir(name))
            return false;
    }
    cacheCurrentProfileDir.mkdir("vcard");

    return true;
}

bool profileRename(const QString &oldname, const QString &name)
{
    // verify the string is sane
    for (int n = 0; n < int(name.length()); ++n) {
        if (!name.at(n).isLetterOrNumber())
            return false;
    }

    // locate the folders
    QStringList paths;
    paths << ApplicationInfo::profilesDir(ApplicationInfo::ConfigLocation);
    if (!paths.contains(ApplicationInfo::profilesDir(ApplicationInfo::DataLocation))) {
        paths << ApplicationInfo::profilesDir(ApplicationInfo::DataLocation);
    }
    if (!paths.contains(ApplicationInfo::profilesDir(ApplicationInfo::CacheLocation))) {
        paths << ApplicationInfo::profilesDir(ApplicationInfo::CacheLocation);
    }

    // First we need to check configDir for existing
    QDir configDir(paths[0]);
    if (!configDir.exists())
        return false;

    // and if all ok we may rename it.
    for (const QString &path : paths) {
        QDir d(path);
        if (!d.exists() || !d.exists(oldname))
            continue;

        if (!d.rename(oldname, name))
            return false;
    }
    return true;
}

static bool folderRemove(const QDir &_d)
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

bool profileDelete(const QStringList &paths)
{
    bool ret = true;
    for (const QString &path : paths) {
        QDir d(path);
        if (!d.exists())
            continue;

        ret = folderRemove(QDir(path));
        if (!ret) {
            break;
        }
    }
    return ret;
}

QString activeProfile;

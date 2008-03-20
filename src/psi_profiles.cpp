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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "profiles.h"
#include "common.h"
#include "applicationinfo.h"
#include <qdir.h>
#include <qfileinfo.h>
#include <qdom.h>

#include <qapplication.h>
//Added by qt3to4:
#include <QTextStream>
#include <QtCrypto>
#include <QList>

#include "eventdlg.h"
#include "chatdlg.h"
#include "pgputil.h"
#include "xmpp_xmlcommon.h"
#include "fancylabel.h"
#include "advwidget.h"
#include "psioptions.h"
#include "varlist.h"
#include "atomicxmlfile.h"
#include "psitoolbar.h"
#include "optionstree.h"

using namespace XMPP;
using namespace XMLHelper;

#define PROXY_NONE       0
#define PROXY_HTTPS      1
#define PROXY_SOCKS4     2
#define PROXY_SOCKS5     3

template<typename T, typename F>
void migrateEntry(const QDomElement& element, const QString& entry, const QString& option, F f)
{
	bool found;
	findSubTag(element, entry, &found);
	if (found) {
		T value;
		f(element, entry, &value);
		PsiOptions::instance()->setOption(option, value);
	}
}

void migrateIntEntry(const QDomElement& element, const QString& entry, const QString& option)
{
	migrateEntry<int>(element, entry, option, readNumEntry);
}

void migrateBoolEntry(const QDomElement& element, const QString& entry, const QString& option)
{
	migrateEntry<bool>(element, entry, option, readBoolEntry);
}

void migrateSizeEntry(const QDomElement& element, const QString& entry, const QString& option)
{
	migrateEntry<QSize>(element, entry, option, readSizeEntry);
}

void migrateStringEntry(const QDomElement& element, const QString& entry, const QString& option)
{
	migrateEntry<QString>(element, entry, option, readEntry);
}

void migrateStringList(const QDomElement& element, const QString& entry, const QString& option)
{
	migrateEntry<QStringList>(element, entry, option, xmlToStringList);
}

void migrateColorEntry(const QDomElement& element, const QString &entry, const QString &option)
{
	migrateEntry<QColor>(element, entry, option, readColorEntry);
}

void migrateRectEntry(const QDomElement& element, const QString &entry, const QString &option)
{
	migrateEntry<QRect>(element, entry, option, readRectEntry);
}



UserAccount::UserAccount()
{
	reset();
}

void UserAccount::reset()
{
	name = "Default";
	opt_enabled = TRUE;
	opt_auto = FALSE;
	tog_offline = TRUE;
	tog_away = TRUE;
	tog_hidden = FALSE;
	tog_agents = TRUE;
	tog_self = FALSE;
	customAuth = FALSE;
	req_mutual_auth = FALSE;
	legacy_ssl_probe = TRUE;
	security_level = QCA::SL_None;
	ssl = SSL_Auto;
	jid = "";
	pass = "";
	opt_pass = FALSE;
	port = 5222;
	opt_host = FALSE;
	host = "";
	opt_automatic_resource = TRUE;
	resource = "Psi";
	priority = 5;
	opt_keepAlive = TRUE;
	allow_plain = XMPP::ClientStream::AllowPlainOverTLS;
	opt_compress = FALSE;
	opt_log = TRUE;
	opt_reconn = FALSE;
	opt_connectAfterSleep = false;
	opt_ignoreSSLWarnings = false;

	proxy_index = 0;
	proxy_type = PROXY_NONE;
	proxy_host = "";
	proxy_port = 8080;
	proxy_user = "";
	proxy_pass = "";

	keybind.clear();

	roster.clear();
}

UserAccount::~UserAccount()
{
}

void UserAccount::fromOptions(OptionsTree *o, QString base)
{
	optionsBase = base;
	
	reset();

	opt_enabled = o->getOption(base + ".enabled").toBool();
	opt_auto = o->getOption(base + ".auto").toBool();
	opt_keepAlive = o->getOption(base + ".keep-alive").toBool();
	opt_compress = o->getOption(base + ".compress").toBool();
	req_mutual_auth = o->getOption(base + ".require-mutual-auth").toBool();
	legacy_ssl_probe = o->getOption(base + ".legacy-ssl-probe").toBool();
	opt_automatic_resource = o->getOption(base + ".automatic-resource").toBool();
	opt_log = o->getOption(base + ".log").toBool();
	opt_reconn = o->getOption(base + ".reconn").toBool();
	opt_ignoreSSLWarnings = o->getOption(base + ".ignore-SSL-warnings").toBool();
	
	// FIX-ME: See FS#771
	if (o->getChildOptionNames().contains(base + ".connect-after-sleep")) {
		opt_connectAfterSleep = o->getOption(base + ".connect-after-sleep").toBool();
	}
	else {
		o->setOption(base + ".connect-after-sleep", opt_connectAfterSleep);
	}
	
	name = o->getOption(base + ".name").toString();
	jid = o->getOption(base + ".jid").toString();

	customAuth = o->getOption(base + ".custom-auth.use").toBool();
	authid = o->getOption(base + ".custom-auth.authid").toString();
	realm = o->getOption(base + ".custom-auth.realm").toString();

	// read password (we must do this after reading the jid, to decode properly)
	QString tmp = o->getOption(base + ".password").toString();
	if(!tmp.isEmpty()) {
		opt_pass = TRUE;
		pass = decodePassword(tmp, jid);
	}
	
	opt_host = o->getOption(base + ".use-host").toBool();
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
	port = o->getOption(base + ".port").toInt();
	
	resource = o->getOption(base + ".resource").toString();
	priority = o->getOption(base + ".priority").toInt();
	
	QString pgpSecretKeyID = o->getOption(base + ".pgp-secret-key-id").toString();
	if (!pgpSecretKeyID.isEmpty()) {
		QCA::KeyStoreEntry e = PGPUtil::instance().getSecretKeyStoreEntry(pgpSecretKeyID);
		if (!e.isNull())
			pgpSecretKey = e.pgpSecretKey();
	}
	
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
	foreach(QString rbase, rosterCache) {
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
	foreach(QVariant k, states) {
		GroupData gd;
		QString sbase = o->mapLookup(base + ".group-state", k);
		gd.open = o->getOption(sbase + ".open").toBool();
		gd.rank = o->getOption(sbase + ".rank").toInt();
		groupState.insert(k.toString(), gd);
	}
	
	proxyID = o->getOption(base + ".proxy-id").toString();

	keybind.fromOptions(o, base + ".pgp-key-bindings");

	dtProxy = o->getOption(base + ".bytestreams-proxy").toString();
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
	o->setOption(base + ".compress", opt_compress);
	o->setOption(base + ".require-mutual-auth", req_mutual_auth);
	o->setOption(base + ".legacy-ssl-probe", legacy_ssl_probe);
	o->setOption(base + ".automatic-resource", opt_automatic_resource);
	o->setOption(base + ".log", opt_log);
	o->setOption(base + ".reconn", opt_reconn);
	o->setOption(base + ".connect-after-sleep", opt_connectAfterSleep);
	o->setOption(base + ".ignore-SSL-warnings", opt_ignoreSSLWarnings);

	o->setOption(base + ".name", name);
	o->setOption(base + ".jid", jid);
	
	o->setOption(base + ".custom-auth.use", customAuth);
	o->setOption(base + ".custom-auth.authid", authid);
	o->setOption(base + ".custom-auth.realm", realm);

	if(opt_pass) {
		o->setOption(base + ".password", encodePassword(pass, jid));
	} else {
		o->setOption(base + ".password", "");
	}
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
	} else {
		o->setOption(base + ".pgp-secret-key-id", "");
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
	foreach(RosterItem ri, roster) {
		QString rbase = base + ".roster-cache.a" + QString::number(idx++);
		o->setOption(rbase + ".jid", ri.jid().full());
		o->setOption(rbase + ".name", ri.name());
		o->setOption(rbase + ".subscription", ri.subscription().toString());
		o->setOption(rbase + ".ask", ri.ask());
		o->setOption(rbase + ".groups", ri.groups());
	}
	
	// now we check for redundant entries
	QStringList groupList;
	QSet<QString> removeList;
	groupList << "/\\/" + name + "\\/\\"; // account name is a very 'special' group

	// special groups that should also have their state remembered
	groupList << qApp->translate("ContactProfile", "General");
	groupList << qApp->translate("ContactProfile", "Agents/Transports");

	// first, add all groups' names to groupList
	foreach(RosterItem i, roster) {
		groupList += i.groups();
	}

	// now, check if there's groupState name entry in groupList
	foreach(QString group, groupState.keys()) {
		if (!groupList.contains(group)) {
			removeList << group;
		}
	}

	// remove redundant groups
	foreach(QString group, removeList) {
		groupState.remove( group );
	}

	// and finally, save the data
	foreach(QString group, groupState.keys()) {
		QString groupBase = o->mapPut(base + ".group-state", group);
		o->setOption(groupBase + ".open", groupState[group].open);
		o->setOption(groupBase + ".rank", groupState[group].rank);
	}
		
	o->setOption(base + ".proxy-id", proxyID);

	keybind.toOptions(o, base + ".pgp-key-bindings");
	o->setOption(base + ".bytestreams-proxy", dtProxy.full());
	
}

void UserAccount::fromXml(const QDomElement &a)
{
	reset();

	bool found;

	readEntry(a, "name", &name);
	readBoolAttribute(a, "enabled", &opt_enabled);
	readBoolAttribute(a, "auto", &opt_auto);
	readBoolAttribute(a, "showOffline", &tog_offline);
	readBoolAttribute(a, "showAway", &tog_away);
	readBoolAttribute(a, "showHidden", &tog_hidden);
	readBoolAttribute(a, "showAgents", &tog_agents);
	readBoolAttribute(a, "showSelf", &tog_self);
	readBoolAttribute(a, "keepAlive", &opt_keepAlive);
	readBoolAttribute(a, "compress", &opt_compress);
	readBoolAttribute(a, "require-mutual-auth", &req_mutual_auth);
	readBoolAttribute(a, "legacy-ssl-probe", &legacy_ssl_probe);
	readBoolAttribute(a, "log", &opt_log);
	readBoolAttribute(a, "reconn", &opt_reconn);
	readBoolAttribute(a, "ignoreSSLWarnings", &opt_ignoreSSLWarnings);
	//readBoolAttribute(a, "gpg", &opt_gpg);
	if (a.hasAttribute("automatic-resource")) {
		readBoolAttribute(a, "automatic-resource", &opt_automatic_resource);
	}
	else {
		opt_automatic_resource = false;
	}
	
	// Will be overwritten if there is a new option
	bool opt_plain = false;
	readBoolAttribute(a, "plain", &opt_plain);
	allow_plain = (opt_plain ? XMPP::ClientStream::AllowPlain : XMPP::ClientStream::NoAllowPlain);
	readNumEntry(a, "allow-plain", (int*) &allow_plain);
	
	// Will be overwritten if there is a new option
	bool opt_ssl = true;
	readBoolAttribute(a, "ssl", &opt_ssl);
	if (opt_ssl)
		ssl = UserAccount::SSL_Legacy;

	readNumEntry(a, "security-level", &security_level);
	readNumEntry(a, "ssl", (int*) &ssl);
	readEntry(a, "host", &host);
	readNumEntry(a, "port", &port);

	// 0.8.6 and >= 0.9
	QDomElement j = findSubTag(a, "jid", &found);
	if(found) {
		readBoolAttribute(j, "manual", &opt_host);
		jid = tagContent(j);
	}
	// 0.8.7
	else {
		QString user, vhost;
		readEntry(a, "username", &user);
		QDomElement j = findSubTag(a, "vhost", &found);
		if(found) {
			readBoolAttribute(j, "manual", &opt_host);
			vhost = tagContent(j);
		}
		else {
			opt_host = false;
			vhost = host;
			host = "";
			port = 0;
		}
		jid = user + '@' + vhost;
	}

	readBoolEntry(a, "useHost", &opt_host);

	// read password (we must do this after reading the jid, to decode properly)
	readEntry(a, "password", &pass);
	if(!pass.isEmpty()) {
		opt_pass = TRUE;
		pass = decodePassword(pass, jid);
	}

	QDomElement ca = findSubTag(a, "custom-auth", &found);
	if(found) {
		readBoolAttribute(ca, "use", &customAuth);
		QDomElement authid_el = findSubTag(ca, "authid", &found);
		if (found)
			authid = tagContent(authid_el);
		QDomElement realm_el = findSubTag(ca, "realm", &found);
		if (found)
			realm = tagContent(realm_el);
	}

	readEntry(a, "resource", &resource);
	readNumEntry(a, "priority", &priority);
	QString pgpSecretKeyID;
	readEntry(a, "pgpSecretKeyID", &pgpSecretKeyID);
	if (!pgpSecretKeyID.isEmpty()) {
		QCA::KeyStoreEntry e = PGPUtil::instance().getSecretKeyStoreEntry(pgpSecretKeyID);
		if (!e.isNull())
			pgpSecretKey = e.pgpSecretKey();
	}

	QDomElement r = findSubTag(a, "roster", &found);
	if(found) {
		for(QDomNode n = r.firstChild(); !n.isNull(); n = n.nextSibling()) {
			QDomElement i = n.toElement();
			if(i.isNull())
				continue;

			if(i.tagName() == "item") {
				RosterItem ri;
				if(!ri.fromXml(i))
					continue;
				roster += ri;
			}
		}
	}

	groupState.clear();
	QDomElement gs = findSubTag(a, "groupState", &found);
	if (found) {
		for (QDomNode n = gs.firstChild(); !n.isNull(); n = n.nextSibling()) {
			QDomElement i = n.toElement();
			if (i.isNull())
				continue;

			if (i.tagName() == "group") {
				GroupData gd;
				gd.open = i.attribute("open") == "true";
				gd.rank = i.attribute("rank").toInt();
                		groupState.insert(i.attribute("name"), gd);
			}
		}
	}

	readNumEntry(a, "proxyindex", &proxy_index);
	readNumEntry(a, "proxytype", &proxy_type);
	readEntry(a, "proxyhost", &proxy_host);
	readNumEntry(a, "proxyport", &proxy_port);
	readEntry(a, "proxyuser", &proxy_user);
	readEntry(a, "proxypass", &proxy_pass);
	if(!proxy_pass.isEmpty())
		proxy_pass = decodePassword(proxy_pass, jid);

	r = findSubTag(a, "pgpkeybindings", &found);
	if(found)
		keybind.fromXml(r);

	QString str;
	readEntry(a, "dtProxy", &str);
	dtProxy = str;
}

#if 0
 LEGOPTFIXME
void OptionsMigration::reset()
{
	bool nix, win, mac;
	nix = win = mac = FALSE;

#ifdef Q_WS_X11
	nix = TRUE;
#endif
#ifdef Q_WS_WIN
	win = TRUE;
#endif
#ifdef Q_WS_MAC
	mac = TRUE;
#endif

	// global
	mwgeom.setRect(64, 64, 150, 360);
	lastStatusString = "";
	useSound = TRUE;
	proxyList.clear();
	acc.clear();

	// LEGOPTP
	LEGOPTP.useleft = FALSE;
	LEGOPTP.singleclick = FALSE;
 	LEGOPTP.hideMenubar = FALSE;
	LEGOPTP.defaultAction = 1;
	LEGOPTP.delChats = dcHour;
	LEGOPTP.browser = 0;
	LEGOPTP.alwaysOnTop = FALSE;
	LEGOPTP.keepSizes = TRUE;
	LEGOPTP.ignoreHeadline = FALSE;
	LEGOPTP.ignoreNonRoster = FALSE;
	LEGOPTP.excludeGroupChatsFromIgnore = TRUE;
	LEGOPTP.useDock = true;
	LEGOPTP.dockDCstyle = win ? TRUE: FALSE;
	LEGOPTP.dockHideMW = FALSE;
	LEGOPTP.dockToolMW = FALSE;
#ifdef Q_WS_MAC
	LEGOPTP.alertStyle = 0;
#else
	LEGOPTP.alertStyle = 2;
#endif
	LEGOPTP.popupMsgs = FALSE;
	LEGOPTP.popupChats = FALSE;
	LEGOPTP.popupHeadlines = FALSE;
	LEGOPTP.popupFiles = FALSE;
	LEGOPTP.noAwayPopup = FALSE;
	LEGOPTP.noUnlistedPopup = false;
	LEGOPTP.raise = FALSE;
	LEGOPTP.incomingAs = 0;
	LEGOPTP.askOnline = FALSE;
	LEGOPTP.askOffline = FALSE;
	LEGOPTP.rosterAnim = TRUE;
	LEGOPTP.autoVCardOnLogin = true;
	LEGOPTP.xmlConsoleOnLogin = false;
	LEGOPTP.asAway = 10;
	LEGOPTP.asXa = 30;
	LEGOPTP.asOffline = 0;
	LEGOPTP.use_asAway = TRUE;
	LEGOPTP.use_asXa = TRUE;
	LEGOPTP.use_asOffline = FALSE;
	LEGOPTP.asMessage = QObject::tr("Auto Status (idle)");
	LEGOPTP.scrollTo = TRUE;
	LEGOPTP.useEmoticons = false;
	LEGOPTP.alertOpenChats = true;
	LEGOPTP.raiseChatWindow = false;
	LEGOPTP.showSubjects = true;
	LEGOPTP.showCounter = false;
	LEGOPTP.chatSays = false;
	LEGOPTP.showGroupCounts = true;
	LEGOPTP.smallChats = false;
	LEGOPTP.brushedMetal = false;
	LEGOPTP.chatLineEdit = true;
	LEGOPTP.useTabs = false;
	LEGOPTP.usePerTabCloseButton = true;
	LEGOPTP.putTabsAtBottom = false;
	LEGOPTP.autoRosterSize = false;
	LEGOPTP.autoRosterSizeGrowTop = false;
	LEGOPTP.autoResolveNicksOnAdd = false;
	LEGOPTP.chatBgImage = "";
	LEGOPTP.rosterBgImage = "";
	LEGOPTP.autoCopy = false;
	LEGOPTP.useCaps = true;
	LEGOPTP.useRC = false;

	LEGOPTP.sp.clear();
	QString name;
	name = QObject::tr("Away from desk");
	LEGOPTP.sp[name] = StatusPreset(name, QObject::tr("I am away from my desk.  Leave a message."), STATUS_AWAY);;
	name = QObject::tr("Showering");
	LEGOPTP.sp[name] = StatusPreset(name, QObject::tr("I'm in the shower.  You'll have to wait for me to get out."), STATUS_AWAY);
	name = QObject::tr("Eating");
	LEGOPTP.sp[name] = StatusPreset(name, QObject::tr("Out eating.  Mmmm.. food."), STATUS_AWAY);
	name = QObject::tr("Sleep");
	LEGOPTP.sp[name] = StatusPreset(name, QObject::tr("Sleep is good.  Zzzzz"),STATUS_DND);
	name = QObject::tr("Work");
	LEGOPTP.sp[name] = StatusPreset(name, QObject::tr("Can't chat.  Gotta work."), STATUS_DND);
	name = QObject::tr("Air");
	LEGOPTP.sp[name] = StatusPreset(name, QObject::tr("Stepping out to get some fresh air."), STATUS_AWAY);
	name = QObject::tr("Movie");
	LEGOPTP.sp[name] = StatusPreset(name, QObject::tr("Out to a movie.  Is that OK with you?"), STATUS_AWAY);
	name = QObject::tr("Secret");
	LEGOPTP.sp[name] = StatusPreset(name, QObject::tr("I'm not available right now and that's all you need to know."), STATUS_XA);
	name = QObject::tr("Out for the night");
	LEGOPTP.sp[name] = StatusPreset(name, QObject::tr("Out for the night."), STATUS_AWAY);
	name = QObject::tr("Greece");
	LEGOPTP.sp[name] = StatusPreset(name, QObject::tr("I have gone to a far away place.  I will be back someday!"), STATUS_XA);
 	LEGOPTP.recentStatus.clear();

	LEGOPTP.color[cOnline]    = QColor("#000000");
	LEGOPTP.color[cListBack]  = QColor("#FFFFFF");
	LEGOPTP.color[cAway]      = QColor("#004BB4");
	LEGOPTP.color[cDND]       = QColor("#7E0000");
	LEGOPTP.color[cOffline]   = QColor("#646464");
	LEGOPTP.color[cStatus]    = QColor("#808080");
	LEGOPTP.color[cProfileFore] = QColor("#FFFFFF");
	LEGOPTP.color[cProfileBack] = QColor("#969696");
	LEGOPTP.color[cGroupFore] = QColor("#5A5A5A");
	LEGOPTP.color[cGroupBack] = QColor("#F0F0F0");
	LEGOPTP.color[cAnimFront] = QColor("#000000");
	LEGOPTP.color[cAnimBack] 	= QColor("#969696");

	LEGOPTP.font[fRoster] = QApplication::font().toString();
	LEGOPTP.font[fMessage] = QApplication::font().toString();
	LEGOPTP.font[fChat] = QApplication::font().toString();
	{
		QFont font = QApplication::font();
		font.setPointSize( font.pointSize() - 2 );
		LEGOPTP.font[fPopup] = font.toString();
	}
	LEGOPTP.clNewHeadings = false;
	LEGOPTP.outlineHeadings = false;

	LEGOPTP.player = "play";
	LEGOPTP.noAwaySound = FALSE;

	LEGOPTP.onevent[eMessage]    = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	LEGOPTP.onevent[eChat1]      = ApplicationInfo::resourcesDir() + "/sound/chat1.wav";
	LEGOPTP.onevent[eChat2]      = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	LEGOPTP.onevent[eSystem]     = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	LEGOPTP.onevent[eHeadline]   = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	LEGOPTP.onevent[eOnline]     = ApplicationInfo::resourcesDir() + "/sound/online.wav";
	LEGOPTP.onevent[eOffline]    = ApplicationInfo::resourcesDir() + "/sound/offline.wav";
	LEGOPTP.onevent[eSend]       = ApplicationInfo::resourcesDir() + "/sound/send.wav";
	LEGOPTP.onevent[eIncomingFT] = ApplicationInfo::resourcesDir() + "/sound/ft_incoming.wav";
	LEGOPTP.onevent[eFTComplete] = ApplicationInfo::resourcesDir() + "/sound/ft_complete.wav";


	LEGOPTP.sizeEventDlg = EventDlg::defaultSize();
	LEGOPTP.sizeChatDlg = ChatDlg::defaultSize();
	LEGOPTP.sizeTabDlg = ChatDlg::defaultSize(); //TODO: no!

	LEGOPTP.jidComplete = true;
	LEGOPTP.grabUrls = false;
	LEGOPTP.noGCSound = true;

	// toolbars
	LEGOPTP.toolbars.clear();

	Options::ToolbarPrefs tbDef;
	tbDef.name = QString::null;
	tbDef.on = false;
	tbDef.locked = false;
	tbDef.stretchable = false;
	tbDef.keys.clear();

	tbDef.dock = Qt::DockTop;
	tbDef.nl = true;
	tbDef.extraOffset = 0;

	tbDef.dirty = true;

	{
		Options::ToolbarPrefs tb[3];
		uint i;
		for (i = 0; i < sizeof(tb)/sizeof(Options::ToolbarPrefs); i++) {
			tb[i] = tbDef;
			tb[i].index = i + 1;
		}

		bool defaultEnableToolbars;
#ifdef Q_WS_MAC
		defaultEnableToolbars = false;
#else
		defaultEnableToolbars = true;
#endif

		// Imitate old Psi button layout by default
		tb[0].name = QObject::tr("Buttons");
		tb[0].on = defaultEnableToolbars;
		tb[0].locked = true;
		tb[0].stretchable = true;
		tb[0].keys << "button_options" << "button_status";
		tb[0].dock = Qt::DockBottom;

		tb[1].name = QObject::tr("Show contacts");
		tb[1].on = false;
		tb[1].locked = true;
		tb[1].keys << "show_offline" << "show_hidden" << "show_agents" << "show_self" << "show_statusmsg"; 

		tb[2].name = QObject::tr("Event notifier");
		tb[2].on = false;
		tb[2].locked = true;
		tb[2].stretchable = true;
		tb[2].keys << "event_notifier";
		tb[2].dock = Qt::DockBottom;
		tb[2].index = 0;

		for ( i = 0; i < sizeof(tb)/sizeof(Options::ToolbarPrefs); i++ )
			LEGOPTP.toolbars["mainWin"].append( tb[i] );
	}

	// groupchat
	LEGOPTP.gcHighlighting = true;
	LEGOPTP.gcHighlights.clear();

	LEGOPTP.gcNickColoring = true;
	LEGOPTP.gcNickColors.clear();

	LEGOPTP.gcNickColors << "Blue";
	LEGOPTP.gcNickColors << "Green";
	LEGOPTP.gcNickColors << "Orange";
	LEGOPTP.gcNickColors << "Purple";
	LEGOPTP.gcNickColors << "Red";

	// popups
	LEGOPTP.ppIsOn        = false;
	LEGOPTP.ppOnline      = true;
	LEGOPTP.ppOffline     = true;
	LEGOPTP.ppStatus      = false;
	LEGOPTP.ppMessage     = true;
	LEGOPTP.ppChat        = true;
	LEGOPTP.ppHeadline    = true;
	LEGOPTP.ppFile        = true;
	LEGOPTP.ppJidClip     = 25;
	LEGOPTP.ppStatusClip  = -1;
	LEGOPTP.ppTextClip    = 300;
	LEGOPTP.ppHideTime    = 10000; // 10 sec
	LEGOPTP.ppBorderColor = QColor (0x52, 0x97, 0xF9);

	// Bouncing of the dock (Mac OS X)
	LEGOPTP.bounceDock = Options::BounceForever;

	// lockdown
	LEGOPTP.lockdown.roster   = false;
	LEGOPTP.lockdown.services = false;

	LEGOPTP.useTransportIconsForContacts = false;

	// iconsets
	LEGOPTP.systemIconset = "default";
	LEGOPTP.emoticons = QStringList("default");
	LEGOPTP.defaultRosterIconset = "default";

	LEGOPTP.serviceRosterIconset.clear();
	LEGOPTP.serviceRosterIconset["transport"]	= "crystal-service.jisp";
	LEGOPTP.serviceRosterIconset["aim"]	= "crystal-aim.jisp";
	LEGOPTP.serviceRosterIconset["gadugadu"]	= "crystal-gadugadu.jisp";
	LEGOPTP.serviceRosterIconset["icq"]	= "crystal-icq.jisp";
	LEGOPTP.serviceRosterIconset["msn"]	= "crystal-msn.jisp";
	LEGOPTP.serviceRosterIconset["yahoo"]	= "crystal-yahoo.jisp";
	LEGOPTP.serviceRosterIconset["sms"]	= "crystal-sms.jisp";

	LEGOPTP.customRosterIconset.clear();

	// roster sorting
	LEGOPTP.rosterContactSortStyle = Options::ContactSortStyle_Status;
	LEGOPTP.rosterGroupSortStyle   = Options::GroupSortStyle_Alpha;
	LEGOPTP.rosterAccountSortStyle = Options::AccountSortStyle_Alpha;

	// disco dialog
	LEGOPTP.discoItems = false;
	LEGOPTP.discoInfo  = true;

	// auto-auth
	LEGOPTP.autoAuth = false;

	// Notify authorization
	LEGOPTP.notifyAuth = false;

	// message events
	LEGOPTP.messageEvents = true;
	LEGOPTP.inactiveEvents = false;

	// event priority
	LEGOPTP.eventPriorityHeadline = 0;
	LEGOPTP.eventPriorityChat     = 1;
	LEGOPTP.eventPriorityMessage  = 1;
	LEGOPTP.eventPriorityAuth     = 2;
	LEGOPTP.eventPriorityFile     = 3;
	LEGOPTP.eventPriorityFile     = 2;

	// Last used path remembering
	LEGOPTP.lastPath = QDir::homeDirPath();
	LEGOPTP.lastSavePath = QDir::homeDirPath();

	// data transfer
	LEGOPTP.dtPort = 8010;
	LEGOPTP.dtExternal = "";

}
#endif

static ToolbarPrefs loadToolbarData( const QDomElement &e )
{
	QDomElement tb_prefs = e;
	ToolbarPrefs tb;

	readEntry(tb_prefs, "name",		&tb.name);
	readBoolEntry(tb_prefs, "on",		&tb.on);
	readBoolEntry(tb_prefs, "locked",	&tb.locked);
	// readBoolEntry(tb_prefs, "stretchable",	&tb.stretchable);
	xmlToStringList(tb_prefs, "keys",	&tb.keys);

	bool found3 = false;
	QDomElement tb_position = findSubTag(tb_prefs, "position", &found3);
	if (found3)
	{
		QString dockStr;
		Qt::Dock dock = Qt::DockTop;
		readEntry(tb_position, "dock", &dockStr);
		if (dockStr == "DockTop")
			dock = Qt::DockTop;
		else if (dockStr == "DockBottom")
			dock = Qt::DockBottom;
		else if (dockStr == "DockLeft")
			dock = Qt::DockLeft;
		else if (dockStr == "DockRight")
			dock = Qt::DockRight;
		else if (dockStr == "DockMinimized")
			dock = Qt::DockMinimized;
		else if (dockStr == "DockTornOff")
			dock = Qt::DockTornOff;
		else if (dockStr == "DockUnmanaged")
			dock = Qt::DockUnmanaged;

		tb.dock = dock;

		// readNumEntry(tb_position, "index",		&tb.index);
		readBoolEntry(tb_position, "nl",		&tb.nl);
		// readNumEntry(tb_position, "extraOffset",	&tb.extraOffset);
	}

	return tb;
}


bool OptionsMigration::fromFile(const QString &fname)
{
	QString confver;
	QDomDocument doc;
	QString progver;

	AtomicXmlFile f(fname);
	if (!f.loadDocument(&doc))
		return false;

	QDomElement base = doc.documentElement();
	if(base.tagName() != "psiconf")
		return FALSE;
	confver = base.attribute("version");
	if(confver != "1.0")
		return FALSE;

	readEntry(base, "progver", &progver);

	migrateRectEntry(base, "geom", "options.ui.contactlist.saved-window-geometry");
	migrateStringList(base, "recentGCList", "options.muc.recent-joins.jids");
	migrateStringList(base, "recentBrowseList", "options.ui.service-discovery.recent-jids");
	migrateStringEntry(base, "lastStatusString", "options.status.last-message");
	migrateBoolEntry(base, "useSound", "options.ui.notifications.sounds.enable");

	bool found;
	QDomElement accs = findSubTag(base, "accounts", &found);
	if(found) {
		for(QDomNode n = accs.firstChild(); !n.isNull(); n = n.nextSibling()) {
			QDomElement a = n.toElement();
			if(a.isNull())
				continue;

			if(a.tagName() == "account") {
				UserAccount ua;
				ua.fromXml(a);
				accMigration.append(ua);
			}
		}
	}

	// convert old proxy config into new
	for(UserAccountList::Iterator it = accMigration.begin(); it != accMigration.end(); ++it) {
		UserAccount &a = *it;
		if(a.proxy_type > 0) {
			ProxyItem p;
			p.name = QObject::tr("%1 Proxy").arg(a.name);
			p.type = "http";
			p.settings.host = a.proxy_host;
			p.settings.port = a.proxy_port;
			p.settings.useAuth = !a.proxy_user.isEmpty();
			p.settings.user = a.proxy_user;
			p.settings.pass = a.proxy_pass;
			proxyMigration.append(p);

			a.proxy_index = proxyMigration.count(); // 1 and up are proxies
		}
	}

	QDomElement prox = findSubTag(base, "proxies", &found);
	if(found) {
		QDomNodeList list = prox.elementsByTagName("proxy");
		for(int n = 0; n < list.count(); ++n) {
			QDomElement e = list.item(n).toElement();
			ProxyItem p;
			p.name = "";
			p.type = "";
			readEntry(e, "name", &p.name);
			readEntry(e, "type", &p.type);
			if(p.type == "0")
				p.type = "http";
			QDomElement pset = e.elementsByTagName("proxySettings").item(0).toElement();
			if(!pset.isNull())
				p.settings.fromXml(pset);
			proxyMigration.append(p);
		}
	}

	// assign storage IDs to proxies and update accounts
	for (int i=0; i < proxyMigration.size(); i++) {
		proxyMigration[i].id = "a"+QString::number(i);
	}
	for (int i=0; i < accMigration.size(); i++) {
		if (accMigration[i].proxy_index != 0) {
			accMigration[i].proxyID = proxyMigration[accMigration[i].proxy_index-1].id;
		}
	}
	
	
	
	PsiOptions::instance()->setOption("options.ui.contactlist.show.offline-contacts", true);
	PsiOptions::instance()->setOption("options.ui.contactlist.show.away-contacts", true);
	PsiOptions::instance()->setOption("options.ui.contactlist.show.hidden-contacts-group", true);
	PsiOptions::instance()->setOption("options.ui.contactlist.show.agent-contacts", true);
	PsiOptions::instance()->setOption("options.ui.contactlist.show.self-contact", true);
		
	for (int i=0; i < accMigration.size(); i++) {
		if (!accMigration[i].opt_enabled) continue;
		PsiOptions::instance()->setOption("options.ui.contactlist.show.offline-contacts", accMigration[i].tog_offline);
		PsiOptions::instance()->setOption("options.ui.contactlist.show.away-contacts", accMigration[i].tog_away);
		PsiOptions::instance()->setOption("options.ui.contactlist.show.hidden-contacts-group", accMigration[i].tog_hidden);
		PsiOptions::instance()->setOption("options.ui.contactlist.show.agent-contacts", accMigration[i].tog_agents);
		PsiOptions::instance()->setOption("options.ui.contactlist.show.self-contact", accMigration[i].tog_self);
		break;
	}

	
	QDomElement p = findSubTag(base, "preferences", &found);
	if(found) {
		bool found;

		QDomElement p_general = findSubTag(p, "general", &found);
		if(found) {
			bool found;

			QDomElement p_roster = findSubTag(p_general, "roster", &found);
			if(found) {
				migrateBoolEntry(p_roster, "useleft", "options.ui.contactlist.use-left-click");
				migrateBoolEntry(p_roster, "singleclick", "options.ui.contactlist.use-single-click");
				bool hideMenu;
				readBoolEntry(p_roster, "hideMenubar", &hideMenu);
				PsiOptions::instance()->setOption("options.ui.contactlist.show-menubar", !hideMenu);
				int defaultAction;
				readNumEntry(p_roster, "defaultAction", &defaultAction);
				PsiOptions::instance()->setOption("options.messages.default-outgoing-message-type", defaultAction == 0 ? "message" : "chat");
				migrateBoolEntry(p_roster, "useTransportIconsForContacts", "options.ui.contactlist.use-transport-icons");

				QDomElement sorting = findSubTag(p_roster, "sortStyle", &found);
				if(found) {
					QString name;

					migrateStringEntry(sorting, "contact", "options.ui.contactlist.contact-sort-style");
					migrateStringEntry(sorting, "group", "options.ui.contactlist.group-sort-style");
					migrateStringEntry(sorting, "account", "options.ui.contactlist.account-sort-style");
					
					/* FIXME
					readEntry(sorting, "contact", &name);
					if ( name == "alpha" )
						lateMigrationData.rosterContactSortStyle = Options::ContactSortStyle_Alpha;
					else
						lateMigrationData.rosterContactSortStyle = Options::ContactSortStyle_Status;

					readEntry(sorting, "group", &name);
					if ( name == "rank" )
						lateMigrationData.rosterGroupSortStyle = Options::GroupSortStyle_Rank;
					else
						lateMigrationData.rosterGroupSortStyle = Options::GroupSortStyle_Alpha;

					readEntry(sorting, "account", &name);
					if ( name == "rank" )
						lateMigrationData.rosterAccountSortStyle = Options::AccountSortStyle_Rank;
					else
						lateMigrationData.rosterAccountSortStyle = Options::AccountSortStyle_Alpha;
					*/
				}
			}

			QDomElement tag = findSubTag(p_general, "misc", &found);
			if(found) {
				int delafterint;
				readNumEntry(tag, "delChats", &delafterint);
				QString delafter;
				switch (delafterint) {
					case 0:
						delafter = "instant";
						break;
					case 1:
						delafter = "hour";
						break;
					case 2:
						delafter = "day";
						break;
					case 3:
						delafter = "never";
						break;
				}
				PsiOptions::instance()->setOption("options.ui.chat.delete-contents-after", delafter);
				migrateBoolEntry(tag, "alwaysOnTop", "options.ui.contactlist.always-on-top");
				migrateBoolEntry(tag, "keepSizes", "options.ui.remember-window-sizes");
				migrateBoolEntry(tag, "ignoreHeadline", "options.messages.ignore-headlines");
				migrateBoolEntry(tag, "ignoreNonRoster", "options.messages.ignore-non-roster-contacts");
				migrateBoolEntry(tag, "excludeGroupChatIgnore", "options.messages.exclude-muc-from-ignore");
				migrateBoolEntry(tag, "scrollTo", "options.ui.contactlist.ensure-contact-visible-on-event");
				migrateBoolEntry(tag, "useEmoticons", "options.ui.emoticons.use-emoticons");
				migrateBoolEntry(tag, "alertOpenChats", "options.ui.chat.alert-for-already-open-chats");
				migrateBoolEntry(tag, "raiseChatWindow", "options.ui.chat.raise-chat-windows-on-new-messages");
				migrateBoolEntry(tag, "showSubjects", "options.ui.message.show-subjects");
				migrateBoolEntry(tag, "showGroupCounts", "options.ui.contactlist.show-group-counts");
				migrateBoolEntry(tag, "showCounter", "options.ui.message.show-character-count");
				migrateBoolEntry(tag, "chatSays", "options.ui.chat.use-chat-says-style");
				migrateBoolEntry(tag, "jidComplete", "options.ui.message.use-jid-auto-completion");
				migrateBoolEntry(tag, "grabUrls", "options.ui.message.auto-grab-urls-from-clipboard");
				migrateBoolEntry(tag, "smallChats", "options.ui.chat.use-small-chats");
				migrateBoolEntry(tag, "brushedMetal", "options.ui.mac.use-brushed-metal-windows");
				migrateBoolEntry(tag, "chatLineEdit", "options.ui.chat.use-expanding-line-edit");
				migrateBoolEntry(tag, "useTabs", "options.ui.tabs.use-tabs");
				migrateBoolEntry(tag, "putTabsAtBottom", "options.ui.tabs.put-tabs-at-bottom");
				migrateBoolEntry(tag, "autoRosterSize", "options.ui.contactlist.automatically-resize-roster");
				migrateBoolEntry(tag, "autoRosterSizeGrowTop", "options.ui.contactlist.grow-roster-upwards");
				migrateBoolEntry(tag, "autoResolveNicksOnAdd", "options.contactlist.resolve-nicks-on-contact-add");
				migrateBoolEntry(tag, "messageEvents", "options.messages.send-composing-events");
				migrateBoolEntry(tag, "inactiveEvents", "options.messages.send-inactivity-events");
				migrateStringEntry(tag, "lastPath", "options.ui.last-used-open-path");
				migrateStringEntry(tag, "lastSavePath", "options.ui.last-used-save-path");
				migrateBoolEntry(tag, "autoCopy", "options.ui.automatically-copy-selected-text");
				migrateBoolEntry(tag, "useCaps", "options.service-discovery.enable-entity-capabilities");
				migrateBoolEntry(tag, "rc", "options.external-control.adhoc-remote-control.enable");
				
				// Migrating for soft return option
				bool found;
				findSubTag(tag, "chatSoftReturn", &found);
				if (found) {
					bool soft;
					readBoolEntry(tag, "chatSoftReturn", &soft);
					QVariantList vl;
					if (soft) 
						vl << qVariantFromValue(QKeySequence(Qt::Key_Enter)) << qVariantFromValue(QKeySequence(Qt::Key_Return));
					else 
						vl << qVariantFromValue(QKeySequence(Qt::Key_Enter+Qt::CTRL)) << qVariantFromValue(QKeySequence(Qt::CTRL+Qt::Key_Return));
					PsiOptions::instance()->setOption("options.shortcuts.chat.send",vl);
				}
			}

			tag = findSubTag(p_general, "dock", &found);
			if(found) {
				migrateBoolEntry(tag, "useDock", "options.ui.systemtray.enable");
				migrateBoolEntry(tag, "dockDCstyle", "options.ui.systemtray.use-double-click");
				migrateBoolEntry(tag, "dockHideMW", "options.contactlist.hide-on-start");
				migrateBoolEntry(tag, "dockToolMW", "options.contactlist.use-toolwindow");
			}

			/*tag = findSubTag(p_general, "security", &found);
			if(found) {
				readEntry(tag, "pgp", &prefs.pgp);
			}*/
		}


		QDomElement p_events = findSubTag(p, "events", &found);
		if(found) {
			bool found;

			int alertstyle;
			readNumEntry(p_events, "alertstyle", &alertstyle);
			QString ase[3] = {"no", "blink", "animate"};
			PsiOptions::instance()->setOption("options.ui.notifications.alert-style", ase[alertstyle]);
			migrateBoolEntry(p_events, "autoAuth", "options.subscriptions.automatically-allow-authorization");
			migrateBoolEntry(p_events, "notifyAuth", "options.ui.notifications.successful-subscription");

			QDomElement tag = findSubTag(p_events, "receive", &found);
			if(found) {
				migrateBoolEntry(tag, "popupMsgs", "options.ui.message.auto-popup");
				migrateBoolEntry(tag, "popupChats", "options.ui.chat.auto-popup");
				migrateBoolEntry(tag, "popupHeadlines", "options.ui.message.auto-popup-headlines");
				migrateBoolEntry(tag, "popupFiles", "options.ui.file-transfer.auto-popup");
				migrateBoolEntry(tag, "noAwayPopup", "options.ui.notifications.popup-dialogs.suppress-while-away");
				migrateBoolEntry(tag, "noUnlistedPopup", "options.ui.notifications.popup-dialogs.suppress-when-not-on-roster");
				migrateBoolEntry(tag, "raise", "options.ui.contactlist.raise-on-new-event");
				int force;
				readNumEntry(tag, "incomingAs", &force);
				QString fe[4] = {"no", "message", "chat", "current-open"};
				PsiOptions::instance()->setOption("options.messages.force-incoming-message-type", fe[force]);
			}

		}

		QDomElement p_pres = findSubTag(p, "presence", &found);
		if(found) {
			bool found;

			QDomElement tag = findSubTag(p_pres, "misc", &found);
			if(found) {
				migrateBoolEntry(tag, "askOnline", "options.status.ask-for-message-on-online");
				migrateBoolEntry(tag, "askOffline", "options.status.ask-for-message-on-offline");
				migrateBoolEntry(tag, "rosterAnim", "options.ui.contactlist.use-status-change-animation");
				migrateBoolEntry(tag, "autoVCardOnLogin", "options.vcard.query-own-vcard-on-login");
				migrateBoolEntry(tag, "xmlConsoleOnLogin", "options.xml-console.enable-at-login");
			}

			tag = findSubTag(p_pres, "autostatus", &found);
			if(found) {
				bool found;
				bool use;
				QDomElement e;
				e = findSubTag(tag, "away", &found);
				if(found) {
					if(e.hasAttribute("use")) {
						readBoolAttribute(e, "use", &use);
						PsiOptions::instance()->setOption("options.status.auto-away.use-away", use);
					}
				}
				e = findSubTag(tag, "xa", &found);
				if(found) {
					if(e.hasAttribute("use"))
						readBoolAttribute(e, "use", &use);
					PsiOptions::instance()->setOption("options.status.auto-away.use-not-availible", use);
				}
				e = findSubTag(tag, "offline", &found);
				if(found) {
					if(e.hasAttribute("use"))
						readBoolAttribute(e, "use", &use);
					PsiOptions::instance()->setOption("options.status.auto-away.use-offline", use);
				}

				migrateIntEntry(tag, "away", "options.status.auto-away.away-after");
				migrateIntEntry(tag, "xa", "options.status.auto-away.not-availible-after");
				migrateIntEntry(tag, "offline", "options.status.auto-away.offline-after");

				migrateStringEntry(tag, "message", "options.status.auto-away.message");
			}

			tag = findSubTag(p_pres, "statuspresets", &found);
			if(found) {
				lateMigrationData.sp.clear();
				for(QDomNode n = tag.firstChild(); !n.isNull(); n = n.nextSibling()) {
					StatusPreset preset(n.toElement());
					if (!preset.name().isEmpty()) 
						lateMigrationData.sp[preset.name()] = preset;
				}
			}
		}

		QDomElement p_lnf = findSubTag(p, "lookandfeel", &found);
		if(found) {
			bool found;

			migrateBoolEntry(p_lnf, "newHeadings", "options.ui.look.contactlist.use-slim-group-headings");
			migrateBoolEntry(p_lnf, "outline-headings", "options.ui.look.contactlist.use-outlined-group-headings");
			migrateIntEntry(p_lnf, "chat-opacity", "options.ui.chat.opacity");
			migrateIntEntry(p_lnf, "roster-opacity", "options.ui.contactlist.opacity");

			QDomElement tag = findSubTag(p_lnf, "colors", &found);
			if(found) {
				migrateColorEntry(tag, "online", "options.ui.look.colors.contactlist.status.online");
				migrateColorEntry(tag, "listback", "options.ui.look.colors.contactlist.background");
				migrateColorEntry(tag, "away", "options.ui.look.colors.contactlist.status.away");
				migrateColorEntry(tag, "dnd", "options.ui.look.colors.contactlist.status.do-not-disturb");
				migrateColorEntry(tag, "offline", "options.ui.look.colors.contactlist.status.offline");
				migrateColorEntry(tag, "status", "options.ui.look.colors.contactlist.status-messages");
				migrateColorEntry(tag, "groupfore", "options.ui.look.colors.contactlist.grouping.header-foreground");
				migrateColorEntry(tag, "groupback", "options.ui.look.colors.contactlist.grouping.header-background");
				migrateColorEntry(tag, "profilefore", "options.ui.look.colors.contactlist.profile.header-foreground");
				migrateColorEntry(tag, "profileback", "options.ui.look.colors.contactlist.profile.header-background");
				migrateColorEntry(tag, "animfront", "options.ui.look.contactlist.status-change-animation.color1");
				migrateColorEntry(tag, "animback", "options.ui.look.contactlist.status-change-animation.color2");
			}

			tag = findSubTag(p_lnf, "fonts", &found);
			if(found) {
				migrateStringEntry(tag, "roster", "options.ui.look.font.contactlist");
				migrateStringEntry(tag, "message", "options.ui.look.font.message");
				migrateStringEntry(tag, "chat", "options.ui.look.font.chat");
				migrateStringEntry(tag, "popup", "options.ui.look.font.passive-popup");
			}
		}

		QDomElement p_sound = findSubTag(p, "sound", &found);
		if(found) {
			bool found;

			QString oldplayer;
			readEntry(p_sound, "player", &oldplayer);
			// psi now auto detects "play" or "aplay"
			// force auto detection on for old default and simple case of aplay on
			// alsa enabled systems.
			if (oldplayer != soundDetectPlayer() && oldplayer != "play") {
				PsiOptions::instance()->setOption("options.ui.notifications.sounds.unix-sound-player", oldplayer);
			} else {
				PsiOptions::instance()->setOption("options.ui.notifications.sounds.unix-sound-player", "");
			}
			migrateBoolEntry(p_sound, "noawaysound", "options.ui.notifications.sounds.silent-while-away");
			bool noGCSound;
			readBoolEntry(p_sound, "noGCSound", &noGCSound);
			PsiOptions::instance()->setOption("options.ui.notifications.sounds.notify-every-muc-message", !noGCSound);

			QDomElement tag = findSubTag(p_sound, "onevent", &found);
			if(found) {
				migrateStringEntry(tag, "message", "options.ui.notifications.sounds.incoming-message");
				migrateStringEntry(tag, "chat1", "options.ui.notifications.sounds.new-chat");
				migrateStringEntry(tag, "chat2", "options.ui.notifications.sounds.chat-message");
				migrateStringEntry(tag, "system", "options.ui.notifications.sounds.system-message");
				migrateStringEntry(tag, "headline", "options.ui.notifications.sounds.incoming-headline");
				migrateStringEntry(tag, "online", "options.ui.notifications.sounds.contact-online");
				migrateStringEntry(tag, "offline", "options.ui.notifications.sounds.contact-offline");
				migrateStringEntry(tag, "send", "options.ui.notifications.sounds.outgoing-chat");
				migrateStringEntry(tag, "incoming_ft", "options.ui.notifications.sounds.incoming-file-transfer");
				migrateStringEntry(tag, "ft_complete", "options.ui.notifications.sounds.completed-file-transfer");
			}
		}

		QDomElement p_sizes = findSubTag(p, "sizes", &found);
		if(found) {
			migrateSizeEntry(p_sizes, "eventdlg", "options.ui.message.size");
			migrateSizeEntry(p_sizes, "chatdlg", "options.ui.chat.size");
			migrateSizeEntry(p_sizes, "tabdlg", "options.ui.tabs.size");
		}

		QDomElement p_toolbars = findSubTag(p, "toolbars", &found);
		if (found) {
			QStringList goodTags;
			goodTags << "toolbar";
			goodTags << "mainWin";

			bool mainWinCleared = false;
			bool oldStyle = true;

			for(QDomNode n = p_toolbars.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement e = n.toElement();
				if( e.isNull() )
					continue;

				QString tbGroup;
				bool isGood = false;
				QStringList::Iterator it = goodTags.begin();
				for ( ; it != goodTags.end(); ++it ) {
					if ( e.tagName().left( (*it).length() ) == *it ) {
						isGood = true;

						if ( e.tagName().left(7) == "toolbar" )
							tbGroup = "mainWin";
						else {
							tbGroup = *it;
							oldStyle = false;
						}

						break;
					}
				}

				if ( isGood ) {
					if ( tbGroup != "mainWin" || !mainWinCleared ) {
						lateMigrationData.toolbars[tbGroup].clear();
						if ( tbGroup == "mainWin" )
							mainWinCleared = true;
					}

					if ( oldStyle ) {
						ToolbarPrefs tb = loadToolbarData( e );
						lateMigrationData.toolbars[tbGroup].append(tb);
					}
					else {
						for(QDomNode nn = e.firstChild(); !nn.isNull(); nn = nn.nextSibling()) {
							QDomElement ee = nn.toElement();
							if( ee.isNull() )
								continue;

							if ( ee.tagName() == "toolbar" ) {
								ToolbarPrefs tb = loadToolbarData( ee );
								lateMigrationData.toolbars[tbGroup].append(tb);
							}
						}
					}
				}
			}

			// event notifier in these versions was not implemented as an action, so add it
			if ( progver == "0.9" || progver == "0.9-CVS" ) {
				// at first, we need to scan the options, to determine, whether event_notifier already available
				bool found = false;
				QList<ToolbarPrefs>::Iterator it = lateMigrationData.toolbars["mainWin"].begin();
				for ( ; it != lateMigrationData.toolbars["mainWin"].end(); ++it) {
					QStringList::Iterator it2 = (*it).keys.begin();
					for ( ; it2 != (*it).keys.end(); ++it2) {
						if ( *it2 == "event_notifier" ) {
							found = true;
							break;
						}
					}
				}

				if ( !found ) {
					ToolbarPrefs tb;
					tb.name = QObject::tr("Event notifier");
					tb.on = false;
					tb.locked = true;
					// tb.stretchable = true;
					tb.keys << "event_notifier";
					tb.dock  = Qt::DockBottom;
					// tb.index = 0;
					lateMigrationData.toolbars["mainWin"].append(tb);
				}
			}
		}

		//group chat
		QDomElement p_groupchat = findSubTag(p, "groupchat", &found);
		if (found) {
			migrateBoolEntry(p_groupchat, "nickcoloring", "options.ui.muc.use-nick-coloring");
			migrateBoolEntry(p_groupchat, "highlighting", "options.ui.muc.use-highlighting");
			migrateStringList(p_groupchat, "highlightwords", "options.ui.muc.highlight-words");
			migrateStringList(p_groupchat, "nickcolors", "options.ui.look.colors.muc.nick-colors");
		}

		// Bouncing dock icon (Mac OS X)
		QDomElement p_dock = findSubTag(p, "dock", &found);
		if(found) {
			PsiOptions::instance()->setOption("options.ui.notifications.bounce-dock", p_dock.attribute("bounce"));
			/* FIXME convert back to some modern enum?
			if (p_dock.attribute("bounce") == "once") {
				lateMigrationData.bounceDock = Options::BounceOnce;
			}
			else if (p_dock.attribute("bounce") == "forever") {
				lateMigrationData.bounceDock = Options::BounceForever;
			}
			else if (p_dock.attribute("bounce") == "never") {
				lateMigrationData.bounceDock = Options::NoBounce;
			}*/
		}

		QDomElement p_popup = findSubTag(p, "popups", &found);
		if(found) {
			migrateBoolEntry(p_popup, "on", "options.ui.notifications.passive-popups.enabled");
			migrateBoolEntry(p_popup, "online", "options.ui.notifications.passive-popups.status.online");
			migrateBoolEntry(p_popup, "offline", "options.ui.notifications.passive-popups.status.offline");
			migrateBoolEntry(p_popup, "statusChange", "options.ui.notifications.passive-popups.status.other-changes");
			migrateBoolEntry(p_popup, "message", "options.ui.notifications.passive-popups.incoming-message");
			migrateBoolEntry(p_popup, "chat", "options.ui.notifications.passive-popups.incoming-chat");
			migrateBoolEntry(p_popup, "headline", "options.ui.notifications.passive-popups.incoming-headline");
			migrateBoolEntry(p_popup, "file", "options.ui.notifications.passive-popups.incoming-file-transfer");
			migrateIntEntry(p_popup,  "jidClip", "options.ui.notifications.passive-popups.maximum-jid-length");
			migrateIntEntry(p_popup,  "statusClip", "options.ui.notifications.passive-popups.maximum-status-length");
			migrateIntEntry(p_popup,  "textClip", "options.ui.notifications.passive-popups.maximum-text-length");
			migrateIntEntry(p_popup,  "hideTime", "options.ui.notifications.passive-popups.duration");
			migrateColorEntry(p_popup, "borderColor", "options.ui.look.colors.passive-popup.border");
		}

		QDomElement p_lockdown = findSubTag(p, "lockdown", &found);
		if(found) {
			migrateBoolEntry(p_lockdown, "roster", "options.ui.contactlist.lockdown-roster");
			migrateBoolEntry(p_lockdown, "services", "options.ui.contactlist.disable-service-discovery");
		}

		QDomElement p_iconset = findSubTag(p, "iconset", &found);
		if(found) {
			migrateStringEntry(p_iconset, "system", "options.iconsets.system");

			QDomElement roster = findSubTag(p_iconset, "roster", &found);
			if (found) {
				migrateStringEntry(roster, "default", "options.iconsets.status");

				QDomElement service = findSubTag(roster, "service", &found);
				if (found) {
					lateMigrationData.serviceRosterIconset.clear();
					for (QDomNode n = service.firstChild(); !n.isNull(); n = n.nextSibling()) {
						QDomElement i = n.toElement();
						if ( i.isNull() )
							continue;

						lateMigrationData.serviceRosterIconset[i.attribute("service")] = i.attribute("iconset");
					}
				}

				QDomElement custom = findSubTag(roster, "custom", &found);
				if (found) {
					lateMigrationData.customRosterIconset.clear();
					for (QDomNode n = custom.firstChild(); !n.isNull(); n = n.nextSibling()) {
						QDomElement i = n.toElement();
						if ( i.isNull() )
							continue;

						lateMigrationData.customRosterIconset[i.attribute("regExp")] = i.attribute("iconset");
					}
				}
			}

			QDomElement emoticons = findSubTag(p_iconset, "emoticons", &found);
			if (found) {
				QStringList emoticons_list;
				for (QDomNode n = emoticons.firstChild(); !n.isNull(); n = n.nextSibling()) {
					QDomElement i = n.toElement();
					if ( i.isNull() )
						continue;

					if ( i.tagName() == "item" ) {
						QString is = i.text();
						emoticons_list << is;
					}
				}
				PsiOptions::instance()->setOption("options.iconsets.emoticons", emoticons_list);
			}
		}

		QDomElement p_tip = findSubTag(p, "tipOfTheDay", &found);
		if (found) {
			migrateIntEntry(p_tip, "num", "options.ui.tip.number");
			migrateBoolEntry(p_tip, "show", "options.ui.tip.show");
		}

		QDomElement p_disco = findSubTag(p, "disco", &found);
		if (found) {
			migrateBoolEntry(p_disco, "items", "options.ui.service-discovery.automatically-get-items");
			migrateBoolEntry(p_disco, "info", "options.ui.service-discovery.automatically-get-info");
		}

		QDomElement p_dt = findSubTag(p, "dt", &found);
		if (found) {
			migrateIntEntry(p_dt, "port", "options.p2p.bytestreams.listen-port");
			migrateStringEntry(p_dt, "external", "options.p2p.bytestreams.external-address");
		}

		QDomElement p_globalAccel = findSubTag(p, "globalAccel", &found);
		if (found) {
			for (QDomNode n = p_globalAccel.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement i = n.toElement();
				if ( i.isNull() )
					continue;

				if ( i.tagName() == "command" && i.hasAttribute("type") ) {
					QVariant k = qVariantFromValue(QKeySequence(i.text()));
					QString shortcut;
					if (i.attribute("type") == "processNextEvent")
						shortcut = "event";
					else
						shortcut = "toggle-visibility";
					PsiOptions::instance()->setOption(QString("options.shortcuts.global.") + shortcut, k);
				}
			}
		}

		QDomElement p_advWidget = findSubTag(p, "advancedWidget", &found);
		if (found) {
			QDomElement stick = findSubTag(p_advWidget, "sticky", &found);
			if (found) {
				bool enabled, toWindows;
				int  offs;

				readBoolAttribute(stick, "enabled", &enabled);
				readNumEntry(stick, "offset", &offs);
				readBoolEntry(stick, "stickToWindows", &toWindows);

				GAdvancedWidget::setStickEnabled( enabled );
				GAdvancedWidget::setStickAt( offs );
				GAdvancedWidget::setStickToWindows( toWindows );
			}
		}
	}

	return TRUE;
}

static int qVersionInt()
{
	static int out = -1;

	if(out == -1) {
		QString str = QString::fromLatin1(qVersion());
		QStringList parts = str.split('.', QString::KeepEmptyParts);
		Q_ASSERT(parts.count() == 3);
		out = 0;
		for(int n = 0; n < 3; ++n) {
			bool ok;
			int x = parts[n].toInt(&ok);
			Q_ASSERT(ok);
			Q_ASSERT(x > 0 && x <= 0xff);
			out <<= x;
		}
	}

	return out;
}

void OptionsMigration::lateMigration()
{
	foreach(QString opt, PsiOptions::instance()->allOptionNames()) {
		if (opt.startsWith("options.status.presets.") ||
		    opt.startsWith("options.iconsets.service-status.") ||
		    opt.startsWith("options.iconsets.custom-status."))
		{
			return;
		}
	}

	PsiOptions *o = PsiOptions::instance();
	// QMap<QString, QString> serviceRosterIconset;
	QMapIterator<QString, QString> iSRI(lateMigrationData.serviceRosterIconset);
	while (iSRI.hasNext()) {
		iSRI.next();
		QString base = o->mapPut("options.iconsets.service-status", iSRI.key());
		o->setOption(base + ".iconset", iSRI.value());
	}

	// QMap<QString, QString> customRosterIconset;
	int idx = 0;
	QMapIterator<QString, QString> iCRI(lateMigrationData.customRosterIconset);
	while (iCRI.hasNext()) {
		iCRI.next();
		QString base = "options.iconsets.custom-status" ".a" + QString::number(idx++);
		o->setOption(base + ".regexp", iCRI.key());
		o->setOption(base + ".iconset", iCRI.value());
	}

	// QMap<QString,StatusPreset> sp; // Status message presets.
	foreach(StatusPreset sp, lateMigrationData.sp) {
		sp.toOptions(o);
	}

	// QMap< QString, QList<ToolbarPrefs> > toolbars;
	QList<ToolbarPrefs> toolbars;
	if(qVersionInt() >= 0x040300) {
		toolbars = lateMigrationData.toolbars["mainWin"];
	} else {
		foreach(ToolbarPrefs tb, lateMigrationData.toolbars["mainWin"]) {
			toolbars.insert(0, tb);
		}
	}
	foreach(ToolbarPrefs tb, toolbars) {
		PsiToolBar::structToOptions(o, tb);
	}
}


QString pathToProfile(const QString &name)
{
	return ApplicationInfo::profilesDir() + "/" + name;
}

QString pathToProfileConfig(const QString &name)
{
	return pathToProfile(name) + "/config.xml";
}

QStringList getProfilesList()
{
	QStringList list;

	QDir d(ApplicationInfo::profilesDir());
	if(!d.exists())
		return list;

	QStringList entries = d.entryList();
	for(QStringList::Iterator it = entries.begin(); it != entries.end(); ++it) {
		if(*it == "." || *it == "..")
			continue;
		QFileInfo info(d, *it);
		if(!info.isDir())
			continue;

		list.append(*it);
	}

	list.sort();

	return list;
}

bool profileExists(const QString &_name)
{
	QString name = _name.lower();

	QStringList list = getProfilesList();
	for(QStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {
		if((*it).lower() == name)
			return TRUE;
	}
	return FALSE;
}

bool profileNew(const QString &name)
{
	if(name.isEmpty())
		return FALSE;

	// verify the string is sane
	for(int n = 0; n < (int)name.length(); ++n) {
		if(!name.at(n).isLetterOrNumber())
			return FALSE;
	}

	// make it
	QDir d(ApplicationInfo::profilesDir());
	if(!d.exists())
		return FALSE;
	QDir p(ApplicationInfo::profilesDir() + "/" + name);
	if(!p.exists()) {
	        if (!d.mkdir(name))
			return FALSE;
	}

	p.mkdir("history");
	p.mkdir("vcard");

	return TRUE;
}

bool profileRename(const QString &oldname, const QString &name)
{
	// verify the string is sane
	for(int n = 0; n < (int)name.length(); ++n) {
		if(!name.at(n).isLetterOrNumber())
			return FALSE;
	}

	// locate the folder
	QDir d(ApplicationInfo::profilesDir());
	if(!d.exists())
		return FALSE;
	if(!d.rename(oldname, name))
		return FALSE;

	return TRUE;
}

static bool folderRemove(const QDir &_d)
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

bool profileDelete(const QString &path)
{
	QDir d(path);
	if(!d.exists())
		return TRUE;

	return folderRemove(QDir(path));
}

QString activeProfile;

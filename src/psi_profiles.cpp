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

// FIXME: this should be a const function (as should other ::toXml functions)
QDomElement UserAccount::toXml(QDomDocument &doc, const QString &tagName)
{
	QDomElement a = doc.createElement(tagName);

	setBoolAttribute(a, "enabled", opt_enabled);
	setBoolAttribute(a, "auto", opt_auto);
	setBoolAttribute(a, "showOffline", tog_offline);
	setBoolAttribute(a, "showAway", tog_away);
	setBoolAttribute(a, "showHidden", tog_hidden);
	setBoolAttribute(a, "showAgents", tog_agents);
	setBoolAttribute(a, "showSelf", tog_self);
	setBoolAttribute(a, "keepAlive", opt_keepAlive);
	setBoolAttribute(a, "compress", opt_compress);
	setBoolAttribute(a, "require-mutual-auth", req_mutual_auth);
	setBoolAttribute(a, "legacy-ssl-probe", legacy_ssl_probe);
	setBoolAttribute(a, "automatic-resource", opt_automatic_resource);
	setBoolAttribute(a, "log", opt_log);
	setBoolAttribute(a, "reconn", opt_reconn);
	setBoolAttribute(a, "ignoreSSLWarnings", opt_ignoreSSLWarnings);
	//setBoolAttribute(a, "gpg", opt_gpg);

	//QString jid = user + '@' + vhost;
	a.appendChild(textTag(doc, "name", name));
	a.appendChild(textTag(doc, "jid", jid));
	
	QDomElement customauth = doc.createElement("custom-auth");
	setBoolAttribute(customauth, "use", customAuth);
	QDomElement ac = textTag(doc, "authid", authid);
	QDomElement re = textTag(doc, "realm", realm);
	customauth.appendChild(ac);
	customauth.appendChild(re);
	a.appendChild(customauth);

	if(opt_pass)
		a.appendChild(textTag(doc, "password", encodePassword(pass, jid) ));
	a.appendChild(textTag(doc, "useHost", opt_host));
	a.appendChild(textTag(doc, "security-level", security_level));
	a.appendChild(textTag(doc, "ssl", ssl));
	a.appendChild(textTag(doc, "host", host));
	a.appendChild(textTag(doc, "port", QString::number(port)));
	a.appendChild(textTag(doc, "resource", resource));
	a.appendChild(textTag(doc, "priority", QString::number(priority)));
	if (!pgpSecretKey.isNull()) {
		a.appendChild(textTag(doc, "pgpSecretKeyID", pgpSecretKey.keyId()));
	}
	a.appendChild(textTag(doc, "allow-plain", allow_plain));
	
	QDomElement r = doc.createElement("roster");
	a.appendChild(r);
	Roster::ConstIterator rit = roster.begin();
	for ( ; rit != roster.end(); ++rit) {
		const RosterItem &i = *rit;
		QDomElement tag = i.toXml(&doc);
		r.appendChild(tag);
	}

	// now we check for redundant entries
	QStringList groupList, removeList;
	groupList << "/\\/" + name + "\\/\\"; // account name is a very 'special' group

	// special groups that should also have their state remembered
	groupList << qApp->translate("ContactProfile", "General");
	groupList << qApp->translate("ContactProfile", "Agents/Transports");

	// first, add all groups' names to groupList
	rit = roster.begin();
	for ( ; rit != roster.end(); ++rit) {
		const RosterItem &i = *rit;
		groupList += i.groups();
	}

	// now, check if there's groupState name entry in groupList
	QMap<QString, GroupData>::Iterator git = groupState.begin();
	for ( ; git != groupState.end(); ++git) {
		bool found = false;

		QStringList::Iterator it = groupList.begin();
		for ( ; it != groupList.end(); ++it)
			if ( git.key() == *it ) {
				found = true;
				break;
			}

		if ( !found )
			removeList << git.key();
	}

	// remove redundant groups
	QStringList::Iterator it = removeList.begin();
	for ( ; it != removeList.end(); ++it)
		groupState.remove( *it );

	// and finally, save the data
	QDomElement gs = doc.createElement("groupState");
	a.appendChild(gs);
	git = groupState.begin();
	for ( ; git != groupState.end(); ++git) {
		//if ( !git.data().open || git.data().rank ) { // don't save unnecessary entries (the default is 'true' to open, and '0' to rank)
			QDomElement group = doc.createElement("group");
			group.setAttribute("name", git.key());
			group.setAttribute("open", git.data().open ? "true" : "false");
			group.setAttribute("rank", QString::number(git.data().rank));
			gs.appendChild(group);
		//}
	}

	a.appendChild(textTag(doc, "proxyindex", QString::number(proxy_index)));
	//a.appendChild(textTag(doc, "proxytype", QString::number(proxy_type)));
	//a.appendChild(textTag(doc, "proxyhost", proxy_host));
	//a.appendChild(textTag(doc, "proxyport", QString::number(proxy_port)));
	//a.appendChild(textTag(doc, "proxyuser", proxy_user));
	//a.appendChild(textTag(doc, "proxypass", encodePassword(proxy_pass, jid)));

	a.appendChild(keybind.toXml(doc, "pgpkeybindings"));
	a.appendChild(textTag(doc, "dtProxy", dtProxy.full()));

	return a;
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

UserProfile::UserProfile()
{
	reset();
}

void UserProfile::reset()
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

	// prefs
	prefs.useleft = FALSE;
	prefs.singleclick = FALSE;
 	prefs.hideMenubar = FALSE;
	prefs.defaultAction = 1;
	prefs.delChats = dcHour;
	prefs.browser = 0;
	prefs.alwaysOnTop = FALSE;
	prefs.keepSizes = TRUE;
	prefs.ignoreHeadline = FALSE;
	prefs.ignoreNonRoster = FALSE;
	prefs.excludeGroupChatsFromIgnore = TRUE;
	prefs.useDock = true;
	prefs.dockDCstyle = win ? TRUE: FALSE;
	prefs.dockHideMW = FALSE;
	prefs.dockToolMW = FALSE;
#ifdef Q_WS_MAC
	prefs.alertStyle = 0;
#else
	prefs.alertStyle = 2;
#endif
	prefs.popupMsgs = FALSE;
	prefs.popupChats = FALSE;
	prefs.popupHeadlines = FALSE;
	prefs.popupFiles = FALSE;
	prefs.noAwayPopup = FALSE;
	prefs.noUnlistedPopup = false;
	prefs.raise = FALSE;
	prefs.incomingAs = 0;
	prefs.askOnline = FALSE;
	prefs.askOffline = FALSE;
	prefs.rosterAnim = TRUE;
	prefs.autoVCardOnLogin = true;
	prefs.xmlConsoleOnLogin = false;
	prefs.asAway = 10;
	prefs.asXa = 30;
	prefs.asOffline = 0;
	prefs.use_asAway = TRUE;
	prefs.use_asXa = TRUE;
	prefs.use_asOffline = FALSE;
	prefs.asMessage = QObject::tr("Auto Status (idle)");
	prefs.scrollTo = TRUE;
	prefs.useEmoticons = false;
	prefs.alertOpenChats = true;
	prefs.raiseChatWindow = false;
	prefs.showSubjects = true;
	prefs.showCounter = false;
	prefs.chatSays = false;
	prefs.showGroupCounts = true;
	prefs.smallChats = false;
	prefs.brushedMetal = false;
	prefs.chatLineEdit = true;
	prefs.useTabs = false;
	prefs.usePerTabCloseButton = true;
	prefs.putTabsAtBottom = false;
	prefs.autoRosterSize = false;
	prefs.autoRosterSizeGrowTop = false;
	prefs.autoResolveNicksOnAdd = false;
	prefs.chatBgImage = "";
	prefs.rosterBgImage = "";
	prefs.autoCopy = false;
	prefs.useCaps = true;
	prefs.useRC = false;

	prefs.sp.clear();
	QString name;
	name = QObject::tr("Away from desk");
	prefs.sp[name] = StatusPreset(name, QObject::tr("I am away from my desk.  Leave a message."), STATUS_AWAY);;
	name = QObject::tr("Showering");
	prefs.sp[name] = StatusPreset(name, QObject::tr("I'm in the shower.  You'll have to wait for me to get out."), STATUS_AWAY);
	name = QObject::tr("Eating");
	prefs.sp[name] = StatusPreset(name, QObject::tr("Out eating.  Mmmm.. food."), STATUS_AWAY);
	name = QObject::tr("Sleep");
	prefs.sp[name] = StatusPreset(name, QObject::tr("Sleep is good.  Zzzzz"),STATUS_DND);
	name = QObject::tr("Work");
	prefs.sp[name] = StatusPreset(name, QObject::tr("Can't chat.  Gotta work."), STATUS_DND);
	name = QObject::tr("Air");
	prefs.sp[name] = StatusPreset(name, QObject::tr("Stepping out to get some fresh air."), STATUS_AWAY);
	name = QObject::tr("Movie");
	prefs.sp[name] = StatusPreset(name, QObject::tr("Out to a movie.  Is that OK with you?"), STATUS_AWAY);
	name = QObject::tr("Secret");
	prefs.sp[name] = StatusPreset(name, QObject::tr("I'm not available right now and that's all you need to know."), STATUS_XA);
	name = QObject::tr("Out for the night");
	prefs.sp[name] = StatusPreset(name, QObject::tr("Out for the night."), STATUS_AWAY);
	name = QObject::tr("Greece");
	prefs.sp[name] = StatusPreset(name, QObject::tr("I have gone to a far away place.  I will be back someday!"), STATUS_XA);
 	prefs.recentStatus.clear();

	prefs.color[cOnline]    = QColor("#000000");
	prefs.color[cListBack]  = QColor("#FFFFFF");
	prefs.color[cAway]      = QColor("#004BB4");
	prefs.color[cDND]       = QColor("#7E0000");
	prefs.color[cOffline]   = QColor("#646464");
	prefs.color[cStatus]    = QColor("#808080");
	prefs.color[cProfileFore] = QColor("#FFFFFF");
	prefs.color[cProfileBack] = QColor("#969696");
	prefs.color[cGroupFore] = QColor("#5A5A5A");
	prefs.color[cGroupBack] = QColor("#F0F0F0");
	prefs.color[cAnimFront] = QColor("#000000");
	prefs.color[cAnimBack] 	= QColor("#969696");

	prefs.font[fRoster] = QApplication::font().toString();
	prefs.font[fMessage] = QApplication::font().toString();
	prefs.font[fChat] = QApplication::font().toString();
	{
		QFont font = QApplication::font();
		font.setPointSize( font.pointSize() - 2 );
		prefs.font[fPopup] = font.toString();
	}

	// calculate the small font size
	const int minimumFontSize = 7;
	prefs.smallFontSize = qApp->font().pointSize();
	prefs.smallFontSize -= 2;
	if ( prefs.smallFontSize < minimumFontSize )
		prefs.smallFontSize = minimumFontSize;
	FancyLabel::setSmallFontSize( prefs.smallFontSize );

	prefs.clNewHeadings = false;
	prefs.outlineHeadings = false;

	prefs.player = "play";
	prefs.noAwaySound = FALSE;

	prefs.onevent[eMessage]    = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	prefs.onevent[eChat1]      = ApplicationInfo::resourcesDir() + "/sound/chat1.wav";
	prefs.onevent[eChat2]      = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	prefs.onevent[eSystem]     = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	prefs.onevent[eHeadline]   = ApplicationInfo::resourcesDir() + "/sound/chat2.wav";
	prefs.onevent[eOnline]     = ApplicationInfo::resourcesDir() + "/sound/online.wav";
	prefs.onevent[eOffline]    = ApplicationInfo::resourcesDir() + "/sound/offline.wav";
	prefs.onevent[eSend]       = ApplicationInfo::resourcesDir() + "/sound/send.wav";
	prefs.onevent[eIncomingFT] = ApplicationInfo::resourcesDir() + "/sound/ft_incoming.wav";
	prefs.onevent[eFTComplete] = ApplicationInfo::resourcesDir() + "/sound/ft_complete.wav";

	// Added by Kiko 020621: sets up the default certificate store
	prefs.trustCertStoreDir = ApplicationInfo::resourcesDir() + "/certs";

	prefs.sizeEventDlg = EventDlg::defaultSize();
	prefs.sizeChatDlg = ChatDlg::defaultSize();
	prefs.sizeTabDlg = ChatDlg::defaultSize(); //TODO: no!

	prefs.jidComplete = true;
	prefs.grabUrls = false;
	prefs.noGCSound = true;

	// toolbars
	prefs.toolbars.clear();

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
			prefs.toolbars["mainWin"].append( tb[i] );
	}

	// groupchat
	prefs.gcHighlighting = true;
	prefs.gcHighlights.clear();

	prefs.gcNickColoring = true;
	prefs.gcNickColors.clear();

	prefs.gcNickColors << "Blue";
	prefs.gcNickColors << "Green";
	prefs.gcNickColors << "Orange";
	prefs.gcNickColors << "Purple";
	prefs.gcNickColors << "Red";

	// popups
	prefs.ppIsOn        = false;
	prefs.ppOnline      = true;
	prefs.ppOffline     = true;
	prefs.ppStatus      = false;
	prefs.ppMessage     = true;
	prefs.ppChat        = true;
	prefs.ppHeadline    = true;
	prefs.ppFile        = true;
	prefs.ppJidClip     = 25;
	prefs.ppStatusClip  = -1;
	prefs.ppTextClip    = 300;
	prefs.ppHideTime    = 10000; // 10 sec
	prefs.ppBorderColor = QColor (0x52, 0x97, 0xF9);

	// Bouncing of the dock (Mac OS X)
	prefs.bounceDock = Options::BounceForever;

	// lockdown
	prefs.lockdown.roster   = false;
	prefs.lockdown.services = false;

	prefs.useTransportIconsForContacts = false;

	// iconsets
	prefs.systemIconset = "default";
	prefs.emoticons = QStringList("default");
	prefs.defaultRosterIconset = "default";

	prefs.serviceRosterIconset.clear();
	prefs.serviceRosterIconset["transport"]	= "crystal-service.jisp";
	prefs.serviceRosterIconset["aim"]	= "crystal-aim.jisp";
	prefs.serviceRosterIconset["gadugadu"]	= "crystal-gadugadu.jisp";
	prefs.serviceRosterIconset["icq"]	= "crystal-icq.jisp";
	prefs.serviceRosterIconset["msn"]	= "crystal-msn.jisp";
	prefs.serviceRosterIconset["yahoo"]	= "crystal-yahoo.jisp";
	prefs.serviceRosterIconset["sms"]	= "crystal-sms.jisp";

	prefs.customRosterIconset.clear();

	// roster sorting
	prefs.rosterContactSortStyle = Options::ContactSortStyle_Status;
	prefs.rosterGroupSortStyle   = Options::GroupSortStyle_Alpha;
	prefs.rosterAccountSortStyle = Options::AccountSortStyle_Alpha;

	// disco dialog
	prefs.discoItems = false;
	prefs.discoInfo  = true;

	// auto-auth
	prefs.autoAuth = false;

	// Notify authorization
	prefs.notifyAuth = false;

	// message events
	prefs.messageEvents = true;
	prefs.inactiveEvents = false;

	// event priority
	prefs.eventPriorityHeadline = 0;
	prefs.eventPriorityChat     = 1;
	prefs.eventPriorityMessage  = 1;
	prefs.eventPriorityAuth     = 2;
	prefs.eventPriorityFile     = 3;
	prefs.eventPriorityFile     = 2;

	// Last used path remembering
	prefs.lastPath = QDir::homeDirPath();
	prefs.lastSavePath = QDir::homeDirPath();

	// data transfer
	prefs.dtPort = 8010;
	prefs.dtExternal = "";

	// advanced widget
	GAdvancedWidget::setStickEnabled( false ); //until this is bugless
	GAdvancedWidget::setStickToWindows( false ); //again
	GAdvancedWidget::setStickAt( 5 );
}

static Options::ToolbarPrefs loadToolbarData( const QDomElement &e )
{
	QDomElement tb_prefs = e;
	Options::ToolbarPrefs tb;

	readEntry(tb_prefs, "name",		&tb.name);
	readBoolEntry(tb_prefs, "on",		&tb.on);
	readBoolEntry(tb_prefs, "locked",	&tb.locked);
	readBoolEntry(tb_prefs, "stretchable",	&tb.stretchable);
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

		readNumEntry(tb_position, "index",		&tb.index);
		readBoolEntry(tb_position, "nl",		&tb.nl);
		readNumEntry(tb_position, "extraOffset",	&tb.extraOffset);
	}

	return tb;
}

bool UserProfile::toFile(const QString &fname)
{
	QDomDocument doc;

	QDomElement base = doc.createElement("psiconf");
	base.setAttribute("version", "1.0");
	doc.appendChild(base);

	base.appendChild(textTag(doc, "progver", ApplicationInfo::version()));
	base.appendChild(textTag(doc, "geom", mwgeom));
	base.appendChild(stringListToXml(doc, "recentGCList", recentGCList));
	base.appendChild(stringListToXml(doc, "recentBrowseList", recentBrowseList));
	base.appendChild(textTag(doc, "lastStatusString", lastStatusString));
	base.appendChild(textTag(doc, "useSound", useSound));

	QDomElement accs = doc.createElement("accounts");
	base.appendChild(accs);
	for(UserAccountList::Iterator ai = acc.begin(); ai != acc.end(); ++ai)
		accs.appendChild((*ai).toXml(doc, "account"));

	QDomElement prox = doc.createElement("proxies");
	base.appendChild(prox);
	for(ProxyItemList::Iterator pi = proxyList.begin(); pi != proxyList.end(); ++pi) {
		const ProxyItem &p = *pi;
		QDomElement i = doc.createElement("proxy");
		i.appendChild(textTag(doc, "name", p.name));
		i.appendChild(textTag(doc, "type", p.type));
		i.appendChild(p.settings.toXml(&doc));
		prox.appendChild(i);
	}

	QDomElement p = doc.createElement("preferences");
	base.appendChild(p);

	{
		QDomElement p_general = doc.createElement("general");
		p.appendChild(p_general);

		{
			QDomElement p_roster = doc.createElement("roster");
			p_general.appendChild(p_roster);

			p_roster.appendChild(textTag(doc, "useleft", prefs.useleft));
			p_roster.appendChild(textTag(doc, "rosterBgImage", prefs.rosterBgImage));
			p_roster.appendChild(textTag(doc, "singleclick", prefs.singleclick));
			p_roster.appendChild(textTag(doc, "hideMenubar", prefs.hideMenubar));
			p_roster.appendChild(textTag(doc, "defaultAction", prefs.defaultAction));
			p_roster.appendChild(textTag(doc, "useTransportIconsForContacts", prefs.useTransportIconsForContacts));

			{
				QDomElement sorting = doc.createElement("sortStyle");
				p_roster.appendChild(sorting);

				QString name = "status";
				if ( prefs.rosterContactSortStyle == Options::ContactSortStyle_Alpha )
					name = "alpha";
				sorting.appendChild(textTag(doc, "contact", name));

				name = "alpha";
				if ( prefs.rosterGroupSortStyle == Options::GroupSortStyle_Rank )
					name = "rank";
				sorting.appendChild(textTag(doc, "group", name));

				name = "alpha";
				if ( prefs.rosterAccountSortStyle == Options::AccountSortStyle_Rank )
					name = "rank";
				sorting.appendChild(textTag(doc, "account", name));
			}
		}

		{
			QDomElement p_misc = doc.createElement("misc");
			p_general.appendChild(p_misc);

			p_misc.appendChild(textTag(doc, "chatBgImage", prefs.chatBgImage));
			p_misc.appendChild(textTag(doc, "smallChats", prefs.smallChats));
			p_misc.appendChild(textTag(doc, "brushedMetal", prefs.brushedMetal));
			p_misc.appendChild(textTag(doc, "chatLineEdit", prefs.chatLineEdit));
			p_misc.appendChild(textTag(doc, "useTabs", prefs.useTabs));
			p_misc.appendChild(textTag(doc, "usePerTabCloseButton", prefs.usePerTabCloseButton));
			p_misc.appendChild(textTag(doc, "putTabsAtBottom", prefs.putTabsAtBottom));
			p_misc.appendChild(textTag(doc, "autoRosterSize", prefs.autoRosterSize));
			p_misc.appendChild(textTag(doc, "autoRosterSizeGrowTop", prefs.autoRosterSizeGrowTop));
			p_misc.appendChild(textTag(doc, "autoResolveNicksOnAdd", prefs.autoResolveNicksOnAdd));
			p_misc.appendChild(textTag(doc, "delChats", prefs.delChats));
			p_misc.appendChild(textTag(doc, "browser", prefs.browser));
			p_misc.appendChild(textTag(doc, "alwaysOnTop", prefs.alwaysOnTop));
			p_misc.appendChild(textTag(doc, "keepSizes", prefs.keepSizes));
			p_misc.appendChild(textTag(doc, "ignoreHeadline", prefs.ignoreHeadline));
			p_misc.appendChild(textTag(doc, "ignoreNonRoster", prefs.ignoreNonRoster));
			p_misc.appendChild(textTag(doc, "excludeGroupChatIgnore", prefs.excludeGroupChatsFromIgnore));
			p_misc.appendChild(textTag(doc, "scrollTo", prefs.scrollTo));
			p_misc.appendChild(textTag(doc, "useEmoticons", prefs.useEmoticons));
			p_misc.appendChild(textTag(doc, "alertOpenChats", prefs.alertOpenChats));
			p_misc.appendChild(textTag(doc, "raiseChatWindow", prefs.raiseChatWindow));
			p_misc.appendChild(textTag(doc, "showSubjects", prefs.showSubjects));
			p_misc.appendChild(textTag(doc, "showCounter", prefs.showCounter));
			p_misc.appendChild(textTag(doc, "chatSays", prefs.chatSays));
			p_misc.appendChild(textTag(doc, "showGroupCounts", prefs.showGroupCounts));
			p_misc.appendChild(textTag(doc, "jidComplete", prefs.jidComplete));
			p_misc.appendChild(textTag(doc, "grabUrls", prefs.grabUrls));
			p_misc.appendChild(textTag(doc, "messageEvents", prefs.messageEvents));
			p_misc.appendChild(textTag(doc, "inactiveEvents", prefs.inactiveEvents));
			p_misc.appendChild(textTag(doc, "lastPath", prefs.lastPath));
			p_misc.appendChild(textTag(doc, "lastSavePath", prefs.lastSavePath));
			p_misc.appendChild(textTag(doc, "autoCopy", prefs.autoCopy));
			p_misc.appendChild(textTag(doc, "useCaps", prefs.useCaps));
			p_misc.appendChild(textTag(doc, "rc", prefs.useRC));
		}
		{
			QDomElement p_dock = doc.createElement("dock");
			p_general.appendChild(p_dock);

			p_dock.appendChild(textTag(doc, "useDock", prefs.useDock));
			p_dock.appendChild(textTag(doc, "dockDCstyle", prefs.dockDCstyle));
			p_dock.appendChild(textTag(doc, "dockHideMW", prefs.dockHideMW));
			p_dock.appendChild(textTag(doc, "dockToolMW", prefs.dockToolMW));
		}
		/*{
			QDomElement p_sec = doc.createElement("security");
			p_general.appendChild(p_sec);

			p_sec.appendChild(textTag(doc, "pgp", prefs.pgp));
		}*/
	}

	// Added by Kiko 020621: stores SSL cert chain validation configuration
	{
		QDomElement p_ssl = doc.createElement("ssl");
		p.appendChild(p_ssl);

		p_ssl.appendChild(textTag(doc, "trustedcertstoredir", prefs.trustCertStoreDir));
	}

	{
		QDomElement p_events = doc.createElement("events");
		p.appendChild(p_events);

		p_events.appendChild(textTag(doc, "alertstyle", prefs.alertStyle));
		p_events.appendChild(textTag(doc, "autoAuth", prefs.autoAuth));
		p_events.appendChild(textTag(doc, "notifyAuth", prefs.notifyAuth));


		{
			QDomElement p_receive = doc.createElement("receive");
			p_events.appendChild(p_receive);

			p_receive.appendChild(textTag(doc, "popupMsgs", prefs.popupMsgs));
			p_receive.appendChild(textTag(doc, "popupChats", prefs.popupChats));
			p_receive.appendChild(textTag(doc, "popupHeadlines", prefs.popupHeadlines));
			p_receive.appendChild(textTag(doc, "popupFiles", prefs.popupFiles));
			p_receive.appendChild(textTag(doc, "noAwayPopup", prefs.noAwayPopup));
			p_receive.appendChild(textTag(doc, "noUnlistedPopup", prefs.noUnlistedPopup));
			p_receive.appendChild(textTag(doc, "raise", prefs.raise));
			p_receive.appendChild(textTag(doc, "incomingAs", prefs.incomingAs));
		}

		{
			QDomElement p_priority = doc.createElement("priority");
			p_events.appendChild(p_priority);

			p_priority.appendChild(textTag(doc, "message",  prefs.eventPriorityMessage));
			p_priority.appendChild(textTag(doc, "chat",     prefs.eventPriorityChat));
			p_priority.appendChild(textTag(doc, "headline", prefs.eventPriorityHeadline));
			p_priority.appendChild(textTag(doc, "auth",     prefs.eventPriorityAuth));
			p_priority.appendChild(textTag(doc, "file",     prefs.eventPriorityFile));
			p_priority.appendChild(textTag(doc, "rosterx",     prefs.eventPriorityRosterExchange));
		}
	}

	{
		QDomElement p_pres = doc.createElement("presence");
		p.appendChild(p_pres);

		{
			QDomElement tag = doc.createElement("misc");
			p_pres.appendChild(tag);

			tag.appendChild(textTag(doc, "askOnline", prefs.askOnline));
			tag.appendChild(textTag(doc, "askOffline", prefs.askOffline));
			tag.appendChild(textTag(doc, "rosterAnim", prefs.rosterAnim));
			tag.appendChild(textTag(doc, "autoVCardOnLogin", prefs.autoVCardOnLogin));
			tag.appendChild(textTag(doc, "xmlConsoleOnLogin", prefs.xmlConsoleOnLogin));
		}
		{
			QDomElement tag = doc.createElement("autostatus");
			p_pres.appendChild(tag);

			QDomElement e;

			e = textTag(doc, "away", prefs.asAway);
			setBoolAttribute(e, "use", prefs.use_asAway);
			tag.appendChild(e);

			e = textTag(doc, "xa", prefs.asXa);
			setBoolAttribute(e, "use", prefs.use_asXa);
			tag.appendChild(e);

			e = textTag(doc, "offline", prefs.asOffline);
			setBoolAttribute(e, "use", prefs.use_asOffline);
			tag.appendChild(e);

			tag.appendChild(textTag(doc, "message", prefs.asMessage));
		}
		{
			// Status presets
			QDomElement p_statuspresets = doc.createElement("statuspresets");
			p_pres.appendChild(p_statuspresets);
			foreach (StatusPreset p, prefs.sp) {
				p_statuspresets.appendChild(p.toXml(doc));
			}
		}
		{
			p_pres.appendChild(stringListToXml(doc, "recentstatus",prefs.recentStatus));
		}
	}

	{
		QDomElement p_lnf = doc.createElement("lookandfeel");
		p.appendChild(p_lnf);

		p_lnf.appendChild(textTag(doc, "newHeadings", prefs.clNewHeadings));
		p_lnf.appendChild(textTag(doc, "outline-headings", prefs.outlineHeadings));

		{
			QDomElement tag = doc.createElement("colors");
			p_lnf.appendChild(tag);

			tag.appendChild(textTag(doc, "online", prefs.color[cOnline].name() ));
			tag.appendChild(textTag(doc, "listback", prefs.color[cListBack].name() ));
			tag.appendChild(textTag(doc, "away", prefs.color[cAway].name() ));
			tag.appendChild(textTag(doc, "dnd", prefs.color[cDND].name() ));
			tag.appendChild(textTag(doc, "offline", prefs.color[cOffline].name() ));
			tag.appendChild(textTag(doc, "status", prefs.color[cStatus].name() ));
			tag.appendChild(textTag(doc, "profilefore", prefs.color[cProfileFore].name() ));
			tag.appendChild(textTag(doc, "profileback", prefs.color[cProfileBack].name() ));
			tag.appendChild(textTag(doc, "groupfore", prefs.color[cGroupFore].name() ));
			tag.appendChild(textTag(doc, "groupback", prefs.color[cGroupBack].name() ));
			tag.appendChild(textTag(doc, "animfront", prefs.color[cAnimFront].name() ));
			tag.appendChild(textTag(doc, "animback", prefs.color[cAnimBack].name() ));
		}

		{
			QDomElement tag = doc.createElement("fonts");
			p_lnf.appendChild(tag);

			tag.appendChild(textTag(doc, "roster", prefs.font[fRoster] ));
			tag.appendChild(textTag(doc, "message", prefs.font[fMessage] ));
			tag.appendChild(textTag(doc, "chat", prefs.font[fChat] ));
			tag.appendChild(textTag(doc, "popup", prefs.font[fPopup] ));
		}
	}

	{
		QDomElement p_sound = doc.createElement("sound");
		p.appendChild(p_sound);

		p_sound.appendChild(textTag(doc, "player", prefs.player));
		p_sound.appendChild(textTag(doc, "noawaysound", prefs.noAwaySound));
		p_sound.appendChild(textTag(doc, "noGCSound", prefs.noGCSound));

		{
			QDomElement p_onevent = doc.createElement("onevent");
			p_sound.appendChild(p_onevent);

			p_onevent.appendChild(textTag(doc, "message", prefs.onevent[eMessage]));
			p_onevent.appendChild(textTag(doc, "chat1", prefs.onevent[eChat1]));
			p_onevent.appendChild(textTag(doc, "chat2", prefs.onevent[eChat2]));
			p_onevent.appendChild(textTag(doc, "system", prefs.onevent[eSystem]));
			p_onevent.appendChild(textTag(doc, "headline", prefs.onevent[eHeadline]));
			p_onevent.appendChild(textTag(doc, "online", prefs.onevent[eOnline]));
			p_onevent.appendChild(textTag(doc, "offline", prefs.onevent[eOffline]));
			p_onevent.appendChild(textTag(doc, "send", prefs.onevent[eSend]));
			p_onevent.appendChild(textTag(doc, "incoming_ft", prefs.onevent[eIncomingFT]));
			p_onevent.appendChild(textTag(doc, "ft_complete", prefs.onevent[eFTComplete]));
		}
	}

	{
		QDomElement p_sizes = doc.createElement("sizes");
		p.appendChild(p_sizes);

		p_sizes.appendChild(textTag(doc, "eventdlg", prefs.sizeEventDlg));
		p_sizes.appendChild(textTag(doc, "chatdlg", prefs.sizeChatDlg));
		p_sizes.appendChild(textTag(doc, "tabdlg", prefs.sizeTabDlg));
	}

	{
		QDomElement p_toolbars = doc.createElement("toolbars");
		p.appendChild(p_toolbars);

		QMap< QString, QList<Options::ToolbarPrefs> >::Iterator itBars = prefs.toolbars.begin();
		for ( ; itBars != prefs.toolbars.end(); ++itBars ) {
			QList<Options::ToolbarPrefs> toolbars = itBars.data();
			QDomElement p_bars = doc.createElement( itBars.key() );
			p_toolbars.appendChild( p_bars );

			QList<Options::ToolbarPrefs>::Iterator it = toolbars.begin();
			for ( ; it != toolbars.end(); ++it) {
				Options::ToolbarPrefs tb = *it;
				if ( tb.keys.size() ||
				     !tb.name.isEmpty() )
				{
					QDomElement tb_prefs = doc.createElement("toolbar");
					p_bars.appendChild(tb_prefs);

					tb_prefs.appendChild(textTag(doc, "name",		tb.name));
					tb_prefs.appendChild(textTag(doc, "on",			tb.on));
					tb_prefs.appendChild(textTag(doc, "locked",		tb.locked));
					tb_prefs.appendChild(textTag(doc, "stretchable",	tb.stretchable));
					tb_prefs.appendChild(stringListToXml(doc, "keys",	tb.keys));

					QDomElement tb_position = doc.createElement("position");
					tb_prefs.appendChild(tb_position);

					QString dockStr;
					Qt::Dock dock = tb.dock;
					if (dock == Qt::DockTop)
						dockStr = "DockTop";
					else if (dock == Qt::DockBottom)
						dockStr = "DockBottom";
					else if (dock == Qt::DockLeft)
						dockStr = "DockLeft";
					else if (dock == Qt::DockRight)
						dockStr = "DockRight";
					else if (dock == Qt::DockMinimized)
						dockStr = "DockMinimized";
					else if (dock == Qt::DockTornOff)
						dockStr = "DockTornOff";
					else if (dock == Qt::DockUnmanaged)
						dockStr = "DockUnmanaged";

					tb_position.appendChild(textTag(doc, "dock",		dockStr));
					tb_position.appendChild(textTag(doc, "index",		tb.index));
					tb_position.appendChild(textTag(doc, "nl",		tb.nl));
					tb_position.appendChild(textTag(doc, "extraOffset",	tb.extraOffset));
				}
			}
		}
	}

	{
		QDomElement pp = doc.createElement("popups");
		p.appendChild(pp);
		pp.appendChild(textTag(doc, "on", prefs.ppIsOn));
		pp.appendChild(textTag(doc, "online", prefs.ppOnline));
		pp.appendChild(textTag(doc, "offline", prefs.ppOffline));
		pp.appendChild(textTag(doc, "statusChange", prefs.ppStatus));
		pp.appendChild(textTag(doc, "message", prefs.ppMessage));
		pp.appendChild(textTag(doc, "chat", prefs.ppChat));
		pp.appendChild(textTag(doc, "headline", prefs.ppHeadline));
		pp.appendChild(textTag(doc, "file", prefs.ppFile));
		pp.appendChild(textTag(doc, "jidClip", prefs.ppJidClip));
		pp.appendChild(textTag(doc, "statusClip", prefs.ppStatusClip));
		pp.appendChild(textTag(doc, "textClip", prefs.ppTextClip));
		pp.appendChild(textTag(doc, "hideTime", prefs.ppHideTime));
		pp.appendChild(textTag(doc, "borderColor", prefs.ppBorderColor.name()));
	}
	
	{
		// Bouncing dock (on Mac OS X)
		QDomElement dock = doc.createElement("dock");
		p.appendChild(dock);
		switch (prefs.bounceDock) {
			case Options::NoBounce :
				dock.setAttribute("bounce", "never");
				break;
			case Options::BounceOnce :
				dock.setAttribute("bounce", "once");
				break;
			case Options::BounceForever :
				dock.setAttribute("bounce", "forever");
				break;
			default:
				break;
		}
	}

	{
		//group chat
		QDomElement gc = doc.createElement("groupchat");
		p.appendChild(gc);
		gc.appendChild(stringListToXml(doc, "highlightwords", prefs.gcHighlights));
		gc.appendChild(stringListToXml(doc, "nickcolors", prefs.gcNickColors));
		gc.appendChild(textTag(doc, "nickcoloring", prefs.gcNickColoring));
		gc.appendChild(textTag(doc, "highlighting", prefs.gcHighlighting));
	}

	{
		QDomElement p_lockdown = doc.createElement("lockdown");
		p.appendChild(p_lockdown);
		p_lockdown.appendChild(textTag(doc, "roster", prefs.lockdown.roster));
		p_lockdown.appendChild(textTag(doc, "services", prefs.lockdown.services));
	}

	{
		QDomElement is = doc.createElement("iconset");
		p.appendChild(is);

		// system
		is.appendChild(textTag(doc, "system", prefs.systemIconset));

		// roster - default
		QDomElement is_roster = doc.createElement("roster");
		is.appendChild(is_roster);
		is_roster.appendChild(textTag(doc, "default", prefs.defaultRosterIconset));

		{
			// roster - service
			QDomElement roster_service = doc.createElement("service");
			is_roster.appendChild(roster_service);

			QMap<QString, QString>::iterator it = prefs.serviceRosterIconset.begin();
			for ( ; it != prefs.serviceRosterIconset.end(); ++it) {
				QDomElement item = doc.createElement("item");
				roster_service.appendChild(item);

				item.setAttribute("service", it.key());
				item.setAttribute("iconset", it.data());
			}
		}

		{
			// roster - custom
			QDomElement roster_custom = doc.createElement("custom");
			is_roster.appendChild(roster_custom);

			QMap<QString, QString>::iterator it = prefs.customRosterIconset.begin();
			for ( ; it != prefs.customRosterIconset.end(); ++it) {
				QDomElement item = doc.createElement("item");
				roster_custom.appendChild(item);

				item.setAttribute("regExp", it.key());
				item.setAttribute("iconset", it.data());
			}
		}

		{
			// emoticons
			QDomElement is_emoticons = doc.createElement("emoticons");
			is.appendChild(is_emoticons);

			QStringList::Iterator it = prefs.emoticons.begin();
			for ( ; it != prefs.emoticons.end(); ++it)
				is_emoticons.appendChild(textTag(doc, "item", *it));
		}
	}

	{
		// disco dialog
		QDomElement p_disco = doc.createElement("disco");
		p.appendChild(p_disco);

		p_disco.appendChild( textTag(doc, "items", prefs.discoItems ) );
		p_disco.appendChild( textTag(doc, "info",  prefs.discoInfo ) );
	}

	{
		// data transfer
		QDomElement p_dt = doc.createElement("dt");
		p.appendChild(p_dt);
		p_dt.appendChild( textTag(doc, "port", prefs.dtPort ) );
		p_dt.appendChild( textTag(doc, "external", prefs.dtExternal ) );
	}

	{
		// advanced widget
		QDomElement p_advWidget = doc.createElement("advancedWidget");
		p.appendChild(p_advWidget);

		QDomElement stick = doc.createElement("sticky");
		p_advWidget.appendChild(stick);

		setBoolAttribute(stick, "enabled", GAdvancedWidget::stickEnabled());
		stick.appendChild( textTag(doc, "offset", GAdvancedWidget::stickAt()) );
		stick.appendChild( textTag(doc, "stickToWindows", GAdvancedWidget::stickToWindows()) );
	}

	QFile f(fname);
	if(!f.open(QIODevice::WriteOnly))
		return FALSE;
	QTextStream t;
	t.setDevice(&f);
	t.setEncoding(QTextStream::UnicodeUTF8);
	t << doc.toString();
	f.close();

	return TRUE;
}

bool UserProfile::fromFile(const QString &fname)
{
	QString confver;
	QDomDocument doc;
	QString progver;

	QFile f(fname);
	if(!f.open(QIODevice::ReadOnly))
		return FALSE;
	if(!doc.setContent(&f))
		return FALSE;
	f.close();

	QDomElement base = doc.documentElement();
	if(base.tagName() != "psiconf")
		return FALSE;
	confver = base.attribute("version");
	if(confver != "1.0")
		return FALSE;

	readEntry(base, "progver", &progver);

	readRectEntry(base, "geom", &mwgeom);
	xmlToStringList(base, "recentGCList", &recentGCList);
	xmlToStringList(base, "recentBrowseList", &recentBrowseList);
	readEntry(base, "lastStatusString", &lastStatusString);
	readBoolEntry(base, "useSound", &useSound);

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
				acc.append(ua);
			}
		}
	}

	// convert old proxy config into new
	for(UserAccountList::Iterator it = acc.begin(); it != acc.end(); ++it) {
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
			proxyList.append(p);

			a.proxy_index = proxyList.count(); // 1 and up are proxies
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
			proxyList.append(p);
		}
	}

	QDomElement p = findSubTag(base, "preferences", &found);
	if(found) {
		bool found;

		QDomElement p_general = findSubTag(p, "general", &found);
		if(found) {
			bool found;

			QDomElement p_roster = findSubTag(p_general, "roster", &found);
			if(found) {
				readEntry(p_roster, "rosterBgImage", &prefs.rosterBgImage);
				readBoolEntry(p_roster, "useleft", &prefs.useleft);
				readBoolEntry(p_roster, "singleclick", &prefs.singleclick);
				readBoolEntry(p_roster, "hideMenubar", &prefs.hideMenubar);
				readNumEntry(p_roster, "defaultAction", &prefs.defaultAction);
				readBoolEntry(p_roster, "useTransportIconsForContacts", &prefs.useTransportIconsForContacts);

				QDomElement sorting = findSubTag(p_roster, "sortStyle", &found);
				if(found) {
					QString name;

					readEntry(sorting, "contact", &name);
					if ( name == "alpha" )
						prefs.rosterContactSortStyle = Options::ContactSortStyle_Alpha;
					else
						prefs.rosterContactSortStyle = Options::ContactSortStyle_Status;

					readEntry(sorting, "group", &name);
					if ( name == "rank" )
						prefs.rosterGroupSortStyle = Options::GroupSortStyle_Rank;
					else
						prefs.rosterGroupSortStyle = Options::GroupSortStyle_Alpha;

					readEntry(sorting, "account", &name);
					if ( name == "rank" )
						prefs.rosterAccountSortStyle = Options::AccountSortStyle_Rank;
					else
						prefs.rosterAccountSortStyle = Options::AccountSortStyle_Alpha;
				}
			}

			QDomElement tag = findSubTag(p_general, "misc", &found);
			if(found) {
				readEntry(tag, "chatBgImage", &prefs.chatBgImage);
				readNumEntry(tag, "delChats", &prefs.delChats);
				readNumEntry(tag, "browser", &prefs.browser);
				readBoolEntry(tag, "alwaysOnTop", &prefs.alwaysOnTop);
				readBoolEntry(tag, "keepSizes", &prefs.keepSizes);
				readBoolEntry(tag, "ignoreHeadline", &prefs.ignoreHeadline);
				readBoolEntry(tag, "ignoreNonRoster", &prefs.ignoreNonRoster);
				readBoolEntry(tag, "excludeGroupChatIgnore", &prefs.excludeGroupChatsFromIgnore);
				readBoolEntry(tag, "scrollTo", &prefs.scrollTo);
				readBoolEntry(tag, "useEmoticons", &prefs.useEmoticons);
				readBoolEntry(tag, "alertOpenChats", &prefs.alertOpenChats);
				readBoolEntry(tag, "raiseChatWindow", &prefs.raiseChatWindow);
				readBoolEntry(tag, "showSubjects", &prefs.showSubjects);
				readBoolEntry(tag, "showGroupCounts", &prefs.showGroupCounts);
				readBoolEntry(tag, "showCounter", &prefs.showCounter);
				readBoolEntry(tag, "chatSays", &prefs.chatSays);
				readBoolEntry(tag, "jidComplete", &prefs.jidComplete);
				readBoolEntry(tag, "grabUrls", &prefs.grabUrls);
				readBoolEntry(tag, "smallChats", &prefs.smallChats);
				readBoolEntry(tag, "brushedMetal", &prefs.brushedMetal);
				readBoolEntry(tag, "chatLineEdit", &prefs.chatLineEdit);
				readBoolEntry(tag, "useTabs", &prefs.useTabs);
				readBoolEntry(tag, "usePerTabCloseButton", &prefs.usePerTabCloseButton);
				readBoolEntry(tag, "putTabsAtBottom", &prefs.putTabsAtBottom);
				readBoolEntry(tag, "autoRosterSize", &prefs.autoRosterSize);
				readBoolEntry(tag, "autoRosterSizeGrowTop", &prefs.autoRosterSizeGrowTop);
				readBoolEntry(tag, "autoResolveNicksOnAdd", &prefs.autoResolveNicksOnAdd);
				readBoolEntry(tag, "messageEvents", &prefs.messageEvents);
				readBoolEntry(tag, "inactiveEvents", &prefs.inactiveEvents);
				readEntry(tag, "lastPath", &prefs.lastPath);
				readEntry(tag, "lastSavePath", &prefs.lastSavePath);
				readBoolEntry(tag, "autoCopy", &prefs.autoCopy);
				readBoolEntry(tag, "useCaps", &prefs.useCaps);
				readBoolEntry(tag, "rc", &prefs.useRC);
				
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
				readBoolEntry(tag, "useDock", &prefs.useDock);
				readBoolEntry(tag, "dockDCstyle", &prefs.dockDCstyle);
				readBoolEntry(tag, "dockHideMW", &prefs.dockHideMW);
				readBoolEntry(tag, "dockToolMW", &prefs.dockToolMW);
			}

			/*tag = findSubTag(p_general, "security", &found);
			if(found) {
				readEntry(tag, "pgp", &prefs.pgp);
			}*/
		}

		// Added by Kiko 020621: retrieves SSL cert chain validation configuration
		QDomElement p_ssl = findSubTag(p,"ssl",&found);
		if(found) {
			readEntry(p_ssl, "trustedcertstoredir", &prefs.trustCertStoreDir);
		}

		QDomElement p_events = findSubTag(p, "events", &found);
		if(found) {
			bool found;

			readNumEntry(p_events, "alertstyle", &prefs.alertStyle);
			readBoolEntry(p_events, "autoAuth", &prefs.autoAuth);
			readBoolEntry(p_events, "notifyAuth", &prefs.notifyAuth);

			QDomElement tag = findSubTag(p_events, "receive", &found);
			if(found) {
				readBoolEntry(tag, "popupMsgs", &prefs.popupMsgs);
				readBoolEntry(tag, "popupChats", &prefs.popupChats);
				readBoolEntry(tag, "popupHeadlines", &prefs.popupHeadlines);
				readBoolEntry(tag, "popupFiles", &prefs.popupFiles);
				readBoolEntry(tag, "noAwayPopup", &prefs.noAwayPopup);
				readBoolEntry(tag, "noUnlistedPopup", &prefs.noUnlistedPopup);
				readBoolEntry(tag, "raise", &prefs.raise);
				readNumEntry(tag, "incomingAs", &prefs.incomingAs);
			}

			tag = findSubTag(p_events, "priority", &found);
			if(found) {
				readNumEntry(tag, "message",  &prefs.eventPriorityMessage);
				readNumEntry(tag, "chat",     &prefs.eventPriorityChat);
				readNumEntry(tag, "headline", &prefs.eventPriorityHeadline);
				readNumEntry(tag, "auth",     &prefs.eventPriorityAuth);
				readNumEntry(tag, "file",     &prefs.eventPriorityFile);
				readNumEntry(tag, "rosterx",  &prefs.eventPriorityRosterExchange);
 			}
		}

		QDomElement p_pres = findSubTag(p, "presence", &found);
		if(found) {
			bool found;

			QDomElement tag = findSubTag(p_pres, "misc", &found);
			if(found) {
				readBoolEntry(tag, "askOnline", &prefs.askOnline);
				readBoolEntry(tag, "askOffline", &prefs.askOffline);
				readBoolEntry(tag, "rosterAnim", &prefs.rosterAnim);
				readBoolEntry(tag, "autoVCardOnLogin", &prefs.autoVCardOnLogin);
				readBoolEntry(tag, "xmlConsoleOnLogin", &prefs.xmlConsoleOnLogin);
			}

			tag = findSubTag(p_pres, "autostatus", &found);
			if(found) {
				bool found;
				QDomElement e;
				e = findSubTag(tag, "away", &found);
				if(found) {
					if(e.hasAttribute("use"))
						readBoolAttribute(e, "use", &prefs.use_asAway);
					else
						prefs.use_asAway = TRUE;
				}
				e = findSubTag(tag, "xa", &found);
				if(found) {
					if(e.hasAttribute("use"))
						readBoolAttribute(e, "use", &prefs.use_asXa);
					else
						prefs.use_asXa = TRUE;
				}
				e = findSubTag(tag, "offline", &found);
				if(found) {
					if(e.hasAttribute("use"))
						readBoolAttribute(e, "use", &prefs.use_asOffline);
					else
						prefs.use_asOffline = TRUE;
				}

				readNumEntry(tag, "away", &prefs.asAway);
				readNumEntry(tag, "xa", &prefs.asXa);
				readNumEntry(tag, "offline", &prefs.asOffline);

				readEntry(tag, "message", &prefs.asMessage);
			}

			tag = findSubTag(p_pres, "statuspresets", &found);
			if(found) {
				prefs.sp.clear();
				for(QDomNode n = tag.firstChild(); !n.isNull(); n = n.nextSibling()) {
					StatusPreset preset(n.toElement());
					if (!preset.name().isEmpty()) 
						prefs.sp[preset.name()] = preset;
				}
			}
			xmlToStringList(p_pres, "recentstatus", &prefs.recentStatus);
		}

		QDomElement p_lnf = findSubTag(p, "lookandfeel", &found);
		if(found) {
			bool found;

			readBoolEntry(p_lnf, "newHeadings", &prefs.clNewHeadings);
			readBoolEntry(p_lnf, "outline-headings", &prefs.outlineHeadings);
			migrateIntEntry(p_lnf, "chat-opacity", "options.ui.chat.opacity");
			migrateIntEntry(p_lnf, "roster-opacity", "options.ui.contactlist.opacity");

			QDomElement tag = findSubTag(p_lnf, "colors", &found);
			if(found) {
				readColorEntry(tag, "online", &prefs.color[cOnline]);
				readColorEntry(tag, "listback", &prefs.color[cListBack]);
				readColorEntry(tag, "away", &prefs.color[cAway]);
				readColorEntry(tag, "dnd", &prefs.color[cDND]);
				readColorEntry(tag, "offline", &prefs.color[cOffline]);
				readColorEntry(tag, "status", &prefs.color[cStatus]);
				readColorEntry(tag, "groupfore", &prefs.color[cGroupFore]);
				readColorEntry(tag, "groupback", &prefs.color[cGroupBack]);
				readColorEntry(tag, "profilefore", &prefs.color[cProfileFore]);
				readColorEntry(tag, "profileback", &prefs.color[cProfileBack]);
				readColorEntry(tag, "animfront", &prefs.color[cAnimFront]);
				readColorEntry(tag, "animback", &prefs.color[cAnimBack]);
			}

			tag = findSubTag(p_lnf, "fonts", &found);
			if(found) {
				readEntry(tag, "roster", &prefs.font[fRoster]);
				readEntry(tag, "message", &prefs.font[fMessage]);
				readEntry(tag, "chat", &prefs.font[fChat]);
				readEntry(tag, "popup", &prefs.font[fPopup]);
			}
		}

		QDomElement p_sound = findSubTag(p, "sound", &found);
		if(found) {
			bool found;

			readEntry(p_sound, "player", &prefs.player);
			readBoolEntry(p_sound, "noawaysound", &prefs.noAwaySound);
			readBoolEntry(p_sound, "noGCSound", &prefs.noGCSound);

			QDomElement tag = findSubTag(p_sound, "onevent", &found);
			if(found) {
				readEntry(tag, "message", &prefs.onevent[eMessage]);
				readEntry(tag, "chat1", &prefs.onevent[eChat1]);
				readEntry(tag, "chat2", &prefs.onevent[eChat2]);
				readEntry(tag, "system", &prefs.onevent[eSystem]);
				readEntry(tag, "headline", &prefs.onevent[eHeadline]);
				readEntry(tag, "online", &prefs.onevent[eOnline]);
				readEntry(tag, "offline", &prefs.onevent[eOffline]);
				readEntry(tag, "send", &prefs.onevent[eSend]);
				readEntry(tag, "incoming_ft", &prefs.onevent[eIncomingFT]);
				readEntry(tag, "ft_complete", &prefs.onevent[eFTComplete]);
			}
		}

		QDomElement p_sizes = findSubTag(p, "sizes", &found);
		if(found) {
			readSizeEntry(p_sizes, "eventdlg", &prefs.sizeEventDlg);
			migrateSizeEntry(p_sizes, "chatdlg", "options.ui.chat.size");
			readSizeEntry(p_sizes, "tabdlg", &prefs.sizeTabDlg);
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
						prefs.toolbars[tbGroup].clear();
						if ( tbGroup == "mainWin" )
							mainWinCleared = true;
					}

					if ( oldStyle ) {
						Options::ToolbarPrefs tb = loadToolbarData( e );
						prefs.toolbars[tbGroup].append(tb);
					}
					else {
						for(QDomNode nn = e.firstChild(); !nn.isNull(); nn = nn.nextSibling()) {
							QDomElement ee = nn.toElement();
							if( ee.isNull() )
								continue;

							if ( ee.tagName() == "toolbar" ) {
								Options::ToolbarPrefs tb = loadToolbarData( ee );
								prefs.toolbars[tbGroup].append(tb);
							}
						}
					}
				}
			}

			// event notifier in these versions was not implemented as an action, so add it
			if ( progver == "0.9" || progver == "0.9-CVS" ) {
				// at first, we need to scan the options, to determine, whether event_notifier already available
				bool found = false;
				QList<Options::ToolbarPrefs>::Iterator it = prefs.toolbars["mainWin"].begin();
				for ( ; it != prefs.toolbars["mainWin"].end(); ++it) {
					QStringList::Iterator it2 = (*it).keys.begin();
					for ( ; it2 != (*it).keys.end(); ++it2) {
						if ( *it2 == "event_notifier" ) {
							found = true;
							break;
						}
					}
				}

				if ( !found ) {
					Options::ToolbarPrefs tb;
					tb.name = QObject::tr("Event notifier");
					tb.on = false;
					tb.locked = true;
					tb.stretchable = true;
					tb.keys << "event_notifier";
					tb.dock  = Qt::DockBottom;
					tb.index = 0;
					prefs.toolbars["mainWin"].append(tb);
				}
			}
		}

		//group chat
		QDomElement p_groupchat = findSubTag(p, "groupchat", &found);
		if (found) {
			readBoolEntry(p_groupchat, "nickcoloring", &prefs.gcNickColoring);
			readBoolEntry(p_groupchat, "highlighting", &prefs.gcHighlighting);
			xmlToStringList(p_groupchat, "highlightwords", &prefs.gcHighlights);
			xmlToStringList(p_groupchat, "nickcolors", &prefs.gcNickColors);
		}

		// Bouncing dock icon (Mac OS X)
		QDomElement p_dock = findSubTag(p, "dock", &found);
		if(found) {
			if (p_dock.attribute("bounce") == "once") {
				prefs.bounceDock = Options::BounceOnce;
			}
			else if (p_dock.attribute("bounce") == "forever") {
				prefs.bounceDock = Options::BounceForever;
			}
			else if (p_dock.attribute("bounce") == "never") {
				prefs.bounceDock = Options::NoBounce;
			}
		}

		QDomElement p_popup = findSubTag(p, "popups", &found);
		if(found) {
			readBoolEntry(p_popup, "on", &prefs.ppIsOn);
			readBoolEntry(p_popup, "online", &prefs.ppOnline);
			readBoolEntry(p_popup, "offline", &prefs.ppOffline);
			readBoolEntry(p_popup, "statusChange", &prefs.ppStatus);
			readBoolEntry(p_popup, "message", &prefs.ppMessage);
			readBoolEntry(p_popup, "chat", &prefs.ppChat);
			readBoolEntry(p_popup, "headline", &prefs.ppHeadline);
			readBoolEntry(p_popup, "file", &prefs.ppFile);
			readNumEntry(p_popup,  "jidClip", &prefs.ppJidClip);
			readNumEntry(p_popup,  "statusClip", &prefs.ppStatusClip);
			readNumEntry(p_popup,  "textClip", &prefs.ppTextClip);
			readNumEntry(p_popup,  "hideTime", &prefs.ppHideTime);
			readColorEntry(p_popup, "borderColor", &prefs.ppBorderColor);
		}

		QDomElement p_lockdown = findSubTag(p, "lockdown", &found);
		if(found) {
			readBoolEntry(p_lockdown, "roster", &prefs.lockdown.roster);
			readBoolEntry(p_lockdown, "services", &prefs.lockdown.services);
		}

		QDomElement p_iconset = findSubTag(p, "iconset", &found);
		if(found) {
			readEntry(p_iconset, "system", &prefs.systemIconset);

			QDomElement roster = findSubTag(p_iconset, "roster", &found);
			if (found) {
				readEntry(roster, "default", &prefs.defaultRosterIconset);

				QDomElement service = findSubTag(roster, "service", &found);
				if (found) {
					prefs.serviceRosterIconset.clear();
					for (QDomNode n = service.firstChild(); !n.isNull(); n = n.nextSibling()) {
						QDomElement i = n.toElement();
						if ( i.isNull() )
							continue;

						prefs.serviceRosterIconset[i.attribute("service")] = i.attribute("iconset");
					}
				}

				QDomElement custom = findSubTag(roster, "custom", &found);
				if (found) {
					prefs.customRosterIconset.clear();
					for (QDomNode n = custom.firstChild(); !n.isNull(); n = n.nextSibling()) {
						QDomElement i = n.toElement();
						if ( i.isNull() )
							continue;

						prefs.customRosterIconset[i.attribute("regExp")] = i.attribute("iconset");
					}
				}
			}

			QDomElement emoticons = findSubTag(p_iconset, "emoticons", &found);
			if (found) {
				prefs.emoticons.clear();
				for (QDomNode n = emoticons.firstChild(); !n.isNull(); n = n.nextSibling()) {
					QDomElement i = n.toElement();
					if ( i.isNull() )
						continue;

					if ( i.tagName() == "item" ) {
						QString is = i.text();
						prefs.emoticons << is;
					}
				}
			}
		}

		QDomElement p_tip = findSubTag(p, "tipOfTheDay", &found);
		if (found) {
			migrateIntEntry(p_tip, "num", "options.ui.tip.number");
			migrateBoolEntry(p_tip, "show", "options.ui.tip.show");
		}

		QDomElement p_disco = findSubTag(p, "disco", &found);
		if (found) {
			readBoolEntry(p_disco, "items", &prefs.discoItems);
			readBoolEntry(p_disco, "info", &prefs.discoInfo);
		}

		QDomElement p_dt = findSubTag(p, "dt", &found);
		if (found) {
			readNumEntry(p_dt, "port", &prefs.dtPort);
			readEntry(p_dt, "external", &prefs.dtExternal);
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

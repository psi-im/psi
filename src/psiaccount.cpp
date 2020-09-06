/*
 * psiaccount.cpp - handles a Psi account
 * Copyright (C) 2001-2005  Justin Karneges
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
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


#include <QFileDialog>
#include <QInputDialog>
#include <QTimer>
#include <QMessageBox>
#include <QPointer>
#include <QApplication>
#include <QPushButton>
#include <QLayout>
#include <QUrl>
#include <QObject>
#include <QMap>
#include <qca.h>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>
#include <QFrame>
#include <QList>
#include <QQueue>
#include <QHostInfo>
#include <QtCrypto>

#include "psiaccount.h"
#include "psiiconset.h"
#include "psicon.h"
#include "profiles.h"
#include "xmpp_tasks.h"
#include "xmpp_xmlcommon.h"
#include "xmpp_caps.h"
#include "xmpp_captcha.h"
#include "s5b.h"
#ifdef FILETRANSFER
#include "filetransfer.h"
#endif
#include "psioptions.h"
#include "textutil.h"
#include "httpauthmanager.h"
#ifdef HAVE_PGPUTIL
#include "pgpkeydlg.h"
#include "pgputil.h"
#endif
#include "applicationinfo.h"
#include "pgptransaction.h"
#include "accountmanagedlg.h"
#include "changepwdlg.h"
#include "xmlconsole.h"
#include "userlist.h"
#include "psievent.h"
#include "jidutil.h"
#include "eventdlg.h"
#include "psiprivacymanager.h"
#include "rosteritemexchangetask.h"
#include "chatdlg.h"
#include "mood.h"
#include "activity.h"
#include "tune.h"
#ifdef GROUPCHAT
#include "groupchatdlg.h"
#endif
#include "statusdlg.h"
#include "infodlg.h"
#include "adduserdlg.h"
#include "historydlg.h"
#include "registrationdlg.h"
#include "searchdlg.h"
#include "discodlg.h"
#include "eventdb.h"
#include "accountmodifydlg.h"
#include "passphrasedlg.h"
#include "voicecaller.h"
#include "voicecalldlg.h"
#ifdef HAVE_JINGLE
#include "jinglevoicecaller.h"
#endif
#ifdef GOOGLE_FT
#include "googleftmanager.h"
#endif
#include "pepmanager.h"
#include "serverinfomanager.h"
#ifdef WHITEBOARDING
#include "sxe/sxemanager.h"
#include "whiteboarding/wbmanager.h"
#endif
#include "bookmarkmanager.h"
#include "vcardfactory.h"
//#include "qssl.h"
#include "mooddlg.h"
#include "activitydlg.h"
#include "qwextend.h"
#include "geolocationdlg.h"
//#include "physicallocation.h"
#include "translationmanager.h"
#include "irisprotocol/iris_discoinfoquerier.h"
#include "iconwidget.h"
#ifdef FILETRANSFER
#include "filetransdlg.h"
#endif
#include "systeminfo.h"
#include "avatars.h"
#include "ahcommanddlg.h"
#include "ahcservermanager.h"
#include "rc.h"
#include "tabdlg.h"
#include "proxy.h"
#include "passdialog.h"
#include "captchadlg.h"
#include "bobfilecache.h"
#include "psicontactlist.h"
#include "psicontact.h"
#include "psiselfcontact.h"
#include "alertable.h"
#include "tabmanager.h"
#include "contactupdatesmanager.h"
#include "fileutil.h"
#include "Certificates/CertificateHelpers.h"
#include "Certificates/CertificateErrorDialog.h"
#include "Certificates/CertificateDisplayDialog.h"
#include "bookmarkmanagedlg.h"
#include "accountloginpassword.h"
#include "alertmanager.h"
#include "popupmanager.h"

#include "psimedia/psimedia.h"
#include "avcall/avcall.h"
#include "avcall/calldlg.h"

#ifdef PSI_PLUGINS
#include "pluginmanager.h"
#endif

#include "../iris/src/xmpp/xmpp-core/protocol.h"



#include "bsocket.h"
/*#ifdef Q_OS_WIN
#include <windows.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif*/

using namespace XMPP;

static AdvancedConnector::Proxy convert_proxy(const UserAccount &acc, const Jid &jid)
{
	bool useHost = false;
	QString host;
	int port = -1;
	if(acc.opt_host) {
		useHost = true;
		host = acc.host;
		if (host.isEmpty()) {
			host = jid.domain();
		}
		port = acc.port;
	}

	AdvancedConnector::Proxy p;
	if(!acc.proxyID.isEmpty()) {
		const ProxyItem &pi = ProxyManager::instance()->getItem(acc.proxyID);
		if(pi.type == "http") // HTTP Connect
			p.setHttpConnect(pi.settings.host, pi.settings.port);
		else if(pi.type == "socks") // SOCKS
			p.setSocks(pi.settings.host, pi.settings.port);
		else if(pi.type == "poll") { // HTTP Poll
			QUrl u = pi.settings.url;
#ifdef HAVE_QT5
			QUrlQuery q(u.query(QUrl::FullyEncoded));
			if(q.queryItems().isEmpty()) {
				if (useHost)
					q.addQueryItem("server",host + ':' + QString::number(port));
				else
					q.addQueryItem("server",jid.domain());
				u.setQuery(q);
			}
#else
			if(u.queryItems().isEmpty()) {
				if (useHost)
					u.addQueryItem("server",host + ':' + QString::number(port));
				else
					u.addQueryItem("server",jid.domain());
			}
#endif
			p.setHttpPoll(pi.settings.host, pi.settings.port, u.toString());
			p.setPollInterval(2);
		}

		if(pi.settings.useAuth)
			p.setUserPass(pi.settings.user, pi.settings.pass);
	}
	return p;
}

struct GCContact
{
	Jid jid;
	Status status;
};

//----------------------------------------------------------------------------
// BlockTransportPopup -- blocks popups on transport status changes
//----------------------------------------------------------------------------

class BlockTransportPopupList;

class BlockTransportPopup : public QObject
{
	Q_OBJECT
public:
	BlockTransportPopup(QObject *, const Jid &);

	Jid jid() const;

private slots:
	void timeout();

private:
	Jid j;
	int userCounter;
	friend class BlockTransportPopupList;
};

BlockTransportPopup::BlockTransportPopup(QObject *parent, const Jid &_j)
: QObject(parent)
{
	j = _j;
	userCounter = 0;

	// Hack for ICQ SMS
	if ( j.domain().left(3) == "icq" ) {
		new BlockTransportPopup(parent, "sms." + j.domain()); // sms.icq.host.com
		new BlockTransportPopup(parent, "sms"  + j.domain().right(j.domain().length() - 3)); // sms.host.com
	}

	QTimer::singleShot(15000, this, SLOT(timeout()));
}

void BlockTransportPopup::timeout()
{
	if ( userCounter > 1 ) {
		QTimer::singleShot(15000, this, SLOT(timeout()));
		userCounter = 0;
	}
	else
		deleteLater();
}

Jid BlockTransportPopup::jid() const
{
	return j;
}

//----------------------------------------------------------------------------
// BlockTransportPopupList
//----------------------------------------------------------------------------

class BlockTransportPopupList : public QObject
{
	Q_OBJECT
public:
	BlockTransportPopupList();

	bool find(const Jid &, bool online = false);
};

BlockTransportPopupList::BlockTransportPopupList()
: QObject()
{
}

bool BlockTransportPopupList::find(const Jid &j, bool online)
{
	if ( j.node().isEmpty() ) // always show popups for transports
		return false;

	QList<BlockTransportPopup *> list = findChildren<BlockTransportPopup *>();
	foreach(BlockTransportPopup* btp, list) {
		if ( j.domain() == btp->jid().domain() ) {
			if ( online )
				btp->userCounter++;
			return true;
		}
	}

	return false;
}


//----------------------------------------------------------------------------
// IdleServer
//----------------------------------------------------------------------------
class IdleServer : public XMPP::Task
{
	Q_OBJECT
public:
	IdleServer(PsiAccount *pa, Task *parent)
		: Task(parent)
		, pa_(pa)
	{
	}

	~IdleServer()
	{
	}

	bool take(const QDomElement &e)
	{
		if (e.tagName() != "iq" || e.attribute("type") != "get")
			return false;

		const QString ns = queryNS(e);
		if (ns == "jabber:iq:last") {
			QDomElement iq = createIQ(doc(), "result", e.attribute("from"), e.attribute("id"));
			QDomElement query = doc()->createElement("query");
			query.setAttribute("xmlns", ns);
			iq.appendChild(query);
			query.setAttribute("seconds", pa_->psi()->idle());

			send(iq);
			return true;
		}
		return false;
	}

private:
	PsiAccount *pa_;
};


//----------------------------------------------------------------------------
// PsiAccount
//----------------------------------------------------------------------------

struct ReconnectData
{
	ReconnectData(int _delay, int _timeout)
		: delay(_delay), timeout(_timeout) {}
	int delay;
	int timeout;
};

static const int RECONNECT_TIMEOUT_ERROR = -10;

static QList<ReconnectData> reconnectData()
{
	static QList<ReconnectData> data;
	static const int max_timeout = 5*60;
	if (data.isEmpty()) {
		data << ReconnectData(15, max_timeout);
		data << ReconnectData(15, max_timeout);
		data << ReconnectData(15, max_timeout);
		data << ReconnectData(15, max_timeout);

		data << ReconnectData(15, max_timeout);
		data << ReconnectData(15, max_timeout);
		data << ReconnectData(15, max_timeout);
		data << ReconnectData(15, max_timeout);

		data << ReconnectData(2*60, max_timeout);
	}

	return data;
}


class PsiAccount::Private : public Alertable
{
	Q_OBJECT
public:
	Private(PsiAccount *parent)
		: Alertable(parent)
		, contactList(0)
		, selfContact(0)
		, psi(0)
		, account(parent)
		, client(0)
		, reconnectingOnce(false)
		, eventQueue(0)
		, xmlConsole(0)
		, blockTransportPopupList(0)
		, privacyManager(0)
		, rosterItemExchangeTask(0)
		, ahcManager(0)
		, rcSetStatusServer(0)
		, rcSetOptionsServer(0)
		, rcForwardServer(0)
		, rcLeaveMucServer(0)
		, avatarFactory(0)
		, voiceCaller(0)
		, avCallManager(0)
		, tabManager(0)
#ifdef GOOGLE_FT
		, googleFTManager(0)
#endif
#ifdef WHITEBOARDING
		, wbManager(0)
#endif
		, serverInfoManager(0)
		, pepManager(0)
		, bookmarkManager(0)
		, httpAuthManager(0)
		, conn(0)
		, stream(0)
		, tls(0)
		, tlsHandler(0)
		, xmlRingbuf(1000)
		, xmlRingbufWrite(0)
		, onlineContactsCount(0)
		, doPopups_(true)
		, reconnectTimeoutTimer_(0)
		, reconnectData_(-1)
		, reconnectInfrequently_(false)
	{
		reconnectTimeoutTimer_ = new QTimer(this);
		reconnectTimeoutTimer_->setSingleShot(true);
		connect(reconnectTimeoutTimer_, SIGNAL(timeout()), SLOT(reconnectTimerTimeout()));
	}

	PsiContactList* contactList;
	PsiContact* selfContact;
	PsiCon *psi;
	PsiAccount *account;
	Client *client;
	UserAccount acc;
	Jid jid, nextJid;
	Status loginStatus;
	bool loginWithPriority;
	bool reconnectingOnce;
	bool nickFromVCard;
	bool pepAvailable;
	bool vcardChecked;
	EventQueue *eventQueue;
	XmlConsole *xmlConsole;
	UserList userList;
	UserListItem self;
	QCA::PGPKey cur_pgpSecretKey;
	QList<Message*> messageQueue;
	BlockTransportPopupList *blockTransportPopupList;
	int userCounter;
	PsiPrivacyManager* privacyManager;
	RosterItemExchangeTask* rosterItemExchangeTask;
	QString currentConnectionError;
	int currentConnectionErrorCondition;

	// Tune
	Tune lastTune;

	// Ad-hoc commands
	AHCServerManager* ahcManager;
	RCSetStatusServer* rcSetStatusServer;
	RCSetOptionsServer* rcSetOptionsServer;
	RCForwardServer* rcForwardServer;
	RCLeaveMucServer* rcLeaveMucServer;

	// Avatars
	AvatarFactory* avatarFactory;
	QString photoHash;

	// Voice Call
	VoiceCaller* voiceCaller;

	AvCallManager *avCallManager;

	TabManager *tabManager;

#ifdef GOOGLE_FT
	// Google file transfer manager
	GoogleFTManager* googleFTManager;
#endif

#ifdef WHITEBOARDING
	// SXE
	SxeManager* sxeManager;
	// Whiteboard
	WbManager* wbManager;
#endif

	// PubSub
	ServerInfoManager* serverInfoManager;
	PEPManager* pepManager;

	// Bookmarks
	BookmarkManager* bookmarkManager;

	// HttpAuth
	HttpAuthManager* httpAuthManager;

	QList<GCContact*> gcbank;
	QStringList groupchats;

	QPointer<AdvancedConnector> conn;
	QPointer<ClientStream> stream;
	QPointer<QCA::TLS> tls;
	QPointer<QCATLSHandler> tlsHandler;
	bool usingSSL;

	QVector<xmlRingElem> xmlRingbuf;
	int xmlRingbufWrite;

	QHostAddress localAddress;

	QList<PsiContact*> contacts;
	int onlineContactsCount;

private:
	bool doPopups_;

public:
	bool noPopup(ActivationType activationType) const
	{
		if (activationType == FromXml || !doPopups_)
			return true;

		if (lastManualStatus().isAvailable()) {
			if (lastManualStatus().type() == XMPP::Status::DND &&
			    PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.suppress-while-dnd").toBool())
			{
				return true;
			}
			if ((lastManualStatus().type() == XMPP::Status::Away || lastManualStatus().type() == XMPP::Status::XA) &&
			    PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.suppress-while-away").toBool())
			{
				return true;
			}
		}

		return false;
	}

	bool noPopupDialogs(ActivationType activationType) const
	{
		if (activationType == FromXml || !doPopups_)
			return true;

		if (lastManualStatus().isAvailable()) {
			if (lastManualStatus().type() == XMPP::Status::DND)
				return true;
			if ((lastManualStatus().type() == XMPP::Status::Away || lastManualStatus().type() == XMPP::Status::XA) &&
				PsiOptions::instance()->getOption("options.ui.notifications.popup-dialogs.suppress-while-away").toBool())
			{
				return true;
			}
		}

		return false;
	}

private slots:
	void removeContact(PsiContact* contact)
	{
		Q_ASSERT(contacts.contains(contact));
		contacts.removeAll(contact);
		emit account->removedContact(contact);
	}

	/**
	 * TODO: make it work with multiple groups per contact.
	 * And with metacontacts too.
	 */
	PsiContact* addContact(const UserListItem& u)
	{
		Q_ASSERT(!findContact(u.jid()));
		// PsiContactGroup* parent = groupsForUserListItem(u).first();
		PsiContact* contact = new PsiContact(u, account);
		contacts.append(contact);
		connect(contact, SIGNAL(destroyed(PsiContact*)), SLOT(removeContact(PsiContact*)));
		emit account->addedContact(contact);
		return contact;
	}

public:
	PsiContact* findContact(const Jid& jid) const
	{
		foreach(PsiContact* contact, contacts)
			if (contact->find(jid))
				return contact;

		return 0;
	}

	PsiContact* findContactOrSelf(const Jid& jid) const
	{
		PsiContact* contact = findContact(jid);
		if (!contact) {
			if (selfContact->find(jid)) {
				contact = selfContact;
			}
		}
		return contact;
	}

	QStringList groupList() const
	{
		QStringList groupList;

		foreach(PsiContact* contact, contacts)
			foreach(QString group, contact->userListItem().groups())
				if (!groupList.contains(group))
					groupList.append(group);

		groupList.sort();
		return groupList;
	}

	QString pathToProfileEvents() const
	{
		return pathToProfile(activeProfile, ApplicationInfo::DataLocation) + "/events-" + JIDUtil::encode(acc.id).toLower() + ".xml";
	}

	void updateOnlineContactsCount()
	{
		int newOnlineContactsCount = 0;
		foreach(const PsiContact* c, contacts) {
			if ( (c->isPrivate() || (c->inList() && c->status().type() != XMPP::Status::Offline) )
				&& !c->isSelf()) {
				++newOnlineContactsCount;
			}
		}

		if (newOnlineContactsCount != onlineContactsCount) {
			onlineContactsCount = newOnlineContactsCount;
			emit account->updatedAccount();
		}
	}

	// FIXME: Rename updateEntry -> updateContact
	void updateEntry(const UserListItem& u)
	{
		if (u.isSelf()) {
			selfContact->update(u);
		}
		else {
			PsiContact* contact = findContact(u.jid());
			if (!contact) {
				contact = addContact(u);
			}
			else {
				contact->update(u);
			}
		}

		updateOnlineContactsCount();
	}

	// FIXME: Rename removeEntry -> removeContact
	void removeEntry(const Jid& jid)
	{
		PsiContact* contact = findContact(jid);
		if (contact) {
			delete contact;
			emit account->removeContact(jid);
		}
		updateOnlineContactsCount();
	}

	void setState(int state)
	{
		const PsiIcon* alert = 0;
		if (state == -1)
			alert = IconsetFactory::iconPtr("psi/connect");
		Alertable::setAlert(alert);
	}

	void setAlert(const Jid& jid, const PsiIcon* icon)
	{
		PsiContact* contact = findContactOrSelf(jid);
		if (contact)
			contact->setAlert(icon);
	}

	void clearAlert(const Jid& jid)
	{
		PsiContact* contact = findContactOrSelf(jid);
		if (contact)
			contact->setAlert(0);
	}

	void animateNick(const Jid& jid)
	{
		PsiContact* contact = findContact(jid);
		if (contact)
			contact->startAnim();
	}

public slots:
	void queueChanged()
	{
		eventQueue->toFile(pathToProfileEvents());
	}

	void loadQueue()
	{
		bool soundEnabled = PsiOptions::instance()->getOption("options.ui.notifications.sounds.enable").toBool();
		PsiOptions::instance()->setOption("options.ui.notifications.sounds.enable", false); // disable the sound and popups
		doPopups_ = false;

		QFileInfo fi( pathToProfileEvents() );
		if ( fi.exists() )
			eventQueue->fromFile(pathToProfileEvents());

		PsiOptions::instance()->setOption("options.ui.notifications.sounds.enable", soundEnabled);
		doPopups_ = true;
	}

	void setEnabled( bool e )
	{
		acc.opt_enabled = e;
		account->cpUpdate(self);
		// account->updateParent();

		eventQueue->setEnabled(e);

		// signals
		emit account->enabledChanged();
		emit account->updatedAccount();
	}

	void client_xmlIncoming(const QString &s)
	{
		xmlRingbuf[xmlRingbufWrite].type = RingXmlIn;
		xmlRingbuf[xmlRingbufWrite].xml = s;
		xmlRingbuf[xmlRingbufWrite].time = QDateTime::currentDateTime();
		xmlRingbufWrite = (xmlRingbufWrite + 1) % xmlRingbuf.count();
	}
	void client_xmlOutgoing(const QString &s)
	{
		xmlRingbuf[xmlRingbufWrite].type = RingXmlOut;
		xmlRingbuf[xmlRingbufWrite].xml = s;
		xmlRingbuf[xmlRingbufWrite].time = QDateTime::currentDateTime();
		xmlRingbufWrite = (xmlRingbufWrite + 1) % xmlRingbuf.count();
	}

	void client_stanzaElementOutgoing(QDomElement &s)
	{
#ifdef PSI_PLUGINS
		PluginManager::instance()->processOutgoingStanza(account, s);
#endif
	}

	void pm_proxyRemoved(QString proxykey)
	{
		if (acc.proxyID == proxykey) acc.proxyID = "";
	}

	void vcardChanged(const Jid &j)
	{
		// our own vcard?
		if(j.compare(jid, false)) {
			const VCard vcard = VCardFactory::instance()->vcard(j);
			if(vcard) {
				vcardPhotoUpdate(vcard.photo());
			}
		}
	}

	void vcardPhotoUpdate(const QByteArray &photoData)
	{
		QString newHash;
		if(!photoData.isEmpty()) {
			newHash = QCA::Hash("sha1").hashToString(photoData);
		}
		if(newHash != photoHash) {
			photoHash = newHash;
			if (account->presenceSent) {
				account->setStatusDirect(loginStatus);
			}
		}
	}

private:
	struct item_dialog2
	{
		QWidget *widget;
		Jid jid;
	};

	QList<item_dialog2*> dialogList;

	bool compareJids(const Jid& j1, const Jid& j2, bool compareResource) const
	{
		return j1.compare(j2, compareResource);
	}

public:
	// implementation for QList<PsiAccount::xmlRingElem> PsiAccount::dumpRingbuf()
	QList< xmlRingElem > dumpRingbuf()
	{
		xmlRingElem el;
		QList< xmlRingElem > ret;
		for(int i=0; i < xmlRingbuf.count(); i++) {
			el = xmlRingbuf[(xmlRingbufWrite + i) % xmlRingbuf.count()];
			if (!el.xml.isEmpty()) {
				ret += el;
			}
		}
		return ret;
	}
	QWidget* findDialog(const QMetaObject& mo, const Jid& jid, bool compareResource) const
	{
		foreach(item_dialog2* i, dialogList) {
			if (mo.cast(i->widget) && compareJids(i->jid, jid, compareResource)) {
				return i->widget;
			}
		}
		return 0;
	}

	void findDialogs(const QMetaObject& mo, const Jid& jid, bool compareResource, QList<void*>* list) const
	{
		foreach(item_dialog2* i, dialogList) {
			if (mo.cast(i->widget) && compareJids(i->jid, jid, compareResource)) {
				list->append(i->widget);
			}
		}
	}

	void findDialogs(const QMetaObject& mo, QList<void*>* list) const
	{
		foreach(item_dialog2* i, dialogList) {
			if (mo.cast(i->widget))
				list->append(i->widget);
		}
	}

	void dialogRegister(QWidget* w, const Jid& jid)
	{
		connect(w, SIGNAL(destroyed(QObject*)), SLOT(forceDialogUnregister(QObject*)));
		item_dialog2 *i = new item_dialog2;
		i->widget = w;
		i->jid = jid;
		dialogList.append(i);
	}

	void dialogUnregister(QWidget* w)
	{
		foreach(item_dialog2 *i, dialogList) {
			if (i->widget == w) {
				dialogList.removeAll(i);
				delete i;
				return;
			}
		}
	}

	void deleteDialogList()
	{
		while (!dialogList.isEmpty()) {
			item_dialog2* i = dialogList.takeFirst();

			delete i->widget;
			delete i;
		}
	}

private slots:
	void forceDialogUnregister(QObject* obj)
	{
		dialogUnregister(static_cast<QWidget*>(obj));
	}

public slots:
	void doModify()
	{
		AccountModifyDlg *w = account->findDialog<AccountModifyDlg*>();
		if(!w) {
			w = new AccountModifyDlg(account, 0);
			w->show();
		}

		bringToFront(w);
	}

	void incoming_call()
	{
		AvCall *sess = avCallManager->takeIncoming();
		AvCallEvent::Ptr ae(new AvCallEvent(sess->jid().full(), sess, account));
		ae->setTimeStamp(QDateTime::currentDateTime());
		account->handleEvent(ae, IncomingStanza);
	}

public:
	void setAutoAway(AutoAway autoAway)
	{
		if (!account->isAvailable())
			return;
		bool withPriority = false;
		Status status = autoAwayStatus(autoAway, withPriority);
		if (status.type() != loginStatus.type() || status.status() != loginStatus.status()) {
			account->setStatusDirect(status, withPriority);
		}
	}

	void setManualStatus(Status status)
	{
		lastManualStatus_ = status;
	}

	const Status& lastManualStatus() const
	{
		return lastManualStatus_;
	}

	void updateAvCallSettings(const UserAccount &acc)
	{
		QString stunHost = acc.stunHost.section(":", 0, 0);
		int stunPort = acc.stunHost.section(":", 1, 1).toInt();
		if (!stunPort) {
			stunPort = 3478; // STUN default port
		}
		avCallManager->setStunBindService(stunHost, stunPort);
		avCallManager->setStunRelayUdpService(stunHost, stunPort, acc.stunUser, acc.stunPass);
		avCallManager->setStunRelayTcpService(stunHost, stunPort, convert_proxy(acc, jid), acc.stunUser, acc.stunPass);
	}

	bool isAlwaysVisibleContact(const Jid& jid) const
	{
		return acc.alwaysVisibleContacts.contains(jid.bare());
	}

	void setAlwaysVisibleContact(const Jid& jid, bool visible)
	{
		if(!visible) {
			acc.alwaysVisibleContacts.removeAll(jid.bare());
		}
		else if(!acc.alwaysVisibleContacts.contains(jid.bare())) {
			acc.alwaysVisibleContacts.append(jid.bare());
		}
	}

	void updateContacts()
	{
		QStringList jids;
		foreach(PsiContact *pc, contacts) {
			const Jid jid = pc->jid();
			jids.append(jid.bare());
			bool vis = isAlwaysVisibleContact(jid);
			if(vis != pc->isAlwaysVisible())
				pc->setAlwaysVisible(vis);
		}
		foreach(const QString& j,  acc.alwaysVisibleContacts) {
			if(!jids.contains(j))
				acc.alwaysVisibleContacts.removeAll(j);
		}
	}

private:
	Status lastManualStatus_;

	XMPP::Status autoAwayStatus(AutoAway autoAway, bool &withPriority)
	{
		withPriority = loginWithPriority;
		if (!lastManualStatus().isAway() && !lastManualStatus().isInvisible()) {
			int priority;
			if (autoAway == AutoAway_Away && PsiOptions::instance()->getOption("options.status.auto-away.force-priority").toBool()) {
				priority = PsiOptions::instance()->getOption("options.status.auto-away.priority").toInt();
				withPriority = true;
			}
			else if (autoAway == AutoAway_XA && PsiOptions::instance()->getOption("options.status.auto-away.force-xa-priority").toBool()) {
				priority = PsiOptions::instance()->getOption("options.status.auto-away.xa-priority").toInt();
				withPriority = true;
			}
			else {
				priority = acc.priority;
				if (autoAway == AutoAway_Away || autoAway == AutoAway_XA) {
					//We reach here when function was called for auto-status (not recover after it)
					// and priority for this auto-status was not set, so we force:
					withPriority = false;
				}
			}

			switch (autoAway) {
			case AutoAway_Away:
				return Status(XMPP::Status::Away, PsiOptions::instance()->getOption("options.status.auto-away.message").toString(), priority);
			case AutoAway_XA:
				return Status(XMPP::Status::XA, PsiOptions::instance()->getOption("options.status.auto-away.message").toString(), priority);
			case AutoAway_Offline:
				return Status(Status::Offline, loginStatus.status(), priority);
			default:
				;
			}
		}
		return lastManualStatus();
	}

public slots:
	void startReconnect()
	{
		reconnectData_++;
		Q_ASSERT(::reconnectData().count());
		reconnectData_ = qMax(0, qMin(reconnectData_, ::reconnectData().count() - 1));
		ReconnectData data = ::reconnectData()[reconnectData_];

		int delay = data.delay * 1000;
		reconnectTimeoutTimer_->stop();
		reconnectTimeoutTimer_->setInterval(data.timeout * 1000);
		account->doReconnect = true;

		reconnectScheduledAt_ = QDateTime::currentDateTime();
		reconnectScheduledAt_ = reconnectScheduledAt_.addSecs(data.delay);
		QTimer::singleShot(delay, account, SLOT(reconnect()));
		emit account->stateChanged();
	}

	void startReconnectTimeout()
	{
		reconnectScheduledAt_ = QDateTime();
		account->doReconnect = false;
		reconnectTimeoutTimer_->start();
	}

	void stopReconnect()
	{
		reconnectScheduledAt_ = QDateTime();
		reconnectData_ = -1;
		account->doReconnect = false;
		reconnectTimeoutTimer_->stop();
	}

private slots:
	void reconnectTimerTimeout()
	{
		reconnectScheduledAt_ = QDateTime();
		account->v_isActive = true;
		account->cs_error(RECONNECT_TIMEOUT_ERROR);
	}

public:
	QDateTime reconnectScheduledAt_;
	QTimer* reconnectTimeoutTimer_;
	int reconnectData_;
	bool reconnectInfrequently_;
};

PsiAccount* PsiAccount::create(const UserAccount &acc, PsiContactList *parent, TabManager *tabManager)
{
	PsiAccount* account = new PsiAccount(acc, parent, tabManager);

	account->init();
	return account;
}

void PsiAccount::init()
{
}

PsiAccount::PsiAccount(const UserAccount &acc, PsiContactList *parent, TabManager *tabManager)
	: QObject(parent)
{
	d = new Private( this );
	QPointer<Private> boom(d);
	d->contactList = parent;
	d->tabManager = tabManager;
	d->psi = parent->psi();
	d->client = 0;
	d->userCounter = 0;
	d->avatarFactory = 0;
	d->voiceCaller = 0;
	d->blockTransportPopupList = new BlockTransportPopupList();

	v_isActive = false;
	isDisconnecting = false;
	notifyOnlineOk = false;
	doReconnect = false;
	presenceSent = false;

	d->loginStatus = Status(Status::Offline);
	d->loginWithPriority = false;
	d->setManualStatus(Status(Status::Offline, "", 0));

	d->eventQueue = new EventQueue(this);
	connect(d->eventQueue, SIGNAL(queueChanged()), SIGNAL(queueChanged()));
	connect(d->eventQueue, SIGNAL(queueChanged()), d, SLOT(queueChanged()));
	connect(d->eventQueue, SIGNAL(eventFromXml(PsiEvent::Ptr)), SLOT(eventFromXml(PsiEvent::Ptr)));
	d->self = UserListItem(true);
	d->self.setSubscription(Subscription::Both);
	d->nickFromVCard = false;
	d->vcardChecked = false;

	// we need to copy groupState, because later initialization will depend on that
	d->acc.groupState = acc.groupState;

	// stream
	d->conn = 0;
	d->tls = 0;
	d->tlsHandler = 0;
	d->stream = 0;
	d->usingSSL = false;

	// create XMPP::Client
	d->client = new Client;

	// Plugins
#ifdef PSI_PLUGINS
	PluginManager::instance()->addAccount(this, d->client);
#endif
	d->client->setOSName(SystemInfo::instance()->osName());
	d->client->setOSVersion(SystemInfo::instance()->osVersion());
	d->client->setClientName(ApplicationInfo::name());
	d->client->setClientVersion(ApplicationInfo::version());
	d->client->setCaps(CapsSpec(ApplicationInfo::capsNode(), QCryptographicHash::Sha1));
	d->client->bobManager()->setCache(BoBFileCache::instance()); // xep-0231

	DiscoItem::Identity identity;
	identity.category = "client";
	identity.type = "pc";
	identity.name = ApplicationInfo::name();
	d->client->setIdentity(identity);
	updateFeatures();

#ifdef FILETRANSFER
	d->client->setFileTransferEnabled(true);
#else
	d->client->setFileTransferEnabled(false);
#endif

	//connect(d->client, SIGNAL(connected()), SLOT(client_connected()));
	//connect(d->client, SIGNAL(handshaken()), SLOT(client_handshaken()));
	//connect(d->client, SIGNAL(error(const StreamError &)), SLOT(client_error(const StreamError &)));
	//connect(d->client, SIGNAL(sslCertReady(const QSSLCert &)), SLOT(client_sslCertReady(const QSSLCert &)));
	//connect(d->client, SIGNAL(closeFinished()), SLOT(client_closeFinished()));
	//connect(d->client, SIGNAL(authFinished(bool, int, const QString &)), SLOT(client_authFinished(bool, int, const QString &)));
	connect(d->client, SIGNAL(rosterRequestFinished(bool, int, const QString &)), SLOT(client_rosterRequestFinished(bool, int, const QString &)));
	connect(d->client, SIGNAL(rosterItemAdded(const RosterItem &)), SLOT(client_rosterItemAdded(const RosterItem &)));
	connect(d->client, SIGNAL(rosterItemAdded(const RosterItem &)), SLOT(client_rosterItemUpdated(const RosterItem &)));
	connect(d->client, SIGNAL(rosterItemUpdated(const RosterItem &)), SLOT(client_rosterItemUpdated(const RosterItem &)));
	connect(d->client, SIGNAL(rosterItemRemoved(const RosterItem &)), SLOT(client_rosterItemRemoved(const RosterItem &)));
	connect(d->client, SIGNAL(resourceAvailable(const Jid &, const Resource &)), SLOT(client_resourceAvailable(const Jid &, const Resource &)));
	connect(d->client, SIGNAL(resourceUnavailable(const Jid &, const Resource &)), SLOT(client_resourceUnavailable(const Jid &, const Resource &)));
	connect(d->client, SIGNAL(presenceError(const Jid &, int, const QString &)), SLOT(client_presenceError(const Jid &, int, const QString &)));
	connect(d->client, SIGNAL(messageReceived(const Message &)), SLOT(client_messageReceived(const Message &)));
	connect(d->client, SIGNAL(subscription(const Jid &, const QString &, const QString&)), SLOT(client_subscription(const Jid &, const QString &, const QString&)));
	connect(d->client, SIGNAL(debugText(const QString &)), SLOT(client_debugText(const QString &)));
	connect(d->client, SIGNAL(groupChatJoined(const Jid &)), SLOT(client_groupChatJoined(const Jid &)));
	connect(d->client, SIGNAL(groupChatLeft(const Jid &)), SLOT(client_groupChatLeft(const Jid &)));
	connect(d->client, SIGNAL(groupChatPresence(const Jid &, const Status &)), SLOT(client_groupChatPresence(const Jid &, const Status &)));
	connect(d->client, SIGNAL(groupChatError(const Jid &, int, const QString &)), SLOT(client_groupChatError(const Jid &, int, const QString &)));
	connect(d->client, SIGNAL(beginImportRoster()), SIGNAL(beginBulkContactUpdate()));
	connect(d->client, SIGNAL(endImportRoster()), SIGNAL(endBulkContactUpdate()));
	connect(d->client, SIGNAL(xmlIncoming(const QString &)), d, SLOT(client_xmlIncoming(const QString &)));
	connect(d->client, SIGNAL(xmlOutgoing(const QString &)), d, SLOT(client_xmlOutgoing(const QString &)));
	connect(d->client, SIGNAL(stanzaElementOutgoing(QDomElement &)), d, SLOT(client_stanzaElementOutgoing(QDomElement &)));

	// Privacy manager
	d->privacyManager = new PsiPrivacyManager(d->account, d->client->rootTask());

	// Caps manager
	d->client->capsManager()->setEnabled(PsiOptions::instance()->getOption("options.service-discovery.enable-entity-capabilities").toBool());

	// Roster item exchange task
	d->rosterItemExchangeTask = new RosterItemExchangeTask(d->client->rootTask());
	connect(d->rosterItemExchangeTask,SIGNAL(rosterItemExchange(const Jid&, const RosterExchangeItems&)),SLOT(actionRecvRosterExchange(const Jid&,const RosterExchangeItems&)));

	// Initialize server info stuff
	d->serverInfoManager = new ServerInfoManager(d->client);
	connect(d->serverInfoManager,SIGNAL(featuresChanged()),SLOT(serverFeaturesChanged()));

	// Initialize PubSub stuff
	d->pepManager = new PEPManager(d->client, d->serverInfoManager);
	connect(d->pepManager,SIGNAL(itemPublished(const Jid&, const QString&, const PubSubItem&)),SLOT(itemPublished(const Jid&, const QString&, const PubSubItem&)));
	connect(d->pepManager,SIGNAL(itemRetracted(const Jid&, const QString&, const PubSubRetraction&)),SLOT(itemRetracted(const Jid&, const QString&, const PubSubRetraction&)));
	d->pepAvailable = false;

#ifdef WHITEBOARDING
	 // Initialize SXE manager
	d->sxeManager = new SxeManager(d->client, this);
	 // Initialize Whiteboard manager
	d->wbManager = new WbManager(d->client, this, d->sxeManager);
	connect(d->wbManager, SIGNAL(wbRequest(Jid,int)),SLOT(wbRequest(Jid,int)));
#endif

	// Avatars
	d->avatarFactory = new AvatarFactory(this);
	d->self.setAvatarFactory(avatarFactory());

	connect(VCardFactory::instance(), SIGNAL(vcardChanged(const Jid&)), d, SLOT(vcardChanged(const Jid&)));

	// Bookmarks
	d->bookmarkManager = new BookmarkManager(this);
	connect(d->bookmarkManager, SIGNAL(availabilityChanged()), SLOT(bookmarksAvailabilityChanged()));

#ifdef USE_PEP
	// Tune Controller
	connect(d->psi->tuneManager(), SIGNAL(stopped()), SLOT(tuneStopped()));
	connect(d->psi->tuneManager(), SIGNAL(playing(const Tune&)),SLOT(tunePlaying(const Tune&)));
#endif

	// HttpAuth
	d->httpAuthManager = new HttpAuthManager(d->client->rootTask());
	connect(d->httpAuthManager, SIGNAL(confirmationRequest(const PsiHttpAuthRequest &)), SLOT(incomingHttpAuthRequest(const PsiHttpAuthRequest &)));

	// Initialize Adhoc Commands server
	d->ahcManager = new AHCServerManager(this);
	d->rcSetStatusServer = 0;
	d->rcSetOptionsServer = 0;
	d->rcForwardServer = 0;
	d->rcLeaveMucServer =0;
	setRCEnabled(PsiOptions::instance()->getOption("options.external-control.adhoc-remote-control.enable").toBool());

	//Idle server
	if(PsiOptions::instance()->getOption("options.service-discovery.last-activity").toBool()) {
		new IdleServer(this, d->client->rootTask());
	}

	d->selfContact = new PsiContact(d->self, this, true);

	// restore cached roster
	for(Roster::ConstIterator it = acc.roster.begin(); it != acc.roster.end(); ++it)
		client_rosterItemUpdated(*it);

	// restore pgp key bindings
	for(VarList::ConstIterator kit = acc.keybind.begin(); kit != acc.keybind.end(); ++kit) {
		const VarListItem &i = *kit;
		UserListItem *u = find(Jid(i.key()));
		if(u) {
			u->setPublicKeyID(i.data());
			cpUpdate(*u);
		}
	}

	setUserAccount(acc);
	connect(ProxyManager::instance(), SIGNAL(proxyRemoved(QString)),
			d, SLOT(pm_proxyRemoved(QString)));

	connect(d->psi, SIGNAL(emitOptionsUpdate()), SLOT(optionsUpdate()));
	//connect(d->psi, SIGNAL(pgpToggled(bool)), SLOT(pgpToggled(bool)));
#ifdef HAVE_PGPUTIL
	connect(&PGPUtil::instance(), SIGNAL(pgpKeysUpdated()), SLOT(pgpKeysUpdated()));
	connect(&PGPUtil::instance(), SIGNAL(newPassPhase(QString,QString)), SLOT(newPgpPassPhase(QString,QString)));
#endif

	d->setEnabled(enabled());

	// Listen to the capabilities manager
	connect(d->client->capsManager(),SIGNAL(capsChanged(const Jid&)),SLOT(capsChanged(const Jid&)));

	//printf("PsiAccount: [%s] loaded\n", name().latin1());

	if(PsiOptions::instance()->getOption("options.xml-console.enable-at-login").toBool() && enabled()) {
		this->showXmlConsole();
		d->xmlConsole->enable();
	}

	// TIP OF THE DAY: You cannot change content of caps extension.
	// You have to rename it. If you just change what's inside, some people
	// will cache one set of features and some people will cache other set.
	// And this of course is a Bad Thing.

	// Voice Calling
#ifdef HAVE_JINGLE
	d->voiceCaller = new JingleVoiceCaller(this);
#endif
	if (d->voiceCaller) {
		connect(d->voiceCaller,SIGNAL(incoming(const Jid&)),SLOT(incomingVoiceCall(const Jid&)));
	}

#ifdef FILETRANSFER
	reconfigureFTManager();
	connect(d->client->fileTransferManager(), SIGNAL(incomingReady()), SLOT(client_incomingFileTransfer()));
#endif
#ifdef GOOGLE_FT
	d->googleFTManager = new GoogleFTManager(client());
	d->client->addExtension("share-v1", Features(QString("http://www.google.com/xmpp/protocol/share/v1")));
	connect(d->googleFTManager,SIGNAL(incomingFileTransfer(GoogleFileTransfer*)),SLOT(incomingGoogleFileTransfer(GoogleFileTransfer*)));
#endif

	if(AvCallManager::isSupported()) {
		d->avCallManager = new AvCallManager(this);
		connect(d->avCallManager, SIGNAL(incomingReady()), d, SLOT(incoming_call()));
		d->updateAvCallSettings(acc);
	}

	// load event queue from disk
	QTimer::singleShot(0, d, SLOT(loadQueue()));

	d->contactList->link(this);

	d->updateContacts(); //update always visible contacts state
}

PsiAccount::~PsiAccount()
{
	bool logged = isActive();
	logout(true, loggedOutStatus());

	if (logged) {
		QEventLoop l; // we need a few cycles to finish sending data in qca on app quit
		for (int i =0; i < 100; i++) {
			l.processEvents();
		}
	}

	setRCEnabled(false);

	emit accountDestroyed();
	// nuke all related dialogs
	deleteAllDialogs();

	while (!d->messageQueue.isEmpty())
		delete d->messageQueue.takeFirst();

#ifdef FILETRANSFER
	d->psi->ftdlg()->killTransfers(this);
#endif

	delete d->avCallManager;
	delete d->voiceCaller;
	delete d->ahcManager;
	delete d->privacyManager;
	delete d->pepManager;
	delete d->serverInfoManager;
#ifdef WHITEBOARDING
	delete d->wbManager;
	delete d->sxeManager;
#endif
	delete d->bookmarkManager;
	delete d->client;
	delete d->httpAuthManager;
	cleanupStream();
	delete d->eventQueue;
	delete d->avatarFactory;

	delete d->blockTransportPopupList;

	qDeleteAll(d->userList);
	d->userList.clear();

	d->contactList->unlink(this);
	delete d;

	//printf("PsiAccount: [%s] unloaded\n", str.latin1());
}

void PsiAccount::cleanupStream()
{
	// GSOC: Get SM state out of stream
	delete d->stream;

	delete d->tls;
	d->tls = 0;
	d->tlsHandler = 0;

	delete d->conn;
	d->conn = 0;

	d->usingSSL = false;

	d->localAddress = QHostAddress();
}

void PsiAccount::updateAlwaysVisibleContact(PsiContact *pc)
{
	d->setAlwaysVisibleContact(pc->jid(), pc->isAlwaysVisible());
}

bool PsiAccount::enabled() const
{
	return d->acc.opt_enabled;
}

void PsiAccount::setEnabled(bool e)
{
	if ( d->acc.opt_enabled == e )
		return;

	if (!e) {
		if (eventQueue()->count()) {
			QMessageBox::information(0, tr("Error"), tr("Unable to disable the account, as it has pending events."));
			return;
		}
		if (isActive()) {
			if (QMessageBox::information(0, tr("Disable Account"), tr("The account is currently active.\nDo you want to log out ?"),QMessageBox::Yes,QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton) == QMessageBox::Yes) {
				logout(false, loggedOutStatus());
			}
			else {
				return;
			}
		}
		delete d->xmlConsole;
		d->xmlConsole = 0;
	}

	d->setEnabled( e );
}

bool PsiAccount::isActive() const
{
	return v_isActive;
}

bool PsiAccount::isConnected() const
{
	return (d->stream && d->stream->isAuthenticated());
}

/**
 * This function returns true when the account is connected to the server,
 * authenticated and is not currently trying to disconnect.
 */
bool PsiAccount::isAvailable() const
{
	return isConnected() && isActive() && loggedIn() && !isDisconnecting;
}

const QString & PsiAccount::name() const
{
	return d->acc.name;
}

const QString &PsiAccount::id() const
{
	return d->acc.id;
}

/**
 * TODO: FIXME: Has side-effects of updating toggle's status, clearing
 * the roster and keybindings.
 */
// FIXME: we should move all PsiAccount::userAccount() users to PsiAccount::accountOptions()
const UserAccount & PsiAccount::userAccount() const
{
	// save the roster and pgp key bindings
	d->acc.roster.clear();
	d->acc.keybind.clear();
	foreach(UserListItem* u, d->userList) {
		if(u->inList())
			d->acc.roster += *u;

		if(!u->publicKeyID().isEmpty())
			d->acc.keybind.set(u->jid().full(), u->publicKeyID());
	}

	return d->acc;
}

UserAccount PsiAccount::accountOptions() const
{
	return d->acc;
}

UserList *PsiAccount::userList() const
{
	return &d->userList;
}

Client *PsiAccount::client() const
{
	return d->client;
}

EventQueue *PsiAccount::eventQueue() const
{
	return d->eventQueue;
}

EDB *PsiAccount::edb() const
{
	return d->psi->edb();
}

PsiCon *PsiAccount::psi() const
{
	return d->psi;
}

AvatarFactory *PsiAccount::avatarFactory() const
{
	return d->avatarFactory;
}

VoiceCaller* PsiAccount::voiceCaller() const
{
	return d->voiceCaller;
}
#ifdef WHITEBOARDING
WbManager* PsiAccount::wbManager() const
{
	return d->wbManager;
}
#endif
PrivacyManager* PsiAccount::privacyManager() const
{
	return d->privacyManager;
}

bool PsiAccount::hasPGP() const
{
	return !d->cur_pgpSecretKey.isNull();
}

QHostAddress *PsiAccount::localAddress() const
{
	QString s = d->localAddress.toString();
	if (s.isEmpty() || s == "0.0.0.0")
		return 0;
	return &d->localAddress;
}

void PsiAccount::setUserAccount(const UserAccount &_acc)
{
	UserAccount acc = _acc;

	bool renamed = false;
	QString oldfname;
	if(d->acc.name != acc.name) {
		renamed = true;
		oldfname = d->pathToProfileEvents();
	}

	d->acc = acc;
	d->setEnabled(enabled());

	// rename queue file?
	if(renamed) {
		QFileInfo oldfi(oldfname);
		QFileInfo newfi(d->pathToProfileEvents());
		if(oldfi.exists()) {
			QDir dir = oldfi.dir();
			dir.rename(oldfi.fileName(), newfi.fileName());
		}
	}

	if(d->stream) {
		if(d->acc.opt_keepAlive)
			d->stream->setNoopTime(55000);  // prevent NAT timeouts every minute
		else
			d->stream->setNoopTime(0);
	}

	// d->cp->setName(d->acc.name);

	Jid j = acc.jid;
	d->nextJid = j;
	if(!isActive()) {
		d->jid = j;
		d->self.setJid(j);
		profileUpdateEntry(d->self);
	}
	if(!d->nickFromVCard)
		setNick(j.node());

	QString pgpSecretKeyID = (d->acc.pgpSecretKey.isNull() ? "" : d->acc.pgpSecretKey.keyId());
	d->self.setPublicKeyID(pgpSecretKeyID);
#ifdef HAVE_PGPUTIL
	if(PGPUtil::instance().pgpAvailable()) {
		bool updateStatus = !PGPUtil::instance().equals(d->acc.pgpSecretKey, d->cur_pgpSecretKey) && loggedIn();
		d->cur_pgpSecretKey = d->acc.pgpSecretKey;
		pgpKeyChanged();
		if (updateStatus) {
			d->loginStatus.setXSigned("");
			setStatusDirect(d->loginStatus);
		}
	}
#endif

	if(d->avCallManager)
		d->updateAvCallSettings(d->acc);

	cpUpdate(d->self);
	emit updatedAccount();
}

void PsiAccount::deleteQueueFile()
{
	QFileInfo fi(d->pathToProfileEvents());
	if(fi.exists()) {
		QDir dir = fi.dir();
		dir.remove(fi.fileName());
	}
}

const Jid & PsiAccount::jid() const
{
	return d->jid;
}

QString PsiAccount::nameWithJid() const
{
	return (name() + " (" + JIDUtil::toString(jid(),true) + ')');
}

void PsiAccount::updateFeatures()
{
	QStringList features = d->psi->xmppFatures();
	// TODO update features depending on account settings and plugins' features

	if (d->voiceCaller) {
		features << "http://www.google.com/xmpp/protocol/voice/v1"; // isn't obsoleted?
	}
#ifdef WHITEBOARDING
	features << SXENS << WBNS;
#endif

	// TODO reset hash
	d->client->setFeatures(Features(features));
}

void PsiAccount::autoLogin()
{
	// auto-login ?
	if (enabled()) {
		bool autoLogin = d->acc.opt_auto;
		if (autoLogin) {
			if (d->acc.opt_autoSameStatus) {
				setStatus(d->acc.lastStatus, d->acc.lastStatusWithPriority, true);
			}
			else {
				setStatus(makeStatus(XMPP::Status::Online, ""), false, true);
			}
		}
	}
}

void PsiAccount::reconnectOnce()
{
	if (isActive()) {
		if (d->reconnectingOnce) {
			//To be sure phase2 will be called just once
			QObject::disconnect(this, SIGNAL(disconnected()), this, SLOT(reconnectOncePhase2()));
		}
		d->reconnectingOnce = true;
		connect(this, SIGNAL(disconnected()), this, SLOT(reconnectOncePhase2()));
		logout(false, Status(Status::Offline, tr("Reconnecting"), 0));
	}
}

void PsiAccount::reconnectOncePhase2()
{
	if (d->reconnectingOnce) {
		d->reconnectingOnce = false;
		QObject::disconnect(this, SIGNAL(disconnected()), this, SLOT(reconnectOncePhase2()));
		setStatus(d->lastManualStatus(), d->loginWithPriority, false);
	}
}

// logs on with the active account settings
void PsiAccount::login()
{
	if(isActive() && !doReconnect)
		return;

	if((d->acc.ssl == UserAccount::SSL_Yes || d->acc.ssl == UserAccount::SSL_Legacy) && !QCA::isSupported("tls")) {
		QString title;
		if (d->psi->contactList()->enabledAccounts().count() > 1) {
			title = QString("%1: ").arg(name());
		}
		title +=  tr("Encryption Error");
		QString message = tr("Cannot connect: Encryption is enabled but no QCA2 SSL/TLS plugin is available.");

		psi()->alertManager()->raiseMessageBox(AlertManager::ConnectionError,
			QMessageBox::Information, title, message);
		return;
	}

	if(d->acc.legacy_ssl_probe) {
		// disable the feature and display a notice
		d->acc.legacy_ssl_probe = false;
		emit updatedAccount();

		QString title;
		if (d->psi->contactList()->enabledAccounts().count() > 1) {
			title = QString("%1: ").arg(name());
		}
		title += tr("Feature Removed");
		QString message = tr("This account was configured to use the \"Probe legacy SSL port\" feature, but this feature is no longer supported. Unless your XMPP server is very outdated, this change should not affect you. If you have trouble connecting, please review your account settings for correctness or contact your XMPP server administrator.");

		psi()->alertManager()->raiseMessageBox(AlertManager::ConnectionError,
			QMessageBox::Information, title, message);
	}

	d->jid = d->nextJid;

	v_isActive = true;
	isDisconnecting = false;
	notifyOnlineOk = false;
	rosterDone = false;
	presenceSent = false;

	stateChanged();

	bool useHost = false;
	QString host;
	int port = -1;
	if(d->acc.opt_host) {
		useHost = true;
		host = d->acc.host;
		if (host.isEmpty()) {
			host = d->jid.domain();
		}
		port = d->acc.port;
	}

	AdvancedConnector::Proxy p = convert_proxy(d->acc, d->jid);

	// stream
	d->conn = new AdvancedConnector;
	if(d->acc.ssl != UserAccount::SSL_No && QCA::isSupported("tls")) {
		d->tls = new QCA::TLS;
		d->tls->setTrustedCertificates(CertificateHelpers::allCertificates(ApplicationInfo::getCertificateStoreDirs()));
		d->tlsHandler = new QCATLSHandler(d->tls);
		d->tlsHandler->setXMPPCertCheck(true);
		connect(d->tlsHandler, SIGNAL(tlsHandshaken()), SLOT(tls_handshaken()));
	}
	d->conn->setProxy(p);

	if (useHost) {
		d->conn->setOptHostPort(host, port);
		d->conn->setOptSSL(d->acc.ssl == UserAccount::SSL_Legacy);
	}

	d->stream = new ClientStream(d->conn, d->tlsHandler);
	d->stream->setRequireMutualAuth(d->acc.req_mutual_auth);
	d->stream->setSSFRange(d->acc.security_level,256);
	d->stream->setAllowPlain(d->acc.allow_plain);
	d->stream->setCompress(d->acc.opt_compress);
	d->stream->setLang(TranslationManager::instance()->currentXMLLanguage());
	if(d->acc.opt_keepAlive) {
		d->stream->setNoopTime(55000);  // prevent NAT timeouts every minute
	} else {
		d->stream->setNoopTime(0);
	}
	connect(d->stream, SIGNAL(connected()), SLOT(cs_connected()));
	connect(d->stream, SIGNAL(securityLayerActivated(int)), SLOT(cs_securityLayerActivated(int)));
	connect(d->stream, SIGNAL(needAuthParams(bool, bool, bool)), SLOT(cs_needAuthParams(bool, bool, bool)));
	connect(d->stream, SIGNAL(authenticated()), SLOT(cs_authenticated()));
	connect(d->stream, SIGNAL(connectionClosed()), SLOT(cs_connectionClosed()), Qt::QueuedConnection);
	connect(d->stream, SIGNAL(delayedCloseFinished()), SLOT(cs_delayedCloseFinished()));
	connect(d->stream, SIGNAL(warning(int)), SLOT(cs_warning(int)));
	connect(d->stream, SIGNAL(error(int)), SLOT(cs_error(int)), Qt::QueuedConnection);

	Jid j = d->jid.withResource((d->acc.opt_automatic_resource ? localHostName() : d->acc.resource ));
	d->stream->setSMEnabled(d->acc.opt_sm);
	d->client->connectToServer(d->stream, j);
}

// disconnect or stop reconnecting
void PsiAccount::logout(bool fast, const Status &s)
{
	if(!isActive())
		return;

#ifdef PSI_PLUGINS
	PluginManager::instance()->logout(this);
#endif
	clearCurrentConnectionError();

	d->stopReconnect();

	forceDisconnect(fast, s);
}

void PsiAccount::forceDisconnect(bool fast, const XMPP::Status &s)
{
	if(!isActive())
		return;

	if(loggedIn()) {
		// Extended Presence
		if (PsiOptions::instance()->getOption("options.extended-presence.tune.publish").toBool() && !d->lastTune.isNull())
			publishTune(Tune());

		// send logout status
		d->client->groupChatLeaveAll(PsiOptions::instance()->getOption("options.muc.leave-status-message").toString());
		d->client->setPresence(s);
	}

	isDisconnecting = true;

	if(!fast)
		simulateRosterOffline();

	v_isActive = false;
	d->loginStatus = Status(Status::Offline);
	stateChanged();

	// Using 100msecs; See note on disconnect()
	QTimer::singleShot(100, this, SLOT(disconnect()));
}

// skz note: I had to split logout() because server seem to need some time to store status
// before stream is closed (weird, I know)
void PsiAccount::disconnect()
{
	if (isDisconnecting) {
		// disconnect
		d->client->close();
		cleanupStream();

		emit disconnected();
		isDisconnecting = false;

		if(d->loginStatus.isAvailable())
			login();
	}
}

// TODO: search through the Psi and replace most of loggedIn() calls with isAvailable()
// because it's safer
bool PsiAccount::loggedIn() const
{
	return (v_isActive && presenceSent);
}

void PsiAccount::tls_handshaken()
{
	bool certificateOk = CertificateHelpers::checkCertificate(d->tls, d->tlsHandler, d->acc.tlsOverrideDomain, d->acc.tlsOverrideCert, this,
						 (d->psi->contactList()->enabledAccounts().count() > 1 ?  QString("%1: ").arg(name()) : "") + tr("Server Authentication"),
						 d->jid.domain());
	if (certificateOk && !d->tlsHandler.isNull()) {
		d->tlsHandler->continueAfterHandshake();
	} else {
		logout(false, loggedOutStatus());
	}
}

void PsiAccount::showCert()
{
	if (!d->tls || !d->tls->isHandshaken()) return;
	QCA::Certificate cert = d->tls->peerCertificateChain().primary();
	int r = d->tls->peerIdentityResult();
	if (r == QCA::TLS::Valid && !d->tlsHandler->certMatchesHostname()) r = QCA::TLS::HostMismatch;
	QCA::Validity validity =  d->tls->peerCertificateValidity();
	CertificateDisplayDialog dlg(cert, r, validity);
	dlg.exec();
}


void PsiAccount::cs_connected()
{
	// get IP address
	ByteStream *bs = d->conn ? d->conn->stream() : 0;
	if (!bs)
		return;

	if(bs->inherits("BSocket") || bs->inherits("XMPP::BSocket")) {
		d->localAddress = ((BSocket *)bs)->address();

		if(d->avCallManager)
			d->avCallManager->setSelfAddress(d->localAddress);
	}
}

void PsiAccount::cs_securityLayerActivated(int layer)
{
	if ((layer == ClientStream::LayerSASL) && (d->stream->saslSSF() <= 1)) {
		 // integrity protected only
	} else {
		d->usingSSL = true;
		stateChanged();
	}
}

void PsiAccount::cs_needAuthParams(bool user, bool pass, bool realm)
{
	if(user) {
		if (d->acc.customAuth && !d->acc.authid.isEmpty())
			d->stream->setUsername(d->acc.authid);
		else
			d->stream->setUsername(d->jid.node());
	}
	else if (d->acc.customAuth && !d->acc.authid.isEmpty())
		qWarning("Custom authentication user not used");

	if(pass) {
		d->stream->setPassword(d->acc.pass);
		if (d->acc.storeSaltedHashedPassword) d->stream->setSCRAMStoredSaltedHash(d->acc.scramSaltedHashPassword);
	}
	if (realm) {
		if (d->acc.customAuth && !d->acc.realm.isEmpty()) {
			d->stream->setRealm(d->acc.realm);
		}
		else {
			d->stream->setRealm(d->jid.domain());
		}
	}
	else if (d->acc.customAuth && !d->acc.realm.isEmpty())
		qWarning("Custom authentication realm not used");

	if(d->acc.customAuth)
		d->stream->setAuthzid(d->jid.bare());
	d->stream->continueAfterParams();
}

void PsiAccount::cs_authenticated()
{
	if (d->conn.isNull() || d->stream.isNull()) {
		cs_error(RECONNECT_TIMEOUT_ERROR);
		return;
	}

	if (d->acc.storeSaltedHashedPassword) {
		d->acc.scramSaltedHashPassword = d->stream->getSCRAMStoredSaltedHash();
		d->acc.pass = "";
	}

	//printf("PsiAccount: [%s] authenticated\n", name().latin1());
	d->conn->changePollInterval(10); // for http poll, slow down after login

	// Update our jid (if necessary)
	if (!d->stream->jid().isEmpty()) {
		d->jid = d->stream->jid().bare();
	}

	QString resource = (d->stream->jid().resource().isEmpty() ? ( d->acc.opt_automatic_resource ? localHostName() : d->acc.resource) : d->stream->jid().resource());

	d->client->start(d->jid.domain(), d->jid.node(), d->acc.pass, resource);
	if (!d->stream->old()) {
		JT_Session *j = new JT_Session(d->client->rootTask());
		connect(j,SIGNAL(finished()),SLOT(sessionStart_finished()));
		j->go(true);
	}
	else {
		sessionStarted();
	}
}

void PsiAccount::sessionStart_finished()
{
	JT_Session *j = (JT_Session*)sender();
	if ( j->success() ) {
		sessionStarted();
	}
	else {
		cs_error(-1);
	}
}


void PsiAccount::sessionStarted()
{
	if (d->voiceCaller)
		d->voiceCaller->initialize();

	// flag roster for delete
	foreach(UserListItem* u, d->userList) {
		if(u->inList())
			u->setFlagForDelete(true);
	}

	// ask for roster
	d->client->rosterRequest();
}

void PsiAccount::cs_connectionClosed()
{
	cs_error(-1);
}

void PsiAccount::cs_delayedCloseFinished()
{
	//printf("PsiAccount: [%s] connection closed\n", name().latin1());
}

void PsiAccount::cs_warning(int w)
{
	if (w == ClientStream::WarnSMReconnection)
		return;

	bool showNoTlsWarning = w == ClientStream::WarnNoTLS && d->acc.ssl == UserAccount::SSL_Yes;
	bool doCleanupStream = !d->stream || showNoTlsWarning;

	if (doCleanupStream) {
		d->client->close();
		cleanupStream();
		v_isActive = false;
		d->loginStatus = Status(Status::Offline);
		stateChanged();
		disconnected();
	}

	if (showNoTlsWarning) {
		QString message = tr("The server does not support TLS encryption.");
		QString title;
		if (d->psi->contactList()->enabledAccounts().count() > 1) {
			title = QString("%1: ").arg(name());
		}
		title += tr("Server Error");

		psi()->alertManager()->raiseMessageBox(AlertManager::ConnectionError, QMessageBox::Critical,
			title, message);
	}
	else if (!doCleanupStream) {
		Q_ASSERT(d->stream);
		d->stream->continueAfterWarning();
	}
}

void PsiAccount::getErrorInfo(int err, AdvancedConnector *conn, Stream *stream, QCATLSHandler *tlsHandler, QString *_str, bool *_reconn, bool *_badPass, bool *_disableAutoConnect, bool *_isAuthError, bool *_isTemporaryAuthFailure)
{
	QString str;
	bool reconn = false;
	bool badPass = false;
	bool disableAutoConnect = false;
	bool isAuthError = false;
	bool isTemporaryAuthFailure = false;

	if(err == -1) {
		str = tr("Disconnected");
		reconn = true;
	}
	else if(err == XMPP::ClientStream::ErrParse) {
		str = tr("XML Parsing Error");
		reconn = true;
	}
	else if(err == XMPP::ClientStream::ErrProtocol) {
		str = tr("XMPP Protocol Error");
		reconn = true;
	}
	else if(err == XMPP::ClientStream::ErrStream) {
		int x;
		QString s, detail;
		reconn = true;
		if (stream) { // Stream can apparently be gone if you disconnect in time
			x = stream->errorCondition();
			detail = stream->errorText();
		} else {
			x = XMPP::Stream::GenericStreamError;
			reconn = false;
		}

		if(x == XMPP::Stream::GenericStreamError)
			s = tr("Generic stream error");
		else if(x == XMPP::ClientStream::Conflict) {
			s = tr("Conflict (remote login replacing this one)");
			reconn = false;
			disableAutoConnect = true;
		}
		else if(x == XMPP::ClientStream::ConnectionTimeout)
			s = tr("Timed out from inactivity");
		else if(x == XMPP::ClientStream::InternalServerError)
			s = tr("Internal server error");
		else if(x == XMPP::ClientStream::InvalidXml)
			s = tr("Invalid XML");
		else if(x == XMPP::ClientStream::PolicyViolation) {
			s = tr("Policy violation");
			reconn = false;
		}
		else if(x == XMPP::ClientStream::ResourceConstraint) {
			s = tr("Server out of resources");
			reconn = false;
		}
		else if(x == XMPP::ClientStream::SystemShutdown) {
			s = tr("Server is shutting down");
		}
		str = tr("XMPP Stream Error: %1").arg(s) + "\n" + detail;
	}
	else if(err == XMPP::ClientStream::ErrConnection) {
		int x = conn ? conn->errorCode() : XMPP::AdvancedConnector::ErrStream;
		QString s;
		reconn = true;
		if(x == XMPP::AdvancedConnector::ErrConnectionRefused)
			s = tr("Unable to connect to server");
		else if(x == XMPP::AdvancedConnector::ErrHostNotFound)
			s = tr("Host not found");
		else if(x == XMPP::AdvancedConnector::ErrProxyConnect)
			s = tr("Error connecting to proxy");
		else if(x == XMPP::AdvancedConnector::ErrProxyNeg)
			s = tr("Error during proxy negotiation");
		else if(x == XMPP::AdvancedConnector::ErrProxyAuth) {
			s = tr("Proxy authentication failed");
			reconn = false;
			isAuthError = true;
		}
		else if(x == XMPP::AdvancedConnector::ErrStream)
			s = tr("Socket/stream error");
		str = tr("Connection Error: %1").arg(s);
	}
	else if(err == XMPP::ClientStream::ErrNeg) {
		QString s, detail;
		int x = stream->errorCondition();
		detail = stream->errorText();
		if(x == XMPP::ClientStream::HostGone)
			s = tr("Host no longer hosted");
		else if(x == XMPP::ClientStream::HostUnknown)
			s = tr("Host unknown");
		else if(x == XMPP::ClientStream::RemoteConnectionFailed) {
			s = tr("A required remote connection failed");
			reconn = true;
		}
		else if(x == XMPP::ClientStream::SeeOtherHost)
			s = tr("See other host: %1").arg(stream->errorText());
		else if(x == XMPP::ClientStream::UnsupportedVersion)
			s = tr("Server does not support proper XMPP version");
		str = tr("Stream Negotiation Error: %1").arg(s) + "\n" + detail;
	}
	else if(err == XMPP::ClientStream::ErrTLS) {
		int x = stream->errorCondition();
		QString s;
		if(x == XMPP::ClientStream::TLSStart)
			s = tr("Server rejected STARTTLS");
		else if(x == XMPP::ClientStream::TLSFail) {
			int t = tlsHandler->tlsError();
			if(t == QCA::TLS::ErrorHandshake)
				s = tr("TLS handshake error");
			else
				s = tr("Broken security layer (TLS)");
		}
		str = s;
	}
	else if(err == XMPP::ClientStream::ErrAuth) {
		int x = stream->errorCondition();
		QString s;
		if(x == XMPP::ClientStream::GenericAuthError) {
			s = tr("Unable to login");
			isAuthError = true;
		} else if(x == XMPP::ClientStream::NoMech) {
			s = tr("No appropriate mechanism available for given security settings (e.g. SASL library too weak, or plaintext authentication not enabled)");
			s += "\n" + stream->errorText();
		} else if(x == XMPP::ClientStream::BadProto) {
			s = tr("Bad server response");
		} else if(x == XMPP::ClientStream::BadServ) {
			s = tr("Server failed mutual authentication");
		} else if(x == XMPP::ClientStream::EncryptionRequired) {
			s = tr("Encryption required for chosen SASL mechanism");
		} else if(x == XMPP::ClientStream::InvalidAuthzid) {
			s = tr("Invalid account information");
			isAuthError = true;
		} else if(x == XMPP::ClientStream::InvalidMech) {
			s = tr("Invalid SASL mechanism");
		} else if(x == XMPP::ClientStream::InvalidRealm) {
			s = tr("Invalid realm");
		} else if(x == XMPP::ClientStream::MechTooWeak) {
			s = tr("SASL mechanism too weak for this account");
		} else if(x == XMPP::ClientStream::NotAuthorized) {
			s = tr("Wrong Password");
			badPass = true;
//			isAuthError = true;
		} else if(x == XMPP::ClientStream::TemporaryAuthFailure) {
			s = tr("Temporary auth failure");
			isAuthError = true;
			isTemporaryAuthFailure = true;
		}
		str = tr("Authentication error: %1").arg(s);
	}
	else if(err == XMPP::ClientStream::ErrSecurityLayer) {
		str = tr("Broken security layer (SASL)");
	}
	else if(err == XMPP::ClientStream::ErrSmResume) {
		str = tr("Server refused to resume the session (SM)");
		reconn = true;
	}
	else {
		str = tr("None");
		reconn = true;
	}
	//printf("str[%s], reconn=%d\n", str.latin1(), reconn);
	*_str = str;
	*_reconn = reconn;
	*_badPass = badPass;
	*_disableAutoConnect = disableAutoConnect;
	*_isAuthError = isAuthError;
	*_isTemporaryAuthFailure = isTemporaryAuthFailure;
}

void PsiAccount::cs_error(int err)
{
	QString str;
	bool reconn;
	bool badPass;
	bool isAuthError;
	bool isTemporaryAuthFailure;

	if (!isActive()) return; // all cleaned up already

	bool disableAutoConnect;
	getErrorInfo(err, d->conn, d->stream, d->tlsHandler, &str, &reconn, &badPass, &disableAutoConnect, &isAuthError, &isTemporaryAuthFailure);
	if (err != RECONNECT_TIMEOUT_ERROR) {
		d->currentConnectionError = str;
		d->currentConnectionErrorCondition = -1;
	}
	if(err == XMPP::ClientStream::ErrAuth && d && d->stream) {
		d->currentConnectionErrorCondition = d->stream->errorCondition();
	}

	d->client->close();
	cleanupStream();

	emit connectionError(d->currentConnectionError);
	//printf("Error: [%s]\n", str.latin1());

	isDisconnecting = true;

	if ( loggedIn() ) { // FIXME: is this condition okay?
		simulateRosterOffline();
	}

	presenceSent = false; // this stops the idle detector?? (FIXME)

	// Auto-Reconnect?
	if(d->acc.opt_reconn && reconn && !badPass) {
		isDisconnecting = false;
		d->startReconnect();
		return;
	}

	v_isActive = false;
	d->loginStatus = Status(Status::Offline);
	stateChanged();
	emit disconnected();
	isDisconnecting = false;

	//If a password failure, prompt for correct password
	if (badPass) {
			passwordPrompt();
			return;
	}

	QString title;
	if (d->psi->contactList()->enabledAccounts().count() > 1) {
		title = QString("%1: ").arg(name());
	}
	title += tr("Server Error");
	QString message = tr("There was an error communicating with the server.\nDetails: %1").arg(str);
	psi()->alertManager()->raiseMessageBox(AlertManager::ConnectionError, QMessageBox::Critical, title, message);
}

void PsiAccount::clearCurrentConnectionError()
{
	d->currentConnectionError = QString();
	d->currentConnectionErrorCondition = -1;
	emit connectionError(d->currentConnectionError);
}

QString PsiAccount::currentConnectionError() const
{
	return d->currentConnectionError;
}

int PsiAccount::currentConnectionErrorCondition() const
{
	return d->currentConnectionErrorCondition;
}

void PsiAccount::client_rosterRequestFinished(bool success, int, const QString &)
{
	if(success) {
		//printf("PsiAccount: [%s] roster retrieved ok.  %d entries.\n", name().latin1(), d->client->roster().count());

		// delete flagged items
		foreach(UserListItem* u, d->userList) {
			if(u->flagForDelete()) {
				//QMessageBox::information(0, "blah", QString("deleting: [%1]").arg(u->jid().full()));

				d->eventQueue->clear(u->jid());
				updateReadNext(u->jid());

				profileRemoveEntry(u->jid());
				d->userList.removeAll(u);
				delete u;
			}
		}

		d->stopReconnect();
	}
	else {
		//printf("PsiAccount: [%s] error retrieving roster: [%d, %s]\n", name().latin1(), code, str.latin1());
	}

	rosterDone = true;

	// Get stored options
	// FIXME: Should be an account-specific option
	//if (PsiOptions::instance()->getOption("options.options-storage.load").toBool())
	//	PsiOptions::instance()->load(d->client);

	// we need to have up-to-date photoHash for initial presence
	d->vcardChanged(jid());
	setStatusDirect(d->loginStatus, d->loginWithPriority);

	emit rosterRequestFinished();
}

void PsiAccount::resolveContactName()
{
	JT_VCard *j = (JT_VCard *)sender();
	if (j->success()) {
		QString nick = j->vcard().nickName();
		QString full = j->vcard().fullName();
		if (!nick.isEmpty()) {
			actionRename(j->jid(), nick);
		} else if (!full.isEmpty()) {
			actionRename(j->jid(), full);
		}
	}
}

void PsiAccount::serverFeaturesChanged()
{
	setPEPAvailable(d->serverInfoManager->hasPEP());

	if (d->serverInfoManager->canMessageCarbons()) {
		JT_MessageCarbons *j = new JT_MessageCarbons(d->client->rootTask());
		j->enable();
		j->go(true);
	}

	if (d->serverInfoManager->features().haveVCard() && !d->vcardChecked) {
		// Get the vcard
		const VCard vcard = VCardFactory::instance()->vcard(d->jid);
		if (PsiOptions::instance()->getOption("options.vcard.query-own-vcard-on-login").toBool() || vcard.isEmpty() || (vcard.nickName().isEmpty() && vcard.fullName().isEmpty()))
			VCardFactory::instance()->getVCard(d->jid, d->client->rootTask(), this, SLOT(slotCheckVCard()));
		else {
			d->nickFromVCard = true;
			// if we get here, one of these fields is non-empty
			if (!vcard.nickName().isEmpty()) {
				setNick(vcard.nickName());
			} else {
				setNick(vcard.fullName());
			}
		}
		d->vcardChecked = true;
	}
}

void PsiAccount::setPEPAvailable(bool b)
{
	if (d->pepAvailable == b)
		return;

	d->pepAvailable = b;

#ifdef PEP
	// Publish support
	if (b && !d->client->extensions().contains("ep")) {
		QStringList pepNodes;
		pepNodes += "http://jabber.org/protocol/mood";
		pepNodes += "http://jabber.org/protocol/activity";
		pepNodes += "http://jabber.org/protocol/tune";
		pepNodes += "http://jabber.org/protocol/geoloc";
		pepNodes += "urn:xmpp:avatar:data";
		pepNodes += "urn:xmpp:avatar:metadata";
		d->client->addExtension("ep",Features(pepNodes));
		setStatusActual(d->loginStatus);
	}
	else if (!b && d->client->extensions().contains("ep")) {
		d->client->removeExtension("ep");
		setStatusActual(d->loginStatus);
	}

	// Publish current tune information
	if (b && d->psi->tuneManager() && PsiOptions::instance()->getOption("options.extended-presence.tune.publish").toBool()) {
		Tune current = d->psi->tuneManager()->currentTune();
		if (!current.isNull())
			publishTune(current);
	}
#endif
}

void PsiAccount::bookmarksAvailabilityChanged()
{
	if (!d->bookmarkManager->isAvailable() ||
		!PsiOptions::instance()->getOption("options.muc.bookmarks.auto-join").toBool())
	{
		return;
	}

#ifdef GROUPCHAT
	foreach(ConferenceBookmark c, d->bookmarkManager->conferences()) {
		Jid cj = c.jid().withResource(QString());
		if (!findDialog<GCMainDlg*>(cj) && c.needJoin()) {
			auto ul = findRelevant(Jid(QString(),cj.domain()));
			if (ul.isEmpty() || !ul[0]->isTransport() || !ul[0]->resourceList().isEmpty()) { // don't join to MUCs on disconnected transports
				actionJoin(c, true, MUCJoinDlg::MucAutoJoin);
			}
		}
	}
#endif
}

void PsiAccount::incomingHttpAuthRequest(const PsiHttpAuthRequest &req)
{
	HttpAuthEvent::Ptr e(new HttpAuthEvent(req, this));
	handleEvent(e, IncomingStanza);
}

void PsiAccount::client_rosterItemAdded(const RosterItem &r)
{
	if ( r.isPush() && r.name().isEmpty() && PsiOptions::instance()->getOption("options.contactlist.resolve-nicks-on-contact-add").toBool() ) {
		// automatically resolve nickname from vCard, if newly added item doesn't have any
		VCardFactory::instance()->getVCard(r.jid(), d->client->rootTask(), this, SLOT(resolveContactName()));
	}
}

void PsiAccount::client_rosterItemUpdated(const RosterItem &r)
{
	// see if the item added is already in our local list
	UserListItem *u = d->userList.find(r.jid());
	if(u) {
		u->setFlagForDelete(false);
		u->setRosterItem(r);
	}
	else {
		// we don't have it at all, so add it
		u = new UserListItem;
		u->setRosterItem(r);
		u->setAvatarFactory(avatarFactory());
		d->userList.append(u);
	}
	u->setInList(true);

	profileUpdateEntry(*u);
}

void PsiAccount::client_rosterItemRemoved(const RosterItem &r)
{
	UserListItem *u = d->userList.find(r.jid());
	if(!u)
		return;

	u->setInList(false);
	simulateContactOffline(u);

	// if the item has messages queued, then move them to 'not in list'
	if(d->eventQueue->count(r.jid()) > 0) {
		u->setInList(false);
		profileUpdateEntry(*u);
	}
	// else remove them for good!
	else {
		profileRemoveEntry(u->jid());
		d->userList.removeAll(u);
		delete u;
	}
}

void PsiAccount::tryVerify(UserListItem *u, UserResource *ur)
{
#ifdef HAVE_PGPUTIL
	if(PGPUtil::instance().pgpAvailable())
		verifyStatus(u->jid().withResource(ur->name()), ur->status());
#endif
}

void PsiAccount::incomingVoiceCall(const Jid& j)
{
	VoiceCallDlg* vc = new VoiceCallDlg(j,voiceCaller());
	vc->show();
	vc->incoming();
}

void PsiAccount::client_resourceAvailable(const Jid &j, const Resource &r)
{
	// Notification
	enum PopupType {
		PopupOnline = 0,
		PopupStatusChange = 1
	};
	PopupType popupType = PopupOnline;

	if ( j.node().isEmpty() )
		new BlockTransportPopup(d->blockTransportPopupList, j);

	bool doSound = false;
	bool doPopup = false;
	foreach(UserListItem* u, findRelevant(j)) {
		bool doAnim = false;
		bool local = false;
		if(u->isSelf() && r.name() == d->client->resource())
			local = true;

		// add/update the resource
		QString oldStatus, oldKey;
		UserResource* rp;
		UserResourceList::Iterator rit = u->userResourceList().find(j.resource());
		bool found = (rit == u->userResourceList().end()) ? false: true;
		if(!found) {
			popupType = PopupOnline;

			UserResource ur(r);
			//ur.setSecurityEnabled(true);
			if(local) {
				ur.setClient(ApplicationInfo::name(),ApplicationInfo::version(),SystemInfo::instance()->os());
			}
			else {
				CapsManager *cm = d->client->capsManager();
				ur.setClient(cm->clientName(j),cm->clientVersion(j),cm->osVersion(j));
			}

			u->userResourceList().append(ur);
			rp = &u->userResourceList().last();

			if(notifyOnlineOk && !local) {
				doAnim = true;
				if (!u->isHidden()) {
					doSound = true;
					doPopup = true;
				}
			}
		}
		else {
			if ( !doPopup )
				popupType = PopupStatusChange;

			oldStatus = (*rit).status().status();
			oldKey = (*rit).status().keyID();
			rp = &(*rit);

			(*rit).setResource(r);

			if (!local && !u->isHidden())
				doPopup = true;
		}

		rp->setPGPVerifyStatus(-1);
		if(!rp->status().xsigned().isEmpty())
			tryVerify(u, rp);

		u->setPresenceError("");
		cpUpdate(*u, r.name(), true);

		if(doAnim && PsiOptions::instance()->getOption("options.ui.contactlist.use-status-change-animation").toBool())
			profileAnimateNick(u->jid());

#ifdef GROUPCHAT
		if (u->isTransport()) {
			foreach(ConferenceBookmark c, d->bookmarkManager->conferences()) {
				Jid cj = c.jid().withResource(QString());
				if (u->jid().domain() == cj.domain() && !findDialog<GCMainDlg*>(cj) && c.needJoin()) {
					// now join MUCs on connected transport
					actionJoin(c, true, MUCJoinDlg::MucAutoJoin);
				}
			}
		}
#endif
	}

	if(doSound)
		playSound(eOnline);

	// Do the popup test earlier (to avoid needless JID lookups)
	if ((popupType == PopupOnline && PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.online").toBool()) || (popupType == PopupStatusChange && PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.other-changes").toBool())) {
		if(notifyOnlineOk && doPopup && !d->blockTransportPopupList->find(j, popupType == PopupOnline) && !d->noPopup(IncomingStanza)) {
			UserListItem *u = findFirstRelevant(j);
			PopupManager::PopupType pt = PopupManager::AlertNone;
			if ( popupType == PopupOnline )
				pt = PopupManager::AlertOnline;
			else if ( popupType == PopupStatusChange )
				pt = PopupManager::AlertStatusChange;

			if ((popupType == PopupOnline && PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.online").toBool()) || (popupType == PopupStatusChange && PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.other-changes").toBool())) {
				psi()->popupManager()->doPopup(this, pt, j, r, u, PsiEvent::Ptr(), false);
			}
		}
		else if ( !notifyOnlineOk ) {
			d->userCounter++;
		}
	}

	// Update entity capabilities.
	// This has to happen after the userlist item has been created.
	if (r.status().caps().isValid()) {
		CapsManager *cm = client()->capsManager();

		// Update the client version
		foreach(UserListItem* u, findRelevant(j)) {
			UserResourceList::Iterator rit = u->userResourceList().find(j.resource());
			if (rit != u->userResourceList().end()) {
				(*rit).setClient(cm->clientName(j),cm->clientVersion(j),cm->osVersion(j)); // FIXME it seems it's impossible if not in cache
				cpUpdate(*u,(*rit).name());
			}
		}
	}
}

void PsiAccount::client_resourceUnavailable(const Jid &j, const Resource &r)
{
	bool doSound = false;
	bool doPopup = false;

	if ( j.node().isEmpty() )
		new BlockTransportPopup(d->blockTransportPopupList, j);

	foreach(UserListItem* u, findRelevant(j)) {
		bool local = false;
		if(u->isSelf() && r.name() == d->client->resource())
			local = true;

		// remove resource
		UserResourceList::Iterator rit = u->userResourceList().find(j.resource());
		bool found = (rit == u->userResourceList().end()) ? false: true;
		if(found) {
			u->setLastUnavailableStatus(r.status());
			//u->userResourceList().removeAll(*rit);//we cant use it since operator== is used for other purpose
			QMutableListIterator<UserResource> i(u->userResourceList());
			while (i.hasNext()) {
				if (i.next().name() == j.resource()) {
					i.remove();
				}
			}

			if(!u->isAvailable())
				u->setLastAvailable(QDateTime::currentDateTime());

			if(!u->isAvailable() || u->isSelf()) {
				// don't sound for our own resource
				if(!isDisconnecting && !local && !u->isHidden()) {
					doSound = true;
					doPopup = true;
				}
			}
		} else {
			// if we get here, then we've received unavailable
			//   presence for a contact that is already considered
			//   unavailable
			u->setLastUnavailableStatus(r.status());

			if (!u->isAvailable()) {
				QDateTime ts = r.status().timeStamp();
				if (ts.isValid()) {
					u->setLastAvailable(ts);
				}
			}

			// no sounds/popups in this case
		}

		u->setPresenceError("");
		cpUpdate(*u, r.name(), true);
	}
	if(doSound)
		playSound(eOffline);

	// Do the popup test earlier (to avoid needless JID lookups)
	if(PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.offline").toBool() &&
	   doPopup && !d->blockTransportPopupList->find(j) && !d->noPopup(IncomingStanza)) {
		UserListItem *u = findFirstRelevant(j);

		if (PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.offline").toBool()) {
			psi()->popupManager()->doPopup(this, PopupManager::AlertOffline, j, r, u, PsiEvent::Ptr(), false);
		}
	}
}

void PsiAccount::client_presenceError(const Jid &j, int, const QString &str)
{
	foreach(UserListItem *u, findRelevant(j)) {
		simulateContactOffline(u);
		u->setPresenceError(str);
		cpUpdate(*u, j.resource(), false);
	}
}

void PsiAccount::client_messageReceived(const Message &m)
{
	//check if it's a server message without a from, and set the from appropriately
	Message _m(m);
	if (_m.from().isEmpty())
	{
		_m.setFrom(jid().domain());
	}

	// if the sender is already in the queue, then queue this message also
	foreach(Message* mi, d->messageQueue) {
		if(mi->from().compare(_m.from())) {
			Message *m = new Message(_m);
			d->messageQueue.append(m);
			return;
		}
	}

	// check to see if message was forwarded from another resource
	if (jid().compare(_m.from(),false)) {
		AddressList oFrom = _m.findAddresses(Address::OriginalFrom);
		AddressList oTo = _m.findAddresses(Address::OriginalTo);
		if ((oFrom.count() > 0) && (oTo.count() > 0)) {
			// might want to store current values in MessageEvent object
			// replace out the from and to addresses with the original addresses
			_m.setFrom(oFrom[0].jid());
			_m.setTo(oTo[0].jid());
		}
	}

#ifdef HAVE_PGPUTIL
	// encrypted message?
	if(PGPUtil::instance().pgpAvailable() && !_m.xencrypted().isEmpty()) {
		Message *m = new Message(_m);
		d->messageQueue.append(m);
		processMessageQueue();
		return;
	}
#endif

	processIncomingMessage(_m);
}

#ifdef WHITEBOARDING
void PsiAccount::wbRequest(const Jid &j, int id)
{
	SxeEvent::Ptr se(new SxeEvent(id, this));
	XMPP::Message m;
	m.setSubject(tr("Whiteboard invitation"));
	m.setFrom(j);
	se->setFrom(j);
	se->setMessage(m);
	handleEvent(se, IncomingStanza);
}
#endif

/**
 * Handles the passed Message \param m. Also message's type could be modified
 * here, if certain options are set.
 *
 * TODO: Generalize option.incomingAs and EventDlg::messagingEnabled()
 * processing in one common function that could be applied to the messages
 * loaded from the event queue, and settings were changed prior to that.
 */
void PsiAccount::processIncomingMessage(const Message &_m)
{
	// skip empty messages, but not if the message contains a data form
	if(_m.body().isEmpty() && _m.urlList().isEmpty() && _m.invite().isEmpty() &&
	        !_m.containsEvents() && _m.chatState() == StateNone && _m.subject().isNull() &&
	        _m.rosterExchangeItems().isEmpty() && _m.mucInvites().isEmpty() &&
	        _m.getForm().fields().empty() && _m.messageReceipt() == ReceiptNone &&
	        _m.getMUCStatuses().isEmpty())
		return;

	// skip headlines?
	if(_m.type() == "headline" && PsiOptions::instance()->getOption("options.messages.ignore-headlines").toBool())
		return;

	if (_m.getForm().registrarType() == "urn:xmpp:captcha") {
		CaptchaChallenge challenge(_m);
		if (challenge.isValid()) {
			QWidget *pw = 0;
			if (_m.from().resource().isEmpty()) {
				pw = findDialog<GCMainDlg*>(_m.from());
				if(!pw) {
					pw = findDialog<MUCJoinDlg*>(_m.from());
				}
			}
			if (!pw) {
				pw = findChatDialog(_m.from());
			}
			CaptchaDlg *dlg = new CaptchaDlg(pw, challenge, this);
			dlg->show();
			bringToFront(dlg);
			return;
		}
	}

#ifdef GROUPCHAT
	if(_m.type() == "groupchat") {
		MessageEvent::Ptr me(new MessageEvent(_m, this));
		me->setOriginLocal(false);
		handleEvent(me, IncomingStanza);
		return;
	}
#endif

	UserListItem *u = findFirstRelevant(_m.from());
	if(u) {
		if(_m.type() == "chat") u->setLastMessageType(1);
		else u->setLastMessageType(0);
	}

	Message m = _m;

	// smartchat: try to match up the incoming event to an existing chat
	// (prior to 0.9, m.from() always contained a resource)
	Jid j;
	ChatDlg *c;
	QList<UserListItem*> ul = findRelevant(m.from());

	// ignore events from non-roster JIDs?
	if (ul.isEmpty() && PsiOptions::instance()->getOption("options.messages.ignore-non-roster-contacts").toBool())
	{
		if (PsiOptions::instance()->getOption("options.messages.exclude-muc-from-ignore").toBool())
		{
#ifdef GROUPCHAT
			GCMainDlg *w = findDialog<GCMainDlg*>(Jid(_m.from().bare()));
			if(!w)
			{
				return;
			}
#endif
		}
		else
		{
			return;
		}
	}

	if(ul.isEmpty())
		j = m.from().bare();
	else
		j = ul.first()->jid();

	/*c = findChatDialog(j);
	if(!c)
		c = findChatDialog(m.from().full());*/

	if(m.type() == "error") {
		Stanza::Error err = m.error();
		QPair<QString, QString> desc = err.description();
		QString msg = desc.first + ".\n" + desc.second;
		if (!err.text.isEmpty())
			msg += "\n" + err.text;

		m.setBody(msg + "\n------\n" + m.body());
	}
	else
	{
		// only toggle if not an invite or body is not empty
		if(_m.invite().isEmpty() && !_m.body().isEmpty() && _m.mucInvites().isEmpty() && _m.rosterExchangeItems().isEmpty())
			toggleSecurity(_m.from(), _m.wasEncrypted());

		// Roster item exchange
		if (!_m.rosterExchangeItems().isEmpty()) {
			RosterExchangeEvent::Ptr ree(new RosterExchangeEvent(j,_m.rosterExchangeItems(), _m.body(), this));
			handleEvent(ree, IncomingStanza);
			return;
		}

		// change the type?
		if (m.type() != "headline" && m.invite().isEmpty() && m.mucInvites().isEmpty()) {
			const QString type = PsiOptions::instance()->getOption("options.messages.force-incoming-message-type").toString();
			if (type == "message")
				m.setType("");
			else if (type == "chat")
				m.setType("chat");
			else if (type == "current-open") {
				c = NULL;
				foreach (ChatDlg *cl, findChatDialogs(m.from(), false)) {
					if (cl->autoSelectContact() || cl->jid().resource().isEmpty() || m.from().resource() == cl->jid().resource()) {
						c = cl;
						break;
					}
				}
				if (c != NULL && !c->isHidden())
					m.setType("chat");
				else
					m.setType("");
			}
		}

		// urls or subject on a chat message?  convert back to regular message
		//if(m.type() == "chat" && (!m.urlList().isEmpty() || !m.subject().isEmpty()))
		//	m.setType("");

		if( m.messageReceipt() == ReceiptRequest && !m.id().isEmpty() &&
			PsiOptions::instance()->getOption("options.ui.notifications.send-receipts").toBool()) {
			UserListItem *u;
			if(j.compare(d->self.jid(), false) || groupchats().contains(j.bare()) || (!d->loginStatus.isInvisible() && (u = d->userList.find(j)) && (u->subscription().type() == Subscription::To || u->subscription().type() == Subscription::Both))) {
				Message tm(m.from());
				tm.setMessageReceiptId(m.id());
				tm.setMessageReceipt(ReceiptReceived);
				dj_sendMessage(tm, false);
			}
		}
	}

	MessageEvent::Ptr me(new MessageEvent(m, this));
	me->setOriginLocal(false);
	handleEvent(me, IncomingStanza);
}

void PsiAccount::client_subscription(const Jid &j, const QString &str, const QString& nick)
{
	// if they remove our subscription, then we lost presence
	if(str == "unsubscribed") {
		UserListItem *u = d->userList.find(j);
		if(u)
			simulateContactOffline(u);
	}

	AuthEvent::Ptr ae(new AuthEvent(j, str, this));
	ae->setTimeStamp(QDateTime::currentDateTime());
	ae->setNick(nick);
	handleEvent(ae, IncomingStanza);
}

void PsiAccount::client_debugText(const QString &txt)
{
	Q_UNUSED(txt);
	//printf("%s\n", qPrintable(txt));
	//fflush(stdout);
}

#ifdef GOOGLE_FT
void PsiAccount::incomingGoogleFileTransfer(GoogleFileTransfer* ft)
{
	if (QMessageBox::information(0, tr("Incoming file"), QString(tr("Do you want to accept %1 (%2 kb) from %3?")).arg(ft->fileName()).arg((float) ft->fileSize() / 1000).arg(ft->peer().full()), QMessageBox::Yes,QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton) == QMessageBox::Yes) {
		GoogleFileTransferProgressDialog* d = new GoogleFileTransferProgressDialog(ft);
		d->show();
		ft->accept();
	}
	else
		ft->reject();
}
#endif

void PsiAccount::client_incomingFileTransfer()
{
#ifdef FILETRANSFER
	FileTransfer *ft = d->client->fileTransferManager()->takeIncoming();
	if(!ft)
		return;

	/*printf("psiaccount: incoming file transfer:\n");
	printf("  From: [%s]\n", ft->peer().full().latin1());
	printf("  Name: [%s]\n", ft->fileName().latin1());
	printf("  Size: %d bytes\n", ft->fileSize());*/

	FileEvent::Ptr fe(new FileEvent(ft->peer().full(), ft, this));
	fe->setTimeStamp(QDateTime::currentDateTime());
	handleEvent(fe, IncomingStanza);
#endif
}

void PsiAccount::reconnect()
{
	if(doReconnect && !isConnected()) {
		//printf("PsiAccount: [%s] reconnecting...\n", name().latin1());
		emit reconnecting();
		v_isActive = false;
		d->startReconnectTimeout();
		login();
	}
}

Status PsiAccount::status() const
{
	return d->loginStatus;
}

Status PsiAccount::loggedOutStatus()
{
	return Status(Status::Offline, tr("Logged out"), 0);
}

void PsiAccount::setStatus(const Status &_s,  bool withPriority, bool isManualStatus)
{
	Status s = _s;
	if (!withPriority)
		s.setPriority(d->acc.defaultPriority(s));

	if (isManualStatus) {
		d->setManualStatus(s);

		if (s.isAvailable()) {
			//Save only non-offline status for reconnect use
			d->acc.lastStatus = s;
			d->acc.lastStatusWithPriority = withPriority;
		}
	}

	// Block all transports' contacts' status change popups from popping
	{
		Roster::ConstIterator rit = d->acc.roster.begin();
		for ( ; rit != d->acc.roster.end(); ++rit) {
			const RosterItem &i = *rit;
			if ( i.jid().node().isEmpty() /*&& i.jid().resource() == "registered"*/ ) // it is very likely then, that it's transport
				new BlockTransportPopup(d->blockTransportPopupList, i.jid());
		}
	}

	d->loginStatus = s;
	d->loginWithPriority = withPriority;

	if(s.isAvailable()) {
		// if we are in the process of disconnecting, then do nothing.
		//   the desired status will be noted in loginStatus for
		//   reconnect
		if(isDisconnecting)
			return;

		// if client is not active then attempt to login
		if(!isActive()) {
			if (!d->acc.opt_pass && !d->acc.storeSaltedHashedPassword) {
				passwordPrompt();
			}
			else
				login();
		}

		// change status
		else {
			if (!isConnected()) {
				disconnect(); // a hack to reset connecting stream.
				login();
			}
			if(rosterDone) {
				setStatusDirect(s, withPriority);
			}

			if(s.isInvisible()) {//&&Pass invis to transports KEVIN
				//this is a nasty hack to let the transports know we're invisible, since they get an offline packet when we go invisible
				foreach(UserListItem* u, d->userList) {
					if(u->isTransport()) {
						JT_Presence *j = new JT_Presence(d->client->rootTask());
						j->pres(u->jid(), s);
						j->go(true);
					}
				}
			}
		}
	}
	else {
		if(isActive())
			logout(false, s);
	}
}

void PsiAccount::showStatusDialog(const QString& presetName)
{
	StatusPreset preset;
	preset.fromOptions(PsiOptions::instance(), presetName);
	Status status(preset.status(), preset.message(), preset.priority().hasValue() ? preset.priority().value() : this->status().priority());
	StatusSetDlg *w = new StatusSetDlg(this, status, preset.priority().hasValue());
	connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)), SLOT(setStatus(const XMPP::Status &, bool, bool)));
	w->show();
}

void PsiAccount::passwordReady(QString password) {
	d->acc.pass = password;
	login();
}

int PsiAccount::defaultPriority(const XMPP::Status &s)
{
	return d->acc.defaultPriority(s);
}

void PsiAccount::passwordPrompt()
{
	PassDialog dialog(d->jid.full());
	dialog.setSavePassword(d->acc.opt_pass);

	if(dialog.exec() == QDialog::Accepted && !dialog.password().isEmpty()) {
		d->acc.pass = dialog.password();
		d->acc.opt_pass = dialog.savePassword();
		login();
	}
}


void PsiAccount::setStatusDirect(const Status &_s, bool withPriority)
{
	Status s = _s;
	if (!withPriority)
		s.setPriority(defaultPriority(s));

	if (d->reconnectingOnce) {
		//If we reconnect once and got here, user chosen status and we must stop reconnecting
		d->reconnectingOnce = false;
		QObject::disconnect(this, SIGNAL(disconnected()), this, SLOT(reconnectOncePhase2()));
	}

	// using pgp?
	if(!d->cur_pgpSecretKey.isNull()) {
		d->loginStatus = s;

		// sign presence
		trySignPresence();
	}
	else {
		/*if(d->psi->pgp() && !d->cur_pgpSecretKeyID.isEmpty())
			s.setKeyID(d->cur_pgpSecretKeyID);
		else
			s.setKeyID("");*/

		// send presence normally
		setStatusActual(s);
	}
}

void PsiAccount::setStatusActual(const Status &_s)
{
	Status s = _s;

	if (!presenceSent) {
		simulateRosterOffline();
	}

	// Add vcard photo hash if available
	if(!d->photoHash.isEmpty()) {
		s.setPhotoHash(d->photoHash);
	}

	// Set the status
	d->loginStatus = s;
	d->client->setPresence(s);
	if(presenceSent) {
		stateChanged();
	}
	else {
		presenceSent = true;
		stateChanged();
		sentInitialPresence();

		clearCurrentConnectionError();
	}
}

bool PsiAccount::noPopup() const
{
	return d->noPopup(UserAction);
}

void PsiAccount::sentInitialPresence()
{
	QTimer::singleShot(15000, this, SLOT(enableNotifyOnline()));
}

void PsiAccount::capsChanged(const Jid& j)
{
	if (!loggedIn())
		return;

	CapsManager *cm = d->client->capsManager();
	QString name = cm->clientName(j);
	QString version = (name.isEmpty() ? QString() : cm->clientVersion(j));
	QString os;

	if (!name.isEmpty()) {
		version = cm->clientVersion(j);
		os = cm->osVersion(j);
	}

	foreach(UserListItem *u, findRelevant(j)) {
		UserResourceList::Iterator rit = u->userResourceList().find(j.resource());
		bool found = (rit == u->userResourceList().end()) ? false: true;
		if(!found)
			continue;
		(*rit).setClient(name,version,os);
		cpUpdate(*u);
	}
}

void PsiAccount::tuneStopped()
{
	if (loggedIn()) {
		publishTune(Tune());
	}
}

void PsiAccount::tunePlaying(const Tune& tune)
{
	if (loggedIn() && PsiOptions::instance()->getOption("options.extended-presence.tune.publish").toBool()) {
		publishTune(tune);
	}
}

void PsiAccount::publishTune(const Tune& tune)
{
	QDomDocument* doc = d->client->rootTask()->doc();
	QDomElement t = doc->createElement("tune");
	t.setAttribute("xmlns", "http://jabber.org/protocol/tune");
	if (!tune.artist().isEmpty())
		t.appendChild(textTag(doc, "artist", tune.artist()));
	if (!tune.name().isEmpty())
		t.appendChild(textTag(doc, "title", tune.name()));
	if (!tune.album().isEmpty())
		t.appendChild(textTag(doc, "source", tune.album()));
	if (!tune.track().isEmpty())
		t.appendChild(textTag(doc, "track", tune.track()));
	if (tune.time() != 0)
		t.appendChild(textTag(doc, "length", QString::number(tune.time())));

	d->lastTune = tune;
	d->pepManager->publish("http://jabber.org/protocol/tune",PubSubItem("current",t));
}

void PsiAccount::setAutoAwayStatus(AutoAway status)
{
	d->setAutoAway(status);
}

void PsiAccount::playSound(PsiAccount::SoundType _onevent)
{
	int onevent = static_cast<int>(_onevent);
	if (onevent < 0) {
		return;
	}

	QString str;
	switch (onevent) {
	case eMessage:
		str = PsiOptions::instance()->getOption("options.ui.notifications.sounds.incoming-message").toString();
		break;
	case eChat1:
		str = PsiOptions::instance()->getOption("options.ui.notifications.sounds.new-chat").toString();
		break;
	case eChat2:
		str = PsiOptions::instance()->getOption("options.ui.notifications.sounds.chat-message").toString();
		break;
	case eGroupChat:
		str = PsiOptions::instance()->getOption("options.ui.notifications.sounds.groupchat-message").toString();
		break;
	case eHeadline:
		str = PsiOptions::instance()->getOption("options.ui.notifications.sounds.incoming-headline").toString();
		break;
	case eSystem:
		str = PsiOptions::instance()->getOption("options.ui.notifications.sounds.system-message").toString();
		break;
	case eOnline:
		str = PsiOptions::instance()->getOption("options.ui.notifications.sounds.contact-online").toString();
		break;
	case eOffline:
		str = PsiOptions::instance()->getOption("options.ui.notifications.sounds.contact-offline").toString();
		break;
	case eSend:
		str = PsiOptions::instance()->getOption("options.ui.notifications.sounds.outgoing-chat").toString();
		break;
	case eIncomingFT:
		str = PsiOptions::instance()->getOption("options.ui.notifications.sounds.incoming-file-transfer").toString();
		break;
	case eFTComplete:
		str = PsiOptions::instance()->getOption("options.ui.notifications.sounds.completed-file-transfer").toString();
		break;
	default:
		Q_ASSERT(false);
	}

	if(str.isEmpty())
		return;

	XMPP::Status::Type s = XMPP::Status::Offline;
	if (loggedIn())
		s = d->lastManualStatus().type();

	if (s == XMPP::Status::DND)
		return;

	// no away sounds?
	if(PsiOptions::instance()->getOption("options.ui.notifications.sounds.silent-while-away").toBool() && (s == STATUS_AWAY || s == STATUS_XA))
		return;

	d->psi->playSound(str);
}

QString PsiAccount::localHostName()
{
	QString hostname = QHostInfo::localHostName();
	int i = hostname.indexOf('.');
	if (i != -1)
		return hostname.left(hostname.indexOf('.'));
	else
		return hostname;
}

bool PsiAccount::validRosterExchangeItem(const RosterExchangeItem& item)
{
	if (item.action() == RosterExchangeItem::Add) {
		return (d->client->roster().find(item.jid(),true) == d->client->roster().end());
	}
	else if (item.action() == RosterExchangeItem::Delete) {
		LiveRoster::ConstIterator i = d->client->roster().find(item.jid(),true);
		if (i == d->client->roster().end())
			return false;

		foreach(QString group, item.groups()) {
			if (!(*i).groups().contains(group))
				return false;
		}
		return true;
	}
	else if (item.action() == RosterExchangeItem::Modify) {
		// TODO
		return false;
	}
	return false;
}

ChatDlg* PsiAccount::findChatDialog(const Jid& jid, bool compareResource) const
{
	return findDialog<ChatDlg*>(jid, compareResource);
}

ChatDlg* PsiAccount::findChatDialogEx(const Jid& jid, bool ignoreResource) const
{
	ChatDlg* cm1 = NULL;
	ChatDlg* cm2 = NULL;
	foreach (ChatDlg *cl, findChatDialogs(jid, false)) {
		if (cl->autoSelectContact() || ignoreResource)
			return cl;
		if (!cm1 && jid.resource() == cl->jid().resource()) {
				cm1 = cl;
				continue;
		}
		if (!cm2 && cl->jid().resource().isEmpty())
			cm2 = cl;
	}
	if (cm1)
		return cm1;
	return cm2;
}

QList<ChatDlg*> PsiAccount::findChatDialogs(const Jid& jid, bool compareResource) const
{
	return findDialogs<ChatDlg*>(jid, compareResource);
}

QList<PsiContact*> PsiAccount::activeContacts() const
{
	QList<PsiContact*> ret;
	foreach(PsiContact *pc, contactList()) {
		if(pc->isActiveContact()) {
			ret.append(pc);
		}
	}
	return ret;
}

QWidget* PsiAccount::findDialog(const QMetaObject& mo, const Jid& jid, bool compareResource) const
{
	return d->findDialog(mo, jid, compareResource);
}

void PsiAccount::findDialogs(const QMetaObject& mo, const Jid& jid, bool compareResource, QList<void*>* list) const
{
	d->findDialogs(mo, jid, compareResource, list);
}

void PsiAccount::findDialogs(const QMetaObject& mo, QList<void*>* list) const
{
	d->findDialogs(mo, list);
}

void PsiAccount::dialogRegister(QWidget *w, const Jid &j)
{
	d->dialogRegister(w, j);
}

void PsiAccount::dialogUnregister(QWidget *w)
{
	d->dialogUnregister(w);
}

void PsiAccount::deleteAllDialogs()
{
	delete d->xmlConsole;
	d->xmlConsole = 0;
	d->deleteDialogList();
}

bool PsiAccount::checkConnected(QWidget *par)
{
	if (!isAvailable()) {
		QMessageBox::information(par, CAP(tr("Error")),
								 tr("You must be connected to the server in order to do this."));
		return false;
	}

	return true;
}

void PsiAccount::modify()
{
	d->doModify();
}

void PsiAccount::reconfigureFTManager()
{
#ifdef FILETRANSFER
	d->client->fileTransferManager()->setDisabled(S5BManager::ns(), d->acc.ibbOnly); // TODO more stream types?
#endif
}

void PsiAccount::changeVCard()
{
	actionInfo(d->jid,false);
}

void PsiAccount::changePW()
{
	if (!checkConnected()) {
		return;
	}

	ChangePasswordDlg *w = findDialog<ChangePasswordDlg*>();
	if (w) {
		bringToFront(w);
	}
	else {
		w = new ChangePasswordDlg(this);
		w->show();
	}
}

void PsiAccount::showXmlConsole()
{
	if(!d->xmlConsole) {
		d->xmlConsole = new XmlConsole(this);
	}
	bringToFront(d->xmlConsole);
}

void PsiAccount::openAddUserDlg()
{
	openAddUserDlg(QString(), QString(), QString());
}

void PsiAccount::openAddUserDlg(const Jid &jid, const QString &nick, const QString &group)
{
	QStringList gl, services, names;
	UserListIt it(d->userList);
	foreach(UserListItem* u, d->userList) {
		if(u->isTransport()) {
			services += u->jid().full();
			names += JIDUtil::nickOrJid(u->name(), u->jid().full());
		}
		foreach(QString group, u->groups()) {
			if(!gl.contains(group))
				gl.append(group);
		}
	}

	AddUserDlg* w;
	if (jid.isEmpty()) {
		w = new AddUserDlg(services, names, gl, this);
	} else {
		w = new AddUserDlg(jid, nick, group, gl, this);
	}
	connect(w, SIGNAL(add(const XMPP::Jid &, const QString &, const QStringList &, bool)), SLOT(dj_add(const XMPP::Jid &, const QString &, const QStringList &, bool)));
	w->show();
}

void PsiAccount::doDisco()
{
	actionDisco(d->jid.domain(), "");
}

void PsiAccount::actionDisco(const Jid &j, const QString &node)
{
	DiscoDlg *w = new DiscoDlg(this, j, node);
	connect(w, SIGNAL(featureActivated(QString, Jid, QString)), SLOT(featureActivated(QString, Jid, QString)));
	w->show();
}

void PsiAccount::featureActivated(QString feature, Jid jid, QString node)
{
	Features f(feature);

	if ( f.canRegister() )
		actionRegister(jid);
	else if ( f.canSearch() )
		actionSearch(jid);
	else if ( f.canGroupchat() )
		actionJoin(jid);
	else if ( f.canCommand() )
		actionExecuteCommand(jid, node);
	else if ( f.canDisco() )
		actionDisco(jid, node);
	else if ( f.isGateway() ) {
		if(QMessageBox::question(0, tr("Unregister from %1").arg(jid.bare()),
					 tr("Are you sure?"),
					 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
			actionUnregister(jid);
	}
	else if ( f.haveVCard() )
		actionInfo(jid);
	else if ( f.id() == Features::FID_Add ) {
		QStringList sl;
		dj_add(jid, QString(), sl, true);
	}
	else if ( f.hasVersion() ) {
		actionQueryVersion(jid);
	}
}

void PsiAccount::actionSendStatus(const Jid &jid)
{
	StatusSetDlg *w = new StatusSetDlg(psi(), makeLastStatus(status().type()), lastPriorityNotEmpty());
	w->setJid(jid);
	connect(w, SIGNAL(setJid(const Jid &, const Status &)), SLOT(setStatusFromDialog(const Jid &, const Status &)));
	w->show();
}

void PsiAccount::actionManageBookmarks()
{
	BookmarkManageDlg *dlg = findDialog<BookmarkManageDlg*>();
	if(dlg) {
		bringToFront(dlg);
	} else {
		dlg = new BookmarkManageDlg(this);
		dlg->show();
	}
}

void PsiAccount::actionJoin(const Jid& mucJid, const QString& password)
{
	actionJoin(ConferenceBookmark(QString(), mucJid, ConferenceBookmark::Never, QString(), password),
			   false);
}

void PsiAccount::actionJoin(const ConferenceBookmark& bookmark, bool connectImmediately, MUCJoinDlg::MucJoinReason reason)
{
#ifdef GROUPCHAT
	MUCJoinDlg* w = new MUCJoinDlg(psi(), this);

	w->setJid(bookmark.jid());
	w->setNick(bookmark.nick().isEmpty() ? JIDUtil::nickOrJid(this->nick(), d->jid.node()) : bookmark.nick());
	w->setPassword(bookmark.password());

	if(reason != MUCJoinDlg::MucAutoJoin
	   || !PsiOptions::instance()->getOption("options.ui.muc.hide-on-autojoin").toBool())
		w->show();
	if (connectImmediately) {
		w->doJoin(reason);
	}
#else
	Q_UNUSED(bookmark);
	Q_UNUSED(connectImmediately);
#endif
}

void PsiAccount::stateChanged()
{
	if(loggedIn()) {
		d->setState(makeSTATUS(status()));
	}
	else {
		if(isActive()) {
			d->setState(-1);
		}
		else {
			d->setState(STATUS_OFFLINE);
		}
	}

	emit updatedActivity();
}

void PsiAccount::simulateContactOffline(const XMPP::Jid& contact)
{
	foreach(UserListItem* u, findRelevant(contact)) {
		simulateContactOffline(u);
	}
}

void PsiAccount::simulateContactOffline(UserListItem *u)
{
	UserResourceList rl = u->userResourceList();
	u->setPresenceError("");
	if(!rl.isEmpty()) {
		for(UserResourceList::ConstIterator rit = rl.begin(); rit != rl.end(); ++rit) {
			const UserResource &r = *rit;
			Jid j = u->jid();
			if(u->jid().resource().isEmpty()) {
				j = j.withResource(r.name());
			}
			client_resourceUnavailable(j, r);
		}
	}
	u->setLastUnavailableStatus(makeStatus(STATUS_OFFLINE,""));
	u->setLastAvailable(QDateTime());
	cpUpdate(*u);
}

void PsiAccount::simulateRosterOffline()
{
	emit beginBulkContactUpdate();

	notifyOnlineOk = false;
	foreach(UserListItem* u, d->userList)
		simulateContactOffline(u);

	// self
	{
		UserListItem *u = &d->self;
		UserResourceList rl = u->userResourceList();
		for(UserResourceList::ConstIterator rit = rl.begin(); rit != rl.end(); ++rit) {
			Jid j = u->jid();
			if(u->jid().resource().isEmpty()) {
				j = j.withResource((*rit).name());
			}
			u->setPresenceError("");
			client_resourceUnavailable(j, *rit);
		}
	}

	while (!d->gcbank.isEmpty())
		delete d->gcbank.takeFirst();

	emit endBulkContactUpdate();
}

bool PsiAccount::notifyOnline() const
{
	return notifyOnlineOk;
}

void PsiAccount::enableNotifyOnline()
{
	if ( d->userCounter > 1 ) {
		QTimer::singleShot(15000, this, SLOT(enableNotifyOnline()));
		d->userCounter = 0;
	}
	else
		notifyOnlineOk = true;
}

void PsiAccount::itemRetracted(const Jid& j, const QString& n, const PubSubRetraction& item)
{
	Q_UNUSED(item);
	// User Tune
	if (n == "http://jabber.org/protocol/tune") {
		// Parse tune
		foreach(UserListItem* u, findRelevant(j)) {
			// FIXME: try to find the right resource using JEP-33 'replyto'
			//UserResourceList::Iterator rit = u->userResourceList().find(<resource>);
			//bool found = (rit == u->userResourceList().end()) ? false: true;
			//if(found)
			//	(*rit).setTune(tune);
			u->setTune(QString());
			cpUpdate(*u);
		}
	}
	else if (n == "http://jabber.org/protocol/mood") {
		foreach(UserListItem* u, findRelevant(j)) {
			u->setMood(Mood());
			cpUpdate(*u);
		}
	}
	else if (n == "http://jabber.org/protocol/activity") {
		foreach(UserListItem* u, findRelevant(j)) {
			u->setActivity(Activity());
			cpUpdate(*u);
		}
	}
	else if (n == "http://jabber.org/protocol/geoloc") {
		// FIXME: try to find the right resource using JEP-33 'replyto'
		// see tune case above
		foreach(UserListItem* u, findRelevant(j)) {
			u->setGeoLocation(GeoLocation());
			cpUpdate(*u);
		}
	}
}

void PsiAccount::itemPublished(const Jid& j, const QString& n, const PubSubItem& item)
{
	// User Tune
	if (n == "http://jabber.org/protocol/tune") {
		// Parse tune
		QDomElement element = item.payload();
		QDomElement e;
		QString tune;

		e = element.firstChildElement("artist");
		if (!e.isNull())
			tune += e.text() + " - ";

		e = element.firstChildElement("title");
		if (!e.isNull())
			tune += e.text();

		foreach(UserListItem* u, findRelevant(j)) {
			// FIXME: try to find the right resource using JEP-33 'replyto'
			//UserResourceList::Iterator rit = u->userResourceList().find(<resource>);
			//bool found = (rit == u->userResourceList().end()) ? false: true;
			//if(found)
			//	(*rit).setTune(tune);
			u->setTune(tune);
			cpUpdate(*u);
		}
	}
	else if (n == "http://jabber.org/protocol/mood") {
		Mood mood(item.payload());
		foreach(UserListItem* u, findRelevant(j)) {
			u->setMood(mood);
			cpUpdate(*u);
		}
	}
	else if (n == "http://jabber.org/protocol/activity") {
		Activity activity(item.payload());
		foreach(UserListItem* u, findRelevant(j)) {
			u->setActivity(activity);
			cpUpdate(*u);
		}
	}
	else if (n == "http://jabber.org/protocol/geoloc") {
		// FIXME: try to find the right resource using JEP-33 'replyto'
		// see tune case above
		GeoLocation geoloc(item.payload());
		foreach(UserListItem* u, findRelevant(j)) {
			u->setGeoLocation(geoloc);
			cpUpdate(*u);
		}
	}
}

Jid PsiAccount::realJid(const Jid &j) const
{
	GCContact *c = findGCContact(j);
	if (c) {
		if (c->status.hasMUCItem()) {
			return c->status.mucItem().jid();
		} else {
			return Jid();
		}
	} else {
		return j;
	}
}

QList<UserListItem*> PsiAccount::findRelevant(const Jid &j) const
{
	QList<UserListItem*> list;

	// self?
	if(j.compare(d->self.jid(), false))
		list.append(&d->self);
	else {
		foreach(UserListItem* u, d->userList) {
			if(!u->jid().compare(j, false))
				continue;

			if(!u->jid().resource().isEmpty()) {
				if(u->jid().resource() != j.resource())
					continue;
			} else {
				// skip status changes from muc participants
				// if the MUC somehow got into userList.
				if (!j.resource().isEmpty() && d->groupchats.contains(j.bare())) continue;
			}
			list.append(u);
		}
	}

	return list;
}

UserListItem *PsiAccount::findFirstRelevant(const Jid &j) const
{
	QList<UserListItem*> list = findRelevant(j);
	if(list.isEmpty())
		return 0;
	else
		return list.first();
}

PsiContact* PsiAccount::selfContact() const
{
	return d->selfContact;
}

const QList<PsiContact*>& PsiAccount::contactList() const
{
	return d->contacts;
}

int PsiAccount::onlineContactsCount() const
{
	return d->onlineContactsCount;
}

PsiContact* PsiAccount::findContact(const Jid& jid) const
{
	return d->findContact(jid);
}

UserListItem *PsiAccount::find(const Jid &j) const
{
	UserListItem *u;
	if(j.compare(d->self.jid()))
		u = &d->self;
	else
		u = d->userList.find(j);

	return u;
}

void PsiAccount::cpUpdate(const UserListItem &u, const QString &rname, bool fromPresence)
{
	PsiEvent::Ptr e = d->eventQueue->peek(u.jid());

	if(e) {
		profileSetAlert(u.jid(), PsiIconset::instance()->event2icon(e));
	}
	else {
		profileClearAlert(u.jid());
	}

	profileUpdateEntry(u);
	emit updateContact(u);
	Jid j = u.jid();
	if(!rname.isEmpty())
		j = j.withResource(rname);
	emit updateContact(j);
	emit updateContact(j, fromPresence);
	d->psi->updateContactGlobal(this, j);
}

EventDlg *PsiAccount::ensureEventDlg(const Jid &j)
{
	EventDlg *w = findDialog<EventDlg*>(j);
	if (!w)
		w = new EventDlg(j, this, true);

	if (w) {
		connect(w, SIGNAL(aReadNext(const Jid &)), SLOT(processReadNext(const Jid &)));
		connect(w, SIGNAL(aChat(const Jid &)), SLOT(actionOpenChat2(const Jid&)));
		connect(w, SIGNAL(aReply(const Jid &, const QString &, const QString &, const QString &)), SLOT(dj_replyMessage(const Jid &, const QString &, const QString &, const QString &)));
		connect(w, SIGNAL(aAuth(const Jid &)), SLOT(ed_addAuth(const Jid &)));
		connect(w, SIGNAL(aDeny(const Jid &)), SLOT(ed_deny(const Jid &)));
		connect(w, SIGNAL(aHttpConfirm(const PsiHttpAuthRequest &)), SLOT(dj_confirmHttpAuth(const PsiHttpAuthRequest &)));
		connect(w, SIGNAL(aHttpDeny(const PsiHttpAuthRequest &)), SLOT(dj_denyHttpAuth(const PsiHttpAuthRequest &)));
		connect(w, SIGNAL(aRosterExchange(const RosterExchangeItems &)), SLOT(dj_rosterExchange(const RosterExchangeItems &)));
		connect(d->psi, SIGNAL(emitOptionsUpdate()), w, SLOT(optionsUpdate()));
		connect(this, SIGNAL(updateContact(const Jid &)), w, SLOT(updateContact(const Jid &)));
		connect(w, SIGNAL(aFormSubmit(const XData&, const QString&, const Jid&)), SLOT(dj_formSubmit(const XData&, const QString&, const Jid&)));
		connect(w, SIGNAL(aFormCancel(const XData&, const QString&, const Jid&)), SLOT(dj_formCancel(const XData&, const QString&, const Jid&)));
	}

	return w;
}

ChatDlg *PsiAccount::ensureChatDlg(const Jid &j)
{
	/*ChatDlg *c = findChatDialog(j);*/
	ChatDlg *c = findChatDialogEx(j);
	if(!c) {
		// create the chatbox
		c = ChatDlg::create(j, this, d->tabManager);
		connect(c, SIGNAL(aSend(const Message &)), SLOT(dj_sendMessage(const Message &)));
		connect(c, SIGNAL(messagesRead(const Jid &)), SLOT(chatMessagesRead(const Jid &)));
		connect(c, SIGNAL(aInfo(const Jid &)), SLOT(actionInfo(const Jid &)));
		connect(c, SIGNAL(aHistory(const Jid &)), SLOT(actionHistory(const Jid &)));
		connect(c, SIGNAL(aFile(const Jid &)), SLOT(actionSendFile(const Jid &)));
		connect(c, SIGNAL(aVoice(const Jid &)), SLOT(actionVoice(const Jid &)));
		connect(d->psi, SIGNAL(emitOptionsUpdate()), c, SLOT(optionsUpdate()));
		connect(this, SIGNAL(updateContact(const Jid &, bool)), c, SLOT(updateContact(const Jid &, bool)));
	}
	else {
		c->setJid(j);
		// on X11, do a special reparent to open on the right desktop
#ifdef HAVE_X11
		/* KIS added an exception for tabs here. We do *not* want chats flying
		 * randomlyi, it pulls them out of tabsets. So instead, we move the
		 * tabset instead. It's just as filthy, unfortunately, but it's the
		 * only way */
		//TODO: This doesn't work as expected atm, it doesn't seem to reparent the tabset
		QWidget *window=c;
		if ( PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool() )
			window = d->tabManager->getManagingTabs(c);
		if(window && window->isHidden()) {
			QPixmap pp = c->windowIcon().pixmap(16,16); // FIXME: 16x16 is just a guess of what size old QWidget::icon() used
			QPixmap p;
			if(!pp.isNull())
				p = pp;
#ifdef __GNUC__
#warning "Removed reparenting call from qwextend"
#endif
			//reparent_good(window, 0, false);
			if(!p.isNull())
				c->setWindowIcon(p);
		}
#endif
	}

	Q_ASSERT(c);
	return c;
}

void PsiAccount::changeStatus(int x, bool forceDialog)
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
		}
	}

	PsiOptions* o = PsiOptions::instance();

	//If option name is not empty (it is empty for Invisible) and option is set to ask for message, show dialog
	if (forceDialog || (!optionName.isEmpty() && o->getOption("options.status.ask-for-message-on-" + optionName).toBool())) {
		StatusSetDlg *w = new StatusSetDlg(this, makeLastStatus(x), lastPriorityNotEmpty());
		connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)), SLOT(setStatus(const XMPP::Status &, bool, bool)));
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
		setStatus(status, false, true);
	}
}

void PsiAccount::actionVoice(const Jid &j)
{
	Jid j2 = j;
	if(j.resource().isEmpty()) {
		UserListItem *u = find(j);
		if(u && u->isAvailable())
			j2 = j2.withResource((*u->userResourceList().priority()).name());
	}

	CallDlg *w = new CallDlg(this, 0);
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->setOutgoing(j2);
	w->show();
/*
	Q_ASSERT(voiceCaller() != NULL);

	Jid jid;
	if (j.resource().isEmpty()) {
		bool found = false;
		UserListItem *u = find(j);
		if (u) {
			const UserResourceList &rl = u->userResourceList();
			for (UserResourceList::ConstIterator it = rl.begin(); it != rl.end() && !found; ++it) {
				if (capsManager()->features(j.withResource((*it).name())).canVoice()) {
					jid = j.withResource((*it).name());
					found = true;
				}
			}
		}

		if (!found)
			return;
	}
	else {
		jid = j;
	}

	VoiceCallDlg* vc = new VoiceCallDlg(jid,voiceCaller());
	vc->show();
	vc->call();
*/
}

void PsiAccount::sendFiles(const Jid& j, const QStringList& l, bool direct)
{
#ifdef FILETRANSFER
	Jid j2 = j;
	if(j.resource().isEmpty()) {
		UserListItem *u = find(j);
		if(u && u->isAvailable())
			j2 = j2.withResource((*u->userResourceList().priority()).name());
	}

	// Create a dialog for each file in the list. Once the xfer dialog itself
	// supports multiple files, only the 'else' branch needs to stay.
	if (!l.isEmpty()) {
		for (QStringList::ConstIterator f = l.begin(); f != l.end(); ++f ) {
			QStringList fl(*f);
			FileRequestDlg *w = new FileRequestDlg(j2, d->psi, this, fl, direct);
			w->show();
		}
	}
	else {
		FileRequestDlg *w = new FileRequestDlg(j2, d->psi, this, l, direct);
		w->show();
	}
#else
	Q_UNUSED(j);
	Q_UNUSED(l);
	Q_UNUSED(direct);
#endif
}

void PsiAccount::actionSendFile(const Jid &j)
{
	QStringList l;
	sendFiles(j,l);
}

void PsiAccount::actionSendFiles(const Jid &j, const QStringList& l)
{
	sendFiles(j, l);
}

void PsiAccount::actionQueryVersion(const Jid& j)
{
	JT_ClientVersion *task = new JT_ClientVersion(d->client->rootTask());
	task->get(j);
	connect(task, SIGNAL(finished()), SLOT(queryVersionFinished()));
	task->go(true);
}

void PsiAccount::queryVersionFinished()
{
	JT_ClientVersion* j = static_cast<JT_ClientVersion*>(sender());
	QString text;
	text += tr("Name:\t") + j->name();
	text += "\n" + tr("Version:\t") + j->version();
	text += "\n" + tr("Os:\t") + j->os();

	QMessageBox::information(NULL, QString(tr("Version Query Information")), text);
}

void PsiAccount::actionExecuteCommand(const Jid& j, const QString& node)
{
	Jid j2 = j;
	if(j.resource().isEmpty()) {
		UserListItem *u = find(j);
		if(u && u->isAvailable())
			j2 = j2.withResource((*u->userResourceList().priority()).name());
	}

	actionExecuteCommandSpecific(j2, node);
}

void PsiAccount::actionExecuteCommandSpecific(const Jid& j, const QString& node)
{
	if (node.isEmpty()) {
		AHCommandDlg *w = new AHCommandDlg(this,j);
		w->show();
	}
	else {
		AHCommandDlg::executeCommand(d->psi, d->client,j,node);
	}
}

void PsiAccount::actionSetBlock(bool blocked)
{
	d->acc.ignore_global_actions = blocked;
}

void PsiAccount::actionSetMood()
{
	QList<PsiAccount*> list;
	list.append(this);
	MoodDlg *w = new MoodDlg(list);
	w->show();
}

void PsiAccount::actionSetActivity()
{
	QList<PsiAccount*> list;
	list.append(this);
	ActivityDlg *w = new ActivityDlg(list);
	w->show();
}

void PsiAccount::actionSetGeoLocation()
{
	QList<PsiAccount*> list;
	list.append(this);
	GeoLocationDlg *w = new GeoLocationDlg(list);
	w->show();
}

void PsiAccount::actionSetAvatar()
{
	QString str = FileUtil::getImageFileName(0);
	if (!str.isEmpty()) {
		avatarFactory()->setSelfAvatar(str);
	}
}

void PsiAccount::actionUnsetAvatar()
{
	avatarFactory()->setSelfAvatar("");
}

void PsiAccount::actionDefault(const Jid &j)
{
	UserListItem *u = find(j);
	if(!u)
		return;

	if(d->eventQueue->count(u->jid()) > 0)
		openNextEvent(*u, UserAction);
	else if (groupchats().contains(j.full()))
	{
		TabbableWidget *tab = findDialog<TabbableWidget*>(Jid(j.full()));
		if(tab) {
			tab->ensureTabbedCorrectly();
			tab->bringToFront();
		}
	}
	else {
		if(PsiOptions::instance()->getOption("options.messages.default-outgoing-message-type").toString() == "message")
			actionSendMessage(u->jid());
		else
			actionOpenChat(u->jid());
	}
}

void PsiAccount::actionRecvEvent(const Jid &j)
{
	UserListItem *u = find(j);
	if(!u)
		return;

	openNextEvent(*u, UserAction);
}

void PsiAccount::actionRecvRosterExchange(const Jid& j, const RosterExchangeItems& items)
{
	handleEvent(RosterExchangeEvent::Ptr(new RosterExchangeEvent(j,items,"", this)), IncomingStanza);
}

void PsiAccount::actionSendMessage(const Jid &j)
{
	EventDlg *w = d->psi->createMessageDlg(j.full(), this);
	if (!w)
		return;
	w->show();
}

void PsiAccount::actionSendMessage(const QList<XMPP::Jid> &j)
{
	QString str;
	bool first = true;
	for(QList<Jid>::ConstIterator it = j.begin(); it != j.end(); ++it) {
		if(!first)
			str += ", ";
		first = false;

		str += (*it).full();
	}

	EventDlg *w = d->psi->createMessageDlg(str, this);
	if (!w)
		return;
	w->show();
}

void PsiAccount::actionSendUrl(const Jid &j)
{
	EventDlg *w = d->psi->createMessageDlg(j.full(), this);
	if (!w)
		return;
	w->setUrlOnShow();
	w->show();
}

void PsiAccount::actionRemove(const Jid &j)
{
	avatarFactory()->removeManualAvatar(j);
	dj_remove(j);
}

void PsiAccount::actionRename(const Jid &j, const QString &name)
{
	dj_rename(j, name);
}

void PsiAccount::actionGroupRename(const QString &oldname, const QString &newname)
{
	QList<UserListItem*> nu;
	foreach(UserListItem* u, d->userList) {
		if(u->inGroup(oldname)) {
			u->removeGroup(oldname);
			u->addGroup(newname);
			cpUpdate(*u);
			if(u->inList()) {
				nu.append(u);
			}
		}
	}

	if(!nu.isEmpty()) {
		foreach(UserListItem* u, nu) {
			JT_Roster *r = new JT_Roster(d->client->rootTask());
			r->set(u->jid(), u->name(), u->groups());
			r->go(true);
		}
	}
}

void PsiAccount::actionHistory(const Jid &j)
{
	HistoryDlg *w = findDialog<HistoryDlg*>(j);
	if(w)
		bringToFront(w);
	else {
		w = new HistoryDlg(j, this);
		//connect(w, SIGNAL(openEvent(PsiEvent *)), SLOT(actionHistoryBox(PsiEvent *)));
		w->show();
	}
}

void PsiAccount::actionHistoryBox(const PsiEvent::Ptr &e)
{
	if (!EventDlg::messagingEnabled())
		return;

	EventDlg *w = new EventDlg(e->from(), this, false);
	connect(w, SIGNAL(aChat(const Jid &)), SLOT(actionOpenChat2(const Jid&)));
	connect(w, SIGNAL(aReply(const Jid &, const QString &, const QString &, const QString &)), SLOT(dj_replyMessage(const Jid &, const QString &, const QString &, const QString &)));
	connect(w, SIGNAL(aAuth(const Jid &)), SLOT(ed_addAuth(const Jid &)));
	connect(w, SIGNAL(aDeny(const Jid &)), SLOT(ed_deny(const Jid &)));
	connect(w, SIGNAL(aRosterExchange(const RosterExchangeItems &)), SLOT(dj_rosterExchange(const RosterExchangeItems &)));
	connect(d->psi, SIGNAL(emitOptionsUpdate()), w, SLOT(optionsUpdate()));
	connect(this, SIGNAL(updateContact(const Jid &)), w, SLOT(updateContact(const Jid &)));
	w->updateEvent(e);
	w->show();
}

ChatDlg* PsiAccount::actionOpenChat(const Jid &j)
{
	UserListItem *u = (findGCContact(j)) ? find(j) : find(j.bare());
	if(!u) {
		qWarning("[%s] not in userlist\n", qPrintable(j.full()));
		return 0;
	}

	// if 'j' is bare, we might want to switch to a specific resource
	QString res;
	if(j.resource().isEmpty()) {
		// first, are there any queued chats?
		/*PsiEvent *e = d->eventQueue->peekFirstChat(j, false);
		if(e) {
			res = e->from().resource();
			// if we have a bare chat, change to 'res'
			ChatDlg *c = findChatDialog(j);
			if(c)
				c->setJid(j.withResource(res));
		}
		// else, is there a priority chat window available?
		else*/ if(u->isAvailable()) {
			QString pr = (*u->userResourceList().priority()).name();
			if(!pr.isEmpty() && findChatDialog(j.withResource(pr)))
				res = pr;
		}
		else {
			QStringList list = hiddenChats(j);
			if(!list.isEmpty())
				res = list.first();
		}
	}

	if(!res.isEmpty())
		return openChat(j.withResource(res), UserAction);
	else
		return openChat(j, UserAction);
}

// unlike actionOpenChat(), this function will work on jids that aren't
//   necessarily in the userlist.
void PsiAccount::actionOpenChat2(const Jid &_j)
{
	Jid j = _j;
	UserListItem *u = findFirstRelevant(j);
	if(u) {
		j = u->jid();
	}
	else {
		// this can happen if the contact is not in the roster at all

		GCContact *c = findGCContact(j);
		if(c) {
			// if the contact is from a groupchat, use invokeGCChat
			invokeGCChat(j);
			return;
		}
		else {
			// otherwise, make an item
			u = new UserListItem;
			// HACK: reverting to bare jid is probably "wrong", but
			//   i think in almost every case it's what you'd want
			j = j.withResource(QString());
			u->setJid(j);
			u->setInList(false);
			d->userList.append(u);
			cpUpdate(*u);
		}
	}
	actionOpenChat(j);
}

void PsiAccount::actionOpenSavedChat(const Jid& j)
{
	openChat(j, FromXml);
}

void PsiAccount::actionOpenChatSpecific(const Jid &j)
{
	openChat(j, UserAction);
}

void PsiAccount::actionOpenPassiveChatSpecific(const Jid &j)
{
	openChat(j, UserPassiveAction);
}

#ifdef WHITEBOARDING
void PsiAccount::actionOpenWhiteboard(const Jid &j)
{
	UserListItem *u = find(j);
	if(!u)
		return;

	// if 'j' is bare, we might want to switch to a specific resource
	QString res;
	if(j.resource().isEmpty()) {
		if(u->isAvailable()) {
			QString pr = (*u->userResourceList().priority()).name();
			if(!pr.isEmpty())
				res = pr;
		}
	}

	if(!res.isEmpty())
	{
		actionOpenWhiteboardSpecific(j.withResource(res));
	}
	else
	{
		actionOpenWhiteboardSpecific(j);
	}
}

/*! \brief Opens a whiteboard to \a target.
 *  \a ownJid and \a groupChat should be specified in the case of a group chat session.
 */

void PsiAccount::actionOpenWhiteboardSpecific(const Jid &target, Jid ownJid, bool groupChat)
{
	if(ownJid.isEmpty())
		ownJid = jid();

	d->wbManager->openWhiteboard(target, ownJid, groupChat, true);
}
#endif

void PsiAccount::actionAgentSetStatus(const Jid &j, const Status &s)
{
	if ( j.node().isEmpty() ) // add all transport popups to block list
		new BlockTransportPopup(d->blockTransportPopupList, j);

	JT_Presence *p = new JT_Presence(d->client->rootTask());
	p->pres(j, s);
	p->go(true);
}

void PsiAccount::actionInfo(const Jid &_j, bool showStatusInfo)
{
	bool useCache = true;
	bool isMucMember = false;
	Jid j;
	if(findGCContact(_j)) {
		useCache = false;
		isMucMember = true;
		j = _j;
	}
	else {
		j = _j.bare();
	}

	InfoDlg *w = findDialog<InfoDlg*>(j);
	if(w) {
		w->infoWidget()->updateStatus();
		w->infoWidget()->setStatusVisibility(showStatusInfo);
		bringToFront(w);
	}
	else {
		VCard vcard;
		if (isMucMember) {
			vcard = VCardFactory::instance()->mucVcard(j);
		} else {
			vcard = VCardFactory::instance()->vcard(j);
		}

		w = new InfoDlg(j.compare(d->jid) ? InfoWidget::Self : isMucMember? InfoWidget::MucContact : InfoWidget::Contact,
		                j, vcard, this, 0, useCache);

		w->infoWidget()->setStatusVisibility(showStatusInfo);
		w->show();

		// automatically retrieve info if it doesn't exist
		if(!vcard && loggedIn())
			w->infoWidget()->doRefresh();
	}
}

void PsiAccount::actionAuth(const Jid &j)
{
	dj_auth(j);
}

void PsiAccount::actionAuthRequest(const Jid &j)
{
	dj_authReq(j);
}

void PsiAccount::actionAuthRemove(const Jid &j)
{
	dj_deny(j);
}

void PsiAccount::actionAdd(const Jid &j)
{
	dj_addAuth(j);
}

void PsiAccount::actionGroupAdd(const Jid &j, const QString &g)
{
	UserListItem *u = d->userList.find(j);
	if(!u)
		return;

	if(!u->addGroup(g))
		return;
	cpUpdate(*u);

	JT_Roster *r = new JT_Roster(d->client->rootTask());
	r->set(u->jid(), u->name(), u->groups());
	r->go(true);
}

void PsiAccount::actionGroupRemove(const Jid &j, const QString &g)
{
	UserListItem *u = d->userList.find(j);
	if(!u)
		return;

	if(!u->removeGroup(g))
		return;
	cpUpdate(*u);

	JT_Roster *r = new JT_Roster(d->client->rootTask());
	r->set(u->jid(), u->name(), u->groups());
	r->go(true);
}

void PsiAccount::actionGroupsSet(const Jid &j, const QStringList &g)
{
	UserListItem *u = d->userList.find(j);
	if(!u)
		return;

	QStringList g1 = g;
	g1.sort();
	QStringList g2 = u->groups();
	g2.sort();
	if (g1 == g2)
		return;

	u->setGroups(g);
	cpUpdate(*u);

	JT_Roster *r = new JT_Roster(d->client->rootTask());
	r->set(u->jid(), u->name(), u->groups());
	r->go(true);
}

void PsiAccount::actionRegister(const Jid &j)
{
	if(!checkConnected())
		return;

	RegistrationDlg *w = findDialog<RegistrationDlg*>(j);
	if(w)
		bringToFront(w);
	else {
		w = new RegistrationDlg(j, this);
		w->show();
	}
}

void PsiAccount::actionUnregister(const Jid &j)
{
	JT_UnRegister *ju = new JT_UnRegister(d->client->rootTask());
	ju->unreg(j);
	ju->go(true);
}

void PsiAccount::actionSearch(const Jid &j)
{
	if(!checkConnected())
		return;

	SearchDlg *w = findDialog<SearchDlg*>(j);
	if(w)
		bringToFront(w);
	else {
		w = new SearchDlg(j, this);
		connect(w, SIGNAL(add(const XMPP::Jid &, const QString &, const QStringList &, bool)), SLOT(dj_add(const XMPP::Jid &, const QString &, const QStringList &, bool)));
		connect(w, SIGNAL(aInfo(const Jid &)), SLOT(actionInfo(const Jid &)));
		w->show();
	}
}

void PsiAccount::actionInvite(const Jid &j, const QString &gc)
{
	Message m;
	Jid room(gc);
	m.setTo(room);
	m.addMUCInvite(MUCInvite(j));

	QString password = d->client->groupChatPassword(room.node(),room.domain());
	if (!password.isEmpty())
		m.setMUCPassword(password);
	m.setTimeStamp(QDateTime::currentDateTime());
	dj_sendMessage(m);
}

void PsiAccount::actionAssignKey(const Jid &j)
{
	if(ensureKey(j)) {
		UserListItem *u = findFirstRelevant(j);
		if(u) {
			cpUpdate(*u);
		}
	}
}

void PsiAccount::actionUnassignKey(const Jid &j)
{
	UserListItem *u = findFirstRelevant(j);
	if(u) {
		u->setPublicKeyID("");
		cpUpdate(*u);
	}
}

void PsiAccount::openUri(const QUrl &uriToOpen)
{
	// entity
	QString path = uriToOpen.path();
	if (path.startsWith('/')) {	// this happens when authority part is present
		path = path.mid(1);
	}
	Jid entity = JIDUtil::fromString(path);

	// query
#ifdef HAVE_QT5
	QUrlQuery uri;
	uri.setQueryDelimiters('=', ';');
	uri.setQuery(uriToOpen.query(QUrl::FullyEncoded));
#else
	QUrl uri(uriToOpen);	// got to copy, because setQueryDelimiters() is not const
	uri.setQueryDelimiters('=', ';');
#endif

	QString querytype = uri.queryItems().value(0).first;	// defaults to empty string

	if (0) {
	//} else if (querytype == "command") {
	//	// action
	//} else if (querytype == "disco") {
	//	actionDisco(entity, uri.queryItemValue("node")); //x
	//	// request, type
	} else if (querytype == "invite") {
		actionJoin(entity, uri.queryItemValue("password"));
		// jid
	} else if (querytype == "join") {
		actionJoin(entity, uri.queryItemValue("password"));
	} else if (querytype == "message") {
#ifdef HAVE_QT5
		QString subject = uri.queryItemValue("subject", QUrl::FullyDecoded);
		QString body = uri.queryItemValue("body", QUrl::FullyDecoded);
#else
		QString subject = uri.queryItemValue("subject");
		QString body = uri.queryItemValue("body");
#endif
		QString type = uri.queryItemValue("type");
		if (type == "chat" && subject.isEmpty()) {
			if (!find(entity.bare())) {
				addUserListItem(entity);
			}
			ChatDlg *dlg = actionOpenChat(entity);
			if (dlg && !body.isEmpty()) {
				dlg->setInputText(body);
			}
		} else {
			dj_newMessage(entity, body, subject, "");
		}
	} else if (querytype == "vcard") {
		actionInfo(entity);

		// thread, from, id
	//} else if (querytype == "pubsub") {
	//	// action, node
	//} else if (querytype == "recvfile") {
	//	// ...
	//} else if (querytype == "register") {
	//} else if (querytype == "remove") {
	} else if (querytype == "roster") {
		openAddUserDlg(entity, uri.queryItemValue("name"), uri.queryItemValue("group"));
	//} else if (querytype == "sendfile") {
	//} else if (querytype == "subscribe") {
	//} else if (querytype == "unregister") {
	//} else if (querytype == "unsubscribe") {
	//} else if (querytype == "vcard") {
	//	pa->actionInfo(entity, true, true);
	} else {

		// TODO: default case - be more smart!! ;-)

		//if (QMessageBox::question(0, tr("Hmm.."), QString(tr("So, you'd like to open %1 URI, right?\n"
		//	"Unfortunately, this URI only identifies an entity, but it doesn't say what action to perform (or at least Psi cannot understand it). "
		//	"So it's pretty much like if I said \"John\" to you - you'd immediately ask \"But what about John?\".\n"
		//	"So... What about %1??\n"
		//	"At worst, you may send a message to %2 to ask what to do (and maybe complain about this URI ;)) "
		//	"Would you like to do this now?")).arg(uri).arg(entity.full()), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
		actionSendMessage(entity);
	}
}

void PsiAccount::dj_sendMessage(const Message &m, bool log)
{
	UserListItem *u = findFirstRelevant(m.to());
	Message nm = m;

	if(PsiOptions::instance()->getOption("options.messages.force-incoming-message-type").toString() == "current-open") {
		if(u) {
			switch(u->lastMessageType()) {
				case 0: nm.setType(""); break;
				case 1: nm.setType("chat"); break;
			}
		}
	}
	
#ifdef PSI_PLUGINS
	if (!nm.body().isEmpty()) {
		QString body = nm.body();
		QString subject = nm.subject();

		if(PluginManager::instance()->processOutgoingMessage(this, nm.to().full(), body, nm.type(), subject))
			return;
		if (body != nm.body()) {
			nm.setBody(body);
		}
		if (subject != nm.subject()) {
			nm.setSubject(subject);
		}
	}
#endif

	if (!nm.body().isEmpty()) {
		UserListItem *u = findFirstRelevant(m.to());
		if (!u || (!u->isConference() && u->subscription().type() != Subscription::Both && u->subscription().type() != Subscription::From)) {
			nm.setNick(nick());
		}
	}

	d->client->sendMessage(nm);

	// only toggle if not an invite or body is not empty
	if(m.invite().isEmpty() && !m.body().isEmpty())
		toggleSecurity(m.to(), m.wasEncrypted());

	// don't log groupchat or encrypted messages
	if(log) {
		if(m.type() != "groupchat" && m.xencrypted().isEmpty()) {
			int type = findGCContact(m.to()) ? EDB::GroupChatContact : EDB::Contact;
			MessageEvent::Ptr me(new MessageEvent(m, this));
			me->setOriginLocal(true);
			me->setTimeStamp(QDateTime::currentDateTime());
			logEvent(m.to(), me, type);
		}
	}

	// don't sound when sending groupchat messages or message events
	if(m.type() != "groupchat" && !m.body().isEmpty() && log)
		playSound(eSend);

	// auto close an open messagebox (if non-chat)
	if(m.type() != "chat" && !m.body().isEmpty()) {
		UserListItem *u = findFirstRelevant(m.to());
		if(u) {
			EventDlg *e = findDialog<EventDlg*>(u->jid());
			if(e)
				e->closeAfterReply();
		}
	}
}

void PsiAccount::dj_newMessage(const Jid &jid, const QString &body, const QString &subject, const QString &thread)
{
	EventDlg *w = d->psi->createMessageDlg(jid.full(), this);
	if (!w)
		return;

	if (!body.isEmpty()) {
		w->setHtml(TextUtil::plain2rich(body));
	}
	if (!subject.isEmpty()) {
		w->setSubject(subject);
	}
	if (!thread.isEmpty()) {
		w->setThread(thread);
	}
	w->show();
}

void PsiAccount::dj_replyMessage(const Jid &jid, const QString &body, const QString &subject, const QString &thread)
{
	QString re = (!subject.isEmpty() && subject.left(3) != "Re:") ? "Re: " : QString();
	dj_newMessage(jid, !body.isEmpty() ? TextUtil::quote(body) : body, re + subject, thread);
}

void PsiAccount::dj_replyMessage(const Jid &j, const QString &body)
{
	dj_replyMessage(j, body, QString(), QString());
}

void PsiAccount::dj_addAuth(const Jid &j)
{
	dj_addAuth(j,QString());
}


void PsiAccount::dj_addAuth(const Jid &j, const QString& nick)
{
	QString name;
	QStringList groups;
	UserListItem *u = d->userList.find(j);
	if(u) {
		name = u->name();
		groups = u->groups();
	}
	else if (!nick.isEmpty()){
		name = nick;
	}

	dj_add(j, name, groups, true);
	dj_auth(j);
}

void PsiAccount::dj_confirmHttpAuth(const PsiHttpAuthRequest &req)
{
	d->httpAuthManager->confirm(req);
}

void PsiAccount::dj_denyHttpAuth(const PsiHttpAuthRequest &req)
{
	d->httpAuthManager->deny(req);
}

void PsiAccount::dj_formSubmit(const XData& data, const QString& thread, const Jid& jid)
{
	Message m;

	m.setTo(jid);
	m.setThread(thread, true);
	m.setForm(data);

	d->client->sendMessage(m);
}

void PsiAccount::dj_formCancel(const XData& data, const QString& thread, const Jid& jid)
{
	Message m;

	m.setTo(jid);
	m.setThread(thread, true);
	m.setForm(data);

	d->client->sendMessage(m);
}

void PsiAccount::dj_add(const XMPP::Jid &j, const QString &name, const QStringList &groups, bool authReq)
{
	JT_Roster *r = new JT_Roster(d->client->rootTask());
	r->set(j, name, groups);
	r->go(true);

	if(authReq)
		dj_authReq(j);
}

void PsiAccount::dj_authReq(const Jid &j)
{
	d->client->sendSubscription(j, "subscribe", nick());
}

void PsiAccount::dj_auth(const Jid &j)
{
	psi()->contactUpdatesManager()->contactAuthorized(this, j);
	d->client->sendSubscription(j, "subscribed");
}

void PsiAccount::dj_deny(const Jid &j)
{
	psi()->contactUpdatesManager()->contactDeauthorized(this, j);
	d->client->sendSubscription(j, "unsubscribed");
}

void PsiAccount::dj_rename(const Jid &j, const QString &name)
{
	UserListItem *u = d->userList.find(j);
	if(!u)
		return;

	QString str;
	if(name == u->jid().full())
		str = "";
	else
		str = name;

	// strange workaround to avoid a null string ??
	QString uname;
	if(u->name().isEmpty())
		uname = "";
	else
		uname = u->name();

	if(uname == str)
		return;
	u->setName(str);

	cpUpdate(*u);

	if(u->inList()) {
		JT_Roster *r = new JT_Roster(d->client->rootTask());
		r->set(u->jid(), u->name(), u->groups());
		r->go(true);
	}
}

void PsiAccount::dj_remove(const Jid &j)
{
	psi()->contactUpdatesManager()->contactRemoved(this, j);

	UserListItem *u = d->userList.find(j);
	if(!u)
		return;

	// remove all events from the queue
	d->eventQueue->clear(j);
	updateReadNext(j);

	// TODO: delete the item immediately (to simulate local change)
	// should be done with care though, as the server could refuse
	// to delete a contact
	if(!u->inList()) {
		profileRemoveEntry(u->jid());
		d->userList.removeAll(u);
		delete u;
	}
	else {
		JT_Roster *r = new JT_Roster(d->client->rootTask());
		r->remove(j);
		r->go(true);

		// if it looks like a transport, unregister (but not if it is the server!!)
		if(u->isTransport() && j.isValid() && !Jid(d->client->host()).compare(u->jid())) {
			actionUnregister(j);
		}
	}
}

void PsiAccount::dj_rosterExchange(const RosterExchangeItems& items)
{
	foreach(RosterExchangeItem item, items) {
		if (!validRosterExchangeItem(item))
			continue;

		if (item.action() == RosterExchangeItem::Add) {
			if (d->client->roster().find(item.jid(),true) == d->client->roster().end()) {
				dj_add(item.jid(),item.name(),item.groups(),true);
			}
		}
		else if (item.action() == RosterExchangeItem::Delete) {
			//dj_remove(item.jid());
		}
		else if (item.action() == RosterExchangeItem::Modify) {
			// TODO
		}
	}
}

void PsiAccount::eventFromXml(const PsiEvent::Ptr &e)
{
	handleEvent(e, FromXml);
}

#ifdef PSI_PLUGINS
void PsiAccount::createNewPluginEvent(int account, const QString &jid, const QString &descr, QObject *receiver, const char *slot)
{
	PluginEvent::Ptr pe(new PluginEvent(account, jid, descr, this));
	connect(pe.data(), SIGNAL(activated(QString,int)), receiver, slot);
	handleEvent(pe, IncomingStanza);
}
#endif

// handle an incoming event
void PsiAccount::handleEvent(const PsiEvent::Ptr &e, ActivationType activationType)
{
	PsiOptions *o = PsiOptions::instance();
	if (e && activationType != FromXml) {
		setEnabled();
	}

	bool doPopup    = false;
	bool putToQueue = true;
	PopupManager::PopupType popupType = PopupManager::AlertNone;
	SoundType soundType = eNone;

	// find someone to accept the event
	Jid j;
	QList<UserListItem*> ul = findRelevant(e->from());
	if(ul.isEmpty()) {
		// if groupchat, then we want the full JID
		if(findGCContact(e->from())) {
			j = e->from();
		}
		else {
			Jid bare = e->from().bare();
			Jid reg = bare.withResource("registered");

			// see if we have a "registered" variant of the jid
			if(findFirstRelevant(reg)) {
				j = reg;
				e->setFrom(reg); // HACK!!
			}
			// retain full jid if sent to "registered"
			else if(e->from().resource() == "registered")
				j = e->from();
			// otherwise don't use the resource for new entries
			else
				j = bare;
		}
	}
	else {
		j = ul.first()->jid();
	}

	e->setJid(j);

#ifdef PSI_PLUGINS
	QDomDocument doc;
	QDomElement eXml = e->toXml(&doc);
	if (PluginManager::instance()->processEvent(this, eXml)) {
		return;
	} else {
		e->fromXml(psi(),this, &eXml);
	}
#endif

	if (d->psi->filterEvent(this, e)) {
		return;
	}

	if (activationType != FromXml) {
		if (e->type() == PsiEvent::Message ||
			e->type() == PsiEvent::Auth
		   )
		{
			bool found = false;

			// don't log private messages
			if (!found &&
				!(e->type() == PsiEvent::Message &&
				 e.staticCast<MessageEvent>()->message().body().isEmpty()))
			{
				bool isMuc = false;
#ifdef GROUPCHAT
				if(e->type() == PsiEvent::Message) {
					MessageEvent::Ptr me = e.staticCast<MessageEvent>();
					if (me->message().type() == "groupchat")
						isMuc = true;
				}
#endif
				if (!isMuc) {
					Jid	chatJid = e->from();
					if(e->type() == PsiEvent::Message) {
						MessageEvent::Ptr me = e.staticCast<MessageEvent>();
						const Message &m = me->message();
						if (m.carbonDirection() == Message::Sent) {
							chatJid = m.to();
						}
					}

					int type = findGCContact(chatJid) ? EDB::GroupChatContact : EDB::Contact;
					logEvent(chatJid, e, type);
				}
			}
		}
	}

	if(e->type() == PsiEvent::Message) {
		MessageEvent::Ptr me = e.staticCast<MessageEvent>();
		const Message &m = me->message();

#ifdef PSI_PLUGINS
		//TODO(mck): clean up
		//UserListItem *ulItem=NULL;
		//if ( !ul.isEmpty() )
		//	ulItem=ul.first();
		if (PluginManager::instance()->processMessage(this, e->from().full(), m.body(), m.subject())) {
			return;
		}
		//PluginManager::instance()->message(this,e->from(),ulItem,((MessageEvent*)e)->message().body());
#endif
		if (m.messageReceipt() == ReceiptReceived) {
			if (o->getOption("options.ui.notifications.request-receipts").toBool()) {
				foreach (ChatDlg *c, findChatDialogs(e->from(), false)) {
					if (c->autoSelectContact()  || c->jid().resource().isEmpty() || e->from().resource() == c->jid().resource()) {
						if (c->autoSelectContact())
							c->setJid(e->from());
						c->incomingMessage(m);
					}
				}
			}
			return;
		}

		// Pass message events to chat window
		if ((m.containsEvents() || m.chatState() != StateNone) && m.body().isEmpty() && m.type() != "groupchat") {
			if (m.carbonDirection() == Message::Sent) {
				return; // ignore own composing for carbon. TODO should we?
			}
			if (o->getOption("options.messages.send-composing-events").toBool()) {
				ChatDlg *c = findChatDialogEx(e->from());
				if (c) {
					c->setJid(e->from());
					c->incomingMessage(m);
				}
			}
			if(m.chatState() == StateComposing) {
				doPopup = true;
				putToQueue = false;
				popupType = PopupManager::AlertComposing;
			}
			else {
				return;
			}
		}

		// pass chat messages directly to a chat window if possible (and deal with sound)
		else if(m.type() == "chat") {
			Jid	chatJid = m.carbonDirection() == Message::Sent ? m.to() : m.from();

			if (m.carbonDirection() == Message::Sent) {
				e->setOriginLocal(true);
				doPopup = false;
			}
			// throw away carbons sent for MUC private messages
			// server sends them to all resources so we have to explicitly skip them or we'll have a lot of duplicates
			if (m.carbonDirection() == Message::Received && m.hasMUCUser()) {
				return;
			}
			ChatDlg *c = findChatDialogEx(chatJid, m.carbonDirection() == Message::Sent);
			if (c && c->jid().resource().isEmpty())
				c->setJid(chatJid);

			//if the chat exists, and is either open in a tab,
			//or in a window
			if( c && ( d->tabManager->isChatTabbed(c) || !c->isHidden() ) ) {
				c->incomingMessage(m);
				soundType = eChat2;
				if (m.carbonDirection() != Message::Sent &&
                    ((o->getOption("options.ui.chat.alert-for-already-open-chats").toBool() && !c->isActiveTab())
                     || (c->isTabbed() && c->getManagingTabDlg()->isHidden()))) {

					// to alert the chat also, we put it in the queue
					me->setSentToChatWindow(true);
				}
				else {
					putToQueue = false;
				}
			}
			else {
				bool firstChat = !d->eventQueue->hasChats(e->from());
				soundType = firstChat ? eChat1: eChat2;
			}

			if (putToQueue && m.carbonDirection() != Message::Sent) {
				doPopup = true;
				popupType = PopupManager::AlertChat;
			}
		} // /chat
		else if (m.type() == "headline") {
			soundType = eHeadline;
			doPopup = true;
			popupType = PopupManager::AlertHeadline;
		} // /headline
#ifdef GROUPCHAT
		else if (m.type() == "groupchat") {
			putToQueue = false;
			bool allowMucEvents = o->getOption("options.ui.muc.allow-highlight-events").toBool();
			if (activationType != FromXml) {
				GCMainDlg *c = findDialog<GCMainDlg*>(e->from());
				if (c) {
					c->message(m, e);
					if (!c->isActiveTab() && c->isLastMessageAlert() && !m.spooled() && allowMucEvents)
						putToQueue = true;
				}
			}
			else if (allowMucEvents)
				putToQueue = true;
		} // /groupchat
#endif
		else if (m.type().isEmpty()) {
			soundType = eMessage;
			doPopup = true;
			popupType = PopupManager::AlertMessage;
		} // /""
		else {
			soundType = eSystem;
		}

		if (m.carbonDirection() == Message::Sent) {
			doPopup = false;
			soundType = eNone;
			putToQueue = false;
		}
		if(m.type() == "error") {
			// FIXME: handle message errors
			//msg.text = QString(tr("<big>[Error Message]</big><br>%1").arg(plain2rich(msg.text)));
		}
	}
	else if(e->type() == PsiEvent::HttpAuth) {
		soundType = eSystem;
	}
#ifdef WHITEBOARDING
	else if(e->type() == PsiEvent::Sxe) {
		soundType = eHeadline;
		doPopup = true;
		popupType = PopupManager::AlertHeadline;
	}
#endif
	else if(e->type() == PsiEvent::File) {
		soundType = eIncomingFT;
		doPopup = true;
		popupType = PopupManager::AlertFile;
	}
	else if(e->type() == PsiEvent::AvCallType) {
		soundType = eIncomingFT;
		doPopup = true;
		popupType = PopupManager::AlertAvCall;
	}
	else if(e->type() == PsiEvent::RosterExchange) {
		RosterExchangeEvent::Ptr re = e.staticCast<RosterExchangeEvent>();
		RosterExchangeItems items;
		foreach(RosterExchangeItem item, re->rosterExchangeItems()) {
			if (validRosterExchangeItem(item))
				items += item;
		}
		if (items.isEmpty()) {
			return;
		}
		re->setRosterExchangeItems(items);
		soundType = eSystem;
	}
	else if (e->type() == PsiEvent::Auth) {
		soundType = eSystem;

		AuthEvent::Ptr ae = e.staticCast<AuthEvent>();
		if(ae->authType() == "subscribe") {
			if(o->getOption("options.subscriptions.automatically-allow-authorization").toBool()) {
				// Check if we want to request auth as well
				UserListItem *u = d->userList.find(ae->from());
				if (!u || (u->subscription().type() != Subscription::Both && u->subscription().type() != Subscription::To)) {
					dj_addAuth(ae->from(),ae->nick());
				}
				else {
					dj_auth(ae->from());
				}
				putToQueue = false;
			}
		}
		else if(ae->authType() == "subscribed") {
			if(!o->getOption("options.ui.notifications.successful-subscription").toBool())
				putToQueue = false;
		}
		else if(ae->authType() == "unsubscribe") {
			putToQueue = false;
		}
	}
#ifdef PSI_PLUGINS
	else if (e->type() == PsiEvent::Plugin) {
		soundType = eSystem;
		doPopup = true;
		popupType = PopupManager::AlertHeadline;
	}
#endif
	else {
		putToQueue = false;
		doPopup = false;
	}

	if (doPopup && !d->noPopup(activationType)) {
		Resource r;
		UserListItem *u = findFirstRelevant(j);
		if (u && u->priority() != u->userResourceList().end()) {
			r = *(u->priority());
		}

		if ((popupType == PopupManager::AlertChat      && o->getOption("options.ui.notifications.passive-popups.incoming-chat").toBool())     ||
		    (popupType == PopupManager::AlertMessage   && o->getOption("options.ui.notifications.passive-popups.incoming-message").toBool())  ||
		    (popupType == PopupManager::AlertHeadline  && o->getOption("options.ui.notifications.passive-popups.incoming-headline").toBool()) ||
		    (popupType == PopupManager::AlertFile      && o->getOption("options.ui.notifications.passive-popups.incoming-file-transfer").toBool()) ||
		    (popupType == PopupManager::AlertAvCall    && o->getOption("options.ui.notifications.passive-popups.incoming-message").toBool()) ||
		    (popupType == PopupManager::AlertComposing && o->getOption("options.ui.notifications.passive-popups.composing").toBool()))
		{
#ifdef PSI_PLUGINS
			if(e->type() != PsiEvent::Plugin) {
#endif
				psi()->popupManager()->doPopup(this, popupType, j, r, u, e, false);
#ifdef PSI_PLUGINS
 			}
 			else {
				psi()->popupManager()->doPopup(this, j, IconsetFactory::iconPtr("psi/headline"), tr("Headline"), 0, 0, e->description(), false, popupType);
			}
#endif
		}
		emit startBounce();
	}

	if (soundType >= 0 && activationType != FromXml) {
		playSound(soundType);
	}

	if ( putToQueue )
		queueEvent(e, activationType);
}

UserListItem* PsiAccount::addUserListItem(const Jid& jid, const QString& nick)
{
	// create item
	UserListItem *u = new UserListItem;
	u->setJid(jid);
	u->setInList(false);
	u->setAvatarFactory(avatarFactory());
	u->setName(nick);
	u->setConference(groupchats().contains(jid.full()));

	// is it a private groupchat?
	Jid j = u->jid();
	GCContact *c = findGCContact(j);
	if(c) {
		u->setName(j.resource());
		u->setPrivate(true);

		// make a resource so the contact appears online
		UserResource ur;
		ur.setName(j.resource());
		ur.setStatus(c->status);
		u->userResourceList().append(ur);
	}

	if(u->isConference()) {
		// make a resource so the contact appears online
		QString name = j.node();
		UserResource ur;
		foreach(ConferenceBookmark c, d->bookmarkManager->conferences())
		{
			if(c.jid().full() == j.bare()) name = c.name();
		}
		u->setInList(true);
		u->setName(name);
		ur.setName("Muc");
		ur.setStatus(status());
		u->userResourceList().append(ur);
	}

	// treat it like a push  [pushinfo]
	//VCard info;
	//if(readUserInfo(item->jid, &info) && !info.field[vNickname].isEmpty())
	//	item->nick = info.field[vNickname];
	//else {
	//	if(localStatus != STATUS_OFFLINE)
	//		serv->getVCard(item->jid);
	//}

	d->userList.append(u);
	cpUpdate(*u);
	return u;
}

void PsiAccount::addMucItem(const Jid& jid)
{
	if (jid.isEmpty())
		return;
	UserListItem *u = find(jid);
	if (u) {
		d->removeEntry(jid);
		d->userList.removeAll(u);
	}
	if (!d->groupchats.contains(jid.bare()))
		d->groupchats += jid.bare();
	addUserListItem(jid.bare(), "");
}

// put an event into the event queue, and update the related alerts
void PsiAccount::queueEvent(const PsiEvent::Ptr &e, ActivationType activationType)
{
	// do we have roster item for this?
	UserListItem *u = find(e->jid());
	if(!u) {
		QString nick;
		if (e->type() == PsiEvent::Auth) {
			AuthEvent::Ptr ae = e.staticCast<AuthEvent>();
			nick = ae->nick();
		}
		else if (e->type() == PsiEvent::Message) {
			MessageEvent::Ptr me = e.staticCast<MessageEvent>();
			if (me->message().type()  != "error")
				nick = me->nick();
		}

		u = addUserListItem(e->jid(), nick);
	}

	//printf("queuing message from [%s] for [%s].\n", e->from().full().latin1(), e->jid().full().latin1());
	d->eventQueue->enqueue(e);

	updateReadNext(e->jid());
	if(PsiOptions::instance()->getOption("options.ui.contactlist.raise-on-new-event").toBool())
		d->psi->raiseMainwin();

	// update the roster
	cpUpdate(*u);

	// if we have both (option.popupMsgs || option.popupChats) && option.alertOpenChats,
	// then option.alertOpenChats will have NO USER-VISIBLE EFFECT as the
	// events will be immediately deleted from the event queue

	// FIXME: We shouldn't be doing this kind of stuff here, because this
	// function is named *queue*Event() not deleteThisMessageSometimes()
	if (!d->noPopupDialogs(activationType)) {
		bool doPopup = false;

		// Check to see if we need to popup
		if (e->type() == PsiEvent::Message) {
			MessageEvent::Ptr me = e.staticCast<MessageEvent>();
			const Message &m = me->message();
			if (m.type() == "chat")
				doPopup = PsiOptions::instance()->getOption("options.ui.chat.auto-popup").toBool();
			else if (m.type() == "headline")
				doPopup = PsiOptions::instance()->getOption("options.ui.message.auto-popup-headlines").toBool();
			else
				doPopup = PsiOptions::instance()->getOption("options.ui.message.auto-popup").toBool();
		}
		else if (e->type() == PsiEvent::File) {
			doPopup = PsiOptions::instance()->getOption("options.ui.file-transfer.auto-popup").toBool();
		}
#ifdef PSI_PLUGINS
		else if (e->type() == PsiEvent::Plugin)
			doPopup = false;
#endif
		else {
			doPopup = PsiOptions::instance()->getOption("options.ui.message.auto-popup").toBool();
		}

		// Popup
		if (doPopup) {
			UserListItem *u = find(e->jid());
			if (u && (!PsiOptions::instance()->getOption("options.ui.notifications.popup-dialogs.suppress-when-not-on-roster").toBool() || u->inList()))
				openNextEvent(*u, activationType);
		}
	}
}

// take the next event from the queue and display it
void PsiAccount::openNextEvent(const UserListItem& u, ActivationType activationType)
{
	PsiEvent::Ptr e = d->eventQueue->peek(u.jid());
	if(!e)
		return;
#ifdef PSI_PLUGINS
	if(e->type() == PsiEvent::Plugin) {
		PluginEvent::Ptr pe = e.staticCast<PluginEvent>();
		pe->activate();
		eventQueue()->dequeue(e);
		queueChanged();
		UserListItem *u = e->account()->find(e->jid());
		if(u)
			e->account()->cpUpdate(*u);
		return;
	}
#endif

	psi()->processEvent(e, activationType);
}

void PsiAccount::openNextEvent(ActivationType activationType)
{
	PsiEvent::Ptr e = d->eventQueue->peekNext();
	if(!e)
		return;

	if(e->type() == PsiEvent::PGP) {
		psi()->processEvent(e, activationType);
		return;
	}

	UserListItem *u = find(e->jid());
	if(!u) {
		u = new UserListItem();
		u->setJid(e->jid());
		u->setInList(false);
		d->userList.append(u);
	}
	openNextEvent(*u, activationType);
}

int PsiAccount::forwardPendingEvents(const Jid &jid)
{
	QList<PsiEvent::Ptr> chatList;
	d->eventQueue->extractMessages(&chatList);
	foreach(const PsiEvent::Ptr &e, chatList) {
		MessageEvent::Ptr me = e.staticCast<MessageEvent>();
		Message m = me->message();

		AddressList oFrom = m.findAddresses(Address::OriginalFrom);
		AddressList oTo = m.findAddresses(Address::OriginalTo);

		if (oFrom.count() == 0)
			m.addAddress(Address(Address::OriginalFrom, m.from()));
		if (oTo.count() == 0)
			m.addAddress(Address(Address::OriginalTo, m.to()));

		m.setTimeStamp(m.timeStamp(), true);
		m.setTo(jid);
		m.setFrom("");

		d->client->sendMessage(m);

		// update the eventdlg
		UserListItem *u = find(e->jid());

		// update the contact
		if(u) {
			cpUpdate(*u);
		}

		updateReadNext(u->jid());
	}
	return chatList.count();
}

void PsiAccount::updateReadNext(const Jid &j)
{
	// update eventdlg's read-next
	EventDlg *w = findDialog<EventDlg*>(j);
	if(w) {
		PsiIcon *nextAnim = 0;
		int nextAmount = d->eventQueue->count(j);
		if(nextAmount > 0)
			nextAnim = PsiIconset::instance()->event2icon(d->eventQueue->peek(j));
		w->updateReadNext(nextAnim, nextAmount);
	}

	queueChanged();
}

void PsiAccount::processReadNext(const Jid &j)
{
	UserListItem *u = find(j);
	if(u)
		processReadNext(*u);
}

void PsiAccount::processReadNext(const UserListItem &u)
{
	EventDlg *w = findDialog<EventDlg*>(u.jid());
	if(!w) {
		// this should NEVER happen
		return;
	}

	// peek the event
	PsiEvent::Ptr e = d->eventQueue->peek(u.jid());
	if(!e)
		return;

	bool isChat = false;
#ifdef GROUPCHAT
	bool isMuc = false;
#endif
	if(e->type() == PsiEvent::Message) {
		MessageEvent::Ptr me = e.staticCast<MessageEvent>();
		const Message &m = me->message();
		if(m.type() == "chat" && m.getForm().fields().empty())
			isChat = true;
#ifdef GROUPCHAT
		else if (m.type() == "groupchat")
			isMuc = true;
#endif
	}

	// if it's a chat message, just open the chat window.  there is no need to do
	// further processing.  the chat window will remove it from the queue, update
	// the cvlist, etc etc.
	if(isChat) {
		openChat(e->from(), UserAction);
		return;
	}

#ifdef GROUPCHAT
	if (isMuc) {
		GCMainDlg *c = findDialog<GCMainDlg*>(e->from());
		if (c)
		{
			c->bringToFront(true);
			return;
		}
	}
#endif

	// remove from queue
	e = d->eventQueue->dequeue(u.jid());

	// update the eventdlg
	w->updateEvent(e);

	// update the contact
	cpUpdate(u);

	updateReadNext(u.jid());
}

void PsiAccount::processChatsHelper(const Jid& j, bool removeEvents)
{
	//printf("processing chats for [%s]\n", j.full().latin1());
	ChatDlg *c = findChatDialogEx(j);
	if(!c)
		return;
	// extract the chats
	QList<PsiEvent::Ptr> chatList;
	bool compareResources = !c->autoSelectContact();

	d->eventQueue->extractChats(&chatList, j, compareResources, removeEvents);

	if(!chatList.isEmpty()) {
		// dump the chats into the chat window, and remove the related cvlist alerts

		// TODO FIXME: Contact's status changes should also be cached in order to
		// insert them to chatdlg correctly.
		//
		// 15:07 *mblsha is Online
		// 15:15 *mblsha is Offline
		// 15:10 <mblsha> hello!

		foreach(const PsiEvent::Ptr &e, chatList) {
			if (e->type() == PsiEvent::Message) {
				MessageEvent::Ptr me = e.staticCast<MessageEvent>();
				const Message &m = me->message();

				// process the message
				if(!me->sentToChatWindow()) {
					c->incomingMessage(m);
					me->setSentToChatWindow(true);
				}
			}
		}

		if (removeEvents) {
			QList<UserListItem*> ul = findRelevant(j);
			if(!ul.isEmpty()) {
				UserListItem *u = ul.first();
				cpUpdate(*u);
				updateReadNext(u->jid());
			}
		}
	}
}

void PsiAccount::processChats(const Jid &j)
{
	processChatsHelper(j, true);
}

ChatDlg* PsiAccount::openChat(const Jid& j, ActivationType activationType)
{
	ChatDlg* chat = ensureChatDlg(j);
	chat->ensureTabbedCorrectly();

	processChats(j);
	if (activationType == UserAction) {
		chat->bringToFront(true);
	}
	else if (activationType == UserPassiveAction) {
		chat->bringToFront(false);
		TabDlg* dlg = chat->getManagingTabDlg();
		if (dlg) {
			QTimer::singleShot(1000, dlg, SLOT(showTabWithoutActivation()));
		}
	}
	return chat;
}

void PsiAccount::chatMessagesRead(const Jid &j)
{
//	if(PsiOptions::instance()->getOption("options.ui.chat.alert-for-already-open-chats").toBool()) {
		processChats(j);
//	}
}

#ifdef GROUPCHAT
void PsiAccount::groupChatMessagesRead(const Jid &j)
{
	d->eventQueue->clear(j);

	QList<UserListItem*> ul = findRelevant(j);
	if(!ul.isEmpty()) {
		UserListItem *u = ul.first();
		cpUpdate(*u);
	}
}
#endif

void PsiAccount::logEvent(const Jid &j, const PsiEvent::Ptr &e, int type)
{
	if (!d->acc.opt_log)
		return;

	if (type == EDB::GroupChatContact && !PsiOptions::instance()->getOption("options.history.store-muc-private").toBool())
		return;

	EDBHandle *h = new EDBHandle(d->psi->edb());
	connect(h, SIGNAL(finished()), SLOT(edb_finished()));
	h->append(id(), j, e, type);
}

void PsiAccount::edb_finished()
{
	EDBHandle *h = (EDBHandle *)sender();
	delete h;
}

void PsiAccount::openGroupChat(const Jid &j, ActivationType activationType, MUCJoinDlg::MucJoinReason reason)
{
#ifdef GROUPCHAT
	GCMainDlg *w = new GCMainDlg(this, j, d->tabManager);
	w->setPassword(d->client->groupChatPassword(j.domain(), j.node()));
	connect(w, SIGNAL(aSend(const Message &)), SLOT(dj_sendMessage(const Message &)));
	connect(w, SIGNAL(messagesRead(const Jid &)), SLOT(groupChatMessagesRead(const Jid &)));
	connect(d->psi, SIGNAL(emitOptionsUpdate()), w, SLOT(optionsUpdate()));
	if(reason != MUCJoinDlg::MucAutoJoin || !PsiOptions::instance()->getOption("options.ui.muc.hide-on-autojoin").toBool()) {
		w->ensureTabbedCorrectly();
		if (activationType == UserAction)
			w->bringToFront();
	}
#endif
}

bool PsiAccount::groupChatJoin(const QString &host, const QString &room, const QString &nick, const QString& pass, bool nohistory)
{
	if (nohistory)
		return d->client->groupChatJoin(host, room, nick, pass, 0);
	else {
		QDateTime since;
		GCMainDlg *w = findDialog<GCMainDlg*>(Jid(room, host));
		if (w)
			since = w->lastMsgTime();

		Status s = d->loginStatus;
		s.setXSigned("");

		return d->client->groupChatJoin(host, room, nick, pass, PsiOptions::instance()->getOption("options.muc.context.maxchars").toInt(), PsiOptions::instance()->getOption("options.muc.context.maxstanzas").toInt(), PsiOptions::instance()->getOption("options.muc.context.seconds").toInt(), since, s);
	}
}

void PsiAccount::groupChatChangeNick(const QString &host, const QString &room, const QString& nick, const Status &s)
{
	d->client->groupChatChangeNick(host, room, nick, s);
}

void PsiAccount::groupChatSetStatus(const QString &host, const QString &room, const Status &s)
{
	d->client->groupChatSetStatus(host, room, s);
}

void PsiAccount::groupChatLeave(const QString &host, const QString &room)
{
	Jid j(room + '@' + host);
	d->groupchats.removeAll(j.bare());
	d->client->groupChatLeave(host, room, PsiOptions::instance()->getOption("options.muc.leave-status-message").toString());
	UserListItem *u = find(j);
	if (u) {
		d->removeEntry(j);
		d->userList.removeAll(u);
	}
}

void PsiAccount::setLocalMucBookmarks(const QStringList &sl)
{
	d->acc.localMucBookmarks = sl;
}

void PsiAccount::setIgnoreMucBookmarks(const QStringList &sl)
{
	d->acc.ignoreMucBookmarks = sl;
}

QStringList PsiAccount::localMucBookmarks() const
{
	return d->acc.localMucBookmarks;
}

QStringList PsiAccount::ignoreMucBookmarks() const
{
	return d->acc.ignoreMucBookmarks;
}

GCContact *PsiAccount::findGCContact(const Jid &j) const
{
	foreach(GCContact *c, d->gcbank) {
		if(c->jid.compare(j))
			return c;
	}
	return 0;
}

Status PsiAccount::gcContactStatus(const Jid &j)
{
	GCContact *c = findGCContact(j);
	if (c) {
		return c->status;
	} else {
		return Status();
	}
}

QStringList PsiAccount::groupchats() const
{
	return d->groupchats;
}

void PsiAccount::client_groupChatJoined(const Jid &j)
{
#ifdef GROUPCHAT
	//d->client->groupChatSetStatus(j.host(), j.user(), d->loginStatus);

	GCMainDlg *m = findDialog<GCMainDlg*>(Jid(j.bare()));
	if(m) {
		m->setPassword(d->client->groupChatPassword(j.domain(), j.node()));
		m->joined();
		return;
	}
	MUCJoinDlg *w = findDialog<MUCJoinDlg*>(j);
	if(!w)
		return;
	MUCJoinDlg::MucJoinReason r = w->getReason();
	w->joined();

	openGroupChat(j, UserAction, r);
//	if(r == MUCJoinDlg::MucAutoJoin && PsiOptions::instance()->getOption("options.ui.muc.hide-on-autojoin").toBool()) {
//		m = findDialog<GCMainDlg*>(Jid(j.bare()));
//		if(m) {
//			QTimer::singleShot(0, m, SLOT(hideTab()));
//		}
//	}

#endif
}

void PsiAccount::client_groupChatLeft(const Jid &j)
{
	// remove all associated groupchat contacts from the bank
	for(QList<GCContact*>::Iterator it = d->gcbank.begin(); it != d->gcbank.end(); ) {
		GCContact *c = *it;

		// contact from this room?
		if(!c->jid.compare(j, false)) {
			++it;
			continue;
		}
		UserListItem *u = find(c->jid);
		if(!u) {
			++it;
			continue;
		}

		simulateContactOffline(u);
		it = d->gcbank.erase(it);
		delete c;
	}
}

void PsiAccount::client_groupChatPresence(const Jid &j, const Status &s)
{
#ifdef GROUPCHAT
	GCMainDlg *w = findDialog<GCMainDlg*>(Jid(j.bare()));
	if(!w)
		return;

	GCContact *c = findGCContact(j);
	if(!c) {
		c = new GCContact;
		c->jid = j;
		c->status = s;
		d->gcbank.append(c);
	}

	w->presence(j.resource(), s);

	// pass through the core presence handling also (so that roster items
	// from groupchat contacts get a resource as well
	Resource r;
	r.setName(j.resource());
	r.setStatus(s);
	if(s.isAvailable())
		client_resourceAvailable(j, r);
	else
		client_resourceUnavailable(j, j.resource());
#endif
}

void PsiAccount::client_groupChatError(const Jid &j, int code, const QString &str)
{
#ifdef GROUPCHAT
	GCMainDlg *w = findDialog<GCMainDlg*>(Jid(j.bare()));
	if(w) {
		w->error(code, str);
	}
	else {
		MUCJoinDlg *w = findDialog<MUCJoinDlg*>(j);
		if(w) {
			w->error(code, str);
		}
	}
#endif
}

QStringList PsiAccount::hiddenChats(const Jid &j) const
{
	QStringList list;

	foreach(ChatDlg* chat, findDialogs<ChatDlg*>(j, false))
		list += chat->jid().resource();

	return list;
}

void PsiAccount::slotCheckVCard()
{
	if (!isConnected() || !isActive())
		return;

	QString nick = d->jid.node();
	JT_VCard* j = static_cast<JT_VCard*>(sender());
	VCard vcard = j->vcard();
	bool changeOwn;
	if (j->success()) {
		if (!vcard.nickName().isEmpty()) {
			d->nickFromVCard = true;
			nick = vcard.nickName();
		} else if (!vcard.fullName().isEmpty()) {
			d->nickFromVCard = true;
			nick = vcard.fullName();
		}
		if (!vcard.photo().isEmpty()) {
			d->vcardPhotoUpdate(j->vcard().photo());
		}
		setNick(nick);

		changeOwn = vcard.isEmpty();
	} else {
		changeOwn = (j->statusCode() == Task::ErrDisc + 1 || j->statusCode() == 404);
	}

	if (changeOwn && PsiOptions::instance()->getOption("options.vcard.query-own-vcard-on-login").toBool()) {
		changeVCard();
	}
}

void PsiAccount::setNick(const QString &nick)
{
	d->self.setName(nick);
	cpUpdate(d->self);
	nickChanged();
}

QString PsiAccount::nick() const
{
	return d->self.name();
}

const Activity &PsiAccount::activity() const
{
	return d->self.activity();
}

const GeoLocation &PsiAccount::geolocation() const
{
	return d->self.geoLocation();
}

const Mood &PsiAccount::mood() const
{
	return d->self.mood();
}

//void PsiAccount::pgpToggled(bool b)
//{
//	QCA::PGPKey oldkey = d->cur_pgpSecretKey;
//
//	// gaining pgp?
//	if(b)
//		d->cur_pgpSecretKey = d->acc.pgpSecretKey;
//	// losing it?
//	else {
//		d->cur_pgpSecretKey = QCA::PGPKey();
//	}
//
//	if(!PGPUtil::instance().equals(oldkey,d->cur_pgpSecretKey)) {
//		pgpKeyChanged();
//		// resend status if online
//		if(loggedIn())
//			setStatusDirect(d->loginStatus);
//	}
//}

void PsiAccount::pgpKeysUpdated()
{
#ifdef HAVE_PGPUTIL
	// are there any sigs that need verifying?
	foreach(UserListItem* u, d->userList) {
		UserResourceList &rl = u->userResourceList();
		for(UserResourceList::Iterator rit = rl.begin(); rit != rl.end(); ++rit) {
			UserResource &r = *rit;
			if(!r.status().xsigned().isEmpty() && r.pgpVerifyStatus() == QCA::SecureMessageSignature::NoKey) {
				QCA::KeyStoreEntry e = PGPUtil::instance().getPublicKeyStoreEntry(r.publicKeyID());
				if (!e.isNull())
					tryVerify(u, &r);
			}
		}
	}
#else
	Q_ASSERT(false);
#endif
}

void PsiAccount::newPgpPassPhase(const QString &id, const QString &pass)
{
	const QString pgpSecretKeyID = (d->acc.pgpSecretKey.isNull() ? "" : d->acc.pgpSecretKey.keyId());
	if(pgpSecretKeyID == id) {
		d->acc.pgpPassPhrase = pass;
	}
}

void PsiAccount::trySignPresence()
{
	QCA::SecureMessageKey skey;
	skey.setPGPSecretKey(d->cur_pgpSecretKey);
#ifdef HAVE_PGPUTIL
	if(!d->cur_pgpSecretKey.isNull() && !d->acc.pgpPassPhrase.isEmpty()) {
		PGPUtil::instance().addPassphrase(d->cur_pgpSecretKey.keyId(), d->acc.pgpPassPhrase);
	}
#endif
	QByteArray plain = d->loginStatus.status().toUtf8();

	PGPTransaction *t = new PGPTransaction(new QCA::OpenPGP());
	connect(t, SIGNAL(finished()), SLOT(pgp_signFinished()));
	t->setFormat(QCA::SecureMessage::Ascii);
	t->setSigner(skey);
	t->startSign(QCA::SecureMessage::Detached);
	t->update(plain);
	t->end();
}

void PsiAccount::pgp_signFinished()
{
#ifdef HAVE_PGPUTIL
	PGPTransaction *t = (PGPTransaction*) sender();
	if (t->success()) {
		Status s = d->loginStatus;
		s.setXSigned(PGPUtil::instance().stripHeaderFooter(QString(t->signature())));
		setStatusActual(s);
	}
	else {
		// Clear passphrase from cache
		if (t->errorCode() == QCA::SecureMessage::ErrorPassphrase) {
			d->acc.pgpPassPhrase.clear();
			QCA::KeyStoreEntry ke = PGPUtil::instance().getSecretKeyStoreEntry(d->cur_pgpSecretKey.keyId());
			if (!ke.isNull())
				PGPUtil::instance().removePassphrase(ke.id());
		}

		PGPUtil::showDiagnosticText(tr("There was an error trying to sign your status.\nReason: %1.")
									.arg(PGPUtil::instance().messageErrorString(t->errorCode())),
									t->diagnosticText());

		logout(false, loggedOutStatus());
		return;
	}
	t->deleteLater();
#else
	Q_ASSERT(false);
#endif
}


void PsiAccount::verifyStatus(const Jid &j, const Status &s)
{
#ifdef HAVE_PGPUTIL
	PGPTransaction *t = new PGPTransaction(new QCA::OpenPGP());
	t->setJid(j);
	connect(t, SIGNAL(finished()), SLOT(pgp_verifyFinished()));
	t->startVerify(PGPUtil::instance().addHeaderFooter(s.xsigned(),1).toUtf8());
	t->update(s.status().toUtf8());
	t->end();
#endif
}


void PsiAccount::pgp_verifyFinished()
{
#ifdef HAVE_PGPUTIL
	PGPTransaction *t = (PGPTransaction*) sender();
	Jid j = t->jid();
	foreach(UserListItem *u, findRelevant(j)) {
		UserResourceList::Iterator rit = u->userResourceList().find(j.resource());
		bool found = (rit == u->userResourceList().end()) ? false: true;
		if(!found)
			continue;
		UserResource &ur = *rit;

		QCA::SecureMessageSignature signer;
		if(t->success()) {
			signer = t->signer();

			ur.setPublicKeyID(signer.key().pgpPublicKey().keyId());
			ur.setPGPVerifyStatus(signer.identityResult());
			ur.setSigTimestamp(signer.timestamp());

			if(u->publicKeyID().isEmpty() &&
			   PsiOptions::instance()->getOption("options.pgp.auto-assign").toBool()) {
				QString keyId = signer.key().pgpPublicKey().keyId();
				QCA::KeyStoreEntry key = PGPUtil::instance().getPublicKeyStoreEntry(keyId);
				if(!key.isNull())
				{
					u->setPublicKeyID(keyId);
				}
			}
		}
		else {
			ur.setPGPVerifyStatus(-1);
		}
		cpUpdate(*u);
	}

	t->deleteLater();
#endif
}

int PsiAccount::sendMessageEncrypted(const Message &_m)
{
#ifdef HAVE_PGPUTIL
	if(!ensureKey(_m.to()))
		return -1;

	QString keyID = findFirstRelevant(_m.to())->publicKeyID();
	QCA::KeyStoreEntry keyEntry = PGPUtil::instance().getPublicKeyStoreEntry(keyID);
	if (keyEntry.isNull())
		return -1;

	QCA::SecureMessageKey key;
	key.setPGPPublicKey(keyEntry.pgpPublicKey());

	PGPTransaction *t = new PGPTransaction(new QCA::OpenPGP());
	t->setMessage(_m);
	connect(t, SIGNAL(finished()), SLOT(pgp_encryptFinished()));
	t->setFormat(QCA::SecureMessage::Ascii);
	t->setRecipient(key);
	t->startEncrypt();
	t->update(_m.body().toUtf8());
	t->end();

	return t->id();
#else
	Q_ASSERT(false);
	return -1;
#endif
}

void PsiAccount::pgp_encryptFinished()
{
#ifdef HAVE_PGPUTIL
	PGPTransaction *pt = (PGPTransaction *)sender();
	int x = pt->id();

	if(pt->success()) {
		Message m = pt->message();
		// log the message here, before we encrypt it
		{
			MessageEvent::Ptr me(new MessageEvent(m, this));
			me->setOriginLocal(true);
			me->setTimeStamp(QDateTime::currentDateTime());
			logEvent(m.to(), me, EDB::Contact);
		}

		Message mwrap;
		mwrap.setTo(m.to());
		mwrap.setType(m.type());
		QString enc = PGPUtil::instance().stripHeaderFooter(pt->read());
		mwrap.setBody(tr("[ERROR: This message is encrypted, and you are unable to decrypt it.]"));
		mwrap.setXEncrypted(enc);
		mwrap.setWasEncrypted(true);
		// FIXME: Should be done cleaner, with an extra method in Iris
		if (m.containsEvent(OfflineEvent)) mwrap.addEvent(OfflineEvent);
		if (m.containsEvent(DeliveredEvent)) mwrap.addEvent(DeliveredEvent);
		if (m.containsEvent(DisplayedEvent)) mwrap.addEvent(DisplayedEvent);
		if (m.containsEvent(ComposingEvent)) mwrap.addEvent(ComposingEvent);
		if (m.containsEvent(CancelEvent)) mwrap.addEvent(CancelEvent);
		if (m.messageReceipt() == ReceiptRequest) mwrap.setMessageReceipt(ReceiptRequest);
		mwrap.setChatState(m.chatState());
		mwrap.setId(m.id());
		dj_sendMessage(mwrap);
	}
	emit encryptedMessageSent(x, pt->success(), pt->errorCode(), pt->diagnosticText());
	pt->deleteLater();
#endif
}


void PsiAccount::processEncryptedMessage(const Message &m)
{
#ifdef HAVE_PGPUTIL
	PGPTransaction *t = new PGPTransaction(new QCA::OpenPGP());
	t->setMessage(m);
	connect(t, SIGNAL(finished()), SLOT(pgp_decryptFinished()));
	t->setFormat(QCA::SecureMessage::Ascii);
	t->startDecrypt();
	t->update(PGPUtil::instance().addHeaderFooter(m.xencrypted(),0).toUtf8());
	t->end();
#endif
}


void PsiAccount::pgp_decryptFinished()
{
	PGPTransaction *pt = (PGPTransaction*) sender();
	bool tryAgain = false;
	if (pt->success()) {
		Message m = pt->message();
		m.setBody(QString::fromUtf8(pt->read()));
		m.setXEncrypted("");
		m.setWasEncrypted(true);
		processIncomingMessage(m);
	}
	else {
		if (loggedIn()) {
			Message m;
			m.setTo(pt->message().from());
			m.setType("error");
			if (!pt->message().id().isEmpty())
				m.setId(pt->message().id());
			m.setBody(pt->message().body());

			m.setError(Stanza::Error(Stanza::Error::Modify,
									 Stanza::Error::NotAcceptable,
									 "Unable to decrypt"));
			d->client->sendMessage(m);
		}
	}

	pt->deleteLater();

	if (tryAgain) {
		processEncryptedMessageNext();
	}
	else {
		processEncryptedMessageDone();
	}
}

void PsiAccount::processMessageQueue()
{
	while(!d->messageQueue.isEmpty()) {
		Message *mp = d->messageQueue.first();

#ifdef HAVE_PGPUTIL
		// encrypted?
		if(PGPUtil::instance().pgpAvailable() && !mp->xencrypted().isEmpty()) {
			processEncryptedMessageNext();
			break;
		}
#endif

		processIncomingMessage(*mp);
		d->messageQueue.removeAll(mp);
		delete mp;
	}
}

void PsiAccount::processEncryptedMessageNext()
{
	// 'peek' and try to process it
	Message *mp = d->messageQueue.first();
	processEncryptedMessage(*mp);
}

void PsiAccount::processEncryptedMessageDone()
{
	// 'pop' the message
	if (!d->messageQueue.isEmpty())
		delete d->messageQueue.takeFirst();

	// do the rest of the queue
	processMessageQueue();
}

void PsiAccount::optionsUpdate()
{
	PsiOptions *o = PsiOptions::instance();
	profileUpdateEntry(d->self);

	// Tune
#ifdef USE_PEP // Tune cleaning not working. It's implemented in psicon.cpp in PsiCon::optionChanged
	bool publish = o->getOption("options.extended-presence.tune.publish").toBool();
	if (!d->lastTune.isNull() && !publish) {
		publishTune(Tune());
	}
	else if (d->lastTune.isNull() && publish) {
		Tune current = d->psi->tuneManager()->currentTune();
		if (!current.isNull())
			publishTune(current);
	}
#endif

	// Remote Controlling
	setRCEnabled(o->getOption("options.external-control.adhoc-remote-control.enable").toBool());

	// Roster item exchange
	d->rosterItemExchangeTask->setIgnoreNonRoster(o->getOption("options.messages.ignore-non-roster-contacts").toBool());

	// Caps manager
	d->client->capsManager()->setEnabled(o->getOption("options.service-discovery.enable-entity-capabilities").toBool());

	updateFeatures();

	if (isConnected()) {
		setStatusActual(d->loginStatus);
	}
}


void PsiAccount::setRCEnabled(bool b)
{
	if (b && !d->rcSetStatusServer) {
		d->rcSetStatusServer = new RCSetStatusServer(d->ahcManager);
		d->rcForwardServer = new RCForwardServer(d->ahcManager);
		d->rcLeaveMucServer = new RCLeaveMucServer(d->ahcManager);
		d->rcSetOptionsServer = new RCSetOptionsServer(d->ahcManager, d->psi);
	}
	else if (!b && d->rcSetStatusServer) {
		delete d->rcSetStatusServer;
		d->rcSetStatusServer = 0;
		delete d->rcForwardServer;
		d->rcForwardServer = 0;
		delete d->rcLeaveMucServer;
		d->rcLeaveMucServer = 0;
		delete d->rcSetOptionsServer;
		d->rcSetOptionsServer = 0;
	}
}

void PsiAccount::invokeGCMessage(const Jid &j)
{
	GCContact *c = findGCContact(j);
	if(!c)
		return;

	// create dummy item, open chat, then destroy item.  HORRIBLE HACK!
	UserListItem *u = new UserListItem;
	u->setJid(j);
	u->setInList(false);
	u->setName(j.resource());
	u->setPrivate(true);
	u->setAvatarFactory(avatarFactory());

	// make a resource so the contact appears online
	UserResource ur;
	ur.setName(j.resource());
	ur.setStatus(c->status);
	u->userResourceList().append(ur);

	d->userList.append(u);
	actionSendMessage(j);
	d->userList.removeAll(u);
	delete u;
}

void PsiAccount::invokeGCChat(const Jid &j)
{
	GCContact *c = findGCContact(j);
	if(!c)
		return;

	// create dummy item, open chat, then destroy item.  HORRIBLE HACK!
	// note: we no longer destroy the item (commented out below)
	UserListItem *u = new UserListItem;
	u->setJid(j);
	u->setInList(false);
	u->setName(j.resource());
	u->setPrivate(true);
	u->setAvatarFactory(avatarFactory());

	// make a resource so the contact appears online
	UserResource ur;
	ur.setName(j.resource());
	ur.setStatus(c->status);
	u->userResourceList().append(ur);

	d->userList.append(u);
	actionOpenChat(j);
	cpUpdate(*u);
	//d->userList.removeAll(u);
	//delete u;
}

void PsiAccount::invokeGCInfo(const Jid &j)
{
	actionInfo(j);
}

void PsiAccount::invokeGCFile(const Jid &j)
{
	actionSendFile(j);
}

void PsiAccount::toggleSecurity(const Jid &j, bool b)
{
	UserListItem *u = findFirstRelevant(j);
	if(!u)
		return;

	bool isBare = j.resource().isEmpty();
	bool isPriority = false;

	// sick sick sick sick sick sick
	UserResource *r1, *r2;
	r1 = r2 = 0;
	UserResourceList::Iterator it1 = u->userResourceList().find(j.resource());
	UserResourceList::Iterator it2 = u->userResourceList().priority();
	r1 = (it1 != u->userResourceList().end() ? &(*it1) : 0);
	r2 = (it2 != u->userResourceList().end() ? &(*it2) : 0);
	if(r1 && (r1 == r2))
		isPriority = true;

	bool needUpdate = false;
	bool sec = u->isSecure(j.resource());
	if(sec != b) {
		u->setSecure(j.resource(), b);
		needUpdate = true;
	}
	if(isBare && r2) {
		sec = u->isSecure(r2->name());
		if(sec != b) {
			u->setSecure(r2->name(), b);
			needUpdate = true;
		}
	}
	if(isPriority) {
		sec = u->isSecure("");
		if(sec != b) {
			u->setSecure("", b);
			needUpdate = true;
		}
	}

	if(needUpdate) {
		cpUpdate(*u);
	}
}

bool PsiAccount::ensureKey(const Jid &j)
{
#ifdef HAVE_PGPUTIL
	if(!PGPUtil::instance().pgpAvailable())
		return false;

	UserListItem *u = findFirstRelevant(j);
	if(!u)
		return false;

	// no key?
	if(u->publicKeyID().isEmpty()) {
		// does the user have any presence signed with a key?
		QString akey;
		const UserResourceList &rl = u->userResourceList();
		for(UserResourceList::ConstIterator it = rl.begin(); it != rl.end(); ++it) {
			const UserResource &r = *it;
			if(!r.publicKeyID().isEmpty()) {
				akey = r.publicKeyID();
				break;
			}
		}

		if(akey.isEmpty() || PGPUtil::instance().getPublicKeyStoreEntry(akey).isNull()) {
			int n = QMessageBox::information(0, CAP(tr("No key")), tr(
				"<p>Psi was unable to locate the OpenPGP key to use for <b>%1</b>.<br>"
				"<br>"
				"This can happen if you do not have the key that the contact is advertising "
				"via signed presence, or if the contact is not advertising any key at all.</p>"
				).arg(JIDUtil::toString(u->jid(),true)), tr("&Choose key manually"), tr("Do &nothing"));
			if(n != 0)
				return false;
		}

		// Select a key
		PGPKeyDlg *w = new PGPKeyDlg(PGPKeyDlg::Public, akey, 0);
		w->setWindowTitle(tr("Public Key: %1").arg(JIDUtil::toString(j,true)));
		int r = w->exec();
		QCA::KeyStoreEntry entry;
		if(r == QDialog::Accepted)
			entry = w->keyStoreEntry();
		delete w;
		if(entry.isNull())
			return false;
		u->setPublicKeyID(entry.pgpPublicKey().keyId());
		cpUpdate(*u);
	}

	return true;
#else
	return false;
#endif
}

ServerInfoManager* PsiAccount::serverInfoManager()
{
	return d->serverInfoManager;
}

PEPManager* PsiAccount::pepManager()
{
	return d->pepManager;
}

BookmarkManager* PsiAccount::bookmarkManager()
{
	return d->bookmarkManager;
}

QStringList PsiAccount::groupList() const
{
	return d->groupList();
}

/**
 * FIXME: Make InfoDlg use some other way of updating items.
 */
void PsiAccount::updateEntry(const UserListItem& u)
{
	profileUpdateEntry(u);
}

AvCallManager *PsiAccount::avCallManager()
{
	return d->avCallManager;
}

/**
 * \brief Returns the current contents of the debug ringbuffer.
 * it doesn't clear the ringbuffer
 * \return a QList<xmlRingElem> of the current debug buffer items.
 */
QList<PsiAccount::xmlRingElem> PsiAccount::dumpRingbuf()
{
	return d->dumpRingbuf();
}

/**
 * Frees ringbuffer memory and makes it compact.
 */
void PsiAccount::clearRingbuf()
{
	d->xmlRingbuf.clear();
	d->xmlRingbufWrite = 0;
	d->xmlRingbuf.resize(10);
}

/**
 * Helper to prevent automated outgoing presences from happening
 */
void PsiAccount::resetLastManualStatusSafeGuard()
{
	d->setManualStatus(XMPP::Status());
}

ContactProfile *PsiAccount::contactProfile() const
{
	return 0;
}

bool PsiAccount::usingSSL() const
{
	return d->usingSSL;
}

void PsiAccount::profileUpdateEntry(const UserListItem& u)
{
	d->updateEntry(u);
}

void PsiAccount::profileRemoveEntry(const Jid& jid)
{
	d->removeEntry(jid);
}

void PsiAccount::profileAnimateNick(const Jid& jid)
{
	d->animateNick(jid);
}

void PsiAccount::profileSetAlert(const Jid& jid, const PsiIcon* icon)
{
	d->setAlert(jid, icon);
}

void PsiAccount::profileClearAlert(const Jid& jid)
{
	d->clearAlert(jid);
}

bool PsiAccount::alerting() const
{
	return d->alerting();
}

QIcon PsiAccount::alertPicture() const
{
	Q_ASSERT(alerting());
	if (!alerting())
		return QIcon();
	return d->currentAlertFrame();
}

void PsiAccount::ed_addAuth(const Jid &j)
{
	dj_addAuth(j);
	if (static_cast<EventDlg*>(sender())->isForAll()) {
		QList<PsiEvent::Ptr> events;
		d->eventQueue->extractByType(PsiEvent::Auth, &events);
		foreach (PsiEvent::Ptr e, events) {
			dj_addAuth(e->jid());
		}
	}
}

void PsiAccount::ed_deny(const Jid &j)
{
	dj_deny(j);
	if (static_cast<EventDlg*>(sender())->isForAll()) {
		QList<PsiEvent::Ptr> events;
		d->eventQueue->extractByType(PsiEvent::Auth, &events);
		foreach (PsiEvent::Ptr e, events) {
			dj_deny(e->jid());
		}
	}
}


#include "psiaccount.moc"

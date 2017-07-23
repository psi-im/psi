/*
 * psiaccount.h - handles a Psi account
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

#ifndef PSIACCOUNT_H
#define PSIACCOUNT_H

#include <QList>
#include <QUrl>

#include "xmpp_rosterx.h"
#include "xmpp_status.h"
#include "psiactions.h"
#include "psievent.h"
#include "mood.h"
#include "activity.h"
#include "geolocation.h"
#include "mucjoindlg.h"

namespace XMPP
{
	class Jid;
	class XData;
	class AdvancedConnector;
	class Stream;
	class QCATLSHandler;
	class PubSubItem;
	class PubSubRetraction;
	class RosterItem;
	class Client;
	//class StreamError;
	class Resource;
	class Message;
};

using namespace XMPP;

class PsiCon;
class PsiContact;
class PsiContactList;
class PsiAccount;
class PsiHttpAuthRequest;
class Tune;
class BookmarkManager;
class URLBookmark;
class ConferenceBookmark;
class VoiceCaller;
class UserAccount;
class ContactProfile;
class QWidget;
class QString;
class EventQueue;
class UserResource;
class UserListItem;
class UserList;
class EventDlg;
class ChatDlg;
class PrivacyManager;
class EDB;
class QSSLCert;
class QHostAddress;
class AvatarFactory;
class PEPManager;
class ServerInfoManager;
class TabManager;
#ifdef GOOGLE_FT
class GoogleFileTransfer;
#endif
class PsiIcon;
class QIcon;
#ifdef WHITEBOARDING
class WbManager;
#endif

// sick sick remove this someday please!
struct GCContact;

class AvCallManager;

class PsiAccount : public QObject
{
	Q_OBJECT
protected:
	PsiAccount(const UserAccount &acc, PsiContactList *parent, TabManager *tabManager);
	virtual void init();

public:
	static PsiAccount* create(const UserAccount &acc, PsiContactList *parent, TabManager *tabManager);
	virtual ~PsiAccount();

	bool enabled() const;
	void setEnabled(bool e = true);

	bool isAvailable() const;
	bool isActive() const;
	bool isConnected() const;
	const QString &name() const;
	const QString &id() const;

	bool alerting() const;
	QIcon alertPicture() const;

	const UserAccount & userAccount() const;
	UserAccount accountOptions() const;
	virtual void setUserAccount(const UserAccount &);
	const Jid & jid() const;
	QString nameWithJid() const;

	void updateFeatures();
	XMPP::Client *client() const;
	virtual ContactProfile *contactProfile() const;
	EventQueue *eventQueue() const;
	EDB *edb() const;
	PsiCon *psi() const;
	AvatarFactory *avatarFactory() const;
	PrivacyManager* privacyManager() const;
	VoiceCaller* voiceCaller() const;
#ifdef WHITEBOARDING
	WbManager* wbManager() const;
#endif
	Status status() const;
	static Status loggedOutStatus();
	int defaultPriority(const XMPP::Status &);
	void setStatusDirect(const XMPP::Status &, bool withPriority = false);
	void setStatusActual(const XMPP::Status &);

	enum AutoAway {
		AutoAway_None = 0,
		AutoAway_Away,
		AutoAway_XA,
		AutoAway_Offline
	};

	void setAutoAwayStatus(AutoAway status);

	bool noPopup() const;
	bool loggedIn() const;
	void setNick(const QString &);
	QString nick() const;
	const Mood &mood() const;
	const Activity &activity() const;
	const GeoLocation &geolocation() const;
	void forceDisconnect(bool fast, const XMPP::Status &s);
	bool hasPGP() const;
	QHostAddress *localAddress() const;
	void passwordReady(QString password);

	ChatDlg* findChatDialog(const Jid& jid, bool compareResource = true) const;
	ChatDlg* findChatDialogEx(const Jid& jid, bool ignoreResource = false) const;
	QList<ChatDlg*> findChatDialogs(const Jid& jid, bool compareResource = true) const;

	QList<PsiContact*> activeContacts() const;

	template<typename T>
	inline T findDialog(const Jid& jid = Jid(), bool compareResource = true) const {
		return static_cast<T>(findDialog(((T)0)->staticMetaObject, jid, compareResource));
	}
	template<typename T>
	inline QList<T> findDialogs(const Jid& jid = Jid(), bool compareResource = true) const {
		QList<T> list;
		findDialogs(((T)0)->staticMetaObject,
		            jid, compareResource,
		            reinterpret_cast<QList<void*>*>(&list));
		return list;
	}
	template<typename T>
	inline QList<T> findAllDialogs() const {
		QList<T> list;
		findDialogs(((T)0)->staticMetaObject,
		            reinterpret_cast<QList<void*>*>(&list));
		return list;
	}

	void dialogRegister(QWidget* w, const Jid& jid = Jid());
	void dialogUnregister(QWidget* w);

	bool notifyOnline() const;

	void updateAlwaysVisibleContact(PsiContact* pc);

	void modify();
	void reconfigureFTManager();
	void changeVCard();
	void changePW();
	void changeStatus(int, bool forceDialog = false);
	void doDisco();

	void showXmlConsole();
	void openAddUserDlg();
	void openAddUserDlg(const XMPP::Jid &jid, const QString &nick, const QString &group);
	void openGroupChat(const Jid &, ActivationType activationType, MUCJoinDlg::MucJoinReason reason = MUCJoinDlg::MucCustomJoin);
	bool groupChatJoin(const QString &host, const QString &room, const QString &nick, const QString& pass, bool nohistory = false);
	void groupChatSetStatus(const QString &host, const QString &room, const Status &);
	void groupChatChangeNick(const QString &host, const QString &room, const QString& nick, const Status &);
	void groupChatLeave(const QString &host, const QString &room);
	void setLocalMucBookmarks(const QStringList &sl);
	void setIgnoreMucBookmarks(const QStringList &sl);
	QStringList localMucBookmarks() const;
	QStringList ignoreMucBookmarks() const;

	Jid realJid(const Jid &j) const;
	PsiContact* selfContact() const;
	const QList<PsiContact*>& contactList() const;
	int onlineContactsCount() const;
	PsiContact* findContact(const Jid& jid) const;
	UserListItem *find(const Jid &) const;
	QList<UserListItem*> findRelevant(const Jid &) const;
	UserListItem *findFirstRelevant(const Jid &) const;
	UserList *userList() const;
	bool usingSSL() const;

	bool checkConnected(QWidget *parent=0);

	enum SoundType {
		eNone = -1,

		eMessage = 0,
		eChat1,
		eChat2,
		eGroupChat,
		eHeadline,
		eSystem,
		eOnline,
		eOffline,
		eSend,
		eIncomingFT,
		eFTComplete,
		eSoundLast
	};
	void playSound(SoundType onevent);

#ifdef PSI_PLUGINS
	void createNewPluginEvent(int account, const QString& jid, const QString& descr, QObject *receiver, const char* slot);
#endif

	QStringList hiddenChats(const Jid &) const;

	int sendMessageEncrypted(const Message &);

	// sucks sucks sucks sucks sucks sucks sucks
	GCContact *findGCContact(const Jid &j) const;
	XMPP::Status gcContactStatus(const Jid &j);
	QStringList groupchats() const;

	void toggleSecurity(const Jid &, bool);
	bool ensureKey(const Jid &);
	void tryVerify(UserListItem *, UserResource *);

	static void getErrorInfo(int err, AdvancedConnector *conn, Stream *stream, QCATLSHandler *tlsHandler, QString *_str, bool *_reconn, bool *_badPass, bool *_disableAutoConnect, bool *_isAuthError, bool *_isTemporaryAuthFailure);

	void deleteQueueFile();
	void sendFiles(const Jid&, const QStringList&, bool direct = false);

	PEPManager* pepManager();
	ServerInfoManager* serverInfoManager();
	BookmarkManager* bookmarkManager();
	AvCallManager *avCallManager();

	void clearCurrentConnectionError();
	QString currentConnectionError() const;
	int currentConnectionErrorCondition() const;

	enum xmlRingType {RingXmlIn, RingXmlOut, RingSysMsg};
	class xmlRingElem { public: int type; QDateTime time; QString xml; };
	QList< xmlRingElem > dumpRingbuf();
	void clearRingbuf();
	void addMucItem(const Jid&);

signals:
	void accountDestroyed();
	void connectionError(const QString& errorInfo);
	void disconnected();
	void reconnecting();
	void updatedActivity();
	void updatedAccount();
	void queueChanged();
	void updateContact(const UserListItem &);
	void updateContact(const Jid &);
	void updateContact(const Jid &, bool);
	void removeContact(const Jid &);
	void nickChanged();
	void pgpKeyChanged();
	void encryptedMessageSent(int, bool, int, const QString &);
	void enabledChanged();
	void startBounce();

public slots:
	void setStatus(const XMPP::Status &, bool withPriority = false, bool isManualStatus = false);
	void showStatusDialog(const QString& presetName);

	void capsChanged(const Jid&);
	void tuneStopped();
	void tunePlaying(const Tune&);

	void incomingVoiceCall(const Jid&);

	void openNextEvent(ActivationType activationType);
	int forwardPendingEvents(const Jid &jid);
	void autoLogin();
	void reconnectOnce();

	void showCert();

	void openUri(const QUrl &uri);

	//dj_ originally referred to 'direct jabber', if you care
	void dj_sendMessage(const Message &, bool log=true);
	void dj_newMessage(const Jid &jid, const QString &body, const QString &subject, const QString &thread);
	void dj_replyMessage(const Jid &jid, const QString &body);
	void dj_replyMessage(const Jid &jid, const QString &body, const QString &subject, const QString &thread);
	void dj_addAuth(const Jid &);
	void dj_addAuth(const Jid &, const QString&);
	void dj_add(const XMPP::Jid &, const QString &, const QStringList &, bool authReq);
	void dj_authReq(const Jid &);
	void dj_auth(const Jid &);
	void dj_deny(const Jid &);
	void dj_rename(const Jid &, const QString &);
	void dj_remove(const Jid &);
	void dj_confirmHttpAuth(const PsiHttpAuthRequest &);
	void dj_denyHttpAuth(const PsiHttpAuthRequest &);
	void dj_rosterExchange(const RosterExchangeItems&);
	void dj_formSubmit(const XData&, const QString&, const Jid&);
	void dj_formCancel(const XData&, const QString&, const Jid&);

	void actionDefault(const Jid &);
	void actionRecvEvent(const Jid &);
	void actionRecvRosterExchange(const Jid&, const RosterExchangeItems&);
	void actionSendMessage(const Jid &);
	void actionSendMessage(const QList<XMPP::Jid> &);
	void actionSendUrl(const Jid &);
	void actionRemove(const Jid &);
	void actionRename(const Jid &, const QString &);
	void actionGroupRename(const QString &, const QString &);
	void actionHistory(const Jid &);
	ChatDlg *actionOpenChat(const Jid &);
	void actionOpenSavedChat(const Jid &);
	void actionOpenChat2(const Jid &);
	void actionOpenChatSpecific(const Jid &);
	void actionOpenPassiveChatSpecific(const Jid &);
#ifdef WHITEBOARDING
	void actionOpenWhiteboard(const Jid &);
	void actionOpenWhiteboardSpecific(const Jid &, Jid = Jid(), bool = false);
#endif
	void actionAgentSetStatus(const Jid &, const Status &s);
	void actionInfo(const Jid &, bool showStatusInfo=true);
	void actionAuth(const Jid &);
	void actionAuthRequest(const Jid &);
	void actionAuthRemove(const Jid &);
	void actionAdd(const Jid &);
	void actionGroupAdd(const Jid &, const QString &);
	void actionGroupRemove(const Jid &, const QString &);
	void actionGroupsSet(const Jid &, const QStringList &);
	void actionHistoryBox(const PsiEvent::Ptr &e);
	void actionRegister(const Jid &);
	void actionUnregister(const Jid &);
	void actionSearch(const Jid &);
	void actionManageBookmarks();
	void actionJoin(const Jid& mucJid, const QString& password = QString());
	void actionJoin(const ConferenceBookmark& bookmark, bool connectImmediately, MUCJoinDlg::MucJoinReason reason = MUCJoinDlg::MucCustomJoin);
	void actionDisco(const Jid &, const QString &);
	void actionInvite(const Jid &, const QString &);
	void actionVoice(const Jid&);
	void actionSendFile(const Jid &);
	void actionSendFiles(const Jid &, const QStringList&);
	void actionExecuteCommand(const Jid& j, const QString& = QString());
	void actionExecuteCommandSpecific(const Jid&, const QString& = QString());
	void actionSetBlock(bool);
	void actionSetMood();
	void actionSetActivity();
	void actionSetGeoLocation();
	void actionSetAvatar();
	void actionUnsetAvatar();
	void actionQueryVersion(const Jid& j);
	void featureActivated(QString feature, Jid jid, QString node);
	void actionSendStatus(const Jid &jid);

	void actionAssignKey(const Jid &);
	void actionUnassignKey(const Jid &);

	void invokeGCMessage(const Jid &);
	void invokeGCChat(const Jid &);
	void invokeGCInfo(const Jid &);
	void invokeGCFile(const Jid &);

private slots:
	void tls_handshaken();
	void cs_connected();
	void cs_securityLayerActivated(int);
	void cs_needAuthParams(bool, bool, bool);
	void cs_authenticated();
	void cs_connectionClosed();
	void cs_delayedCloseFinished();
	void cs_warning(int);
	void cs_error(int);
	void client_rosterRequestFinished(bool, int, const QString &);
	void resolveContactName();
	void client_rosterItemAdded(const RosterItem &);
	void client_rosterItemUpdated(const RosterItem &);
	void client_rosterItemRemoved(const RosterItem &);
	void client_resourceAvailable(const Jid &, const Resource &);
	void client_resourceUnavailable(const Jid &, const Resource &);
	void client_presenceError(const Jid &, int, const QString &);
	void client_messageReceived(const Message &);
	void client_subscription(const Jid &, const QString &, const QString&);
	void client_debugText(const QString &);
	void client_groupChatJoined(const Jid &);
	void client_groupChatLeft(const Jid &);
	void client_groupChatPresence(const Jid &, const Status &);
	void client_groupChatError(const Jid &, int, const QString &);
#ifdef GOOGLE_FT
	void incomingGoogleFileTransfer(GoogleFileTransfer* ft);
#endif
	void client_incomingFileTransfer();
	void sessionStart_finished();

	void serverFeaturesChanged();
	void setPEPAvailable(bool);

	void bookmarksAvailabilityChanged();

	void incomingHttpAuthRequest(const PsiHttpAuthRequest &);

	void reconnectOncePhase2();
	void reconnect();
	void disconnect();
	void enableNotifyOnline();

#ifdef WHITEBOARDING
	void wbRequest(const Jid& j, int id);
#endif

	void itemPublished(const Jid&, const QString&, const PubSubItem&);
	void itemRetracted(const Jid&, const QString&, const PubSubRetraction&);

	void chatMessagesRead(const Jid &);
#ifdef GROUPCHAT
	void groupChatMessagesRead(const Jid &);
#endif

	void slotCheckVCard();
	void edb_finished();
	//void pgpToggled(bool);
	void pgpKeysUpdated();

	void trySignPresence();
	void pgp_signFinished();
	void pgp_verifyFinished();
	void pgp_encryptFinished();
	void pgp_decryptFinished();

	void ed_addAuth(const Jid &);
	void ed_deny(const Jid &);

	void optionsUpdate();

	void processReadNext(const UserListItem &);
	void processReadNext(const Jid &);
	void queryVersionFinished();

protected:
	bool validRosterExchangeItem(const RosterExchangeItem&);
	QString localHostName();

	void publishTune(const Tune&);
	void setRCEnabled(bool);
	void sessionStarted();

	virtual void stateChanged();
	virtual void profileUpdateEntry(const UserListItem& u);
	virtual void profileRemoveEntry(const Jid& jid);
	virtual void profileAnimateNick(const Jid& jid);
	virtual void profileSetAlert(const Jid& jid, const PsiIcon* icon);
	virtual void profileClearAlert(const Jid& jid);

private slots:
	void eventFromXml(const PsiEvent::Ptr &e);
	void simulateContactOffline(const XMPP::Jid& contact);
	void newPgpPassPhase(const QString& id, const QString& pass);

private:
	void handleEvent(const PsiEvent::Ptr &e, ActivationType activationType);

public:
	QStringList groupList() const;
	void updateEntry(const UserListItem& u);

	void resetLastManualStatusSafeGuard();

signals:
	void addedContact(PsiContact*);
	void removedContact(PsiContact*);

	void beginBulkContactUpdate();
	void endBulkContactUpdate();
	void rosterRequestFinished();

public:
	class Private;
private:
	Private *d;

	void login();
	void logout(bool fast, const Status &s);

	void deleteAllDialogs();
	void simulateContactOffline(UserListItem *);
	void simulateRosterOffline();
	void cpUpdate(const UserListItem &, const QString &rname="", bool fromPresence=false);
	UserListItem* addUserListItem(const Jid& jid, const QString& nick="");
	void logEvent(const Jid &, const PsiEvent::Ptr &, int);
	void queueEvent(const PsiEvent::Ptr &e, ActivationType activationType);
	void openNextEvent(const UserListItem &, ActivationType activationType);
	void updateReadNext(const Jid &);
	ChatDlg *ensureChatDlg(const Jid &);
	void lastStepLogin();
	void processIncomingMessage(const Message &);
	void processEncryptedMessage(const Message &);
	void processMessageQueue();
	void processEncryptedMessageNext();
	void processEncryptedMessageDone();
	void verifyStatus(const Jid &j, const Status &s);
	inline void passwordPrompt();
	void sentInitialPresence();
	void requestAvatarsForAllContacts();

	void processChatsHelper(const Jid& jid, bool removeEvents);
	void processChats(const Jid &);
	ChatDlg *openChat(const Jid &, ActivationType activationType);
	EventDlg *ensureEventDlg(const Jid &);
	friend class PsiCon;

	bool isDisconnecting, notifyOnlineOk, doReconnect, rosterDone, presenceSent, v_isActive;
	void cleanupStream();

	QWidget* findDialog(const QMetaObject& mo, const Jid& jid, bool compareResource) const;
	void findDialogs(const QMetaObject& mo, const Jid& jid, bool compareResource, QList<void*>* list) const;
	void findDialogs(const QMetaObject& mo, QList<void*>* list) const;

	friend class Private;
};

#endif

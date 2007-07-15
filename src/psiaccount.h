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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef PSIACCOUNT_H
#define PSIACCOUNT_H

#include <QList>

#include "xmpp_rosterx.h"
#include "xmpp_status.h"

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
class PsiContactList;
class PsiAccount;
class PsiEvent;
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
class CapsManager;
class EDB;
class QSSLCert;
class QHostAddress;
class AvatarFactory;
class PEPManager;
class ServerInfoManager;
#ifdef GOOGLE_FT
class GoogleFileTransfer;
#endif

// sick sick remove this someday please!
struct GCContact;

class PsiAccount : public QObject
{
	Q_OBJECT
public:
	PsiAccount(const UserAccount &acc, PsiContactList *parent);
	~PsiAccount();

	bool enabled() const;
	void setEnabled(bool e = TRUE);

	bool isActive() const;
	bool isConnected() const;
	const QString &name() const;

	const UserAccount & userAccount() const;
	void setUserAccount(const UserAccount &);
	const Jid & jid() const;
	QString nameWithJid() const;

	XMPP::Client *client() const;
	ContactProfile *contactProfile() const;
	EventQueue *eventQueue() const;
	EDB *edb() const;
	PsiCon *psi() const;
	AvatarFactory *avatarFactory() const;
	PrivacyManager* privacyManager() const;
	CapsManager* capsManager() const;
	VoiceCaller* voiceCaller() const;
	Status status() const;
	void setStatusDirect(const Status &, bool withPriority = false);
	void setStatusActual(const Status &);
	void login();
	void logout(bool fast=false, const Status &s = Status("", "Logged out", 0, false));
	bool loggedIn() const;
	void setNick(const QString &);
	QString nick() const;
	bool hasPGP() const;
	QHostAddress *localAddress() const;

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

	void dialogRegister(QWidget* w, const Jid& jid = Jid());
	void dialogUnregister(QWidget* w);

	void modify();
	void changeVCard();
	void changePW();
	void changeStatus(int);
	void doDisco();

	void showXmlConsole();
	void openAddUserDlg();
	void openGroupChat(const Jid &);
	bool groupChatJoin(const QString &host, const QString &room, const QString &nick, const QString& pass, bool nohistory = false);
	void groupChatSetStatus(const QString &host, const QString &room, const Status &);
	void groupChatChangeNick(const QString &host, const QString &room, const QString& nick, const Status &);
	void groupChatLeave(const QString &host, const QString &room);

	UserListItem *find(const Jid &) const;
	QList<UserListItem*> findRelevant(const Jid &) const;
	UserListItem *findFirstRelevant(const Jid &) const;
	UserList *userList() const;

	bool checkConnected(QWidget *parent=0);
	void playSound(const QString &);

	QStringList hiddenChats(const Jid &) const;

	int sendMessageEncrypted(const Message &);

	// sucks sucks sucks sucks sucks sucks sucks
	GCContact *findGCContact(const Jid &j);
	QStringList groupchats() const;

	void toggleSecurity(const Jid &, bool);
	bool ensureKey(const Jid &);
	void tryVerify(UserListItem *, UserResource *);

	static void getErrorInfo(int err, AdvancedConnector *conn, Stream *stream, QCATLSHandler *tlsHandler, QString *_str, bool *_reconn);

	void deleteQueueFile();
	void sendFiles(const Jid&, const QStringList&, bool direct = false);
	
	PEPManager* pepManager();
	ServerInfoManager* serverInfoManager();
	BookmarkManager* bookmarkManager();

signals:
	void disconnected();
	void reconnecting();
	void updatedActivity();
	void updatedAccount();
	void queueChanged();
	void updateContact(const UserListItem &);
	void updateContact(const Jid &);
	void updateContact(const Jid &, bool);
	void nickChanged();
	void pgpKeyChanged();
	void encryptedMessageSent(int, bool, int);
	void enabledChanged();

public slots:
	void setStatus(const XMPP::Status &, bool withStatus = false);

	void capsChanged(const Jid&);
	void tuneStopped();
	void tunePlaying(const Tune&);

	void incomingVoiceCall(const Jid&);
	
	void secondsIdle(int);
	void openNextEvent();
	int forwardPendingEvents(const Jid &jid);
	void autoLogin();

	void showCert();

	//dj_ originally referred to 'direct jabber', if you care
	void dj_sendMessage(const Message &, bool log=true);
	void dj_composeMessage(const Jid &jid, const QString &body);
	void dj_composeMessage(const Jid &jid, const QString &body, const QString &subject, const QString &thread);
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
	void actionOpenChat(const Jid &);
	void actionOpenChatSpecific(const Jid &);
#ifdef WHITEBOARDING
	void actionOpenWhiteboard(const Jid &);
	void actionOpenWhiteboardSpecific(const Jid &, Jid = Jid(), bool = false);
#endif
	void actionAgentSetStatus(const Jid &, Status &s);
	void actionInfo(const Jid &, bool showStatusInfo=true);
	void actionAuth(const Jid &);
	void actionAuthRequest(const Jid &);
	void actionAuthRemove(const Jid &);
	void actionAdd(const Jid &);
	void actionGroupAdd(const Jid &, const QString &);
	void actionGroupRemove(const Jid &, const QString &);
	void actionHistoryBox(PsiEvent *);
	void actionRegister(const Jid &);
	void actionSearch(const Jid &);
	void actionJoin(const Jid &, const QString & = QString());
	void actionDisco(const Jid &, const QString &);
	void actionInvite(const Jid &, const QString &);
	void actionVoice(const Jid&);
	void actionSendFile(const Jid &);
	void actionSendFiles(const Jid &, const QStringList&);
	void actionExecuteCommand(const Jid& j, const QString& = QString());
	void actionExecuteCommandSpecific(const Jid&, const QString& = QString());
	void actionSetMood();
	void actionSetAvatar();
	void actionUnsetAvatar();
	void featureActivated(QString feature, Jid jid, QString node);

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

	void getBookmarks_success(const QList<URLBookmark>&, const QList<ConferenceBookmark>&);

	void incomingHttpAuthRequest(const PsiHttpAuthRequest &);

	void reconnect();
	void disconnect();
	void enableNotifyOnline();
	
	void itemPublished(const Jid&, const QString&, const PubSubItem&);
	void itemRetracted(const Jid&, const QString&, const PubSubRetraction&);
	
	void chatMessagesRead(const Jid &);

	void slotCheckVCard();
	void edb_finished();
	//void pgpToggled(bool);
	void pgpKeysUpdated();

	void trySignPresence();
	void pgp_signFinished();
	void pgp_verifyFinished();
	void pgp_encryptFinished();
	void pgp_decryptFinished();
	
	void optionsUpdate();

	void processReadNext(const UserListItem &);
	void processReadNext(const Jid &);

protected:
	bool validRosterExchangeItem(const RosterExchangeItem&);
	QString localHostName();

	void publishTune(const Tune&);
	void setSendChatState(bool);
	void setRCEnabled(bool);
	void sessionStarted();

private slots:
	void handleEvent(PsiEvent *);

public:
	class Private;
private:
	Private *d;

	void deleteAllDialogs();
	void stateChanged();
	void simulateContactOffline(UserListItem *);
	void simulateRosterOffline();
	void cpUpdate(const UserListItem &, const QString &rname="", bool fromPresence=false);
	void logEvent(const Jid &, PsiEvent *);
	void queueEvent(PsiEvent *);
	void openNextEvent(const UserListItem &);
	void updateReadNext(const Jid &);
	ChatDlg *ensureChatDlg(const Jid &);
	void lastStepLogin();
	void processIncomingMessage(const Message &);
	void processEncryptedMessage(const Message &);
	void processMessageQueue();
	void processEncryptedMessageNext();
	void processEncryptedMessageDone();
	void verifyStatus(const Jid &j, const Status &s);

	void processChats(const Jid &);
	void openChat(const Jid &);
	EventDlg *ensureEventDlg(const Jid &);
	friend class PsiCon;

	bool isDisconnecting, notifyOnlineOk, doReconnect, usingAutoStatus, rosterDone, presenceSent, v_isActive;
	void cleanupStream();

	QWidget* findDialog(const QMetaObject& mo, const Jid& jid, bool compareResource) const;
	void findDialogs(const QMetaObject& mo, const Jid& jid, bool compareResource, QList<void*>* list) const;

	friend class Private;
};

#endif

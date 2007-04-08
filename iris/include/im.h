/*
 * im.h - XMPP "IM" library API
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef XMPP_IM_H
#define XMPP_IM_H

#include <qdatetime.h>
//Added by qt3to4:
#include <QList>
#include "xmpp.h"

namespace XMPP
{
	class Url
	{
	public:
		Url(const QString &url="", const QString &desc="");
		Url(const Url &);
		Url & operator=(const Url &);
		~Url();

		QString url() const;
		QString desc() const;

		void setUrl(const QString &);
		void setDesc(const QString &);

	private:
		class Private;
		Private *d;
	};

	class Address
	{
	public:
		typedef enum { Unknown, To, Cc, Bcc, ReplyTo, ReplyRoom, NoReply, OriginalFrom, OriginalTo } Type;

		Address(Type type = Unknown, const Jid& jid = Jid());
		Address(const QDomElement&);

		const Jid& jid() const;
		const QString& uri() const;
		const QString& node() const;
		const QString& desc() const;
		bool delivered() const;
		Type type() const;
		
		QDomElement toXml(Stanza&) const;
		void fromXml(const QDomElement& t);

		void setJid(const Jid &);
		void setUri(const QString &);
		void setNode(const QString &);
		void setDesc(const QString &);
		void setDelivered(bool);
		void setType(Type);

	private:
		Jid v_jid;
		QString v_uri, v_node, v_desc;
		bool v_delivered;
		Type v_type;
	};
	
	typedef QList<Url> UrlList;
	typedef QList<Address> AddressList;
	typedef QMap<QString, QString> StringMap;
	typedef enum { OfflineEvent, DeliveredEvent, DisplayedEvent,
			ComposingEvent, CancelEvent } MsgEvent;
	typedef enum { StateNone, StateActive, StateComposing, StatePaused, 
			StateInactive, StateGone } ChatState;
	
	class PubSubItem 
	{
	public:
		PubSubItem();
		PubSubItem(const QString& id, const QDomElement& payload);
		const QString& id() const;
		const QDomElement& payload() const;

	private:
		QString id_;
		QDomElement payload_;
	};

	class MUCItem
	{
	public:
		enum Affiliation { UnknownAffiliation, Outcast, NoAffiliation, Member, Admin, Owner };
		enum Role { UnknownRole, NoRole, Visitor, Participant, Moderator };

		MUCItem(Role = UnknownRole, Affiliation = UnknownAffiliation);
		MUCItem(const QDomElement&);

		void setNick(const QString&);
		void setJid(const Jid&);
		void setAffiliation(Affiliation);
		void setRole(Role);
		void setActor(const Jid&);
		void setReason(const QString&);

		const QString& nick() const;
		const Jid& jid() const;
		Affiliation affiliation() const;
		Role role() const;
		const Jid& actor() const;
		const QString& reason() const;

		void fromXml(const QDomElement&);
		QDomElement toXml(QDomDocument&);

		bool operator==(const MUCItem& o);

	private:
		QString nick_;
		Jid jid_, actor_;
		Affiliation affiliation_;
		Role role_;	
		QString reason_;
	};
	
	class MUCInvite
	{
	public:
		MUCInvite();
		MUCInvite(const QDomElement&);
		MUCInvite(const Jid& to, const QString& reason = QString());

		const Jid& to() const;
		void setTo(const Jid&);
		const Jid& from() const;
		void setFrom(const Jid&);
		const QString& reason() const;
		void setReason(const QString&);
		bool cont() const;
		void setCont(bool);


		void fromXml(const QDomElement&);
		QDomElement toXml(QDomDocument&) const;
		bool isNull() const;
		
	private:
		Jid to_, from_;
		QString reason_, password_;
		bool cont_;
	};
	
	class MUCDecline
	{
	public:
		MUCDecline();
		MUCDecline(const Jid& to, const QString& reason);
		MUCDecline(const QDomElement&);

		const Jid& to() const;
		void setTo(const Jid&);
		const Jid& from() const;
		void setFrom(const Jid&);
		const QString& reason() const;
		void setReason(const QString&);

		void fromXml(const QDomElement&);
		QDomElement toXml(QDomDocument&) const;
		bool isNull() const;
		
	private:
		Jid to_, from_;
		QString reason_;
	};
	
	class MUCDestroy
	{
	public:
		MUCDestroy();
		MUCDestroy(const QDomElement&);

		const Jid& jid() const;
		void setJid(const Jid&);
		const QString& reason() const;
		void setReason(const QString&);

		void fromXml(const QDomElement&);
		QDomElement toXml(QDomDocument&) const;
		
	private:
		Jid jid_;
		QString reason_;
	};

	class RosterExchangeItem
	{
	public:
		enum Action { Add, Delete, Modify };

		RosterExchangeItem(const Jid& jid, const QString& name = "", const QStringList& groups = QStringList(), Action = Add);
		RosterExchangeItem(const QDomElement&);
		
		const Jid& jid() const;
		Action action() const;
		const QString& name() const;
		const QStringList& groups() const;
		bool isNull() const;

		void setJid(const Jid&);
		void setAction(Action);
		void setName(const QString&);
		void setGroups(const QStringList&);

		QDomElement toXml(Stanza&) const;
		void fromXml(const QDomElement&);
		
	private:
		Jid jid_;
		QString name_;
		QStringList groups_;
		Action action_;
	};
	typedef QList<RosterExchangeItem> RosterExchangeItems;

	class HTMLElement
	{
	public:
		HTMLElement();
		HTMLElement(const QDomElement &body);

		void setBody(const QDomElement &body);
		const QDomElement& body() const;
		QString toString(const QString &rootTagName = "body") const;
		QString text() const;

	private:
		QDomElement body_;
	};

	class Message
	{
	public:
		Message(const Jid &to="");
		Message(const Message &from);
		Message & operator=(const Message &from);
		~Message();

		Jid to() const;
		Jid from() const;
		QString id() const;
		QString type() const;
		QString lang() const;
		QString subject(const QString &lang="") const;
		QString body(const QString &lang="") const;
		QString thread() const;
		Stanza::Error error() const;

		void setTo(const Jid &j);
		void setFrom(const Jid &j);
		void setId(const QString &s);
		void setType(const QString &s);
		void setLang(const QString &s);
		void setSubject(const QString &s, const QString &lang="");
		void setBody(const QString &s, const QString &lang="");
		void setThread(const QString &s);
		void setError(const Stanza::Error &err);

		// JEP-0060
		const QString& pubsubNode() const;
		const QList<PubSubItem>& pubsubItems() const;

		// JEP-0091
		QDateTime timeStamp() const;
		void setTimeStamp(const QDateTime &ts, bool send = false);

		// JEP-0071
		HTMLElement html(const QString &lang="") const;
		void setHTML(const HTMLElement &s, const QString &lang="");
		bool containsHTML() const;

		// JEP-0066
		UrlList urlList() const;
		void urlAdd(const Url &u);
		void urlsClear();
		void setUrlList(const UrlList &list);

		// JEP-0022
		QString eventId() const;
		void setEventId(const QString& id);
		bool containsEvents() const;
		bool containsEvent(MsgEvent e) const;
		void addEvent(MsgEvent e);

		// JEP-0085
		ChatState chatState() const;
		void setChatState(ChatState);
 
		// JEP-0027
		QString xencrypted() const;
		void setXEncrypted(const QString &s);
		
		// JEP-0033
		AddressList addresses() const;
		AddressList findAddresses(Address::Type t) const;
		void addAddress(const Address &a);
		void clearAddresses();
		void setAddresses(const AddressList &list);

		// JEP-144
		const RosterExchangeItems& rosterExchangeItems() const;
		void setRosterExchangeItems(const RosterExchangeItems&);

		// JEP-172
		void setNick(const QString&);
		const QString& nick() const;

		// MUC
		void setMUCStatus(int);
		bool hasMUCStatus() const;
		int mucStatus() const;
		void addMUCInvite(const MUCInvite&);
		const QList<MUCInvite>& mucInvites() const;
		void setMUCDecline(const MUCDecline&);
		const MUCDecline& mucDecline() const;
		const QString& mucPassword() const;
		void setMUCPassword(const QString&);

		// Obsolete invitation
		QString invite() const;
		void setInvite(const QString &s);

		// for compatibility.  delete me later
		bool spooled() const;
		void setSpooled(bool);
		bool wasEncrypted() const;
		void setWasEncrypted(bool);

		Stanza toStanza(Stream *stream) const;
		bool fromStanza(const Stanza &s, int tzoffset);

	private:
		class Private;
		Private *d;
	};

	class Subscription
	{
	public:
		enum SubType { None, To, From, Both, Remove };

		Subscription(SubType type=None);

		int type() const;

		QString toString() const;
		bool fromString(const QString &);

	private:
		SubType value;
	};

	class Status
	{
	public:
		enum Type { Online, Away, FFC, XA, DND, Offline };

		Status(const QString &show="", const QString &status="", int priority=0, bool available=true);
		~Status();

		int priority() const;
		Type type() const;
		const QString & show() const;
		const QString & status() const;
		QDateTime timeStamp() const;
		const QString & keyID() const;
		bool isAvailable() const;
		bool isAway() const;
		bool isInvisible() const;
		bool hasError() const;
		int errorCode() const;
		const QString & errorString() const;

		const QString & xsigned() const;
		const QString & songTitle() const;
		const QString & capsNode() const;
		const QString & capsVersion() const;
		const QString & capsExt() const;
		
		bool isMUC() const;
		bool hasMUCItem() const;
		const MUCItem & mucItem() const;
		bool hasMUCDestroy() const;
		const MUCDestroy & mucDestroy() const;
		bool hasMUCStatus() const;
		int mucStatus() const;
		const QString& mucPassword() const;
		bool hasMUCHistory() const;
		int mucHistoryMaxChars() const;
		int mucHistoryMaxStanzas() const;
		int mucHistorySeconds() const;

		void setPriority(int);
		void setType(Type);
		void setShow(const QString &);
		void setStatus(const QString &);
		void setTimeStamp(const QDateTime &);
		void setKeyID(const QString &);
		void setIsAvailable(bool);
		void setIsInvisible(bool);
		void setError(int, const QString &);
		void setCapsNode(const QString&);
		void setCapsVersion(const QString&);
		void setCapsExt(const QString&);
		
		void setMUC();
		void setMUCItem(const MUCItem&);
		void setMUCDestroy(const MUCDestroy&);
		void setMUCStatus(int);
		void setMUCPassword(const QString&);
		void setMUCHistory(int maxchars, int maxstanzas, int seconds);

		void setXSigned(const QString &);
		void setSongTitle(const QString &);

		// JEP-153: VCard-based Avatars
		const QString& photoHash() const;
		void setPhotoHash(const QString&);
		bool hasPhotoHash() const;

	private:
		int v_priority;
		Type v_type;
		QString v_show, v_status, v_key;
		QDateTime v_timeStamp;
		bool v_isAvailable;
		bool v_isInvisible;
		QString v_photoHash;
		bool v_hasPhotoHash;

		QString v_xsigned;
		// gabber song extension
		QString v_songTitle;
		QString v_capsNode, v_capsVersion, v_capsExt;

		// MUC
		bool v_isMUC, v_hasMUCItem, v_hasMUCDestroy;
		MUCItem v_mucItem;
		MUCDestroy v_mucDestroy;
		int v_mucStatus;
		QString v_mucPassword;
		int v_mucHistoryMaxChars, v_mucHistoryMaxStanzas, v_mucHistorySeconds;

		int ecode;
		QString estr;
	};

	class Resource
	{
	public:
		Resource(const QString &name="", const Status &s=Status());
		~Resource();

		const QString & name() const;
		int priority() const;
		const Status & status() const;

		void setName(const QString &);
		void setStatus(const Status &);

	private:
		QString v_name;
		Status v_status;
	};

	class ResourceList : public QList<Resource>
	{
	public:
		ResourceList();
		~ResourceList();

		ResourceList::Iterator find(const QString &);
		ResourceList::Iterator priority();

		ResourceList::ConstIterator find(const QString &) const;
		ResourceList::ConstIterator priority() const;
	};

	class RosterItem
	{
	public:
		RosterItem(const Jid &jid="");
		virtual ~RosterItem();

		const Jid & jid() const;
		const QString & name() const;
		const QStringList & groups() const;
		const Subscription & subscription() const;
		const QString & ask() const;
		bool isPush() const;
		bool inGroup(const QString &) const;

		virtual void setJid(const Jid &);
		void setName(const QString &);
		void setGroups(const QStringList &);
		void setSubscription(const Subscription &);
		void setAsk(const QString &);
		void setIsPush(bool);
		bool addGroup(const QString &);
		bool removeGroup(const QString &);

		QDomElement toXml(QDomDocument *) const;
		bool fromXml(const QDomElement &);

	private:
		Jid v_jid;
		QString v_name;
		QStringList v_groups;
		Subscription v_subscription;
		QString v_ask;
		bool v_push;
	};

	class Roster : public QList<RosterItem>
	{
	public:
		Roster();
		~Roster();

		Roster::Iterator find(const Jid &);
		Roster::ConstIterator find(const Jid &) const;

	private:
		class RosterPrivate *d;
	};

        class Features
	{
	public:
		Features();
		Features(const QStringList &);
		Features(const QString &);
		~Features();

		QStringList list() const; // actual featurelist
		void setList(const QStringList &);
		void addFeature(const QString&);

		// features
		bool canRegister() const;
		bool canSearch() const;
		bool canMulticast() const;
		bool canGroupchat() const;
		bool canVoice() const;
		bool canDisco() const;
		bool canChatState() const;
		bool canCommand() const;
		bool isGateway() const;
		bool haveVCard() const;

		enum FeatureID {
			FID_Invalid = -1,
			FID_None,
			FID_Register,
			FID_Search,
			FID_Groupchat,
			FID_Disco,
			FID_Gateway,
			FID_VCard,
			FID_AHCommand,

			// private Psi actions
			FID_Add
		};

		// useful functions
		bool test(const QStringList &) const;

		QString name() const;
		static QString name(long id);
		static QString name(const QString &feature);

		long id() const;
		static long id(const QString &feature);
		static QString feature(long id);

		class FeatureName;
	private:
		QStringList _list;
	};

	class AgentItem
	{
	public:
		AgentItem() { }

		const Jid & jid() const { return v_jid; }
		const QString & name() const { return v_name; }
		const QString & category() const { return v_category; }
		const QString & type() const { return v_type; }
		const Features & features() const { return v_features; }

		void setJid(const Jid &j) { v_jid = j; }
		void setName(const QString &n) { v_name = n; }
		void setCategory(const QString &c) { v_category = c; }
		void setType(const QString &t) { v_type = t; }
		void setFeatures(const Features &f) { v_features = f; }

	private:
		Jid v_jid;
		QString v_name, v_category, v_type;
		Features v_features;
	};

	typedef QList<AgentItem> AgentList;

	class DiscoItem
	{
	public:
		DiscoItem();
		~DiscoItem();

		const Jid &jid() const;
		const QString &node() const;
		const QString &name() const;

		void setJid(const Jid &);
		void setName(const QString &);
		void setNode(const QString &);

		enum Action {
			None = 0,
			Remove,
			Update
		};

		Action action() const;
		void setAction(Action);

		const Features &features() const;
		void setFeatures(const Features &);

		struct Identity
		{
			QString category;
			QString name;
			QString type;
		};

		typedef QList<Identity> Identities;

		const Identities &identities() const;
		void setIdentities(const Identities &);

		// some useful helper functions
		static Action string2action(QString s);
		static QString action2string(Action a);

		DiscoItem & operator= (const DiscoItem &);
		DiscoItem(const DiscoItem &);

		operator AgentItem() const { return toAgentItem(); }
		AgentItem toAgentItem() const;
		void fromAgentItem(const AgentItem &);

	private:
		class Private;
		Private *d;
	};

	typedef QList<DiscoItem> DiscoList;

	class FormField
	{
	public:
		enum { username, nick, password, name, first, last, email, address, city, state, zip, phone, url, date, misc };
		FormField(const QString &type="", const QString &value="");
		~FormField();

		int type() const;
		QString fieldName() const;
		QString realName() const;
		bool isSecret() const;
		const QString & value() const;
		void setType(int);
		bool setType(const QString &);
		void setValue(const QString &);

	private:
		int tagNameToType(const QString &) const;
		QString typeToTagName(int) const;

		int v_type;
		QString v_value;

		class Private;
		Private *d;
	};

	class Form : public QList<FormField>
	{
	public:
		Form(const Jid &j="");
		~Form();

		Jid jid() const;
		QString instructions() const;
		QString key() const;
		void setJid(const Jid &);
		void setInstructions(const QString &);
		void setKey(const QString &);

	private:
		Jid v_jid;
		QString v_instructions, v_key;

		class Private;
		Private *d;
	};

	class SearchResult
	{
	public:
		SearchResult(const Jid &jid="");
		~SearchResult();

		const Jid & jid() const;
		const QString & nick() const;
		const QString & first() const;
		const QString & last() const;
		const QString & email() const;

		void setJid(const Jid &);
		void setNick(const QString &);
		void setFirst(const QString &);
		void setLast(const QString &);
		void setEmail(const QString &);

	private:
		Jid v_jid;
		QString v_nick, v_first, v_last, v_email;
	};

	class Client;
	class LiveRosterItem;
	class LiveRoster;
	class S5BManager;
	class IBBManager;
	class JidLinkManager;
	class FileTransferManager;

	class Task : public QObject
	{
		Q_OBJECT
	public:
		enum { ErrDisc };
		Task(Task *parent);
		Task(Client *, bool isRoot);
		virtual ~Task();

		Task *parent() const;
		Client *client() const;
		QDomDocument *doc() const;
		QString id() const;

		bool success() const;
		int statusCode() const;
		const QString & statusString() const;

		void go(bool autoDelete=false);
		virtual bool take(const QDomElement &);
		void safeDelete();

	signals:
		void finished();

	protected:
		virtual void onGo();
		virtual void onDisconnect();
		void send(const QDomElement &);
		void setSuccess(int code=0, const QString &str="");
		void setError(const QDomElement &);
		void setError(int code=0, const QString &str="");
		void debug(const char *, ...);
		void debug(const QString &);
		bool iqVerify(const QDomElement &x, const Jid &to, const QString &id, const QString &xmlns="");

	private slots:
		void clientDisconnected();
		void done();

	private:
		void init();

		class TaskPrivate;
		TaskPrivate *d;
	};

	class Client : public QObject
	{
		Q_OBJECT

	public:
		Client(QObject *parent=0);
		~Client();

		bool isActive() const;
		void connectToServer(ClientStream *s, const Jid &j, bool auth=true);
		void start(const QString &host, const QString &user, const QString &pass, const QString &resource);
		void close(bool fast=false);

		Stream & stream();
		const LiveRoster & roster() const;
		const ResourceList & resourceList() const;

		void send(const QDomElement &);
		void send(const QString &);

		QString host() const;
		QString user() const;
		QString pass() const;
		QString resource() const;
		Jid jid() const;

		void rosterRequest();
		void sendMessage(const Message &);
		void sendSubscription(const Jid &, const QString &, const QString& nick = QString());
		void setPresence(const Status &);

		void debug(const QString &);
		QString genUniqueId();
		Task *rootTask();
		QDomDocument *doc() const;

		QString OSName() const;
		QString timeZone() const;
		int timeZoneOffset() const;
		QString clientName() const;
		QString clientVersion() const;
		QString capsNode() const;
		QString capsVersion() const;
		QString capsExt() const;

		void setOSName(const QString &);
		void setTimeZone(const QString &, int);
		void setClientName(const QString &);
		void setClientVersion(const QString &);
		void setCapsNode(const QString &);
		void setCapsVersion(const QString &);

		void setIdentity(DiscoItem::Identity);
		DiscoItem::Identity identity();

		void setFeatures(const Features& f);
		const Features& features() const;

		void addExtension(const QString& ext, const Features& f);
		void removeExtension(const QString& ext);
		const Features& extension(const QString& ext) const;
		QStringList extensions() const;
		
		S5BManager *s5bManager() const;
		IBBManager *ibbManager() const;
		JidLinkManager *jidLinkManager() const;

		void setFileTransferEnabled(bool b);
		FileTransferManager *fileTransferManager() const;

		QString groupChatPassword(const QString& host, const QString& room) const;
		bool groupChatJoin(const QString &host, const QString &room, const QString &nick, const QString& password = QString(), int maxchars = -1, int maxstanzas = -1, int seconds = -1, const Status& = Status());
		void groupChatSetStatus(const QString &host, const QString &room, const Status &);
		void groupChatChangeNick(const QString &host, const QString &room, const QString &nick, const Status &);
		void groupChatLeave(const QString &host, const QString &room);

	signals:
		void activated();
		void disconnected();
		//void authFinished(bool, int, const QString &);
		void rosterRequestFinished(bool, int, const QString &);
		void rosterItemAdded(const RosterItem &);
		void rosterItemUpdated(const RosterItem &);
		void rosterItemRemoved(const RosterItem &);
		void resourceAvailable(const Jid &, const Resource &);
		void resourceUnavailable(const Jid &, const Resource &);
		void presenceError(const Jid &, int, const QString &);
		void subscription(const Jid &, const QString &, const QString &);
		void messageReceived(const Message &);
		void debugText(const QString &);
		void xmlIncoming(const QString &);
		void xmlOutgoing(const QString &);
		void groupChatJoined(const Jid &);
		void groupChatLeft(const Jid &);
		void groupChatPresence(const Jid &, const Status &);
		void groupChatError(const Jid &, int, const QString &);

		void incomingJidLink();

	private slots:
		//void streamConnected();
		//void streamHandshaken();
		//void streamError(const StreamError &);
		//void streamSSLCertificateReady(const QSSLCert &);
		//void streamCloseFinished();
		void streamError(int);
		void streamReadyRead();
		void streamIncomingXml(const QString &);
		void streamOutgoingXml(const QString &);

		void slotRosterRequestFinished();

		// basic daemons
		void ppSubscription(const Jid &, const QString &, const QString&);
		void ppPresence(const Jid &, const Status &);
		void pmMessage(const Message &);
		void prRoster(const Roster &);

		void s5b_incomingReady();
		void ibb_incomingReady();

	public:
		class GroupChat;
	private:
		void cleanup();
		void distribute(const QDomElement &);
		void importRoster(const Roster &);
		void importRosterItem(const RosterItem &);
		void updateSelfPresence(const Jid &, const Status &);
		void updatePresence(LiveRosterItem *, const Jid &, const Status &);

		class ClientPrivate;
		ClientPrivate *d;
	};

	class LiveRosterItem : public RosterItem
	{
	public:
		LiveRosterItem(const Jid &j="");
		LiveRosterItem(const RosterItem &);
		~LiveRosterItem();

		void setRosterItem(const RosterItem &);

		ResourceList & resourceList();
		ResourceList::Iterator priority();

		const ResourceList & resourceList() const;
		ResourceList::ConstIterator priority() const;

		bool isAvailable() const;
		const Status & lastUnavailableStatus() const;
		bool flagForDelete() const;

		void setLastUnavailableStatus(const Status &);
		void setFlagForDelete(bool);

	private:
		ResourceList v_resourceList;
		Status v_lastUnavailableStatus;
		bool v_flagForDelete;
	};

	class LiveRoster : public QList<LiveRosterItem>
	{
	public:
		LiveRoster();
		~LiveRoster();

		void flagAllForDelete();
		LiveRoster::Iterator find(const Jid &, bool compareRes=true);
		LiveRoster::ConstIterator find(const Jid &, bool compareRes=true) const;
	};
}

#endif

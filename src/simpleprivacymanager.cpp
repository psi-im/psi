#include "psiaccount.h"
#include "simpleprivacymanager.h"
#include "privacylist_b.h"
#include "xmpp_client.h"
#include "xmpp_status.h"
#include "xmpp_tasks.h"
#include "userlist.h"

static const QString BLOCKED_LIST_NAME = "blocked";

static XMPP::Jid processJid(const XMPP::Jid& jid)
{
	return jid.userHost();
}

static bool privacyListItemForJid(const PrivacyListItem& item, const Jid& jid)
{
	return item.type() == PrivacyListItem::JidType &&
	       processJid(item.value()) == processJid(jid);
}

SimplePrivacyManager::SimplePrivacyManager(PsiAccount* account)
	: PsiPrivacyManager(account->client()->rootTask())
	, account_(account)
	, accountAvailable_(false)
	, isAvailable_(false)
{
	connect(account, SIGNAL(updatedActivity()), SLOT(accountStateChanged()));

	connect(this, SIGNAL(listReceived(const PrivacyList&)), SLOT(listReceived(const PrivacyList&)));
	connect(this, SIGNAL(listsReceived(const QString&, const QString&, const QStringList&)), SLOT(listsReceived(const QString&, const QString&, const QStringList&)));
	connect(this, SIGNAL(listsError()), SLOT(listsError()));

	connect(this, SIGNAL(changeDefaultList_success()), SLOT(changeDefaultList_success()));
	connect(this, SIGNAL(changeDefaultList_error()), SLOT(changeDefaultList_error()));
	connect(this, SIGNAL(changeActiveList_success()), SLOT(changeActiveList_success()));
	connect(this, SIGNAL(changeActiveList_error()), SLOT(changeActiveList_error()));
}

SimplePrivacyManager::~SimplePrivacyManager()
{
	qDeleteAll(lists_);
}

bool SimplePrivacyManager::isAvailable() const
{
	return isAvailable_;
}

void SimplePrivacyManager::setIsAvailable(bool available)
{
	if (available != isAvailable_) {
		isAvailable_ = available;
		emit availabilityChanged();
	}
}

void SimplePrivacyManager::accountStateChanged()
{
	if (!account_->isAvailable()) {
		setIsAvailable(false);
	}

	if (account_->isAvailable() && !accountAvailable_) {
		requestListNames();
	}

	accountAvailable_ = account_->isAvailable();
}

static QStringList findDifferences(QStringList previous, QStringList current)
{
	QStringList result;
	foreach(QString i, previous) {
		if (!current.contains(i))
			result += i;
	}
	return result;
}

void SimplePrivacyManager::listReceived(const PrivacyList& list)
{
	QStringList previouslyBlockedContacts = blockedContacts();

	if (lists_.contains(list.name()))
		*lists_[list.name()] = list;
	else
		lists_[list.name()] = new PrivacyList(list);

	if (list.name() == BLOCKED_LIST_NAME)
		invalidateBlockedListCache();

	QStringList currentlyBlockedContacts = blockedContacts();
	QStringList updatedContacts;
	updatedContacts += findDifferences(previouslyBlockedContacts, currentlyBlockedContacts);
	updatedContacts += findDifferences(currentlyBlockedContacts, previouslyBlockedContacts);

	foreach(QString contact, updatedContacts) {
		emit simulateContactOffline(contact);

		if (!isContactBlocked(contact)) {
			if (isAuthorized(contact)) {
				JT_Presence* p = new JT_Presence(account_->client()->rootTask());
				p->pres(processJid(contact), account_->status());
				p->go(true);
			}

			{
				JT_Presence* p = new JT_Presence(account_->client()->rootTask());
				p->probe(processJid(contact));
				p->go(true);
			}
		}
	}

	emit listChanged(updatedContacts);
}

void SimplePrivacyManager::listsReceived(const QString& defaultList, const QString& activeList, const QStringList& lists)
{
	Q_UNUSED(activeList);

	if (!lists.contains(BLOCKED_LIST_NAME)) {
		createBlockedList();
	}
	else {
		requestList(BLOCKED_LIST_NAME);
	}

	if (defaultList.isEmpty()) {
		changeDefaultList(BLOCKED_LIST_NAME);
	}

	changeActiveList(BLOCKED_LIST_NAME);

	setIsAvailable(true);
}

void SimplePrivacyManager::listsError()
{
	setIsAvailable(false);
}

void SimplePrivacyManager::changeDefaultList_success()
{
}

void SimplePrivacyManager::changeDefaultList_error()
{
	qWarning("SimplePrivacyManager::changeDefaultList_error()");
}

void SimplePrivacyManager::changeActiveList_success()
{
}

void SimplePrivacyManager::changeActiveList_error()
{
	qWarning("SimplePrivacyManager::changeActiveList_error()");
}

void SimplePrivacyManager::createBlockedList()
{
	PrivacyList list(BLOCKED_LIST_NAME);
	PrivacyListItem allowAll;
	allowAll.setType(PrivacyListItem::FallthroughType);
	allowAll.setAction(PrivacyListItem::Allow);
	allowAll.setAll();

	list.insertItem(0, allowAll);
	changeList(list);
}

PrivacyList* SimplePrivacyManager::blockedList() const
{
	return lists_[BLOCKED_LIST_NAME];
}

QStringList SimplePrivacyManager::blockedContacts() const
{
	QStringList result;
	if (blockedList()) {
		foreach(PrivacyListItem item, blockedList()->items()) {
			if (item.type() == PrivacyListItem::JidType &&
			    item.action() == PrivacyListItem::Deny) {
				result << processJid(item.value()).full();
			}
		}
	}
	return result;
}

void SimplePrivacyManager::invalidateBlockedListCache()
{
	isBlocked_.clear();

	if (!blockedList())
		return;

	foreach(PrivacyListItem item, blockedList()->items()) {
		if (item.type() == PrivacyListItem::JidType &&
		    item.action() == PrivacyListItem::Deny) {
			isBlocked_[processJid(item.value()).full()] = true;
		}
	}
}

bool SimplePrivacyManager::isContactBlocked(const XMPP::Jid& jid) const
{
	return isBlocked_.contains(processJid(jid).full());
}

PrivacyListItem SimplePrivacyManager::blockItemFor(const XMPP::Jid& jid) const
{
	PrivacyListItem item;
	item.setType(PrivacyListItem::JidType);
	item.setValue(processJid(jid).full());
	item.setAction(PrivacyListItem::Deny);
	item.setAll();
	return item;
}

void SimplePrivacyManager::setContactBlocked(const XMPP::Jid& jid, bool blocked)
{
	if (!blockedList() || isContactBlocked(jid) == blocked)
		return;

	if (blocked && isAuthorized(jid)) {
		JT_Presence* p = new JT_Presence(account_->client()->rootTask());
		//p->pres(processJid(jid), account_->loggedOutStatus());
		p->pres(processJid(jid), Status(Status::Offline, "Logged out", 0));
		p->go(true);
	}

	PrivacyList newList(*blockedList());
	newList.clear();

	foreach(PrivacyListItem item, blockedList()->items()) {
		if (privacyListItemForJid(item, jid))
			continue;

		newList.appendItem(item);
	}

	if (blocked)
		newList.insertItem(0, blockItemFor(jid));

	changeList(newList);
}

bool SimplePrivacyManager::isAuthorized(const XMPP::Jid& jid) const
{
	UserListItem* u = account_->findFirstRelevant(processJid(jid));
	return u && (u->subscription().type() == Subscription::Both ||
	             u->subscription().type() == Subscription::From);
}

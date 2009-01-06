#ifndef SIMPLEPRIVACYMANAGER_H
#define SIMPLEPRIVACYMANAGER_H

#include "psiprivacymanager.h"

#include <QHash>

#include "xmpp_jid.h"

class PsiAccount;
class PrivacyListItem;

class SimplePrivacyManager : public PsiPrivacyManager
{
	Q_OBJECT

public:
	SimplePrivacyManager(PsiAccount* account);
	~SimplePrivacyManager();

	bool isAvailable() const;

	bool isContactBlocked(const XMPP::Jid& jid) const;
	void setContactBlocked(const XMPP::Jid& jid, bool blocked);

signals:
	void availabilityChanged();
	void listChanged(const QStringList& contacts);

	void simulateContactOffline(const XMPP::Jid& contact);

private slots:
	void listReceived(const PrivacyList& p);
	void listsReceived(const QString& defaultList, const QString& activeList, const QStringList& lists);
	void listsError();

	void accountStateChanged();

	void changeDefaultList_success();
	void changeDefaultList_error();
	void changeActiveList_success();
	void changeActiveList_error();

private:
	PsiAccount* account_;
	bool accountAvailable_;
	bool isAvailable_;
	QHash<QString, PrivacyList*> lists_;
	QHash<QString, bool> isBlocked_;

	void invalidateBlockedListCache();
	void setIsAvailable(bool available);

	void createBlockedList();
	PrivacyList* blockedList() const;
	PrivacyListItem blockItemFor(const XMPP::Jid& jid) const;

	QStringList blockedContacts() const;
	bool isAuthorized(const XMPP::Jid& jid) const;
};

#endif

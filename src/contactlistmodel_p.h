#pragma once

#include <QDateTime>
#include <QModelIndex>
#include <QTimer>

#include "contactlistitem.h"
#include "contactlistmodel.h"
#include "psicontact.h"

class ContactListModel::Private : public QObject
{
    Q_OBJECT

public:
    enum Operation {
        AddContact           = 1 << 0,
        RemoveContact        = 1 << 1,
        UpdateContact        = 1 << 2,
        ContactGroupsChanged = 1 << 3
    };

    Private(ContactListModel *parent);
    ~Private();

    void realAddContact(PsiContact *contact);
    void addContacts(const QList<PsiContact*> &contacts);
    void updateContacts(const QList<PsiContact*> &contacts);

    void addOperation(PsiContact* contact, Operation operation);
    int simplifiedOperationList(int operations) const;

    ContactListItem::SpecialGroupType specialGroupFor(PsiContact *contact);

public slots:
    void commit();
    void clear();
    void addContact(PsiContact *contact);
    void removeContact(PsiContact *contact);
    void contactUpdated();
    void contactGroupsChanged();
    void updateAccount();

private slots:
    void onAccountDestroyed();

private:
    void cleanUpAccount(PsiAccount *account);

public:
    ContactListModel *q;
    bool groupsEnabled;
    bool accountsEnabled;
    PsiContactList *contactList;
    QTimer *commitTimer;
    QDateTime commitTimerStartTime;
    QHash<PsiContact*, QPersistentModelIndex> monitoredContacts; // always keeps all the contacts
    QHash<PsiContact*, int> operationQueue;
    QStringList collapsed;
    QStringList hidden;
};

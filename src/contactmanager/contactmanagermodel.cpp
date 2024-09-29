/*
 * contactmanagermodel.cpp
 * Copyright (C) 2010  Sergey Ilinykh
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

#include "contactmanagermodel.h"

#include "iris/xmpp_client.h"
#include "iris/xmpp_tasks.h"
#include "psiaccount.h"
#include "userlist.h"

#include "iris/xmpp_liveroster.h"

#include <QDebug>
#include <QSet>

class CMModelItem::Private : public QSharedData {
public:
    Private(const RosterItem &item) :
        jid(item.jid()), name(item.name()), groups(item.groups()), subscription(item.subscription())
    {
    }

    Jid          jid;
    QString      name;
    QStringList  groups;
    Subscription subscription;
};

CMModelItem::CMModelItem(const RosterItem &item) : d(new Private(item)) { }

CMModelItem::CMModelItem(const CMModelItem &other) : d(other.d) { }

CMModelItem::~CMModelItem() = default;

const Jid          &CMModelItem::jid() const { return d->jid; }
const QString      &CMModelItem::name() const { return d->name; }
const QStringList  &CMModelItem::groups() const { return d->groups; }
const Subscription &CMModelItem::subscription() const { return d->subscription; }

class ContactManagerModel::Private {
    ContactManagerModel *q;

public:
    PsiAccount   *pa;
    UserList      userList;
    QStringList   columnNames;
    QList<Role>   roles;
    QSet<QString> checks; // TODO move it too CMModelItem. it's pointless to have it after refactoring

    Private(ContactManagerModel *q, PsiAccount *pa) : q(q), pa(pa)
    {
        columnNames << "" << tr("Nick") << tr("Group") << tr("Node") << tr("Domain") << tr("Subscription");
        roles << CheckRole << NickRole << GroupRole << NodeRole << DomainRole << SubscriptionRole;

        std::ranges::for_each(pa->client()->roster(), [this](const RosterItem &item) { userList.push_front(item); });

        connect(pa->client(), &XMPP::Client::rosterItemAdded, q,
                [this](const RosterItem &item) { contactAdded(item); });
        connect(pa->client(), &XMPP::Client::rosterItemRemoved, q,
                [this](const RosterItem &item) { contactRemoved(item); });
        connect(pa->client(), &XMPP::Client::rosterItemUpdated, q,
                [this](const RosterItem &item) { contactUpdated(item); });
    }

    QString userFieldString(const CMModelItem &u, ContactManagerModel::Role columnRole) const
    {
        QString data;
        switch (columnRole) {
        case NodeRole: // node
            data = u.jid().node();
            break;
        case DomainRole: // domain
            data = u.jid().domain();
            break;
        case NickRole: // nick
            data = u.name();
            break;
        case GroupRole: // group
            if (u.groups().isEmpty()) {
                data = "";
            } else {
                data = u.groups().first();
            }
            break;
        case SubscriptionRole: // subscription
            data = u.subscription().toString();
            break;
        default:
            break;
        }
        return data;
    }
    void removeUsers(const UserList &users)
    {
        for (const auto &item : users) {
            if (item.jid().node().isEmpty() && !Jid(pa->client()->host()).compare(item.jid())) {
                JT_UnRegister *ju = new JT_UnRegister(pa->client()->rootTask());
                ju->unreg(item.jid());
                ju->go(true);
            }
            JT_Roster *r = new JT_Roster(pa->client()->rootTask());
            r->remove(item.jid());
            r->go(true);
        }
    }

    void changeDomain(const UserList &users, const QString &domain)
    {
        auto items = findUsers(users);
        for (auto [idx, item] : items) {
            if (item.jid().node().isEmpty()) {
                continue;
            }
            JT_Roster *r = new JT_Roster(pa->client()->rootTask());
            r->set(item.jid().withDomain(domain), item.name(), item.groups());
            r->go(true);
            r = new JT_Roster(pa->client()->rootTask());
            r->remove(item.jid());
            r->go(true);
        }
    }

    void changeGroups(const UserList &users, const QStringList &groups)
    {
        auto items = findUsers(users);
        for (auto [idx, item] : items) {
            JT_Roster *r = new JT_Roster(pa->client()->rootTask());
            r->set(item.jid(), item.name(), groups);
            r->go(true);
        }
    }

private:
    QList<std::pair<int, CMModelItem>> findUsers(const UserList &users)
    {
        QList<std::pair<int, CMModelItem>> ret;
        for (auto const &item : users) {
            auto it = std::ranges::find_if(userList, [&item](const auto &u) { return u.jid() == item.jid(); });
            if (it != userList.end()) {
                ret.push_back({ int(std::distance(userList.begin(), it)), *it });
            }
        }
        return ret;
    }

    inline std::optional<int> findRow(const CMModelItem &item)
    {
        auto it = std::ranges::find_if(userList, [&item](auto const &i) { return item.jid() == i.jid(); });
        if (it != userList.end()) {
            return std::distance(userList.begin(), it);
        }
        return {};
    }

    void contactAdded(const RosterItem &ri)
    {
        q->beginInsertRows({}, int(userList.size()), int(userList.size()));
        userList.push_back(ri);
        q->endInsertRows();
    }

    void contactRemoved(const RosterItem &ri)
    {
        auto row = findRow(ri);
        if (row) {
            q->beginRemoveRows(QModelIndex(), *row, *row);
            userList.erase(userList.begin() + *row);
            q->endRemoveRows();
        }
    }

    void contactUpdated(const RosterItem &ri)
    {
        auto row = findRow(ri);
        if (row) {
            emit q->dataChanged(q->index(*row, 1), q->index(*row, columnNames.count() - 1));
        }
    }
};

ContactManagerModel::ContactManagerModel(QObject *parent, PsiAccount *pa) :
    QAbstractTableModel(parent), d(new Private(this, pa))
{
}

ContactManagerModel::~ContactManagerModel() { qDebug("ContactManagerModel destroyed"); }

int ContactManagerModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return int(d->userList.size());
}

int ContactManagerModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return d->columnNames.count();
}

QVariant ContactManagerModel::data(const QModelIndex &index, int role) const
{
    Role columnRole = d->roles[index.column()];
    if (index.row() < d->userList.size()) {
        auto u = d->userList.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
            return d->userFieldString(u, columnRole);
        case Qt::TextAlignmentRole:
            if (columnRole == CheckRole || columnRole == NodeRole)
                return int(Qt::AlignRight | Qt::AlignVCenter);
            break;
        case Qt::CheckStateRole:
            if (columnRole == CheckRole) {
                return d->checks.contains(u.jid().full()) ? 2 : 0;
            }
        }
    }
    return QVariant();
}

QVariant ContactManagerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            return d->columnNames[section];
        } else {
            return section + 1;
        }
    }
    return QVariant();
}

QStringList ContactManagerModel::manageableFields()
{
    QStringList ret = d->columnNames;
    ret.removeFirst();
    return ret;
}

Qt::ItemFlags ContactManagerModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags      = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    Role          columnRole = d->roles[index.column()];
    if (columnRole == CheckRole) {
        flags |= (Qt::ItemIsUserCheckable);
    }
    return flags;
}

bool ContactManagerModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    Q_UNUSED(role);
    if (index.isValid()) {
        Role columnRole = d->roles[index.column()];
        if (columnRole == CheckRole) {
            QString jid = d->userList.at(index.row()).jid().full();
            if (value.toInt() == 3) { // iversion
                if (d->checks.contains(jid)) {
                    d->checks.remove(jid);
                } else {
                    d->checks.insert(jid);
                }
                emit dataChanged(index, index);
            } else {
                if (d->checks.contains(jid)) {
                    if (!value.toBool()) {
                        d->checks.remove(jid);
                        emit dataChanged(index, index);
                    }
                } else {
                    if (value.toBool()) {
                        d->checks.insert(jid);
                        emit dataChanged(index, index);
                    }
                }
            }
        }
    }
    return false;
}

ContactManagerModel::UserList ContactManagerModel::checkedUsers()
{
    UserList users;
    for (auto u : d->userList) {
        if (d->checks.contains(u.jid().full())) {
            users.push_back(u);
        }
    }
    return users;
}

void ContactManagerModel::invertByMatch(int columnIndex, int matchType, const QString &str)
{
    emit               layoutAboutToBeChanged();
    Role               columnRole = d->roles[columnIndex];
    QString            data;
    QRegularExpression reg;
    if (matchType == ContactManagerModel::RegexpMatch) {
        reg = QRegularExpression(str);
    }
    for (auto u : d->userList) {
        data = d->userFieldString(u, columnRole);
        if ((matchType == ContactManagerModel::SimpleMatch && str == data)
            || (matchType == ContactManagerModel::RegexpMatch && reg.match(data).hasMatch())) {
            QString jid = u.jid().full();
            if (d->checks.contains(jid)) {
                d->checks.remove(jid);
            } else {
                d->checks.insert(jid);
            }
        }
    }
    emit layoutChanged();
}

void ContactManagerModel::removeUsers(const UserList &users) { d->removeUsers(users); }

void ContactManagerModel::changeDomain(const UserList &users, const QString &domain) { d->changeDomain(users, domain); }

void ContactManagerModel::changeGroups(const UserList &users, const QStringList &groups)
{
    d->changeGroups(users, groups);
}

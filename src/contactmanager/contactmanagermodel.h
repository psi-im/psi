/*
 * contactmanagermodel.h
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

#ifndef CONTACTMANAGERMODEL_H
#define CONTACTMANAGERMODEL_H

#include <QAbstractTableModel>
#include <QExplicitlySharedDataPointer>
#include <QStringList>

#include <deque>
#include <memory>

class PsiAccount;
class UserListItem;

namespace XMPP {
class Jid;
class RosterItem;
class Subscription;
}
using namespace XMPP;

class CMModelItem {
    class Private;
    QExplicitlySharedDataPointer<Private> d;

public:
    CMModelItem(const RosterItem &item);
    ~CMModelItem();

    CMModelItem(const CMModelItem &other);

    const Jid          &jid() const;
    const QString      &name() const;
    const QStringList  &groups() const;
    const Subscription &subscription() const;
};

class ContactManagerModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Role {
        // DisplayRole / EditRole
        CheckRole,
        NickRole,
        GroupRole,
        NodeRole,
        DomainRole,
        SubscriptionRole
    };
    const static int SimpleMatch = 1;
    const static int RegexpMatch = 2;

    using UserList = std::deque<CMModelItem>;

    ContactManagerModel(QObject *parent, PsiAccount *pa);
    ~ContactManagerModel();

    int           rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int           columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant      data(const QModelIndex &index, int role) const override;
    QVariant      headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool          setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    QStringList manageableFields();
    UserList    checkedUsers();
    void        invertByMatch(int columnIndex, int matchType, const QString &str);

    void removeUsers(const UserList &users);
    void changeDomain(const UserList &users, const QString &domain);
    void changeGroups(const UserList &users, const QStringList &groups);

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // CONTACTMANAGERMODEL_H

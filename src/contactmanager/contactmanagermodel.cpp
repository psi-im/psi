/*
 * contactmanagermodel.cpp
 * Copyright (C) 2010 Rion
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "contactmanagermodel.h"
#include "psiaccount.h"
#include "userlist.h"
#include "xmpp_tasks.h"
#include "QDebug"


ContactManagerModel::ContactManagerModel(QObject * parent, PsiAccount *pa) :
		QAbstractTableModel(parent),
		pa_(pa)
{
	columnNames
			<<""
			<<tr("Nick")
			<<tr("Group")
			<<tr("Node")
			<<tr("Domain")
			<<tr("Subscription");
	roles	<<CheckRole
			<<NickRole
			<<GroupRole
			<<NodeRole
			<<DomainRole
			<<SubscriptionRole;
	connect(pa_, SIGNAL(updateContact(UserListItem)), this, SLOT(view_contactUpdated(UserListItem)));
	connect(pa_->client(), SIGNAL(rosterItemUpdated(const RosterItem &)), this, SLOT(client_rosterItemUpdated(const RosterItem &)));
}

void ContactManagerModel::reloadUsers()
{
	beginResetModel();
	clear();
	UserList *ul = pa_->userList();
	foreach (UserListItem *u, *ul) {
		if (u->inList()) {
			addContact(u);
		}
	}
	endResetModel();
}

void ContactManagerModel::clear()
{
	_userList.clear();
	checks.clear();
}

int ContactManagerModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) return 0;
	return _userList.count();
}

int ContactManagerModel::columnCount ( const QModelIndex & parent) const
{
	Q_UNUSED(parent);
	return columnNames.count();
}

QVariant ContactManagerModel::data(const QModelIndex &index, int role) const
{
	Role columnRole = roles[index.column()];
	UserListItem *u = _userList.at(index.row());
	if (u) {
		switch (columnRole) {
			case CheckRole: //checkbox
				if (role == Qt::CheckStateRole) {
					return checks.contains(u->jid().full())?2:0;
				} else if (role == Qt::TextAlignmentRole) {
					return (int)(Qt::AlignRight | Qt::AlignVCenter);
				}
				break;
			case NodeRole:
				if (role == Qt::TextAlignmentRole) {
					return (int)(Qt::AlignRight | Qt::AlignVCenter);
				}
			default:
				if (role == Qt::DisplayRole) {
					return userFieldString(u, columnRole);
				}
		}
	}
	return QVariant();
}

QString ContactManagerModel::userFieldString(UserListItem *u, ContactManagerModel::Role columnRole) const
{
	QString data;
	switch (columnRole) {
		case NodeRole: //node
			data = u->jid().node();
			break;
		case DomainRole: //domain
			data = u->jid().domain();
			break;
		case NickRole: //nick
			data = u->name();
			break;
		case GroupRole: //group
			if (u->groups().isEmpty()) {
				data = "";
			} else {
				data = u->groups().first();
			}
			break;
		case SubscriptionRole: //subscription
			data = u->subscription().toString();
			break;
		default:
			break;
	}
	return data;
}

QVariant ContactManagerModel::headerData(int section, Qt::Orientation orientation,
					 int role) const
{
	if (role == Qt::DisplayRole) {
		if (orientation == Qt::Horizontal) {
			return columnNames[section];
		} else {
			return section+1;
		}
	}
	return QVariant();
}


QStringList ContactManagerModel::manageableFields()
{
	QStringList ret = columnNames;
	ret.removeFirst();
	return ret;
}

void ContactManagerModel::addContact(UserListItem *u)
{
	_userList.append(u);
}

Qt::ItemFlags ContactManagerModel::flags ( const QModelIndex & index ) const
{
	Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	Role columnRole = roles[index.column()];
	if ( columnRole == CheckRole ) {
		flags |= ( Qt::ItemIsUserCheckable );
	}
	return flags;
}

bool ContactManagerModel::setData ( const QModelIndex & index, const QVariant & value, int role )
{
	Q_UNUSED(role);
	if (index.isValid()) {
		Role columnRole = roles[index.column()];
		if ( columnRole == CheckRole ) {
			QString jid = _userList.at(index.row())->jid().full();
			if (value.toInt() == 3) { //iversion
				if (checks.contains(jid)) {
					checks.remove(jid);
				} else {
					checks.insert(jid);
				}
				emit dataChanged ( index, index );
			} else {
				if (checks.contains(jid)) {
					if (!value.toBool()) {
						checks.remove(jid);
						emit dataChanged ( index, index );
					}
				} else {
					if (value.toBool()) {
						checks.insert(jid);
						emit dataChanged ( index, index );
					}
				}
			}
		}
	}
	return false;
}

ContactManagerModel::Role ContactManagerModel::sortRole;
Qt::SortOrder ContactManagerModel::sortOrder = Qt::AscendingOrder;

void ContactManagerModel::sort ( int column, Qt::SortOrder order = Qt::AscendingOrder )
{
	Role columnRole = roles[column];
	ContactManagerModel::sortRole = columnRole;
	ContactManagerModel::sortOrder = order;
	if (columnRole != CheckRole) {
		emit layoutAboutToBeChanged();
		qSort(_userList.begin(), _userList.end(), ContactManagerModel::sortLessThan);
		emit layoutChanged();
	}
}

bool ContactManagerModel::sortLessThan(UserListItem *u1, UserListItem *u2)
{
	QString g1, g2;
	bool result = false;
	switch (ContactManagerModel::sortRole) {
		case NodeRole: //node
			result = u1->jid().node() < u2->jid().node();
			break;
		case DomainRole: //domain
			result = u1->jid().domain() < u2->jid().domain();
			break;
		case NickRole: //nick
			result = u1->name() < u2->name();
			break;
		case GroupRole: //group
			if (!u1->groups().isEmpty()) {
				g1 = u1->groups().first();
			}
			if (!u2->groups().isEmpty()) {
				g2 = u2->groups().first();
			}
			result = g1 < g2;
			break;
		case SubscriptionRole: //subscription
			result = u1->subscription().toString() < u2->subscription().toString();
			break;
		default:
			break;
	}
	return ContactManagerModel::sortOrder == Qt::AscendingOrder? result : !result;
}

QList<UserListItem *> ContactManagerModel::checkedUsers()
{
	QList<UserListItem *> users;
	foreach (UserListItem *u, _userList) {
		if (checks.contains(u->jid().full())) {
			users.append(u);
		}
	}
	return users;
}

void ContactManagerModel::invertByMatch(int columnIndex, int matchType, const QString &str)
{
	emit layoutAboutToBeChanged();
	Role columnRole = roles[columnIndex];
	QString data;
	QRegExp reg;
	if (matchType == ContactManagerModel::RegexpMatch) {
		reg = QRegExp(str);
	}
	foreach (UserListItem *u, _userList) {
		data = userFieldString(u, columnRole);
		if ((matchType == ContactManagerModel::SimpleMatch && str == data) ||
			(matchType == ContactManagerModel::RegexpMatch && reg.indexIn(data) != -1)) {
			QString jid = u->jid().full();
			if (checks.contains(jid)) {
				checks.remove(jid);
			} else {
				checks.insert(jid);
			}
		}
	}
	emit layoutChanged();
}

void ContactManagerModel::view_contactUpdated(const UserListItem &u)
{
	contactUpdated(u.jid());
}

void ContactManagerModel::client_rosterItemUpdated(const RosterItem &item)
{
	contactUpdated(item.jid());
}

void  ContactManagerModel::contactUpdated(const Jid &jid)
{
	int i = 0;
	foreach (UserListItem *lu, _userList) {
		if (lu->jid() == jid) {
			dataChanged(index(i, 1), index(i, columnNames.count()-1));
		}
		i++;
	}
}

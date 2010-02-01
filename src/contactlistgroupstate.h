/*
 * contactlistgroupstate.h - saves state of groups in a contact list model
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef CONTACTLISTGROUPSTATE_H
#define CONTACTLISTGROUPSTATE_H

#include <QObject>
#include <QMap>

class ContactListModel;
class ContactListGroup;

class QTimer;
class QModelIndex;
class QDomElement;
class QDomDocument;

class ContactListGroupState : public QObject
{
	Q_OBJECT
public:
	typedef QMap<QString, bool> GroupExpandedState;

	ContactListGroupState(QObject* parent = 0);
	~ContactListGroupState();

	bool groupExpanded(const ContactListGroup* group) const;
	void setGroupExpanded(const ContactListGroup* group, bool expanded);

	GroupExpandedState groupExpandedState() const;
	void restoreGroupExpandedState(GroupExpandedState groupExpandedState);

	int groupOrder(const ContactListGroup* group) const;
	void setGroupOrder(const ContactListGroup* group, int order);

	void updateGroupList(const ContactListModel* model);

	void load(const QString& id);

public slots:
	void save();

signals:
	void orderChanged();

private:
	QTimer* orderChangedTimer_;
	QTimer* saveGroupStateTimer_;
	QString id_;
	GroupExpandedState expanded_;
	QMap<QString, int> order_;

	QStringList groupNames(const ContactListModel* model, const QModelIndex& parent, QStringList parentName) const;
};

#endif

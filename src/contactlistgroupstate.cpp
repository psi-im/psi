/*
 * contactlistgroupstate.cpp - saves state of groups in a contact list model
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "contactlistgroupstate.h"

#include <QTimer>
#include <QStringList>
#include <QDomElement>

#include "contactlistmodel.h"
#include "contactlistgroup.h"
#include "psioptions.h"
#include "xmpp_xmlcommon.h"

static const QString groupStateOptionPath = "options.main-window.contact-list.group-state.%1";

ContactListGroupState::ContactListGroupState(QObject* parent)
	: QObject(parent)
{
	orderChangedTimer_ = new QTimer(this);
	connect(orderChangedTimer_, SIGNAL(timeout()), SIGNAL(orderChanged()));
	orderChangedTimer_->setSingleShot(true);
	orderChangedTimer_->setInterval(0);

	saveGroupStateTimer_ = new QTimer(this);
	connect(saveGroupStateTimer_, SIGNAL(timeout()), SLOT(save()));
	saveGroupStateTimer_->setSingleShot(true);
	saveGroupStateTimer_->setInterval(1000);
}

ContactListGroupState::~ContactListGroupState()
{
}

bool ContactListGroupState::groupExpanded(const ContactListGroup* group) const
{
	Q_ASSERT(group);
	QString fullName = group->fullName();
	if (expanded_.contains(fullName))
		return expanded_[fullName];
	return true;
}

void ContactListGroupState::setGroupExpanded(const ContactListGroup* group, bool expanded)
{
	Q_ASSERT(group);
	expanded_[group->fullName()] = expanded;
	saveGroupStateTimer_->start();
}

ContactListGroupState::GroupExpandedState ContactListGroupState::groupExpandedState() const
{
	return expanded_;
}

void ContactListGroupState::restoreGroupExpandedState(ContactListGroupState::GroupExpandedState groupExpandedState)
{
	expanded_ = groupExpandedState;
}

int ContactListGroupState::groupOrder(const ContactListGroup* group) const
{
	Q_ASSERT(group);
	QString fullName = group->fullName();
	if (order_.contains(fullName))
		return order_[fullName];
	if (group->isFake())
		return -1;
	return 0;
}

void ContactListGroupState::setGroupOrder(const ContactListGroup* group, int order)
{
	orderChangedTimer_->start();
	saveGroupStateTimer_->start();

	Q_ASSERT(group);
	order_[group->fullName()] = order;
}

bool ContactListGroupState::isHidden(const ContactListGroup* group) const
{
	const QString groupName = group->fullName();
	if(hidden_.contains(groupName)) {
		return hidden_.value(groupName);
	}

	return false;
}

void ContactListGroupState::setHidden(const ContactListGroup* group, bool hidden)
{
	saveGroupStateTimer_->start();

	hidden_[group->fullName()] = hidden;
}

void ContactListGroupState::updateGroupList(const ContactListModel* model)
{
	GroupExpandedState newExpanded;
	QMap<QString, int> newOrder;

	foreach(QString group, groupNames(model, QModelIndex(), QStringList())) {
		if (expanded_.contains(group))
			if (!expanded_[group])
				newExpanded[group] = expanded_[group];

		if (order_.contains(group))
			newOrder[group] = order_[group];
	}

	expanded_ = newExpanded;
	order_ = newOrder;
}

QStringList ContactListGroupState::groupNames(const ContactListModel* model, const QModelIndex& parent, QStringList parentName) const
{
	QStringList result;

	for (int row = 0; row < model->rowCount(parent); ++row) {
		QModelIndex index = model->index(row, 0, parent);
		if (model->isGroupType(index)) {
			QStringList groupName = parentName;
			groupName << index.data(ContactListModel::InternalGroupNameRole).toString();
			foreach(QString name, groupNames(model, index, groupName)) {
				if (!result.contains(name))
					result += name;
			}
		}
	}

	if (result.isEmpty()) {
		for (int len = 1; len < parentName.count() + 1; ++len)
			result += QStringList(parentName.mid(0, len)).join(ContactListGroup::groupDelimiter());
	}

	return result;
}

void ContactListGroupState::load(const QString& id)
{
	expanded_.clear();
	order_.clear();

	id_ = id;
	if (id_.isEmpty())
		return;

	QDomDocument doc;
	if (!doc.setContent(PsiOptions::instance()->getOption(groupStateOptionPath.arg(id_)).toString()))
		return;

	QDomElement root = doc.documentElement();
	if (root.tagName() != "group-state" || root.attribute("version") != "1.0")
		return;

	{
		QDomElement expanded = root.firstChildElement("expanded");
		if (!expanded.isNull()) {
			for (QDomNode n = expanded.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement e = n.toElement();
				if (e.isNull())
					continue;

				if (e.tagName() == "item") {
					expanded_[e.attribute("fullName")] = e.text() == "true";
				}
			}
		}
	}
	{
		QDomElement order = root.firstChildElement("order");
		if (!order.isNull()) {
			for (QDomNode n = order.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement e = n.toElement();
				if (e.isNull())
					continue;

				if (e.tagName() == "item") {
					order_[e.attribute("fullName")] = e.text().toInt();
				}
			}
		}
	}
	{
		QDomElement hidden = root.firstChildElement("hidden");
		if (!hidden.isNull()) {
			for (QDomNode n = hidden.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement e = n.toElement();
				if (e.isNull())
					continue;

				if (e.tagName() == "item") {
					hidden_[e.attribute("fullName")] = e.text() == "true";
				}
			}
		}
	}
}

void ContactListGroupState::save()
{
	if (id_.isEmpty())
		return;

	QDomDocument doc;
	QDomElement root = doc.createElement("group-state");
	root.setAttribute("version", "1.0");
	doc.appendChild(root);

	{
		QDomElement expanded = XMLHelper::emptyTag(&doc, "expanded");
		root.appendChild(expanded);

		QMap<QString, bool>::iterator i = expanded_.begin();
		for (; i != expanded_.end(); ++i) {
			if (i.value() == false) {
				QDomElement item = textTag(&doc, "item", "false");
				item.setAttribute("fullName", i.key());
				expanded.appendChild(item);
			}
		}
	}
	{
		QDomElement order = XMLHelper::emptyTag(&doc, "order");
		root.appendChild(order);

		QMap<QString, int>::iterator i = order_.begin();
		for (; i != order_.end(); ++i) {
			if (i.value() != 0) {
				QDomElement item = textTag(&doc, "item", QString::number(i.value()));
				item.setAttribute("fullName", i.key());
				order.appendChild(item);
			}
		}
	}
	{
		QDomElement hidden = XMLHelper::emptyTag(&doc, "hidden");
		root.appendChild(hidden);

		QMap<QString, bool>::iterator i = hidden_.begin();
		for (; i != hidden_.end(); ++i) {
			if (i.value() == true) {
				QDomElement item = textTag(&doc, "item", "true");
				item.setAttribute("fullName", i.key());
				hidden.appendChild(item);
			}
		}
	}

	PsiOptions::instance()->setOption(groupStateOptionPath.arg(id_), doc.toString());
}

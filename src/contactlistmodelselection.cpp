/*
 * contactlistmodelselection.cpp - stores persistent contact list selections
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

#include "contactlistmodelselection.h"

#include <QDomElement>
#include <QStringList>

#include "xmpp_xmlcommon.h"
#include "psiaccount.h"
#include "psicontact.h"
#include "contactlistnestedgroup.h"
#include "contactlistaccountgroup.h"
#include "contactlistitemproxy.h"
#include "textutil.h"

static const QString psiRosterSelectionMimeType = "application/psi-roster-selection";

ContactListModelSelection::ContactListModelSelection(QList<ContactListItemProxy*> items)
	: QMimeData()
	, mimeData_(0)
{
	QDomDocument doc;
	QDomElement root = doc.createElement("items");
	root.setAttribute("version", "2.0");
	doc.appendChild(root);

	// TODO: maybe also embed a random instance-specific token to
	// prevent drag'n'drop with other running Psi instances?

	QStringList jids;

	foreach(ContactListItemProxy* itemProxy, items) {
		Q_ASSERT(itemProxy);

		PsiContact* contact = 0;
		ContactListNestedGroup* group = 0;
		ContactListAccountGroup* account = 0;
		if ((contact = dynamic_cast<PsiContact*>(itemProxy->item()))) {
			QDomElement tag = textTag(&doc, "contact", contact->jid().full());
			tag.setAttribute("account", contact->account()->id());
			tag.setAttribute("group", itemProxy->parent() ? itemProxy->parent()->fullName() : "");
			root.appendChild(tag);

			jids << contact->jid().full();
		}
		else if ((account = dynamic_cast<ContactListAccountGroup*>(itemProxy->item()))) {
			QDomElement tag = doc.createElement("account");
			tag.setAttribute("id", account->account()->id());
			root.appendChild(tag);

			jids << account->displayName();
		}
		else if ((group = dynamic_cast<ContactListNestedGroup*>(itemProxy->item()))) {
			// if group->fullName() consists only of whitespace when we'll try
			// to read it back we'll get an empty string, so we're using CDATA
			// QDomElement tag = textTag(&doc, "group", group->fullName());
			QDomElement tag = doc.createElement("group");
			QDomText text = doc.createCDATASection(TextUtil::escape(group->fullName()));
			tag.appendChild(text);

			root.appendChild(tag);

			jids << group->fullName();
		}
		else {
			qWarning("ContactListModelSelection::ContactListModelSelection(): Unable to serialize %d, unsupported type", itemProxy->item()->type());
		}
	}

	setText(jids.join(";"));
	setData(psiRosterSelectionMimeType, doc.toByteArray());
}

ContactListModelSelection::ContactListModelSelection(const QMimeData* mimeData)
	: QMimeData()
	, mimeData_(mimeData)
{
	const ContactListModelSelection* other = dynamic_cast<const ContactListModelSelection*>(mimeData_);
	if (other) {
		mimeData_ = other->mimeData();
	}
}

const QString& ContactListModelSelection::mimeType()
{
	return psiRosterSelectionMimeType;
}

QDomElement ContactListModelSelection::rootElementFor(const QMimeData* mimeData) const
{
	QDomDocument doc;
	if (!doc.setContent(mimeData->data(psiRosterSelectionMimeType)))
		return QDomElement();

	QDomElement root = doc.documentElement();
	if (root.tagName() != "items" || root.attribute("version") != "2.0")
		return QDomElement();

	return root;
}

bool ContactListModelSelection::haveRosterSelectionIn(const QMimeData* mimeData) const
{
	return !rootElementFor(mimeData).isNull();
}

QList<ContactListModelSelection::Contact> ContactListModelSelection::contactsFor(const QMimeData* mimeData) const
{
	QList<Contact> result;
	QDomElement root = rootElementFor(mimeData);
	if (root.isNull())
		return result;

	for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement e = n.toElement();
		if (e.isNull())
			continue;

		if (e.tagName() == "contact") {
			Jid jid = tagContent(e);
			result << Contact(jid.full(),
			                  e.attribute("account"),
			                  e.attribute("group"));
		}
	}

	return result;
}

QList<ContactListModelSelection::Group> ContactListModelSelection::groupsFor(const QMimeData* mimeData) const
{
	QList<Group> result;
	QDomElement root = rootElementFor(mimeData);
	if (root.isNull())
		return result;

	for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement e = n.toElement();
		if (e.isNull())
			continue;

		if (e.tagName() == "group") {
			QString groupName = TextUtil::unescape(tagContent(e));
			result << Group(groupName);
		}
	}

	return result;
}

QList<ContactListModelSelection::Account> ContactListModelSelection::accountsFor(const QMimeData* mimeData) const
{
	QList<Account> result;
	QDomElement root = rootElementFor(mimeData);
	if (root.isNull())
		return result;

	for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement e = n.toElement();
		if (e.isNull())
			continue;

		if (e.tagName() == "account") {
			result << Account(e.attribute("id"));
		}
	}

	return result;
}

const QMimeData* ContactListModelSelection::mimeData() const
{
	return mimeData_ ? mimeData_ : this;
}

bool ContactListModelSelection::haveRosterSelection() const
{
	return haveRosterSelectionIn(mimeData());
}

QList<ContactListModelSelection::Contact> ContactListModelSelection::contacts() const
{
	return contactsFor(mimeData());
}

QList<ContactListModelSelection::Group> ContactListModelSelection::groups() const
{
	return groupsFor(mimeData());
}

QList<ContactListModelSelection::Account> ContactListModelSelection::accounts() const
{
	return accountsFor(mimeData());
}

bool ContactListModelSelection::isMultiSelection() const
{
	return (contacts().count() + groups().count()) > 1;
}

void ContactListModelSelection::debugSelection(const QMimeData* data, const QString& name)
{
	qWarning("*** debugSelection %s", qPrintable(name));
	ContactListModelSelection selection(data);
	foreach(const ContactListModelSelection::Contact& c, selection.contacts()) {
		qWarning("\tc: '%s' group: '%s' account: '%s'", qPrintable(c.jid), qPrintable(c.group), qPrintable(c.account));
	}
	foreach(const ContactListModelSelection::Group& g, selection.groups()) {
		qWarning("\tg: '%s'", qPrintable(g.fullName));
	}
	foreach(const ContactListModelSelection::Account& a, selection.accounts()) {
		qWarning("\ta: '%s'", qPrintable(a.id));
	}
}

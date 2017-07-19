/*
 * contactlistitem.cpp - base class for contact list items
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

#include "contactlistitem.h"

#include "psicontact.h"
#include "psiaccount.h"
#include "contactlistmodel.h"
#include "userlist.h"
#include "avatars.h"
#include "contactlistaccountmenu.h"
#include "contactlistgroupmenu.h"

#include <QCoreApplication>
#include <QTextDocument>

ContactListItem::ContactListItem(ContactListModel *model, Type type, SpecialGroupType specialGropType)
	: AbstractTreeItem()
	, _model(model)
	, _type(type)
	, _specialGroupType(specialGropType)
	, _editing(false)
	, _selfValid(true)
	, _contact(nullptr)
	, _account(nullptr)
	, _expanded(true)
	, _internalName()
	, _displayName()
	, _totalContacts(0)
	, _onlineContacts(0)
	, _shouldBeVisible(type != Type::GroupType)
	, _hidden(false)
{
	switch (_specialGroupType) {
	case SpecialGroupType::GeneralSpecialGroupType:
		_internalName = QString::fromUtf8("☣special_group_general");
		_displayName = PsiContact::generalGroupName();
		break;

	case SpecialGroupType::NotInListSpecialGroupType:
		_internalName = QString::fromUtf8("☣special_group_notinlist");
		_displayName = PsiContact::notInListGroupName();
		break;

	case SpecialGroupType::TransportsSpecialGroupType:
		_internalName = QString::fromUtf8("☣special_group_transports");
		_displayName = QCoreApplication::translate("ContactListItem", "Agents/Transports");
		break;

	case SpecialGroupType::MucPrivateChatsSpecialGroupType:
		_internalName = QString::fromUtf8("☣special_group_mucprivatechats");
		_displayName = QCoreApplication::translate("ContactListItem", "Private messages");
		break;

	case SpecialGroupType::ConferenceSpecialGroupType:
		_internalName = QString::fromUtf8("☣special_group_conferences");
		_displayName = QCoreApplication::translate("ContactListItem", "Conferences");
		break;

	default:
		break;
	}
}

ContactListItem::~ContactListItem()
{
	_selfValid = false; // just for a check for already freed but still mapped memory. though does not give any guarantee
}

ContactListModel *ContactListItem::model() const
{
	return _model;
}

ContactListItem::Type ContactListItem::type() const
{
	return _type;
}

ContactListItem::SpecialGroupType ContactListItem::specialGroupType() const
{
	return _specialGroupType;
}

bool ContactListItem::isRoot() const
{
	return _type == Type::RootType;
}

bool ContactListItem::isAccount() const
{
	return _type == Type::AccountType;
}

bool ContactListItem::isGroup() const
{
	return _type == Type::GroupType;
}

bool ContactListItem::isContact() const
{
	return _type == Type::ContactType;
}

bool ContactListItem::isEditable() const
{
	if (_type == Type::AccountType) {
		return true;
	}
	else if (_type == Type::ContactType) {
		return _contact->account() && _contact->inList();
	}
	else if (_type == Type::GroupType && _specialGroupType != SpecialGroupType::NoneSpecialGroupType) {
		return false;
	}
	else {
		AbstractTreeItemList children = AbstractTreeItem::children();
		for (const auto &child: children) {
			ContactListItem *item = static_cast<ContactListItem*>(child);
			if (item->isEditable()) {
				return true;
			}
		}
		return false;
	}

}

bool ContactListItem::isDragEnabled() const
{
	return _type == Type::ContactType && !_contact->isSelf();
}

bool ContactListItem::isRemovable() const
{
	bool res = false;
	if (_type == Type::GroupType) {
		switch (_specialGroupType) {
		case SpecialGroupType::GeneralSpecialGroupType:
		case SpecialGroupType::NotInListSpecialGroupType:
		case SpecialGroupType::MucPrivateChatsSpecialGroupType:
		case SpecialGroupType::NoneSpecialGroupType:
			res = true;
			break;

		default:
			break;
		}
	}
	else if (_type == Type::ContactType){
		Q_ASSERT(_contact);
		res = _contact->isRemovable();
	}
	return res;
}

bool ContactListItem::isExpandable() const
{
	return _type != Type::ContactType;
}

bool ContactListItem::expanded() const
{
	return _expanded;
}

void ContactListItem::setExpanded(bool expanded)
{
	_expanded = expanded;
}

ContactListItemMenu *ContactListItem::contextMenu()
{
	ContactListItemMenu *menu = nullptr;
	switch (_type) {
	case Type::ContactType:  menu = _contact->contextMenu(_model);                 break;
	case Type::AccountType:  menu = new ContactListAccountMenu(_account, _model);  break;
	case Type::GroupType:    menu = new ContactListGroupMenu(this, _model);        break;
	default:                                                                      break;
	}

	return menu;
}

bool ContactListItem::isFixedSize() const
{
	return true;
}

bool ContactListItem::lessThan(const ContactListItem* other) const
{
	if (_type == Type::GroupType && other->_type == Type::GroupType) {
		if (_specialGroupType != other->_specialGroupType) {
			return _specialGroupType < other->_specialGroupType;
		}
		else {
			return QString::localeAwareCompare(_displayName.toLower(), other->_displayName.toLower()) < 0;
		}
	}
	else if (_type == Type::ContactType && other->_type == Type::ContactType) {
		int rank = rankStatus(_contact->status().type()) - rankStatus(other->_contact->status().type());
		if (rank == 0)
			rank = QString::localeAwareCompare(_contact->name().toLower(), other->_contact->name().toLower());
		return rank < 0;
	}
	else if (_type == Type::AccountType && other->_type == Type::AccountType) {
		return QString::localeAwareCompare(_account->name().toLower(), other->_account->name().toLower()) < 0;
	}
	else if (_type == Type::ContactType && other->_type == Type::GroupType) {
		if (_contact->isSelf()) {
			return true;
		}
		else {
			return false;
		}
	}
	else if (_type == Type::GroupType && other->_type == Type::ContactType) {
		if (other->_contact->isSelf()) {
			return false;
		}
		else {
			return true;
		}
	}

	return false;
}

QString ContactListItem::name() const
{
	QString name;
	switch (_type) {
	case Type::AccountType:  name = _account->name();  break;
	case Type::ContactType:  name = _contact->name();  break;
	case Type::GroupType:    name = _displayName;      break;
	default:                                           break;
	}

	return name;
}

void ContactListItem::setName(const QString &name)
{
	switch (_type) {
	case Type::ContactType:
		Q_ASSERT(_contact);
		if (_contact) {
			_contact->setName(name);
		}
		break;

	case Type::GroupType:
		_displayName = name;
		break;

	default:
		break;
	}
}

QString ContactListItem::internalName() const
{
	QString res;

	switch (_type) {
	case Type::AccountType:	 res = _account->id();                   break;
	case Type::GroupType:    res = account()->id() + "::" + name();  break;
	default:                                                         break;
	}

	return res;
}

bool ContactListItem::editing() const
{
	return _editing;
}

void ContactListItem::setEditing(bool editing)
{
	_editing = editing;
}

void ContactListItem::setContact(PsiContact *contact)
{
	_contact = contact;
}

PsiContact *ContactListItem::contact() const
{
	return _contact;
}

void ContactListItem::setAccount(PsiAccount *account)
{
	_account = account;
}

PsiAccount *ContactListItem::account() const
{
	const ContactListItem *item = this;
	while (item && item->_type != Type::AccountType)
		item = item->parent();
	return item->_account;
}

bool ContactListItem::shouldBeVisible() const
{
	return _shouldBeVisible;
}

void ContactListItem::setHidden(bool hidden)
{
	_hidden = hidden;
	_model->updateItem(this);
}

bool ContactListItem::isHidden() const
{
	return _hidden;
}

QList<PsiContact*> ContactListItem::contacts()
{
	AbstractTreeItemList children = AbstractTreeItem::children();
	QList<PsiContact*> res;
	for (const auto &child: children) {
		ContactListItem *item = static_cast<ContactListItem*>(child);
		if (item->isContact()) {
			res << item->_contact;
		}
		else {
			res += item->contacts();
		}
	}

	return res;
}

ContactListItem *ContactListItem::findAccount(PsiAccount *account)
{
	AbstractTreeItemList children = AbstractTreeItem::children();
	ContactListItem *res = nullptr;
	for (const auto &child: children) {
		ContactListItem *item = static_cast<ContactListItem*>(child);
		if (item->isAccount() && item->account() == account) {
			res = item;
			break;
		}
	}

	return res;
}

ContactListItem *ContactListItem::findGroup(const QString &groupName)
{
	QString id;
	QString realGroupName;

	if (_type == Type::RootType) {
		id = groupName.section("::", 0, 0);
		realGroupName = groupName.section("::", 1, 1);
	}

	AbstractTreeItemList children = AbstractTreeItem::children();
	ContactListItem *res = nullptr;

	for (const auto &child: children) {
		ContactListItem *item = static_cast<ContactListItem*>(child);

		if (_type == Type::AccountType) {
			if (item->isGroup() && item->name() == groupName) {
				res = item;
				break;
			}
		}
		else if (_type == Type::RootType) {
			if (item->isAccount() && item->account()->id() == id) {
				res = item->findGroup(realGroupName);
			}
		}
	}

	return res;
}

ContactListItem *ContactListItem::findGroup(ContactListItem::SpecialGroupType specialGroupType)
{
	AbstractTreeItemList children = AbstractTreeItem::children();
	ContactListItem *res = nullptr;
	for (const auto &child: children) {
		ContactListItem *item = static_cast<ContactListItem*>(child);
		if (item->isGroup() && item->specialGroupType() == specialGroupType) {
			res = item;
			break;
		}
	}

	return res;
}

ContactListItem *ContactListItem::findContact(PsiContact *contact)
{
	AbstractTreeItemList children = AbstractTreeItem::children();
	ContactListItem *res = nullptr;
	for (const auto &child: children) {
		ContactListItem *item = static_cast<ContactListItem*>(child);
		if (item->contact() == contact) {
			res = item;
			break;
		}
	}

	return res;
}

void ContactListItem::setValue(int role, const QVariant &value)
{
	Q_UNUSED(role);
	Q_UNUSED(value);
}

QVariant ContactListItem::value(int role) const
{
	QVariant res;
	Q_ASSERT(_selfValid);
	// Common roles first
	switch (role) {
	case ContactListModel::TypeRole:      res = QVariant::fromValue(_type);  break;
	case ContactListModel::ExpandedRole:  res = _expanded;                   break;

	default:

		// Other roles. Behaviour relies from type.
		switch (_type) {
		case Type::ContactType:
			Q_ASSERT(_contact);
			switch (role) {
			case Qt::EditRole:                                 res = _contact->name();                                  break;
			case Qt::DisplayRole:                              res = _contact->name();                                  break;
			case Qt::ToolTipRole:                              res = _contact->userListItem().makeTip(true, false);     break;
			case ContactListModel::JidRole:                    res = _contact->jid().full();                            break;
			case ContactListModel::PictureRole:                res = _contact->picture();                               break;
			case ContactListModel::StatusTextRole:             res = _contact->statusText().simplified();               break;
			case ContactListModel::StatusTypeRole:             res = _contact->status().type();                         break;
			case ContactListModel::PresenceErrorRole:          res = _contact->userListItem().presenceError();          break;
			case ContactListModel::IsAgentRole:                res = _contact->isAgent();                               break;
			case ContactListModel::AuthorizesToSeeStatusRole:  res = _contact->authorizesToSeeStatus();                 break;
			case ContactListModel::AskingForAuthRole:          res = _contact->askingForAuth();                         break;
			case ContactListModel::IsAlertingRole:             res = _contact->alerting();                              break;
			case ContactListModel::AlertPictureRole:           res = _contact->alertPicture();                          break;
			case ContactListModel::IsAnimRole:                 res = _contact->isAnimated();                            break;
			case ContactListModel::PhaseRole:                  res = false;                                             break;
			case ContactListModel::GeolocationRole:            res = !_contact->userListItem().geoLocation().isNull();  break;
			case ContactListModel::TuneRole:                   res = !_contact->userListItem().tune().isEmpty();        break;
			case ContactListModel::ClientRole:                 res = _contact->userListItem().clients();                break;
			case ContactListModel::IsMucRole:                  res = _contact->userListItem().isConference();           break;
			case ContactListModel::MucMessagesRole:            res = _contact->userListItem().pending();                break;
			case ContactListModel::BlockRole:                  res = _contact->isBlocked();                             break;
			case ContactListModel::IsSecureRole:               res = _contact->userListItem().isSecure();               break;

			case ContactListModel::AvatarRole:
				if (_contact->isPrivate())
					res = _contact->account()->avatarFactory()->getMucAvatar(_contact->jid());
				else
					res = _contact->account()->avatarFactory()->getAvatar(_contact->jid());

				break;

			case ContactListModel::MoodRole:
				return QVariant::fromValue(_contact->userListItem().mood());

			case ContactListModel::ActivityRole:
				return QVariant::fromValue(_contact->userListItem().activity());
				break;

			default:
				break;
			}

			break;

		case Type::AccountType:
			Q_ASSERT(_account);
			switch (role) {
			case Qt::EditRole:                          res = _account->name();                                              break;
			case Qt::DisplayRole:                       res = _account->name();                                              break;
			case Qt::ToolTipRole:                       res = _account->selfContact()->userListItem().makeTip(true, false);  break;
			case ContactListModel::JidRole:             res = _account->jid().full();                                        break;
			case ContactListModel::StatusTextRole:      res = _account->status().status().simplified();                      break;
			case ContactListModel::StatusTypeRole:      res = _account->status().type();                                     break;
			case ContactListModel::OnlineContactsRole:  res = _account->onlineContactsCount();                               break;
			case ContactListModel::TotalContactsRole:   res = _account->contactList().count();                               break;
			case ContactListModel::UsingSSLRole:        res = _account->usingSSL();                                          break;
			case ContactListModel::IsAlertingRole:      res = _account->alerting();                                          break;
			case ContactListModel::AlertPictureRole:    res = _account->alertPicture();                                      break;
			default:                                                                                                         break;
			}

			break;

		case Type::GroupType:
			switch (role) {
			case Qt::EditRole:                          res = name();  break;
			case Qt::DisplayRole:                       res = name();  break;
			case Qt::ToolTipRole: {
				QString text = name();
#ifdef HAVE_QT5
				text = text.toHtmlEscaped();
#else
				text = Qt::escape(text);
#endif
				res = text + QString(" (%1/%2)").arg(_onlineContacts).arg(_totalContacts);
			} break;

			case ContactListModel::DisplayGroupRole:
				updateContactsCount();
				res = name() + QString(" (%1/%2)").arg(_onlineContacts).arg(_totalContacts);
				break;

			case ContactListModel::OnlineContactsRole:
				updateContactsCount();
				res = _onlineContacts;
				break;

			case ContactListModel::TotalContactsRole:
				updateContactsCount();
				res = _totalContacts;
				break;

			default:
				break;
			}
			break;

		default:
			break;
		}

		break;
	}

	return res;
}

void ContactListItem::updateContactsCount() const
{
	if (_type != Type::GroupType)
		return;

	_totalContacts = 0;
	_onlineContacts = 0;
	_shouldBeVisible = false;

	AbstractTreeItemList children = AbstractTreeItem::children();
	for (const auto &child: children) {
		ContactListItem *item = static_cast<ContactListItem*>(child);
		if (item->_type == Type::ContactType) {
			_totalContacts++;
			if (item->_contact->alerting()
				|| item->_contact->isAlwaysVisible()) {

				_shouldBeVisible = true;
			}

			if (item->_contact->isOnline()) {
				_onlineContacts++;
			}
		}
		else if (item->_type == Type::GroupType) {
			item->updateContactsCount();
			if (item->shouldBeVisible()) {
				_shouldBeVisible = true;
			}
			_totalContacts += item->_totalContacts;
			_onlineContacts += item->_onlineContacts;
		}
	}
}

QList<ContactListItem*> ContactListItem::allChildren() const
{
	AbstractTreeItemList children = AbstractTreeItem::children();
	QList<ContactListItem*> res;
	for (const auto &child: children) {
		ContactListItem *item = static_cast<ContactListItem*>(child);
		res << item;
		res += item->allChildren();
	}

	return res;
}

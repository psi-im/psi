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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "contactlistitem.h"

#include "psicontact.h"

ContactListItem::ContactListItem(QObject* parent)
	: QObject(parent)
	, editing_(false)
{
}

ContactListItem::~ContactListItem()
{
}

bool ContactListItem::isEditable() const
{
	return false;
}

bool ContactListItem::isDragEnabled() const
{
	return isEditable();
}

bool ContactListItem::isRemovable() const
{
	return false;
}

bool ContactListItem::isExpandable() const
{
	return false;
}

bool ContactListItem::expanded() const
{
	return false;
}

void ContactListItem::setExpanded(bool expanded)
{
	Q_UNUSED(expanded);
}

ContactListItemMenu* ContactListItem::contextMenu(ContactListModel* model)
{
	Q_UNUSED(model);
	return 0;
}

bool ContactListItem::isFixedSize() const
{
	return true;
}

bool ContactListItem::compare(const ContactListItem* other) const
{
	const PsiContact* left  = dynamic_cast<const PsiContact*>(this);
	const PsiContact* right = dynamic_cast<const PsiContact*>(other);
	if (!left ^ !right) {
		return !right;
	}
	return comparisonName() < other->comparisonName();
}

QString ContactListItem::comparisonName() const
{
	return name();
}

bool ContactListItem::editing() const
{
	return editing_;
}

void ContactListItem::setEditing(bool editing)
{
	editing_ = editing;
}

const QString& ContactListItem::displayName() const
{
	return name();
}

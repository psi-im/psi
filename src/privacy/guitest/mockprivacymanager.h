/*
 * Copyright (C) 2006  Remko Troncon
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
 
#ifndef MOCKPRIVACYMANAGER_H
#define MOCKPRIVACYMANAGER_H

#include <QObject>
#include <QStringList>

#include "privacymanager.h"
#include "privacylistitem.h"

class MockPrivacyManager : public PrivacyManager
{
	Q_OBJECT

public:
	MockPrivacyManager();

	virtual void requestListNames();
	virtual void changeDefaultList(const QString& name);
	virtual void changeActiveList(const QString& name);
	virtual void changeList(const PrivacyList& list);
	virtual void getDefaultList();
	virtual void requestList(const QString& name);

private:
	PrivacyListItem createItem(PrivacyListItem::Type type, const QString& value, PrivacyListItem::Action action, bool message, bool presence_in, bool presence_out, bool iq);

};

#endif

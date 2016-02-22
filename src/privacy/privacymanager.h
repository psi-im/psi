/*
 * privacymanager.h
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef PRIVACYMANAGER_H
#define PRIVACYMANAGER_H

#include <QObject>
#include <QStringList>

class QString;
class PrivacyList;

class PrivacyManager : public QObject
{
	Q_OBJECT

public:
	virtual void requestListNames() = 0;
	virtual void changeDefaultList(const QString& name) = 0;
	virtual void changeActiveList(const QString& name) = 0;
	virtual void changeList(const PrivacyList& list) = 0;
	virtual void getDefaultList() = 0;
	virtual void requestList(const QString& name) = 0;

signals:
	void changeDefaultList_success(QString);
	void changeDefaultList_error();
	void changeActiveList_success(QString);
	void changeActiveList_error();
	void changeList_success(QString);
	void changeList_error();
	void defaultListAvailable(const PrivacyList&);
	void defaultListError();
	void listChangeSuccess();
	void listChangeError();
	void listReceived(const PrivacyList& p);
	void listError();
	void listsReceived(const QString& defaultList, const QString& activeList, const QStringList& lists);
	void listsError();
};

#endif

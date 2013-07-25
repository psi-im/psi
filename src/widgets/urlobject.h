/*
 * urlobject.h - helper class for handling links
 * Copyright (C) 2003-2006  Michail Pishchagin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef URLOBJECT_H
#define URLOBJECT_H

#include <QObject>

class QMenu;

// TODO: This class should be further refactored into more modular system.
// It should allow registering/unregistering of QRegExps corresponding to
// different actions. Also class's base API should be made public.
//
// Examples of future use:
// URLObject::getInstance()::registerAction(QRegExp("^http:\/\/"), act_open_browser);

class URLObject : public QObject
{
	Q_OBJECT

protected:
	URLObject();

public:
	static URLObject *getInstance();
	QMenu *createPopupMenu(const QString &lnk);

	void popupAction(QString lnk);

signals:
	void openURL(QString);

public:
	class Private;
private:
	Private *d;
};

#endif

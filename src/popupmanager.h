/*
 * popupmanager.h
 * Copyright (C) 2011-2012  Evgeny Khryukin
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

#ifndef POPUPMANAGER_H
#define POPUPMANAGER_H

#include <QStringList>
#include "psievent.h"

class PsiAccount;
class PsiCon;
class UserListItem;
class PsiIcon;
class QPixmap;

namespace XMPP {
	class Jid;
	class Resource;
}
using namespace XMPP;

class PopupManager
{

public:
	PopupManager(PsiCon* psi);
	virtual ~PopupManager();

	enum PopupType {
		AlertNone = 0,

		AlertOnline = 1,
		AlertOffline = 2,
		AlertStatusChange = 3,

		AlertMessage = 4,
		AlertComposing = 5,
		AlertChat = 6,
		AlertHeadline = 7,
		AlertFile = 8,
		AlertAvCall = 9,
		AlertGcHighlight = 10,
		AlertCustom = 11
	};

	int registerOption(const QString& name, int initValue = 5, const QString& path = QString());
	void unregisterOption(const QString& name);
	void setValue(const QString& name, int value);
	int value(const QString& name) const;
	const QString optionPath(const QString& name) const;
	const QStringList optionsNamesList() const;

	QStringList availableTypes() const;
	QString currentType() const;

	void doPopup(PsiAccount* account, PopupType type, const Jid& j, const Resource& r,
			    UserListItem* u = 0, const PsiEvent::Ptr &e = PsiEvent::Ptr(), bool checkNoPopup = true);
	void doPopup(PsiAccount *account, const Jid &j, const PsiIcon *titleIcon, const QString& titleText,
			    const QPixmap *avatar, const PsiIcon *icon, const QString& text, bool checkNoPopup = true, PopupType type = AlertNone);

private:
	class Private;
	Private* d;
};

#endif

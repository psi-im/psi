/*
 * psipopup.h - the Psi passive popup class
 * Copyright (C) 2003  Michail Pishchagin
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

#ifndef PSIPOPUP_H
#define PSIPOPUP_H

#include <qobject.h>

class PsiCon;
class PsiAccount;
class UserListItem;
class FancyPopup;
class Icon;
class PsiEvent;
namespace XMPP {
	class Jid;
	class Resource;
}
using namespace XMPP;


class PsiPopup : public QObject
{
	Q_OBJECT
public:
	PsiPopup(const Icon *titleIcon, QString titleText, PsiAccount *acc);
	~PsiPopup();

	enum PopupType {
		AlertNone = 0,

		AlertOnline,
		AlertOffline,
		AlertStatusChange,

		AlertMessage,
		AlertChat,
		AlertHeadline,
		AlertFile
	};
	PsiPopup(PopupType type, PsiAccount *acc);

	void setData(const Icon *icon, QString text);
	void setData(const Jid &, const Resource &, const UserListItem * = 0, const PsiEvent * = 0);

	void show();

	QString id() const;
	FancyPopup *popup();

	static void deleteAll();

public:
	class Private;
private:
	Private *d;
	friend class Private;
};

#endif

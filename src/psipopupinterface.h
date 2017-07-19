/*
 * psipopuinterface.h
 * Copyright (C) 2012  Evgeny Khryukin
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

#ifndef PSIPOPUPINTERFACE_H
#define PSIPOPUPINTERFACE_H

#include "popupmanager.h"
#include "psievent.h"

class PsiPopupInterface;

class PsiPopupPluginInterface
{
public:
	virtual QString name() const = 0;
	virtual bool isAvailable() const { return true; }
	virtual PsiPopupInterface* popup(QObject* ) = 0;
};

Q_DECLARE_INTERFACE(PsiPopupPluginInterface, "psi-im.org.PsiPopupInterface/0.1")


class PsiPopupInterface
{
public:
	virtual void popup(PsiAccount* account, PopupManager::PopupType type, const Jid& j, const Resource& r, const UserListItem* = 0, const PsiEvent::Ptr& = PsiEvent::Ptr()) = 0;
	virtual void popup(PsiAccount* account, PopupManager::PopupType type, const Jid& j, const PsiIcon* titleIcon, const QString& titleText,
				    const QPixmap* avatar, const PsiIcon* icon, const QString& text) = 0;

	void setDuration(int d) { duration_ = d; }

protected:
	static QString clipText(QString text);
	static QString title(PopupManager::PopupType type, bool *doAlertIcon, PsiIcon **icon);
	int duration() const { return duration_; }

private:
	int duration_;
};

#endif

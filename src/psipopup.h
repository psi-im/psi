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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef PSIPOPUP_H
#define PSIPOPUP_H

#ifndef QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include "psipopupinterface.h"
#include "psievent.h"

class FancyPopup;

class PsiPopup : public QObject, public PsiPopupInterface
{
	Q_OBJECT

public:
	PsiPopup(QObject* parent = 0);
	~PsiPopup();

	virtual void popup(PsiAccount* account, PopupManager::PopupType type, const Jid& j, const Resource& r, const UserListItem* = 0, const PsiEvent::Ptr& = PsiEvent::Ptr());
	virtual void popup(PsiAccount* account, PopupManager::PopupType type, const Jid& j, const PsiIcon* titleIcon, const QString& titleText,
		   const QPixmap* avatar, const PsiIcon* icon, const QString& text);

	static void deleteAll();

private:
	void setData(const Jid &, const Resource &, const UserListItem * = 0, const PsiEvent::Ptr &event = PsiEvent::Ptr());
	void setData(const QPixmap *avatar, const PsiIcon *icon, const QString& text);
	void setJid(const Jid &j);
	QString id() const;
	FancyPopup *popup() const;
	void show();

private:
	class Private;
	Private *d;
	friend class Private;
};

class PsiPopupPlugin : public QObject, public PsiPopupPluginInterface
{
	Q_OBJECT
#ifdef HAVE_QT5
	Q_PLUGIN_METADATA(IID "org.psi-im.Psi.PsiPopupPluginInterface")
#endif
	Q_INTERFACES(PsiPopupPluginInterface)

public:
	virtual ~PsiPopupPlugin() { PsiPopup::deleteAll(); }
	virtual QString name() const { return "Classic"; }
	virtual PsiPopupInterface* popup(QObject* p) { return new PsiPopup(p); }
};

#endif

/*
 * psigrowlnotifier.h: Psi's interface to Growl
 * Copyright (C) 2005  Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
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

#ifndef PSIGROWLNOTIFIER_H
#define PSIGROWLNOTIFIER_H

#ifndef QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include "psipopupinterface.h"

class GrowlNotifier;
class NotificationContext;

using namespace XMPP;

/**
 * An interface for Psi to Growl.
 * This class uses GrowlNotifier to interface with Growl.
 * There is at most 1 PsiGrowlNotifier per Psi session (Singleton). This
 * notifier can be retrieved using instance().
 *
 * \see GrowlNotifier
 */
class PsiGrowlNotifier : public QObject, public PsiPopupInterface
{
	Q_OBJECT

public:
	static PsiGrowlNotifier* instance();
	static bool isAvailable();

	virtual void popup(PsiAccount* account, PopupManager::PopupType type, const Jid& j, const Resource& r, const UserListItem* = 0, const PsiEvent::Ptr& = PsiEvent::Ptr());
	virtual void popup(PsiAccount* account, PopupManager::PopupType type, const Jid& j, const PsiIcon* titleIcon, const QString& titleText,
		   const QPixmap* avatar, const PsiIcon* icon, const QString& text);

public slots:
	void notificationClicked(void*);
	void notificationTimedOut(void*);

private slots:
	void cleanup();

private:
	PsiGrowlNotifier();
	void tryDeleteContext(NotificationContext* context);

	static PsiGrowlNotifier* instance_;
	GrowlNotifier* gn_;
	QList<NotificationContext*> contexts_;
};

class PsiGrowlNotifierPlugin : public QObject, public PsiPopupPluginInterface
{
	Q_OBJECT
#ifdef HAVE_QT5
	Q_PLUGIN_METADATA(IID "org.psi-im.Psi.PsiPopupPluginInterface")
#endif
	Q_INTERFACES(PsiPopupPluginInterface)

public:
	virtual QString name() const { return "Growl"; }
	virtual PsiPopupInterface* popup(QObject* ) { return PsiGrowlNotifier::instance(); }
	virtual bool isAvailable() const { return PsiGrowlNotifier::isAvailable(); }
};

#endif

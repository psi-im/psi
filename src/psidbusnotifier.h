/*
 * psidbusnotifier.h: Psi's interface to org.freedesktop.Notify
 * Copyright (C) 2012  Evgeny Khryukin
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

#ifndef PSIDBUSNOTIFIER_H
#define PSIDBUSNOTIFIER_H

#ifndef QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include "psipopupinterface.h"
#include "xmpp_jid.h"

class QDBusPendingCallWatcher;
class QTimer;

class PsiDBusNotifier : public QObject, public PsiPopupInterface
{
	Q_OBJECT

public:
	PsiDBusNotifier(QObject* parent = 0);
	~PsiDBusNotifier();
	static bool isAvailable();

	virtual void popup(PsiAccount *account, PopupManager::PopupType type, const Jid& j, const Resource& r, const UserListItem* = 0, const PsiEvent::Ptr& = PsiEvent::Ptr());
	virtual void popup(PsiAccount* account, PopupManager::PopupType type, const Jid& j, const PsiIcon* titleIcon, const QString& titleText,
			   const QPixmap* avatar, const PsiIcon* icon, const QString& text);

private slots:
	void popupClosed(uint id, uint reason);
	void asyncCallFinished(QDBusPendingCallWatcher*);
	void readyToDie();

private:
	static bool checkServer();
	static QStringList capabilities();

private:
	Jid jid_;
	uint id_;
	PsiAccount *account_;
	PsiEvent::Ptr event_;
	QTimer *lifeTimer_;
	static QStringList caps_;
};

class PsiDBusNotifierPlugin : public QObject, public PsiPopupPluginInterface
{
	Q_OBJECT
#ifdef HAVE_QT5
	Q_PLUGIN_METADATA(IID "org.psi-im.Psi.PsiPopupPluginInterface")
#endif
	Q_INTERFACES(PsiPopupPluginInterface)

public:
	virtual QString name() const { return "DBus"; }
	virtual PsiPopupInterface* popup(QObject* p) { return new PsiDBusNotifier(p); }
	virtual bool isAvailable() { return PsiDBusNotifier::isAvailable(); }
};

#endif

/*
 * activeprofiles_win.cpp - Class for interacting with other app instances
 * Copyright (C) 2006  Maciej Niedzielski
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

#include "activeprofiles.h"
#include "applicationinfo.h"
#include "psicon.h"

#include <QCoreApplication>
#include <QWidget>
#include <QTimer>
#include <windows.h>

/*
	Implementor notes:
	
	This file uses WinAPI a lot. It is important to remember that we still want to support Win9x family.
	For that reason, we have to use QT_WA macro for functions that exist in two versions.
	
	Also note that writing QString("x").toLocal8Bit().constData() is a bad idea and must not be done.
*/

class ActiveProfiles::Private : public QWidget
{
public:
	Private(ActiveProfiles *aprof) : app(ApplicationInfo::IPCName()), home(ApplicationInfo::homeDir()), profile(""), ap(aprof), mutex(0), changesMutex(0) {

		app.replace('\\', '/');	// '\\' has a special meaning in mutex name
		home.replace('\\', '/');

		const QString m = QString("%1 ChangesMutex {4F5AEDA9-7D3D-4ebe-8614-FB338146CE80}").arg(app);
		const QString c = QString("%1 IPC Command {4F5AEDA9-7D3D-4ebe-8614-FB338146CE80}").arg(app);
		
		QT_WA(
			changesMutex = CreateMutex(0, FALSE, (LPCWSTR)m.utf16());
			psiIpcCommand = RegisterWindowMessage((LPCWSTR)c.utf16());
		,
			QByteArray a = m.toLocal8Bit();	// must not call constData() of a temp object
			changesMutex = CreateMutexA(0, FALSE, (LPCSTR)a.constData());
			a = c.toLocal8Bit();
			psiIpcCommand = RegisterWindowMessageA((LPCSTR)a.constData());
		)

		if (!changesMutex) {
			qWarning("Couldn't create IPC mutex");
		}
		if (!psiIpcCommand) {
			qWarning("Couldn't register IPC WM_message");
		}
	}

	QString app, home, profile;
	ActiveProfiles * const ap;
	HANDLE mutex, changesMutex;

	QString mutexName(const QString &profile) const {
		return "ProfileMutex\0x01" + app + "\0x01" + home + "\0x01" + profile + "\0x01 {4F5AEDA9-7D3D-4ebe-8614-FB338146CE80}";
	}

	QString windowName(const QString &profile) const {
		return "ProfileWindow\0x01" + app + "\0x01" + home + "\0x01" + profile + "\0x01 {4F5AEDA9-7D3D-4ebe-8614-FB338146CE80}";
	}

	void startChanges()	{
		WaitForSingleObject(changesMutex, INFINITE);
	}

	void endChanges() {
		ReleaseMutex(changesMutex);
	}

	void setWindowText(const QString &text) {
		QT_WA(
			SetWindowTextW(winId(), (LPCWSTR)text.utf16());
		,
			QByteArray a = text.toLocal8Bit();
			SetWindowTextA(winId(), (LPCSTR)a.constData());
		)
	}

	// WM_PSICOMMAND
	static UINT psiIpcCommand;	// = RegisterWindowMessage()
	static WPARAM raiseCommand;	// = 1

	// WM_COPYDATA
	static const DWORD stringListMessage = 1;

	bool sendMessage(const QString &to, UINT message, WPARAM wParam, LPARAM lParam) const;
	bool winEvent(MSG *msg, long *result);

	bool sendStringList(const QString &to, const QStringList &list) const;

	QString pickProfile() const;
};

UINT ActiveProfiles::Private::psiIpcCommand = 0;
WPARAM ActiveProfiles::Private::raiseCommand = 1;

QString ActiveProfiles::Private::pickProfile() const
{
	QStringList profiles = getProfilesList();
	foreach (QString p, profiles) {
		if (ap->isActive(p)) {
			return p;
		}
	}
	return QString();
}

bool ActiveProfiles::Private::sendMessage(const QString &to, UINT message, WPARAM wParam, LPARAM lParam) const
{
	QString profile = to;
	if (profile.isEmpty()) {
		profile = pickProfile();
	}
	if (profile.isEmpty()) {
		return false;
	}

	HWND hwnd;
	QT_WA(
		hwnd = FindWindowW(0, (LPCWSTR)windowName(to).utf16());
	,
		QByteArray a = windowName(to).toLocal8Bit();
		hwnd = FindWindowA(0, (LPCSTR)a.constData());
	)

	if (!hwnd)
		return false;

	SendMessageA(hwnd, message, wParam, lParam);
	return true;
}

bool ActiveProfiles::Private::sendStringList(const QString &to, const QStringList &list) const
{
	if (to.isEmpty())
		return false;

	QByteArray ba;

	ba.append(list[0].toUtf8());
	for (int i = 1; i < list.size(); ++i) {
		const int z = ba.size();
		ba.append(" " + list[i].toUtf8());
		ba[z] = '\0';
	}

	COPYDATASTRUCT cd;
	cd.dwData = stringListMessage;
	cd.cbData = ba.size()+1;
	cd.lpData = (void*)ba.data();

	return sendMessage(to, WM_COPYDATA, (WPARAM)winId(), (LPARAM)(LPVOID)&cd);
}

bool ActiveProfiles::Private::winEvent(MSG *msg, long *result)
{
	if (msg->message == WM_COPYDATA) {
		*result = FALSE;
		COPYDATASTRUCT *cd = (COPYDATASTRUCT *)msg->lParam;
		if (cd->dwData == stringListMessage) {
			char *data = (char*)cd->lpData;
			const char *end = data + cd->cbData - 1;

			// handle this error here, not to worry later
			if (*end != '\0') {
				return true;
			}

			QStringList list;

			while (data < end) {
				QString s = QString::fromUtf8(data);
				list << s;
				data += strlen(data) + 1;
			}

			if (list.count() > 1) {
				if (list[0] == "openUri") {
					emit ap->openUriRequested(list.value(1));
					*result = TRUE;
				} else if (list[0] == "setStatus") {
					emit ap->setStatusRequested(list.value(1), list.value(2));
					*result = TRUE;
				}
			}
		}
		return true;
	}
	else if (msg->message == psiIpcCommand) {
		*result = FALSE;
		if (msg->wParam == raiseCommand) {
			emit ap->raiseRequested();
			*result = TRUE;
		}
		return true;
	}

	return false;
}

ActiveProfiles::ActiveProfiles()
	: QObject(QCoreApplication::instance())
{
	d = new ActiveProfiles::Private(this);
}

ActiveProfiles::~ActiveProfiles()
{
	delete d;
	d = 0;
}

bool ActiveProfiles::setThisProfile(const QString &profile)
{
	if (profile == d->profile)
		return true;

	if (profile.isEmpty()) {
		unsetThisProfile();
		return true;
	}

	d->startChanges();
	HANDLE m;
	QT_WA(
		m = CreateMutexW(0, TRUE, (LPCWSTR)d->mutexName(profile).utf16());
	,
		QByteArray a = d->mutexName(profile).toLocal8Bit();
		m = CreateMutexA(0, TRUE, (LPCSTR)a.constData());
	)	
		
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		CloseHandle(m);
		d->endChanges();
		return false;
	}
	else {
		if (d->mutex) {
			CloseHandle(d->mutex);
		}
		d->mutex = m;
		d->profile = profile;
		d->setWindowText(d->windowName(profile));
		d->endChanges();
		return true;
	}
}

void ActiveProfiles::unsetThisProfile()
{
	d->startChanges();
	CloseHandle(d->mutex);
	d->mutex = 0;
	d->profile = QString::null;
	d->setWindowText("");
	d->endChanges();
}

QString ActiveProfiles::thisProfile() const
{
	return d->profile;
}

bool ActiveProfiles::isActive(const QString &profile) const
{	
	HANDLE m;
	QT_WA(
		m = OpenMutexW(0, FALSE, (LPCWSTR)d->mutexName(profile).utf16());
	,
		QByteArray a = d->mutexName(profile).toLocal8Bit();
		m = OpenMutexA(0, FALSE, (LPCSTR)a.constData());
	)
	if (GetLastError() == ERROR_FILE_NOT_FOUND) {
		return false;
	}
	else {
		CloseHandle(m);
		return true;
	}
}

bool ActiveProfiles::isAnyActive() const
{
	return !d->pickProfile().isEmpty();
}


bool ActiveProfiles::raise(const QString &profile, bool withUI) const
{
	QLabel *lab = 0;
	if (withUI) {
		lab = new QLabel(tr("This psi profile is already running...<br>please wait..."));
		QTimer::singleShot(250, lab, SLOT(show()));
	}

	bool res = d->sendMessage(profile, d->psiIpcCommand, d->raiseCommand, 0);

	if (withUI) {
		lab->hide();
		delete lab;
	}

	return res;
}

bool ActiveProfiles::openUri(const QString &profile, const QString &uri) const
{
	QStringList list;
	list << "openUri" << uri;
	return d->sendStringList(profile.isEmpty()? d->pickProfile() : profile, list);
}

bool ActiveProfiles::setStatus(const QString &profile, const QString &status, const QString &message) const
{
	QStringList list;
	list << "setStatus" << status << message;
	return d->sendStringList(profile.isEmpty()? d->pickProfile() : profile, list);
}

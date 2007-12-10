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

#include "activepsiprofiles.h"
#include "applicationinfo.h"
#include "psicon.h"

#include <QWidget>
#include <windows.h>

class ActiveProfiles::Private : public QWidget
{
public:
	Private(ActiveProfiles *aprof) : app(ApplicationInfo::IPCName()), home(ApplicationInfo::homeDir()), ap(aprof), profile(""), mutex(0), changesMutex(0) {

		app.replace('\\', '/');	// '\\' has a special meaning in mutex name
		home.replace('\\', '/');

		changesMutex = CreateMutex(0, FALSE, QString("%1 ChangesMutex {4F5AEDA9-7D3D-4ebe-8614-FB338146CE80}").arg(app).utf16());
		if (changesMutex == NULL)
			qWarning("Couldn't create IPC mutex");

		psiIpcCommand = RegisterWindowMessage(QString("%1 IPC Command {4F5AEDA9-7D3D-4ebe-8614-FB338146CE80}").arg(app).utf16());
		if (psiIpcCommand == 0)
			qWarning("Couldn't register IPC WM_message");
	}

	ActiveProfiles * const ap;
	QString app, home, profile;
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

	// WM_PSICOMMAND
	static UINT psiIpcCommand;	// = RegisterWindowMessage()
	static WPARAM raiseCommand;	// = 1

	// WM_COPYDATA
	static const int stringListMessage = 1;

	bool sendMessage(const QString &to, UINT message, WPARAM wParam, LPARAM lParam) const;
	bool winEvent(MSG *msg, long *result);

	bool sendStringList(const QString &to, const QStringList &list) const;
};

UINT ActiveProfiles::Private::psiIpcCommand = 0;
WPARAM ActiveProfiles::Private::raiseCommand = 1;

bool ActiveProfiles::Private::sendMessage(const QString &to, UINT message, WPARAM wParam, LPARAM lParam) const
{
	HWND hwnd = FindWindow(0, windowName(to).utf16());

	if (!hwnd)
		return false;

	SendMessage(hwnd, message, wParam, lParam);
	return true;
}

bool ActiveProfiles::Private::sendStringList(const QString &to, const QStringList &list) const
{
	if (to.isEmpty())
		return false;

	QByteArray ba;

	ba.append(list[0].toUtf8());
	for ( int i = 1; i < list.size(); ++i ) {
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
	*result = 1;	// by default - not ok

	if (msg->message == WM_COPYDATA) {
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

			if (list[0] == "openUri") {
				emit ap->openUri(list[1]);
				*result = 0;	// ok
			}
		}
		return true;
	}
	else if (msg->message == psiIpcCommand) {
		if (msg->wParam == raiseCommand) {
			emit ap->raiseMainWindow();
			*result = 0; // ok
		}
		return true;
	}

	return false;
}

ActiveProfiles::ActiveProfiles()
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
	HANDLE m = CreateMutex(0, TRUE, d->mutexName(profile).utf16());
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		CloseHandle(m);
		d->endChanges();
		return false;
	}
	else {
		CloseHandle(d->mutex);
		d->mutex = m;
		d->profile = profile;
		//d->setWindowTitle() does not work - use SetWindowText
		SetWindowText(d->winId(), d->windowName(profile).utf16());
		d->endChanges();
		return true;
	}
}

void ActiveProfiles::unsetThisProfile()
{
	d->startChanges();
	CloseHandle(d->mutex);
	d->profile = QString::null;
	//d->setWindowTitle("");
	SetWindowText(d->winId(), L"");
	d->endChanges();
}

QString ActiveProfiles::thisProfile() const
{
	return d->profile;
}

bool ActiveProfiles::isActive(const QString &profile) const
{
	HANDLE m = OpenMutex(0, FALSE, d->mutexName(profile).utf16());
	if (GetLastError() == ERROR_FILE_NOT_FOUND) {
		return false;
	}
	else {
		CloseHandle(m);
		return true;
	}
}

bool ActiveProfiles::raiseOther(QString profile, bool withUI) const
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

bool ActiveProfiles::sendOpenUri(const QString &uri, const QString &profile) const
{
	QStringList list;
	list << "openUri" << uri;
	return d->sendStringList(isActive(profile)? profile : pickProfile(), list);
}

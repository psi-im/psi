/*
 * winampcontroller.cpp 
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <windows.h>
#include <qglobal.h>

#ifdef Q_CC_MSVC
#pragma warning(push)
#pragma warning(disable: 4100)
#endif

// this file generates eight C4100 warnings, when compiled with MSVC2003
#include "third-party/wa_ipc.h"

#ifdef Q_CC_MSVC
#pragma warning(pop)
#endif

#include "winampcontroller.h"


/**
 * \class WinAmpController
 * \brief A controller for WinAmp.
 */


/**
 * \brief Constructs the controller.
 */
WinAmpController::WinAmpController() : PollingTuneController()
{
}


Tune WinAmpController::currentTune()
{
	Tune tune;
	TCHAR app_title[2048];
#ifdef UNICODE
	HWND h = FindWindow(L"Winamp v1.x", NULL);
#else
	HWND h = FindWindow("Winamp v1.x", NULL);
#endif
	if (h && SendMessage(h,WM_WA_IPC,0,IPC_ISPLAYING) == 1 && GetWindowText(h, app_title, 2048)) {
#ifdef UNICODE
		QString title = QString::fromUtf16((const ushort*) app_title);
#else
		QString title(app_title);
#endif

		// Chop off WinAmp title
		if (title.endsWith("- Winamp"))
			title.chop(8);
		title = title.trimmed();

		// Chop off track number
		if (title.contains(" "))
			title = title.right(title.size() - title.indexOf(" ") - 1);
		tune.setName(title);
		tune.setTime(SendMessage(h,WM_WA_IPC,1,IPC_GETOUTPUTTIME));
	}
	return tune;
}

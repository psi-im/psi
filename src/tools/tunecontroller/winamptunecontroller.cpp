/*
 * winamptunecontroller.cpp
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "winamptunecontroller.h"

#ifdef Q_CC_MSVC
#pragma warning(push)
#pragma warning(disable : 4100)
#endif

// this file generates eight C4100 warnings, when compiled with MSVC2003
#include "plugins/winamp/third-party/wa_ipc.h"

#ifdef Q_CC_MSVC
#pragma warning(pop)
#endif

#include <QString>
#include <windows.h>

/**
 * \class WinAmpTuneController
 * \brief A controller for WinAmp.
 */

static const int NormInterval       = 3000;
static const int AntiscrollInterval = 100;

/**
 * \brief Constructs the controller.
 */
WinAmpTuneController::WinAmpTuneController() : PollingTuneController(), antiscrollCounter_(0)
{
    startPoll();
    setInterval(NormInterval);
}

QString tchar2str(const char *str) { return QString::fromLocal8Bit(str); }
QString tchar2str(const wchar_t *str) { return QString::fromWCharArray(str); }

// Returns a title of a track currently being played by WinAmp with given HWND (passed in waWnd)
QPair<bool, QString> WinAmpTuneController::getTrackTitle(const HWND &waWnd) const
{
    TCHAR   waTitle[2048];
    QString title;

    // Get WinAmp window title. It always contains name of the track
    SendMessage(waWnd, WM_GETTEXT, static_cast<WPARAM>(sizeof(waTitle) / sizeof(waTitle[0])),
                reinterpret_cast<LPARAM>(waTitle));
    // Now, waTitle contains WinAmp window title
    title = tchar2str(waTitle);
    if (title[0] == '*' || (title.length() && title[title.length() - 1] == '*')) {
        // request to be called again soon.
        return QPair<bool, QString>(false, QString());
    }

    // Check whether there is a need to do the all stuff
    if (!title.length()) {
        return QPair<bool, QString>(true, title);
    }

    QString winamp(" - Winamp ***");
    int     winampLength = winamp.length();

    // Is title scrolling on the taskbar enabled?
    title += title + title;
    int waLast = title.indexOf(winamp, -1);
    if (waLast != -1) {
        if (title.length()) {
            title.remove(waLast, title.length() - waLast);
        }
        int waFirst;
        while ((waFirst = title.indexOf(winamp)) != -1) {
            title.remove(0, waFirst + winampLength);
        }
    } else {
        title = tchar2str(waTitle); // Title is not scrolling
    }

    // Remove leading and trailing spaces
    title = title.trimmed();

    // Remove trailing " - Winamp" from title
    if (title.length()) {
        winamp       = " - Winamp";
        winampLength = winamp.length();
        int waFirst  = title.indexOf(winamp);
        if (waFirst != -1) {
            title.remove(waFirst, waFirst + winampLength);
        }
    }

    // Remove track number from title
    if (title.length()) {
        QString dot(". ");
        int     dotFirst = title.indexOf(dot);
        if (dotFirst != -1) {
            // All symbols before the dot are digits?
            bool allDigits = true;
            for (int pos = dotFirst; pos > 0; --pos) {
                allDigits = allDigits && title[pos].isNumber();
            }
            if (allDigits) {
                title.remove(0, dotFirst + dot.length());
            }
        }
    }

    // Remove leading and trailing spaces
    if (title.length()) {
        while (title.length() && title[0] == ' ') {
            title.remove(0, 1);
        }
        while (title.length() && title[title.length() - 1] == ' ') {
            title.remove(title.length() - 1, 1);
        }
    }

    return QPair<bool, QString>(true, title);
}

/**
 * Polls for new song info.
 */
void WinAmpTuneController::check()
{

    Tune tune;
#ifdef UNICODE
    HWND h = FindWindow(L"Winamp v1.x", nullptr);
#else
    HWND h = FindWindow("Winamp v1.x", nullptr);
#endif
    if (h && SendMessage(h, WM_WA_IPC, 0, IPC_ISPLAYING) == 1) {
        tune = getTune(h);
    }
    prevTune_ = tune;
    setInterval(NormInterval);
    PollingTuneController::check();
}

Tune WinAmpTuneController::getTune(const HWND &hWnd)
{
    Tune tune     = Tune();
    int  position = int(SendMessage(hWnd, WM_WA_IPC, 0, IPC_GETLISTPOS));
    if (position != -1) {
        if (hWnd && SendMessage(hWnd, WM_WA_IPC, 0, IPC_ISPLAYING) == 1) {
            QPair<bool, QString> trackpair(getTrackTitle(hWnd));
            if (!trackpair.first) {
                // getTrackTitle wants us to retry in a few ms...
                int interval = AntiscrollInterval;
                if (++antiscrollCounter_ > 10) {
                    antiscrollCounter_ = 0;
                    interval           = NormInterval;
                }
                setInterval(interval);
                return Tune();
            }
            antiscrollCounter_ = 0;
            tune.setName(trackpair.second);
            tune.setURL(trackpair.second);
            tune.setTrack(QString::number(position + 1));
            tune.setTime(uint(SendMessage(hWnd, WM_WA_IPC, 1, IPC_GETOUTPUTTIME)));
        }
    }
    return tune;
}

Tune WinAmpTuneController::currentTune() const { return prevTune_; }

#ifndef SYSTEMWATCH_WIN_H
#define SYSTEMWATCH_WIN_H

#include "systemwatch.h"

#include <qt_windows.h>

class WinSystemWatch : public SystemWatch
{
public:
	WinSystemWatch();

	// Pass WM_POWERBROADCAST and WM_QUERYENDSESSION 
	// messages from QApplication::winFilterEvent() here
	void processWinEvent(MSG *m);
};

#endif

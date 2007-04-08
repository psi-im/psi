#ifndef SYSTEMWATCH_WIN_H
#define SYSTEMWATCH_WIN_H

#include "systemwatch.h"

class WinSystemWatch : public SystemWatchImpl
{
	Q_OBJECT

public:
	virtual ~WinSystemWatch();
	static WinSystemWatch* instance();
	// Win32 doesn't have an equivalent of idleSleep

private:
	WinSystemWatch();
	
	static WinSystemWatch* instance_;
public:
	class WinSystemWatchPrivate;
private:
	WinSystemWatchPrivate *d;
};

#endif

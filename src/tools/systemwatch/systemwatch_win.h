#ifndef SYSTEMWATCH_WIN_H
#define SYSTEMWATCH_WIN_H

#include "systemwatch.h"

#include <qt_windows.h>

class WinSystemWatch : public SystemWatch {
public:
    WinSystemWatch();
    ~WinSystemWatch();

private:
    class EventFilter;
    EventFilter *d;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool processWinEvent(MSG *m, long *result);
#else
    bool processWinEvent(MSG *m, qintptr *result);
#endif
};

#endif // SYSTEMWATCH_WIN_H

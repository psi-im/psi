#include <kcrash.h>

#include "crash.h"

namespace Crash {
void registerSigsegvHandler(QString progname)
{
    KCrash::setApplicationName(progname);
    KCrash::setCrashHandler(KCrash::defaultCrashHandler);
}
}; // namespace Crash

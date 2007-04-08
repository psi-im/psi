#include "crash.h"
#include <kcrash.h>

namespace Crash {

void registerSigsegvHandler(QString progname)
{
	KCrash::setApplicationName(progname);
	KCrash::setCrashHandler(KCrash::defaultCrashHandler);
}

}; // namespace Crash

#ifndef CRASH_H
#define CRASH_H

#include <qstring.h>

namespace Crash {

	void registerSigsegvHandler(QString progname);

};

#endif

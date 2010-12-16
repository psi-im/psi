#ifndef CRASH_H
#define CRASH_H

#include <QString>

namespace Crash {

	void registerSigsegvHandler(QString progname);

};

#endif

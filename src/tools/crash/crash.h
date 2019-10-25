#ifndef CRASH_H
#define CRASH_H

#include <QString>

namespace Crash {
void registerSigsegvHandler(QString progname);
}; // namespace Crash

#endif // CRASH_H

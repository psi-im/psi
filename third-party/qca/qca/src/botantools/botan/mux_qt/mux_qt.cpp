/*
Copyright (C) 1999-2007 The Botan Project. All rights reserved.

Redistribution and use in source and binary forms, for any use, with or without
modification, is permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions, and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions, and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

IN NO EVENT SHALL THE AUTHOR(S) OR CONTRIBUTOR(S) BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
// LICENSEHEADER_END
namespace QCA { // WRAPNS_LINE
/*************************************************
* Qt Thread Mutex Source File                    *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/mux_qt.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <qmutex.h>
namespace QCA { // WRAPNS_LINE

#if QT_VERSION <= 0x040000 && !defined(QT_THREAD_SUPPORT)
   #error Your version of Qt does not support threads or mutexes
#endif

namespace Botan {

/*************************************************
* Qt Mutex Factory                               *
*************************************************/
Mutex* Qt_Mutex_Factory::make()
   {
   class Qt_Mutex : public Mutex
      {
      public:
         void lock() { mutex.lock(); }
         void unlock() { mutex.unlock(); }
      private:
         QMutex mutex;
      };

   return new Qt_Mutex();
   }

}
} // WRAPNS_LINE

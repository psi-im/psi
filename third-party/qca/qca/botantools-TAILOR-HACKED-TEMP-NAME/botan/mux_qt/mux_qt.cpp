namespace QCA {
/*
Copyright (C) 1999-2004 The Botan Project. All rights reserved.

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
/*************************************************
* Qt Thread Mutex Source File                    *
* (C) 1999-2004 The Botan Project                *
*************************************************/

}
#include <botan/mux_qt.h>
namespace QCA {
}
#include <botan/exceptn.h>
namespace QCA {
}
#include <QMutex>
namespace QCA {

namespace Botan {

/*************************************************
* Wrapper Type for Qt Thread Mutex               *
*************************************************/
struct mutex_wrapper
   {
   QMutex m;
   };

/*************************************************
* Constructor                                    *
*************************************************/
Qt_Mutex::Qt_Mutex()
   {
   mutex = new mutex_wrapper;
   }

/*************************************************
* Destructor                                     *
*************************************************/
Qt_Mutex::~Qt_Mutex()
   {
   delete mutex;
   }

/*************************************************
* Lock the Mutex                                 *
*************************************************/
void Qt_Mutex::lock()
   {
   mutex->m.lock();
   }

/*************************************************
* Unlock the Mutex                               *
*************************************************/
void Qt_Mutex::unlock()
   {
   mutex->m.unlock();
   }

}
}

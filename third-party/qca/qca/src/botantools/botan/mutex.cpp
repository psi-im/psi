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
* Mutex Source File                              *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <stdlib.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/mutex.h>
namespace QCA { // WRAPNS_LINE
#ifndef BOTAN_NO_LIBSTATE
} // WRAPNS_LINE
#include <botan/libstate.h>
namespace QCA { // WRAPNS_LINE
#endif

namespace Botan {

/*************************************************
* Mutex_Holder Constructor                       *
*************************************************/
Mutex_Holder::Mutex_Holder(Mutex* m) : mux(m)
   {
   if(!mux)
      throw Invalid_Argument("Mutex_Holder: Argument was NULL");
   mux->lock();
   }

/*************************************************
* Mutex_Holder Destructor                        *
*************************************************/
Mutex_Holder::~Mutex_Holder()
   {
   mux->unlock();
   }

#ifndef BOTAN_NO_LIBSTATE
/*************************************************
* Named_Mutex_Holder Constructor                 *
*************************************************/
Named_Mutex_Holder::Named_Mutex_Holder(const std::string& name) :
   mutex_name(name)
   {
   global_state().get_named_mutex(mutex_name)->lock();
   }

/*************************************************
* Named_Mutex_Holder Destructor                  *
*************************************************/
Named_Mutex_Holder::~Named_Mutex_Holder()
   {
   global_state().get_named_mutex(mutex_name)->unlock();
   }
#endif

/*************************************************
* Default Mutex Factory                          *
*************************************************/
#ifdef BOTAN_FIX_GDB
namespace {
#else
Mutex* Default_Mutex_Factory::make()
   {
#endif
   class Default_Mutex : public Mutex
      {
      public:
         class Mutex_State_Error : public Internal_Error
            {
            public:
               Mutex_State_Error(const std::string& where) :
                  Internal_Error("Default_Mutex::" + where + ": " +
                                 "Mutex is already " + where + "ed") {}
            };

         void lock()
            {
            if(locked)
               throw Mutex_State_Error("lock");
            locked = true;
            }

         void unlock()
            {
            if(!locked)
               throw Mutex_State_Error("unlock");
            locked = false;
            }

         Default_Mutex() { locked = false; }
      private:
         bool locked;
      };

#ifdef BOTAN_FIX_GDB
   } // end unnamed namespace
Mutex* Default_Mutex_Factory::make()
   {
#endif

   return new Default_Mutex;
   }

}
} // WRAPNS_LINE

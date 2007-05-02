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
* Mutex Source File                              *
* (C) 1999-2004 The Botan Project                *
*************************************************/

}
#include <botan/mutex.h>
namespace QCA {
}
#include <botan/exceptn.h>
namespace QCA {

#ifndef BOTAN_NO_INIT_H
}
# include <botan/init.h>
namespace QCA {
#endif

namespace Botan {

namespace {

/*************************************************
* Global Mutex Variables                         *
*************************************************/
Mutex* mutex_factory = 0;
Mutex* mutex_init_lock = 0;

/*************************************************
* Default Mutex                                  *
*************************************************/
class Default_Mutex : public Mutex
   {
   public:
      void lock();
      void unlock();
      Mutex* clone() const { return new Default_Mutex; }
      Default_Mutex() { locked = false; }
   private:
      bool locked;
   };

/*************************************************
* Lock the mutex                                 *
*************************************************/
void Default_Mutex::lock()
   {
   if(locked)
      {
      abort();
      throw Internal_Error("Default_Mutex::lock: Mutex is already locked");
      }
   locked = true;
   }

/*************************************************
* Unlock the mutex                               *
*************************************************/
void Default_Mutex::unlock()
   {
   if(!locked)
      {
      abort();
      throw Internal_Error("Default_Mutex::unlock: Mutex is already unlocked");
      }
   locked = false;
   }

}

/*************************************************
* Get a mew mutex                                *
*************************************************/
Mutex* get_mutex()
   {
   if(mutex_factory == 0)
      return new Default_Mutex;
   return mutex_factory->clone();
   }

/*************************************************
* Initialize a mutex atomically                  *
*************************************************/
void initialize_mutex(Mutex*& mutex)
   {
   if(mutex) return;

   if(mutex_init_lock)
      {
      Mutex_Holder lock(mutex_init_lock);
      if(mutex == 0)
         mutex = get_mutex();
      }
   else
      mutex = get_mutex();
   }

namespace Init {

/*************************************************
* Set the Mutex type                             *
*************************************************/
void set_mutex_type(Mutex* mutex)
   {
   delete mutex_factory;
   delete mutex_init_lock;

   mutex_factory = mutex;

   if(mutex) mutex_init_lock = get_mutex();
   else      mutex_init_lock = 0;
   }

}

}
}

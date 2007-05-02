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
* Module Factory Source File                     *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/modules.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/defalloc.h>
namespace QCA { // WRAPNS_LINE
#ifndef BOTAN_TOOLS_ONLY
} // WRAPNS_LINE
#include <botan/def_char.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/eng_def.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/es_file.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/timers.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_MUTEX_PTHREAD)
} // WRAPNS_LINE
#  include <botan/mux_pthr.h>
namespace QCA { // WRAPNS_LINE
#elif defined(BOTAN_EXT_MUTEX_WIN32)
} // WRAPNS_LINE
#  include <botan/mux_win32.h>
namespace QCA { // WRAPNS_LINE
#elif defined(BOTAN_EXT_MUTEX_QT)
} // WRAPNS_LINE
#  include <botan/mux_qt.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_ALLOC_MMAP)
} // WRAPNS_LINE
#  include <botan/mmap_mem.h>
namespace QCA { // WRAPNS_LINE
#endif

#ifndef BOTAN_TOOLS_ONLY

#if defined(BOTAN_EXT_TIMER_HARDWARE)
} // WRAPNS_LINE
#  include <botan/tm_hard.h>
namespace QCA { // WRAPNS_LINE
#elif defined(BOTAN_EXT_TIMER_POSIX)
} // WRAPNS_LINE
#  include <botan/tm_posix.h>
namespace QCA { // WRAPNS_LINE
#elif defined(BOTAN_EXT_TIMER_UNIX)
} // WRAPNS_LINE
#  include <botan/tm_unix.h>
namespace QCA { // WRAPNS_LINE
#elif defined(BOTAN_EXT_TIMER_WIN32)
} // WRAPNS_LINE
#  include <botan/tm_win32.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_ENGINE_AEP)
} // WRAPNS_LINE
#  include <botan/eng_aep.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_ENGINE_GNU_MP)
} // WRAPNS_LINE
#  include <botan/eng_gmp.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_ENGINE_OPENSSL)
} // WRAPNS_LINE
#  include <botan/eng_ossl.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_AEP)
} // WRAPNS_LINE
#  include <botan/es_aep.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_EGD)
} // WRAPNS_LINE
#  include <botan/es_egd.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_UNIX)
} // WRAPNS_LINE
#  include <botan/es_unix.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_BEOS)
} // WRAPNS_LINE
#  include <botan/es_beos.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_CAPI)
} // WRAPNS_LINE
#  include <botan/es_capi.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_WIN32)
} // WRAPNS_LINE
#  include <botan/es_win32.h>
namespace QCA { // WRAPNS_LINE
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_FTW)
} // WRAPNS_LINE
#  include <botan/es_ftw.h>
namespace QCA { // WRAPNS_LINE
#endif

#endif

namespace Botan {

/*************************************************
* Return a mutex factory, if available           *
*************************************************/
Mutex_Factory* Builtin_Modules::mutex_factory() const
   {
#if defined(BOTAN_EXT_MUTEX_PTHREAD)
   return new Pthread_Mutex_Factory;
#elif defined(BOTAN_EXT_MUTEX_WIN32)
   return new Win32_Mutex_Factory;
#elif defined(BOTAN_EXT_MUTEX_QT)
   return new Qt_Mutex_Factory;
#else
   return 0;
#endif
   }

/*************************************************
* Find a high resolution timer, if possible      *
*************************************************/
#ifndef BOTAN_TOOLS_ONLY
Timer* Builtin_Modules::timer() const
   {
#if defined(BOTAN_EXT_TIMER_HARDWARE)
   return new Hardware_Timer;
#elif defined(BOTAN_EXT_TIMER_POSIX)
   return new POSIX_Timer;
#elif defined(BOTAN_EXT_TIMER_UNIX)
   return new Unix_Timer;
#elif defined(BOTAN_EXT_TIMER_WIN32)
   return new Win32_Timer;
#else
   return new Timer;
#endif
   }
#endif

/*************************************************
* Find any usable allocators                     *
*************************************************/
std::vector<Allocator*> Builtin_Modules::allocators() const
   {
   std::vector<Allocator*> allocators;

#if defined(BOTAN_EXT_ALLOC_MMAP)
   allocators.push_back(new MemoryMapping_Allocator);
#endif

   allocators.push_back(new Locking_Allocator);
   allocators.push_back(new Malloc_Allocator);

   return allocators;
   }

/*************************************************
* Return the default allocator                   *
*************************************************/
std::string Builtin_Modules::default_allocator() const
   {
   if(should_lock)
      {
#if defined(BOTAN_EXT_ALLOC_MMAP)
      return "mmap";
#else
      return "locking";
#endif
      }
   else
      return "malloc";
   }

#ifndef BOTAN_TOOLS_ONLY

/*************************************************
* Register any usable entropy sources            *
*************************************************/
std::vector<EntropySource*> Builtin_Modules::entropy_sources() const
   {
   std::vector<EntropySource*> sources;

   sources.push_back(new File_EntropySource);

#if defined(BOTAN_EXT_ENTROPY_SRC_AEP)
   sources.push_back(new AEP_EntropySource);
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_EGD)
   sources.push_back(new EGD_EntropySource);
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_CAPI)
   sources.push_back(new Win32_CAPI_EntropySource);
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_WIN32)
   sources.push_back(new Win32_EntropySource);
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_UNIX)
   sources.push_back(new Unix_EntropySource);
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_BEOS)
   sources.push_back(new BeOS_EntropySource);
#endif

#if defined(BOTAN_EXT_ENTROPY_SRC_FTW)
   sources.push_back(new FTW_EntropySource);
#endif

   return sources;
   }

/*************************************************
* Find any usable engines                        *
*************************************************/
std::vector<Engine*> Builtin_Modules::engines() const
   {
   std::vector<Engine*> engines;

   if(use_engines)
      {
#if defined(BOTAN_EXT_ENGINE_AEP)
      engines.push_back(new AEP_Engine);
#endif

#if defined(BOTAN_EXT_ENGINE_GNU_MP)
      engines.push_back(new GMP_Engine);
#endif

#if defined(BOTAN_EXT_ENGINE_OPENSSL)
      engines.push_back(new OpenSSL_Engine);
#endif
      }

   engines.push_back(new Default_Engine);

   return engines;
   }

/*************************************************
* Find the best transcoder option                *
*************************************************/
Charset_Transcoder* Builtin_Modules::transcoder() const
   {
   return new Default_Charset_Transcoder;
   }

#endif

/*************************************************
* Builtin_Modules Constructor                    *
*************************************************/
#ifdef BOTAN_TOOLS_ONLY
Builtin_Modules::Builtin_Modules() :
   should_lock(true)
   {
   }
#else
Builtin_Modules::Builtin_Modules(const InitializerOptions& args) :
   should_lock(args.secure_memory()),
   use_engines(args.use_engines())
   {
   }
#endif

}
} // WRAPNS_LINE

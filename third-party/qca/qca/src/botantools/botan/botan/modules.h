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
* Module Factory Header File                     *
* (C) 1999-2007 The Botan Project                *
*************************************************/

#ifndef BOTAN_MODULE_FACTORIES_H__
#define BOTAN_MODULE_FACTORIES_H__

#ifndef BOTAN_TOOLS_ONLY
} // WRAPNS_LINE
#include <botan/init.h>
namespace QCA { // WRAPNS_LINE
#endif
} // WRAPNS_LINE
#include <string>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <vector>
namespace QCA { // WRAPNS_LINE

namespace Botan {

/*************************************************
* Module Builder Interface                       *
*************************************************/
class Modules
   {
   public:
      virtual class Mutex_Factory* mutex_factory() const = 0;
#ifndef BOTAN_TOOLS_ONLY
      virtual class Timer* timer() const = 0;
      virtual class Charset_Transcoder* transcoder() const = 0;
#endif

      virtual std::string default_allocator() const = 0;

      virtual std::vector<class Allocator*> allocators() const = 0;
#ifndef BOTAN_TOOLS_ONLY
      virtual std::vector<class EntropySource*> entropy_sources() const = 0;
      virtual std::vector<class Engine*> engines() const = 0;
#endif

      virtual ~Modules() {}
   };

/*************************************************
* Built In Modules                               *
*************************************************/
class Builtin_Modules : public Modules
   {
   public:
      class Mutex_Factory* mutex_factory() const;
#ifndef BOTAN_TOOLS_ONLY
      class Timer* timer() const;
      class Charset_Transcoder* transcoder() const;
#endif

      std::string default_allocator() const;

      std::vector<class Allocator*> allocators() const;
#ifndef BOTAN_TOOLS_ONLY
      std::vector<class EntropySource*> entropy_sources() const;
      std::vector<class Engine*> engines() const;
#endif

#ifdef BOTAN_TOOLS_ONLY
      Builtin_Modules();
#else
      Builtin_Modules(const InitializerOptions&);
#endif
   private:
#ifdef BOTAN_TOOLS_ONLY
      const bool should_lock;
#else
      const bool should_lock, use_engines;
#endif
   };

}

#endif
} // WRAPNS_LINE

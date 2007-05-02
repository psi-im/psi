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
* Library Internal/Global State Header File      *
* (C) 1999-2007 The Botan Project                *
*************************************************/

#ifndef BOTAN_LIB_STATE_H__
#define BOTAN_LIB_STATE_H__

#ifdef BOTAN_TOOLS_ONLY
} // WRAPNS_LINE
#include <botan/allocate.h>
namespace QCA { // WRAPNS_LINE
#else
} // WRAPNS_LINE
#include <botan/base.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/enums.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/ui.h>
namespace QCA { // WRAPNS_LINE
#endif
} // WRAPNS_LINE
#include <string>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <vector>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <map>
namespace QCA { // WRAPNS_LINE

namespace Botan {

/*************************************************
* Global State Container Base                    *
*************************************************/
class Library_State
   {
   public:
#ifndef BOTAN_TOOLS_ONLY
      class Engine_Iterator
         {
         public:
            class Engine* next();
            Engine_Iterator(const Library_State& l) : lib(l) { n = 0; }
         private:
            const Library_State& lib;
            u32bit n;
         };
      friend class Engine_Iterator;

      class UI
         {
         public:
            virtual void pulse(Pulse_Type) {}
            virtual ~UI() {}
         };
#endif

      int prealloc_size;
      Allocator* get_allocator(const std::string& = "") const;
      void add_allocator(Allocator*);
#ifdef BOTAN_TOOLS_ONLY
      void set_default_allocator(const std::string&);
#else
      void set_default_allocator(const std::string&) const;
#endif

#ifndef BOTAN_TOOLS_ONLY
      bool rng_is_seeded() const { return rng->is_seeded(); }
      void randomize(byte[], u32bit);

      void set_prng(RandomNumberGenerator*);
      void add_entropy_source(EntropySource*, bool = true);
      void add_entropy(const byte[], u32bit);
      void add_entropy(EntropySource&, bool);
      u32bit seed_prng(bool, u32bit);
#endif

      void load(class Modules&);

#ifndef BOTAN_TOOLS_ONLY
      void set_timer(class Timer*);
      u64bit system_clock() const;

      class Config& config() const;

      void add_engine(class Engine*);
#endif

      class Mutex* get_mutex() const;
      class Mutex* get_named_mutex(const std::string&);

#ifndef BOTAN_TOOLS_ONLY
      void set_x509_state(class X509_GlobalState*);
      class X509_GlobalState& x509_state();

      void pulse(Pulse_Type) const;
      void set_ui(UI*);

      void set_transcoder(class Charset_Transcoder*);
      std::string transcode(const std::string,
                            Character_Set, Character_Set) const;
#endif

      Library_State(class Mutex_Factory*);
      ~Library_State();
   private:
      Library_State(const Library_State&) {}
      Library_State& operator=(const Library_State&) { return (*this); }

#ifndef BOTAN_TOOLS_ONLY
      class Engine* get_engine_n(u32bit) const;
#endif

      class Mutex_Factory* mutex_factory;
#ifndef BOTAN_TOOLS_ONLY
      class Timer* timer;
      class Config* config_obj;
      class X509_GlobalState* x509_state_obj;
#endif

      std::map<std::string, class Mutex*> locks;
      std::map<std::string, Allocator*> alloc_factory;
      mutable Allocator* cached_default_allocator;
#ifdef BOTAN_TOOLS_ONLY
      std::string default_allocator_type;
#endif

#ifndef BOTAN_TOOLS_ONLY
      UI* ui;
      class Charset_Transcoder* transcoder;
      RandomNumberGenerator* rng;
#endif
      std::vector<Allocator*> allocators;
#ifndef BOTAN_TOOLS_ONLY
      std::vector<EntropySource*> entropy_sources;
      std::vector<class Engine*> engines;
#endif
   };

/*************************************************
* Global State                                   *
*************************************************/
Library_State& global_state();
void set_global_state(Library_State*);
Library_State* swap_global_state(Library_State*);

}

#endif
} // WRAPNS_LINE

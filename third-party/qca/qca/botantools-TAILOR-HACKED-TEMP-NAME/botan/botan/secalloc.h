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
* Secure Memory Allocator Header File            *
* (C) 1999-2004 The Botan Project                *
*************************************************/

#ifndef BOTAN_SECURE_ALLOCATOR_H__
#define BOTAN_SECURE_ALLOCATOR_H__

}
#include <botan/exceptn.h>
namespace QCA {
}
#include <botan/mutex.h>
namespace QCA {
}
#include <vector>
namespace QCA {

namespace Botan {

/*************************************************
* Secure Memory Allocator                        *
*************************************************/
class SecureAllocator
   {
   public:
      class Buffer
         {
         public:
            void* buf;
            u32bit length;
            bool in_use;
            Buffer() { buf = 0; length = 0; in_use = false; }
            Buffer(void* b, u32bit l, bool used = false)
               { buf = b; length = l; in_use = used; }
         };

      void* allocate(u32bit) const;
      void deallocate(void*, u32bit) const;

      void init();
      void destroy();

      SecureAllocator();
      virtual ~SecureAllocator();
   private:
      void* get_block(u32bit) const;
      void free_block(void*, u32bit) const;

      virtual void* alloc_block(u32bit) const = 0;
      virtual void dealloc_block(void*, u32bit) const = 0;
      virtual bool should_prealloc() const { return false; }

      void* alloc_hook(void*, u32bit) const;
      void dealloc_hook(void*, u32bit) const;
      void consistency_check() const;

      void* find_free_block(u32bit) const;
      void defrag_free_list() const;

      static bool are_contiguous(const Buffer&, const Buffer&);
      u32bit find_block(void*) const;
      bool same_buffer(Buffer&, Buffer&) const;
      void remove_empty_buffers(std::vector<Buffer>&) const;

      const u32bit PREF_SIZE, ALIGN_TO;
      mutable std::vector<Buffer> real_mem, free_list;
      mutable Mutex* lock;
      mutable u32bit defrag_counter;
      bool initialized, destroyed;
   };

}

#endif
}

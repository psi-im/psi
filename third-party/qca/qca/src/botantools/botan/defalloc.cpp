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
* Basic Allocators Source File                   *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/defalloc.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/libstate.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/util.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <cstdlib>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <cstring>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <stdlib.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <string.h>
namespace QCA { // WRAPNS_LINE

namespace Botan {

namespace {

/*************************************************
* Perform Memory Allocation                      *
*************************************************/
void* do_malloc(u32bit n, bool do_lock)
   {
   void* ptr = malloc(n);

   if(!ptr)
      return 0;

   if(do_lock)
      lock_mem(ptr, n);

   memset(ptr, 0, n);
   return ptr;
   }

/*************************************************
* Perform Memory Deallocation                    *
*************************************************/
void do_free(void* ptr, u32bit n, bool do_lock)
   {
   if(!ptr)
      return;

   memset(ptr, 0, n);
   if(do_lock)
      unlock_mem(ptr, n);

   free(ptr);
   }

}

/*************************************************
* Malloc_Allocator's Allocation                  *
*************************************************/
void* Malloc_Allocator::alloc_block(u32bit n)
   {
   return do_malloc(n, false);
   }

/*************************************************
* Malloc_Allocator's Deallocation                *
*************************************************/
void Malloc_Allocator::dealloc_block(void* ptr, u32bit n)
   {
   do_free(ptr, n, false);
   }

/*************************************************
* Locking_Allocator's Allocation                 *
*************************************************/
void* Locking_Allocator::alloc_block(u32bit n)
   {
   return do_malloc(n, true);
   }

/*************************************************
* Locking_Allocator's Deallocation               *
*************************************************/
void Locking_Allocator::dealloc_block(void* ptr, u32bit n)
   {
   do_free(ptr, n, true);
   }

/*************************************************
* Get an allocator                               *
*************************************************/
Allocator* Allocator::get(bool locking)
   {
   std::string type = "";
   if(!locking)
      type = "malloc";

   Allocator* alloc = global_state().get_allocator(type);
   if(alloc)
      return alloc;

   throw Exception("Couldn't find an allocator to use in get_allocator");
   }

}
} // WRAPNS_LINE

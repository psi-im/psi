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
* Pooling Allocator Source File                  *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/mem_pool.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/libstate.h>
namespace QCA { // WRAPNS_LINE
#ifdef BOTAN_TOOLS_ONLY
} // WRAPNS_LINE
#include <botan/mem_ops.h>
namespace QCA { // WRAPNS_LINE
#else
} // WRAPNS_LINE
#include <botan/config.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/bit_ops.h>
namespace QCA { // WRAPNS_LINE
#endif
} // WRAPNS_LINE
#include <botan/util.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <algorithm>
namespace QCA { // WRAPNS_LINE

namespace Botan {

namespace {

/*************************************************
* Decide how much memory to allocate at once     *
*************************************************/
u32bit choose_pref_size(u32bit provided)
   {
   if(provided)
      return provided;

#ifdef BOTAN_TOOLS_ONLY
   u32bit result = (u32bit)global_state().prealloc_size;
#else
   u32bit result = global_config().option_as_u32bit("base/memory_chunk");
#endif
   if(result)
      return result;

   return 16*1024;
   }

}

/*************************************************
* Memory_Block Constructor                       *
*************************************************/
Pooling_Allocator::Memory_Block::Memory_Block(void* buf)
   {
   buffer = static_cast<byte*>(buf);
   bitmap = 0;
   buffer_end = buffer + (BLOCK_SIZE * BITMAP_SIZE);
   }

/*************************************************
* See if ptr is contained by this block          *
*************************************************/
bool Pooling_Allocator::Memory_Block::contains(void* ptr,
                                               u32bit length) const throw()
   {
   return ((buffer <= ptr) &&
           (buffer_end >= (byte*)ptr + length * BLOCK_SIZE));
   }

/*************************************************
* Allocate some memory, if possible              *
*************************************************/
byte* Pooling_Allocator::Memory_Block::alloc(u32bit n) throw()
   {
   if(n == 0 || n > BITMAP_SIZE)
      return 0;

   if(n == BITMAP_SIZE)
      {
      if(bitmap)
         return 0;
      else
         {
         bitmap = ~bitmap;
         return buffer;
         }
      }

   bitmap_type mask = ((bitmap_type)1 << n) - 1;
   u32bit offset = 0;

   while(bitmap & mask)
      {
      mask <<= 1;
      ++offset;

      if((bitmap & mask) == 0)
         break;
      if(mask >> 63)
         break;
      }

   if(bitmap & mask)
      return 0;

   bitmap |= mask;
   return buffer + offset * BLOCK_SIZE;
   }

/*************************************************
* Mark this memory as free, if we own it         *
*************************************************/
void Pooling_Allocator::Memory_Block::free(void* ptr, u32bit blocks) throw()
   {
   clear_mem((byte*)ptr, blocks * BLOCK_SIZE);

   const u32bit offset = ((byte*)ptr - buffer) / BLOCK_SIZE;

   if(offset == 0 && blocks == BITMAP_SIZE)
      bitmap = ~bitmap;
   else
      {
      for(u32bit j = 0; j != blocks; ++j)
         bitmap &= ~((bitmap_type)1 << (j+offset));
      }
   }

/*************************************************
* Pooling_Allocator Constructor                  *
*************************************************/
Pooling_Allocator::Pooling_Allocator(u32bit p_size, bool) :
   PREF_SIZE(choose_pref_size(p_size))
   {
   mutex = global_state().get_mutex();
   last_used = blocks.begin();
   }

/*************************************************
* Pooling_Allocator Destructor                   *
*************************************************/
Pooling_Allocator::~Pooling_Allocator()
   {
   delete mutex;
   if(blocks.size())
      throw Invalid_State("Pooling_Allocator: Never released memory");
   }

/*************************************************
* Free all remaining memory                      *
*************************************************/
void Pooling_Allocator::destroy()
   {
   Mutex_Holder lock(mutex);

   blocks.clear();

   for(u32bit j = 0; j != allocated.size(); ++j)
      dealloc_block(allocated[j].first, allocated[j].second);
   allocated.clear();
   }

/*************************************************
* Allocation                                     *
*************************************************/
void* Pooling_Allocator::allocate(u32bit n)
   {
   const u32bit BITMAP_SIZE = Memory_Block::bitmap_size();
   const u32bit BLOCK_SIZE = Memory_Block::block_size();

   Mutex_Holder lock(mutex);

   if(n <= BITMAP_SIZE * BLOCK_SIZE)
      {
      const u32bit block_no = round_up(n, BLOCK_SIZE) / BLOCK_SIZE;

      byte* mem = allocate_blocks(block_no);
      if(mem)
         return mem;

      get_more_core(PREF_SIZE);

      mem = allocate_blocks(block_no);
      if(mem)
         return mem;

      throw Memory_Exhaustion();
      }

   void* new_buf = alloc_block(n);
   if(new_buf)
      return new_buf;

   throw Memory_Exhaustion();
   }

/*************************************************
* Deallocation                                   *
*************************************************/
void Pooling_Allocator::deallocate(void* ptr, u32bit n)
   {
   const u32bit BITMAP_SIZE = Memory_Block::bitmap_size();
   const u32bit BLOCK_SIZE = Memory_Block::block_size();

   if(ptr == 0 || n == 0)
      return;

   Mutex_Holder lock(mutex);

   if(n > BITMAP_SIZE * BLOCK_SIZE)
      dealloc_block(ptr, n);
   else
      {
      const u32bit block_no = round_up(n, BLOCK_SIZE) / BLOCK_SIZE;

      std::vector<Memory_Block>::iterator i =
         std::lower_bound(blocks.begin(), blocks.end(), Memory_Block(ptr));

      if(i == blocks.end() || !i->contains(ptr, block_no))
         throw Invalid_State("Pointer released to the wrong allocator");

      i->free(ptr, block_no);
      }
   }

/*************************************************
* Try to get some memory from an existing block  *
*************************************************/
byte* Pooling_Allocator::allocate_blocks(u32bit n)
   {
   if(blocks.empty())
      return 0;

   std::vector<Memory_Block>::iterator i = last_used;

   do
      {
      byte* mem = i->alloc(n);
      if(mem)
         {
         last_used = i;
         return mem;
         }

      ++i;
      if(i == blocks.end())
         i = blocks.begin();
      }
   while(i != last_used);

   return 0;
   }

/*************************************************
* Allocate more memory for the pool              *
*************************************************/
void Pooling_Allocator::get_more_core(u32bit in_bytes)
   {
   const u32bit BITMAP_SIZE = Memory_Block::bitmap_size();
   const u32bit BLOCK_SIZE = Memory_Block::block_size();

   const u32bit TOTAL_BLOCK_SIZE = BLOCK_SIZE * BITMAP_SIZE;

   const u32bit in_blocks = round_up(in_bytes, BLOCK_SIZE) / TOTAL_BLOCK_SIZE;
   const u32bit to_allocate = in_blocks * TOTAL_BLOCK_SIZE;

   void* ptr = alloc_block(to_allocate);
   if(ptr == 0)
      throw Memory_Exhaustion();

   allocated.push_back(std::make_pair(ptr, to_allocate));

   for(u32bit j = 0; j != in_blocks; ++j)
      {
      byte* byte_ptr = static_cast<byte*>(ptr);
      blocks.push_back(Memory_Block(byte_ptr + j * TOTAL_BLOCK_SIZE));
      }

   std::sort(blocks.begin(), blocks.end());
   last_used = std::lower_bound(blocks.begin(), blocks.end(), Memory_Block(ptr));
   }

const u32bit Pooling_Allocator::Memory_Block::BITMAP_SIZE = 8 * sizeof(Pooling_Allocator::Memory_Block::bitmap_type);
const u32bit Pooling_Allocator::Memory_Block::BLOCK_SIZE = 64;

}
} // WRAPNS_LINE

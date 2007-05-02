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
* Secure Allocator Source File                   *
* (C) 1999-2004 The Botan Project                *
*************************************************/

}
#include <algorithm>
#include <botan/secalloc.h>
namespace QCA {
#ifndef BOTAN_NO_CONF_H
}
# include <botan/conf.h>
namespace QCA {
#else
namespace Botan {
int botan_memory_chunk = 65536;
int botan_prealloc = 2;
}
#endif
}
#include <botan/util.h>
namespace QCA {

namespace Botan {

namespace {

/*************************************************
* Buffer Comparison Operators                    *
*************************************************/
bool Buffer_Cmp(const SecureAllocator::Buffer& a,
                const SecureAllocator::Buffer& b)
   { return ((const byte*)a.buf < (const byte*)b.buf); }

bool Empty_Buffer(const SecureAllocator::Buffer& block)
   { return (block.length == 0); }

}

/*************************************************
* SecureAllocator Constructor                    *
*************************************************/
SecureAllocator::SecureAllocator() :
   PREF_SIZE(
#ifndef BOTAN_NO_CONF_H
	Config::get_u32bit("base/memory_chunk")
#else
	botan_memory_chunk
#endif
	), ALIGN_TO(8)
   {
   if(!PREF_SIZE)
      throw Internal_Error("The base/memory_chunk option is unset");
   lock = get_mutex();
   initialized = destroyed = false;
   defrag_counter = 0;
   }

/*************************************************
* SecureAllocator Destructor                     *
*************************************************/
SecureAllocator::~SecureAllocator()
   {
   delete lock;
   if(!initialized)
      throw Invalid_State("SecureAllocator: Was never initialized");
   if(!destroyed)
      throw Invalid_State("SecureAllocator: Never released memory");
   }

/*************************************************
* Allocate some initial buffers                  *
*************************************************/
void SecureAllocator::init()
   {
#ifndef BOTAN_NO_CONF_H
   for(u32bit j = 0; j != 2; j++)
#else
   for(u32bit j = 0; j != (u32bit)botan_prealloc; j++)
#endif
      {
      void* ptr = 0;
      try {
         ptr = alloc_block(PREF_SIZE);
      }
      catch(Exception) {}

      if(ptr)
         real_mem.push_back(Buffer(ptr, PREF_SIZE, false));
      }

   initialized = true;
   }

/*************************************************
* Free all remaining memory                      *
*************************************************/
void SecureAllocator::destroy()
   {
   if(destroyed)
      throw Invalid_State("SecureAllocator::destroy(): Already destroyed");

   destroyed = true;
   for(u32bit j = 0; j != real_mem.size(); j++)
      dealloc_block(real_mem[j].buf, real_mem[j].length);
   }

/*************************************************
* Return true if these buffers are contiguous    *
*************************************************/
bool SecureAllocator::are_contiguous(const Buffer& a, const Buffer& b)
   {
   if((const byte*)a.buf + a.length == (const byte*)b.buf)
      return true;
   return false;
   }

/*************************************************
* See if two free blocks are from the same block *
*************************************************/
bool SecureAllocator::same_buffer(Buffer& a, Buffer& b) const
   {
   return (find_block(a.buf) == find_block(b.buf));
   }

/*************************************************
* Find the block containing this memory          *
*************************************************/
u32bit SecureAllocator::find_block(void* addr) const
   {
   for(u32bit j = 0; j != real_mem.size(); j++)
      {
      const byte* buf_addr = (const byte*)real_mem[j].buf;
      if(buf_addr <= (byte*)addr &&
         (byte*)addr < buf_addr + real_mem[j].length)
         return j;
      }
   throw Internal_Error("SecureAllocator::find_block: no buffer found");
   }

/*************************************************
* Remove empty buffers from list                 *
*************************************************/
void SecureAllocator::remove_empty_buffers(std::vector<Buffer>& list) const
   {
   std::vector<Buffer>::iterator empty;

   empty = std::find_if(list.begin(), list.end(), Empty_Buffer);
   while(empty != list.end())
      {
      list.erase(empty);
      empty = std::find_if(list.begin(), list.end(), Empty_Buffer);
      }
   }

/*************************************************
* Allocation                                     *
*************************************************/
void* SecureAllocator::allocate(u32bit n) const
   {
   struct Memory_Exhaustion : public Exception
      {
      Memory_Exhaustion() : Exception("SecureAllocator: Ran out of memory") {}
      };

   if(n == 0) return 0;
   n = round_up(n, ALIGN_TO);

   Mutex_Holder holder(lock);

   void* new_buf = find_free_block(n);
   if(new_buf)
      return alloc_hook(new_buf, n);

   Buffer block;
   block.length = ((PREF_SIZE > n) ? PREF_SIZE : n);
   block.buf = get_block(block.length);
   if(!block.buf)
      throw Memory_Exhaustion();
   free_list.push_back(block);

   new_buf = find_free_block(n);
   if(new_buf)
      return alloc_hook(new_buf, n);

   throw Memory_Exhaustion();
   }

/*************************************************
* Deallocation                                   *
*************************************************/
void SecureAllocator::deallocate(void* ptr, u32bit n) const
   {
   const u32bit RUNS_TO_DEFRAGS = 16;

   if(ptr == 0 || n == 0) return;

   n = round_up(n, ALIGN_TO);
   memset(ptr, 0, n);

   Mutex_Holder holder(lock);

   dealloc_hook(ptr, n);

   free_list.push_back(Buffer(ptr, n));
   if(free_list.size() >= 2)
      std::inplace_merge(free_list.begin(), free_list.end() - 1,
                         free_list.end(), Buffer_Cmp);

   defrag_counter = (defrag_counter + 1) % RUNS_TO_DEFRAGS;
   if(defrag_counter == 0)
      {
      for(u32bit j = 0; j != free_list.size(); j++)
         {
         bool erase = false;
         if(free_list[j].buf == 0) continue;

         for(u32bit k = 0; k != real_mem.size(); k++)
            if(free_list[j].buf == real_mem[k].buf &&
               free_list[j].length == real_mem[k].length)
               erase = true;

         if(erase)
            {
            const u32bit buf = find_block(free_list[j].buf);
            free_block(real_mem[buf].buf, real_mem[buf].length);
            free_list[j].buf = 0;
            free_list[j].length = 0;
            }
         }

      defrag_free_list();
      }
   }

/*************************************************
* Handle Allocating New Memory                   *
*************************************************/
void* SecureAllocator::get_block(u32bit n) const
   {
   for(u32bit j = 0; j != real_mem.size(); j++)
      if(!real_mem[j].in_use && real_mem[j].length == n)
         {
         real_mem[j].in_use = true;
         return real_mem[j].buf;
         }

   void* ptr = 0;
   try {
      ptr = alloc_block(n);
   }
   catch(Exception) {}

   if(ptr)
      real_mem.push_back(Buffer(ptr, n, true));
   return ptr;
   }

/*************************************************
* Handle Deallocating Memory                     *
*************************************************/
void SecureAllocator::free_block(void* ptr, u32bit n) const
   {
   if(!ptr) return;

   u32bit free_blocks = 0;
   for(u32bit j = 0; j != real_mem.size(); j++)
      if(!real_mem[j].in_use) free_blocks++;

   bool free_this_block = (free_blocks > 2 || n != PREF_SIZE) ? true : false;

   for(u32bit j = 0; j != real_mem.size(); j++)
      if(real_mem[j].buf == ptr)
         {
         if(!real_mem[j].in_use || real_mem[j].length != n)
            throw Internal_Error("SecureAllocator: Size mismatch in free");

         if(free_this_block)
            {
            dealloc_block(real_mem[j].buf, real_mem[j].length);
            real_mem[j].buf = 0;
            real_mem[j].length = 0;
            remove_empty_buffers(real_mem);
            }
         else
            real_mem[j].in_use = false;

         return;
         }

   throw Internal_Error("SecureAllocator: Unknown pointer was freed");
   }

/*************************************************
* Defragment the free list                       *
*************************************************/
void SecureAllocator::defrag_free_list() const
   {
   if(free_list.size() < 2) return;

   for(u32bit j = 0; j != free_list.size(); j++)
      {
      if(free_list[j].length == 0)
         continue;

      if(j > 0 &&
         are_contiguous(free_list[j-1], free_list[j]) &&
         same_buffer(free_list[j-1], free_list[j]))
         {
         free_list[j].buf = free_list[j-1].buf;
         free_list[j].length += free_list[j-1].length;
         free_list[j-1].length = 0;
         }

      if(j < free_list.size() - 1 &&
         are_contiguous(free_list[j], free_list[j+1]) &&
         same_buffer(free_list[j], free_list[j+1]))
         {
         free_list[j+1].buf = free_list[j].buf;
         free_list[j+1].length += free_list[j].length;
         free_list[j].length = 0;
         }
      }
   remove_empty_buffers(free_list);
   }

/*************************************************
* Find a block on the free list                  *
*************************************************/
void* SecureAllocator::find_free_block(u32bit n) const
   {
   void* retval = 0;

   for(u32bit j = 0; j != free_list.size(); j++)
      if(free_list[j].length >= n)
         {
         retval = free_list[j].buf;

         if(free_list[j].length == n)
            free_list.erase(free_list.begin() + j);
         else if(free_list[j].length > n)
            {
            free_list[j].length -= n;
            free_list[j].buf = ((byte*)free_list[j].buf) + n;
            }
         break;
         }

   return retval;
   }

/*************************************************
* Allocation hook for debugging                  *
*************************************************/
void* SecureAllocator::alloc_hook(void* ptr, u32bit) const
   {
   return ptr;
   }

/*************************************************
* Deallocation hook for debugging                *
*************************************************/
void SecureAllocator::dealloc_hook(void*, u32bit) const
   {
   }

/*************************************************
* Run internal consistency checks                *
*************************************************/
void SecureAllocator::consistency_check() const
   {
   for(u32bit j = 0; j != free_list.size(); j++)
      {
      const byte* byte_buf = (const byte*)free_list[j].buf;
      const u32bit length = free_list[j].length;

      for(u32bit k = 0; k != length; k++)
         if(byte_buf[k])
            throw Internal_Error("SecureAllocator: free list corrupted");
      }
   }

}
}

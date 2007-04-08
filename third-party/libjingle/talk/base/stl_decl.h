/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _STL_DECL_H
#define _STL_DECL_H

#if defined(_MSC_VER) && _MSC_VER <= 1200 // 1200 == VC++ 6.0
#pragma warning(disable:4786)
#endif

#include <sys/types.h>

namespace std {
  template <class Key> struct hash;
  template <class Key> struct equal_to;
  template <class Key> struct less;
  template <class T> class allocator;
  template <class Key, class Val,
            class Compare,
            class Alloc> class map;
  template <class T, class Alloc> class vector;
  template <class T, class Alloc> class list;
  template <class T, class Alloc> class slist;
  template <class T, class Alloc, size_t BufSiz> class deque;
  template <class T, class Sequence> class stack;
  template <class T, class Sequence> class queue;
  template <class T, class Sequence, class Compare> class priority_queue;
  template <class T1, class T2> struct pair;
  template <class Key, class Compare, class Alloc> class set;
}

/////////////////////////////////////////////////////////////////////////////
// Workaround declaration problem with defaults
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER) && _MSC_VER <= 1200 // 1200 == VC++ 6.0

#define STD_MAP(T1, T2) \
  std::map<T1 , T2, std::less<T1>, std::allocator<T2> > 

#define STD_VECTOR(T1) \
  std::vector<T1, std::allocator<T1> >

#define STD_SET(T1) \
  std::set<T1, std::less<T1>, std::allocator<T1> >

#else

#define STD_MAP(T1, T2) \
  std::map<T1, T2, std::less<T1>, std::allocator<std::pair<const T1, T2 > > >

#define STD_VECTOR(T1) \
  std::vector<T1, std::allocator<T1> >

#define STD_SET(T1) \
  std::set<T1, std::less<T1>, std::allocator<T1> >

#endif


#endif // _STL_DECL_H

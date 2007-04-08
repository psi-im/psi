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

#include "talk/base/stringutils.h"
#include "talk/base/common.h"

namespace cricket {

#ifdef WIN32
int ascii_string_compare(const wchar_t* s1, const char* s2, size_t n,
                         CharacterTransformation transformation) {
  wchar_t c1, c2;
  while (true) {
    if (n-- == 0) return 0;
    c1 = transformation(*s1);
    // Double check that characters are not UTF-8
    ASSERT(static_cast<unsigned char>(*s2) < 128);
    // Note: *s2 gets implicitly promoted to wchar_t
    c2 = transformation(*s2);
    if (c1 != c2) return (c1 < c2) ? -1 : 1;
    if (!c1) return 0;
    ++s1;
    ++s2;
  }
}

size_t asccpyn(wchar_t* buffer, size_t buflen,
               const char* source, size_t srclen) {
  if (buflen <= 0)
    return 0;
  
  if (srclen == SIZE_UNKNOWN) {
    srclen = strlenn(source, buflen - 1);
  } else if (srclen >= buflen) {
    srclen = buflen - 1;
  }
#if _DEBUG
  // Double check that characters are not UTF-8
  for (size_t pos = 0; pos < srclen; ++pos)
    ASSERT(static_cast<unsigned char>(source[pos]) < 128);
#endif  // _DEBUG
  std::copy(source, source + srclen, buffer);
  buffer[srclen] = 0;
  return srclen;
}

#endif  // WIN32

}  // namespace cricket

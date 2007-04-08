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

#include "talk/p2p/base/helpers.h"
#include "talk/base/time.h"
#include <cstdlib>
#include <cassert>

// TODO: Change this implementation to use OpenSSL's RAND_bytes.  That will
//       give cryptographically random values on all platforms.

#ifdef WIN32
#include <time.h>
#include <windows.h>
#endif

namespace cricket {

static long g_seed = 1L;

int GetRandom() {
  return ((g_seed = g_seed * 214013L + 2531011L) >> 16) & 0x7fff;
}

void SetRandomSeed(unsigned long seed)
{
  g_seed = (long)seed;
}

static bool s_initrandom;

void InitRandom(const char *client_unique, size_t len) {
  s_initrandom = true;

  // Hash this string - unique per client

  uint32 hash = 0;
  if (client_unique != NULL) {
    for (int i = 0; i < (int)len; i++)
      hash = ((hash << 2) + hash) + client_unique[i];
  }

  // Now initialize the seed against a high resolution
  // counter

#ifdef WIN32
  LARGE_INTEGER big;
  QueryPerformanceCounter(&big);
  SetRandomSeed(big.LowPart ^ hash);
#else
  SetRandomSeed(Time() ^ hash);
#endif
}

const char BASE64[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

// Generates a random string of the given length.  We generate base64 values so
// that they will be printable, though that's not necessary.

std::string CreateRandomString(int len) {
  // Random number generator should of been initialized!
  assert(s_initrandom);
  if (!s_initrandom)
    InitRandom(0, 0);

  std::string str;
  for (int i = 0; i < len; i++)
#if defined(_MSC_VER) && _MSC_VER < 1300
    str.insert(str.end(), BASE64[GetRandom() & 63]);
#else
    str.push_back(BASE64[GetRandom() & 63]);
#endif
  return str;
}

uint32 CreateRandomId() {
  uint8 b1 = (uint8)(GetRandom() & 255);
  uint8 b2 = (uint8)(GetRandom() & 255);
  uint8 b3 = (uint8)(GetRandom() & 255);
  uint8 b4 = (uint8)(GetRandom() & 255);
  return b1 | (b2 << 8) | (b3 << 16) | (b4 << 24);
}

bool IsBase64Char(char ch) {
  return (('A' <= ch) && (ch <= 'Z')) ||
         (('a' <= ch) && (ch <= 'z')) ||
         (('0' <= ch) && (ch <= '9')) ||
         (ch == '+') || (ch == '/');
}

bool IsBase64Encoded(const std::string& str) {
  for (size_t i = 0; i < str.size(); ++i) {
    if (!IsBase64Char(str.at(i)))
      return false;
  }
  return true;
}

} // namespace cricket

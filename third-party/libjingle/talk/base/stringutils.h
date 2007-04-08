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

#ifndef __STRINGUTILS_H__
#define __STRINGUTILS_H__

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef WIN32
#include <wchar.h>
#endif  // WIN32

#include <string>

///////////////////////////////////////////////////////////////////////////////
// Rename a bunch of common string functions so they are consistent across
// platforms and between char and wchar_t variants.
// Here is the full list of functions that are unified:
//  strlen, strcmp, stricmp, strncmp, strnicmp
//  strchr, vsnprintf, strtoul, tolowercase
// tolowercase is like tolower, but not compatible with end-of-file value
// Note that the wchar_t versions are not available on Linux
///////////////////////////////////////////////////////////////////////////////

inline char tolowercase(char c) {
  return static_cast<char>(tolower(c));
}

#ifdef WIN32

inline size_t strlen(const wchar_t* s) {
  return wcslen(s);
}
inline int strcmp(const wchar_t* s1, const wchar_t* s2) {
  return wcscmp(s1, s2);
}
inline int stricmp(const wchar_t* s1, const wchar_t* s2) {
  return _wcsicmp(s1, s2);
}
inline int strncmp(const wchar_t* s1, const wchar_t* s2, size_t n) {
  return wcsncmp(s1, s2, n);
}
inline int strnicmp(const wchar_t* s1, const wchar_t* s2, size_t n) {
  return _wcsnicmp(s1, s2, n);
}
inline const wchar_t* strchr(const wchar_t* s, wchar_t c) {
  return wcschr(s, c);
}
inline int vsnprintf(char* buf, size_t n, const char* fmt, va_list args) {
  return _vsnprintf(buf, n, fmt, args);
}
inline int vsnprintf(wchar_t* buf, size_t n, const wchar_t* fmt, va_list args) {
  return _vsnwprintf(buf, n, fmt, args);
}
inline unsigned long strtoul(const wchar_t* snum, wchar_t** end, int base) {
  return wcstoul(snum, end, base);
}
inline wchar_t tolowercase(wchar_t c) {
  return static_cast<wchar_t>(towlower(c));
}

#endif  // WIN32

#ifdef POSIX

inline int _stricmp(const char* s1, const char* s2) {
  return strcasecmp(s1, s2);
}
inline int _strnicmp(const char* s1, const char* s2, size_t n) {
  return strncasecmp(s1, s2, n);
}

#endif // POSIX

///////////////////////////////////////////////////////////////////////////////
// Traits simplifies porting string functions to be CTYPE-agnostic
///////////////////////////////////////////////////////////////////////////////

namespace cricket {

const size_t SIZE_UNKNOWN = static_cast<size_t>(-1);

template<class CTYPE>
struct Traits {
  // STL string type
  //typedef XXX string;
  // Null-terminated string
  //inline static const CTYPE* empty_str();
};

///////////////////////////////////////////////////////////////////////////////
// String utilities which work with char or wchar_t
///////////////////////////////////////////////////////////////////////////////

template<class CTYPE>
inline const CTYPE* nonnull(const CTYPE* str, const CTYPE* def_str = NULL) {
  return str ? str : (def_str ? def_str : Traits<CTYPE>::empty_str());
}

template<class CTYPE>
const CTYPE* strchr(const CTYPE* str, const CTYPE* chs) {
  for (size_t i=0; str[i]; ++i) {
    for (size_t j=0; chs[j]; ++j) {
      if (str[i] == chs[j]) {
        return str + i;
      }
    }
  }
  return 0;
}

template<class CTYPE>
const CTYPE* strchrn(const CTYPE* str, size_t slen, CTYPE ch) {
  for (size_t i=0; i<slen && str[i]; ++i) {
    if (str[i] == ch) {
      return str + i;
    }
  }
  return 0;
}

template<class CTYPE>
size_t strlenn(const CTYPE* buffer, size_t buflen) {
  size_t bufpos = 0;
  while (buffer[bufpos] && (bufpos < buflen)) {
    ++bufpos;
  }
  return bufpos;
}

// Safe versions of strncpy, strncat, snprintf and vsnprintf that always
// null-terminate.

template<class CTYPE>
size_t strcpyn(CTYPE* buffer, size_t buflen,
               const CTYPE* source, size_t srclen = SIZE_UNKNOWN) {
  if (buflen <= 0)
    return 0;

  if (srclen == SIZE_UNKNOWN) {
    srclen = strlenn(source, buflen - 1);
  } else if (srclen >= buflen) {
    srclen = buflen - 1;
  }
  memcpy(buffer, source, srclen * sizeof(CTYPE));
  buffer[srclen] = 0;
  return srclen;
}

template<class CTYPE>
size_t strcatn(CTYPE* buffer, size_t buflen,
               const CTYPE* source, size_t srclen = SIZE_UNKNOWN) {
  if (buflen <= 0)
    return 0;
  
  size_t bufpos = strlenn(buffer, buflen - 1);
  return bufpos + strcpyn(buffer + bufpos, buflen - bufpos, source, srclen);
}

template<class CTYPE>
size_t sprintfn(CTYPE* buffer, size_t buflen, const CTYPE* format, ...) {
  va_list args;
  va_start(args, format);
  size_t len = vsprintfn(buffer, buflen, format, args);
  va_end(args);
  return len;
}

template<class CTYPE>
size_t vsprintfn(CTYPE* buffer, size_t buflen, const CTYPE* format,
                 va_list args) {
  int len = vsnprintf(buffer, buflen, format, args);
  if ((len < 0) || (static_cast<size_t>(len) >= buflen)) {
    len = static_cast<int>(buflen - 1);
    buffer[len] = 0;
  }
  return len;
}

///////////////////////////////////////////////////////////////////////////////
// Allow safe comparing and copying ascii (not UTF-8) with both wide and
// non-wide character strings.
///////////////////////////////////////////////////////////////////////////////

inline int asccmp(const char* s1, const char* s2) {
  return strcmp(s1, s2);
}
inline int ascicmp(const char* s1, const char* s2) {
  return _stricmp(s1, s2);
}
inline int ascncmp(const char* s1, const char* s2, size_t n) {
  return strncmp(s1, s2, n);
}
inline int ascnicmp(const char* s1, const char* s2, size_t n) {
  return _strnicmp(s1, s2, n);
}
inline size_t asccpyn(char* buffer, size_t buflen,
                      const char* source, size_t srclen = SIZE_UNKNOWN) {
  return strcpyn(buffer, buflen, source, srclen);
}

#ifdef WIN32

typedef wchar_t(*CharacterTransformation)(wchar_t);
inline wchar_t identity(wchar_t c) { return c; }
int ascii_string_compare(const wchar_t* s1, const char* s2, size_t n,
                         CharacterTransformation transformation);

inline int asccmp(const wchar_t* s1, const char* s2) {
  return ascii_string_compare(s1, s2, static_cast<size_t>(-1), identity);
}
inline int ascicmp(const wchar_t* s1, const char* s2) {
  return ascii_string_compare(s1, s2, static_cast<size_t>(-1), tolowercase);
}
inline int ascncmp(const wchar_t* s1, const char* s2, size_t n) {
  return ascii_string_compare(s1, s2, n, identity);
}
inline int ascnicmp(const wchar_t* s1, const char* s2, size_t n) {
  return ascii_string_compare(s1, s2, n, tolowercase);
}
size_t asccpyn(wchar_t* buffer, size_t buflen,
               const char* source, size_t srclen = SIZE_UNKNOWN);

#endif  // WIN32

///////////////////////////////////////////////////////////////////////////////
// Traits<char> specializations
///////////////////////////////////////////////////////////////////////////////

template<>
struct Traits<char> {
  typedef std::string string;
  inline static const char* Traits<char>::empty_str() { return ""; }
};

///////////////////////////////////////////////////////////////////////////////
// Traits<wchar_t> specializations (Windows only, currently)
///////////////////////////////////////////////////////////////////////////////

#ifdef WIN32

template<>
struct Traits<wchar_t> {
  typedef std::wstring string;
  inline static const wchar_t* Traits<wchar_t>::empty_str() { return L""; }
};

#endif  // WIN32

}  // namespace cricket

#endif // __STRINGUTILS_H__

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

#ifndef __BYTEBUFFER_H__
#define __BYTEBUFFER_H__

#include "talk/base/basictypes.h"
#include <string>

namespace cricket {

class ByteBuffer {
public:
  ByteBuffer();
  ByteBuffer(const char* bytes, size_t len);
  ByteBuffer(const char* bytes); // uses strlen
  ~ByteBuffer();

  const char* Data() const { return bytes_ + start_; }
  size_t Length() { return end_ - start_; }
  size_t Capacity() { return size_ - start_; }

  bool ReadUInt8(uint8& val);
  bool ReadUInt16(uint16& val);
  bool ReadUInt32(uint32& val);
  bool ReadString(std::string& val, size_t len); // append to val
  bool ReadBytes(char* val, size_t len);

  void WriteUInt8(uint8 val);
  void WriteUInt16(uint16 val);
  void WriteUInt32(uint32 val);
  void WriteString(const std::string& val);
  void WriteBytes(const char* val, size_t len);

  void Resize(size_t size);
  void Shift(size_t size);

private:
  char* bytes_;
  size_t size_;
  size_t start_;
  size_t end_;
};

} // namespace cricket

#endif // __BYTEBUFFER_H__

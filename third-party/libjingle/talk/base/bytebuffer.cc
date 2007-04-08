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

#include "talk/base/basictypes.h"
#include "talk/base/bytebuffer.h"
#include "talk/base/byteorder.h"
#include <algorithm>
#include <cassert>

#if defined(_MSC_VER) && _MSC_VER < 1300
namespace std {
  using ::memcpy;
}
#endif

namespace cricket {

const int DEFAULT_SIZE = 4096;

ByteBuffer::ByteBuffer() {
  start_ = 0;
  end_   = 0;
  size_  = DEFAULT_SIZE;
  bytes_ = new char[size_];
}

ByteBuffer::ByteBuffer(const char* bytes, size_t len) {
  start_ = 0;
  end_   = len;
  size_  = len;
  bytes_ = new char[size_];
  memcpy(bytes_, bytes, end_);
}

ByteBuffer::ByteBuffer(const char* bytes) {
  start_ = 0;
  end_   = strlen(bytes);
  size_  = end_;
  bytes_ = new char[size_];
  memcpy(bytes_, bytes, end_);
}

ByteBuffer::~ByteBuffer() {
  delete bytes_;
}

bool ByteBuffer::ReadUInt8(uint8& val) {
  return ReadBytes(reinterpret_cast<char*>(&val), 1);
}

bool ByteBuffer::ReadUInt16(uint16& val) {
  uint16 v;
  if (!ReadBytes(reinterpret_cast<char*>(&v), 2)) {
    return false;
  } else {
    val = NetworkToHost16(v);
    return true;
  }
}

bool ByteBuffer::ReadUInt32(uint32& val) {
  uint32 v;
  if (!ReadBytes(reinterpret_cast<char*>(&v), 4)) {
    return false;
  } else {
    val = NetworkToHost32(v);
    return true;
  }
}

bool ByteBuffer::ReadString(std::string& val, size_t len) {
  if (len > Length()) {
    return false;
  } else {
    val.append(bytes_ + start_, len);
    start_ += len;
    return true;
  }
}

bool ByteBuffer::ReadBytes(char* val, size_t len) {
  if (len > Length()) {
    return false;
  } else {
    memcpy(val, bytes_ + start_, len);
    start_ += len;
    return true;
  }
}

void ByteBuffer::WriteUInt8(uint8 val) {
  WriteBytes(reinterpret_cast<const char*>(&val), 1);
}

void ByteBuffer::WriteUInt16(uint16 val) {
  uint16 v = HostToNetwork16(val);
  WriteBytes(reinterpret_cast<const char*>(&v), 2);
}

void ByteBuffer::WriteUInt32(uint32 val) {
  uint32 v = HostToNetwork32(val);
  WriteBytes(reinterpret_cast<const char*>(&v), 4);
}

void ByteBuffer::WriteString(const std::string& val) {
  WriteBytes(val.c_str(), val.size());
}

void ByteBuffer::WriteBytes(const char* val, size_t len) {
  if (Length() + len > Capacity())
    Resize(Length() + len);

  memcpy(bytes_ + end_, val, len);
  end_ += len;
}

void ByteBuffer::Resize(size_t size) {
  if (size > size_)
    size = _max(size, 3 * size_ / 2);

  size_t len = _min(end_ - start_, size);
  char* new_bytes = new char[size];
  memcpy(new_bytes, bytes_ + start_, len);
  delete [] bytes_;

  start_ = 0;
  end_   = len;
  size_  = size;
  bytes_ = new_bytes;
}

void ByteBuffer::Shift(size_t size) {
  if (size > Length())
    return;

  end_ = Length() - size;
  memmove(bytes_, bytes_ + start_ + size, end_);
  start_ = 0;
}

} // namespace cricket

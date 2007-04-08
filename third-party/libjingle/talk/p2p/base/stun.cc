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

#include "talk/base/logging.h"
#include "talk/p2p/base/stun.h"
#include <iostream>
#include <cassert>

#if defined(_MSC_VER) && _MSC_VER < 1300
namespace std {
  using ::memcpy;
}
#endif

namespace cricket {

const std::string STUN_ERROR_REASON_BAD_REQUEST = "BAD REQUEST";
const std::string STUN_ERROR_REASON_UNAUTHORIZED = "UNAUTHORIZED";
const std::string STUN_ERROR_REASON_UNKNOWN_ATTRIBUTE = "UNKNOWN ATTRIBUTE";
const std::string STUN_ERROR_REASON_STALE_CREDENTIALS = "STALE CREDENTIALS";
const std::string STUN_ERROR_REASON_INTEGRITY_CHECK_FAILURE = "INTEGRITY CHECK FAILURE";
const std::string STUN_ERROR_REASON_MISSING_USERNAME = "MISSING USERNAME";
const std::string STUN_ERROR_REASON_USE_TLS = "USE TLS";
const std::string STUN_ERROR_REASON_SERVER_ERROR = "SERVER ERROR";
const std::string STUN_ERROR_REASON_GLOBAL_FAILURE = "GLOBAL FAILURE";

StunMessage::StunMessage() : type_(0), length_(0),
    transaction_id_("0000000000000000") {
  assert(transaction_id_.size() == 16);
  attrs_ = new std::vector<StunAttribute*>();
}

StunMessage::~StunMessage() {
  for (unsigned i = 0; i < attrs_->size(); i++)
    delete (*attrs_)[i];
  delete attrs_;
}

void StunMessage::SetTransactionID(const std::string& str) {
  assert(str.size() == 16);
  transaction_id_ = str;
}

void StunMessage::AddAttribute(StunAttribute* attr) {
  attrs_->push_back(attr);
  length_ += attr->length() + 4;
}

const StunAddressAttribute*
StunMessage::GetAddress(StunAttributeType type) const {
  switch (type) {
  case STUN_ATTR_MAPPED_ADDRESS:
  case STUN_ATTR_RESPONSE_ADDRESS:
  case STUN_ATTR_SOURCE_ADDRESS:
  case STUN_ATTR_CHANGED_ADDRESS:
  case STUN_ATTR_REFLECTED_FROM:
  case STUN_ATTR_ALTERNATE_SERVER:
  case STUN_ATTR_DESTINATION_ADDRESS:
  case STUN_ATTR_SOURCE_ADDRESS2:
    return reinterpret_cast<const StunAddressAttribute*>(GetAttribute(type));

  default:
    assert(0);
    return 0;
  }
}

const StunUInt32Attribute*
StunMessage::GetUInt32(StunAttributeType type) const {
  switch (type) {
  case STUN_ATTR_CHANGE_REQUEST:
  case STUN_ATTR_LIFETIME:
  case STUN_ATTR_BANDWIDTH:
  case STUN_ATTR_OPTIONS:
    return reinterpret_cast<const StunUInt32Attribute*>(GetAttribute(type));

  default:
    assert(0);
    return 0;
  }
}

const StunByteStringAttribute*
StunMessage::GetByteString(StunAttributeType type) const {
  switch (type) {
  case STUN_ATTR_USERNAME:
  case STUN_ATTR_PASSWORD:
  case STUN_ATTR_MESSAGE_INTEGRITY:
  case STUN_ATTR_DATA:
  case STUN_ATTR_MAGIC_COOKIE:
    return reinterpret_cast<const StunByteStringAttribute*>(GetAttribute(type));

  default:
    assert(0);
    return 0;
  }
}

const StunErrorCodeAttribute* StunMessage::GetErrorCode() const {
  return reinterpret_cast<const StunErrorCodeAttribute*>(
      GetAttribute(STUN_ATTR_ERROR_CODE));
}

const StunUInt16ListAttribute* StunMessage::GetUnknownAttributes() const {
  return reinterpret_cast<const StunUInt16ListAttribute*>(
      GetAttribute(STUN_ATTR_UNKNOWN_ATTRIBUTES));
}

const StunTransportPrefsAttribute* StunMessage::GetTransportPrefs() const {
  return reinterpret_cast<const StunTransportPrefsAttribute*>(
      GetAttribute(STUN_ATTR_TRANSPORT_PREFERENCES));
}

const StunAttribute* StunMessage::GetAttribute(StunAttributeType type) const {
  for (unsigned i = 0; i < attrs_->size(); i++) {
    if ((*attrs_)[i]->type() == type)
      return (*attrs_)[i];
  }
  return 0;
}

bool StunMessage::Read(ByteBuffer* buf) {
  if (!buf->ReadUInt16(type_))
    return false;

  if (!buf->ReadUInt16(length_))
    return false;

  std::string transaction_id;
  if (!buf->ReadString(transaction_id, 16))
    return false;
  assert(transaction_id.size() == 16);
  transaction_id_ = transaction_id;

  if (length_ > buf->Length())
    return false;

  attrs_->resize(0);

  size_t rest = buf->Length() - length_;
  while (buf->Length() > rest) {
    uint16 attr_type, attr_length;
    if (!buf->ReadUInt16(attr_type))
      return false;
    if (!buf->ReadUInt16(attr_length))
      return false;

    StunAttribute* attr = StunAttribute::Create(attr_type, attr_length);
    if (!attr || !attr->Read(buf))
      return false;

    attrs_->push_back(attr);
  }

  if (buf->Length() != rest) {
    // fixme: shouldn't be doing this
    LOG(LERROR) << "wrong message length" 
               << " (" << (int)rest << " != " << (int)buf->Length() << ")";
    return false;
  }

  return true;
}

void StunMessage::Write(ByteBuffer* buf) const {
  buf->WriteUInt16(type_);
  buf->WriteUInt16(length_);
  buf->WriteString(transaction_id_);

  for (unsigned i = 0; i < attrs_->size(); i++) {
    buf->WriteUInt16((*attrs_)[i]->type());
    buf->WriteUInt16((*attrs_)[i]->length());
    (*attrs_)[i]->Write(buf);
  }
}

StunAttribute::StunAttribute(uint16 type, uint16 length)
    : type_(type), length_(length) {
}

StunAttribute* StunAttribute::Create(uint16 type, uint16 length) {
  switch (type) {
  case STUN_ATTR_MAPPED_ADDRESS:
  case STUN_ATTR_RESPONSE_ADDRESS:
  case STUN_ATTR_SOURCE_ADDRESS:
  case STUN_ATTR_CHANGED_ADDRESS:
  case STUN_ATTR_REFLECTED_FROM:
  case STUN_ATTR_ALTERNATE_SERVER:
  case STUN_ATTR_DESTINATION_ADDRESS:
  case STUN_ATTR_SOURCE_ADDRESS2:
    if (length != StunAddressAttribute::SIZE)
      return 0;
    return new StunAddressAttribute(type);

  case STUN_ATTR_CHANGE_REQUEST:
  case STUN_ATTR_LIFETIME:
  case STUN_ATTR_BANDWIDTH:
  case STUN_ATTR_OPTIONS:
    if (length != StunUInt32Attribute::SIZE)
      return 0;
    return new StunUInt32Attribute(type);

  case STUN_ATTR_USERNAME:
  case STUN_ATTR_PASSWORD:
  case STUN_ATTR_MAGIC_COOKIE:
    return (length % 4 == 0) ? new StunByteStringAttribute(type, length) : 0;

  case STUN_ATTR_MESSAGE_INTEGRITY:
    return (length == 20) ? new StunByteStringAttribute(type, length) : 0;

  case STUN_ATTR_DATA:
    return new StunByteStringAttribute(type, length);

  case STUN_ATTR_ERROR_CODE:
    if (length < StunErrorCodeAttribute::MIN_SIZE)
      return 0;
    return new StunErrorCodeAttribute(type, length);

  case STUN_ATTR_UNKNOWN_ATTRIBUTES:
    return (length % 2 == 0) ? new StunUInt16ListAttribute(type, length) : 0;

  case STUN_ATTR_TRANSPORT_PREFERENCES:
    if ((length != StunTransportPrefsAttribute::SIZE1) &&
	(length != StunTransportPrefsAttribute::SIZE2))
      return 0;
    return new StunTransportPrefsAttribute(type, length);

  default:
    return 0;
  }
}

StunAddressAttribute* StunAttribute::CreateAddress(uint16 type) {
  switch (type) {
  case STUN_ATTR_MAPPED_ADDRESS:
  case STUN_ATTR_RESPONSE_ADDRESS:
  case STUN_ATTR_SOURCE_ADDRESS:
  case STUN_ATTR_CHANGED_ADDRESS:
  case STUN_ATTR_REFLECTED_FROM:
  case STUN_ATTR_ALTERNATE_SERVER:
  case STUN_ATTR_DESTINATION_ADDRESS:
  case STUN_ATTR_SOURCE_ADDRESS2:
    return new StunAddressAttribute(type);

  default:
    assert(false);
    return 0;
  }
}

StunUInt32Attribute* StunAttribute::CreateUInt32(uint16 type) {
  switch (type) {
  case STUN_ATTR_CHANGE_REQUEST:
  case STUN_ATTR_LIFETIME:
  case STUN_ATTR_BANDWIDTH:
  case STUN_ATTR_OPTIONS:
    return new StunUInt32Attribute(type);

  default:
    assert(false);
    return 0;
  }
}

StunByteStringAttribute* StunAttribute::CreateByteString(uint16 type) {
  switch (type) {
  case STUN_ATTR_USERNAME:
  case STUN_ATTR_PASSWORD:
  case STUN_ATTR_MESSAGE_INTEGRITY:
  case STUN_ATTR_DATA:
  case STUN_ATTR_MAGIC_COOKIE:
    return new StunByteStringAttribute(type, 0);

  default:
    assert(false);
    return 0;
  }
}

StunErrorCodeAttribute* StunAttribute::CreateErrorCode() {
  return new StunErrorCodeAttribute(
      STUN_ATTR_ERROR_CODE, StunErrorCodeAttribute::MIN_SIZE);
}

StunUInt16ListAttribute* StunAttribute::CreateUnknownAttributes() {
  return new StunUInt16ListAttribute(STUN_ATTR_UNKNOWN_ATTRIBUTES, 0);
}

StunTransportPrefsAttribute* StunAttribute::CreateTransportPrefs() {
  return new StunTransportPrefsAttribute(
      STUN_ATTR_TRANSPORT_PREFERENCES, StunTransportPrefsAttribute::SIZE1);
}

StunAddressAttribute::StunAddressAttribute(uint16 type)
    : StunAttribute(type, SIZE), family_(0), port_(0), ip_(0) {
}

bool StunAddressAttribute::Read(ByteBuffer* buf) {
  uint8 dummy;
  if (!buf->ReadUInt8(dummy))
    return false;
  if (!buf->ReadUInt8(family_))
    return false;
  if (!buf->ReadUInt16(port_))
    return false;
  if (!buf->ReadUInt32(ip_))
    return false;
  return true;
}

void StunAddressAttribute::Write(ByteBuffer* buf) const {
  buf->WriteUInt8(0);
  buf->WriteUInt8(family_);
  buf->WriteUInt16(port_);
  buf->WriteUInt32(ip_);
}

StunUInt32Attribute::StunUInt32Attribute(uint16 type)
    : StunAttribute(type, SIZE), bits_(0) {
}

bool StunUInt32Attribute::GetBit(int index) const {
  assert((0 <= index) && (index < 32));
  return static_cast<bool>((bits_ >> index) & 0x1);
}

void StunUInt32Attribute::SetBit(int index, bool value) {
  assert((0 <= index) && (index < 32));
  bits_ &= ~(1 << index);
  bits_ |= value ? (1 << index) : 0;
}

bool StunUInt32Attribute::Read(ByteBuffer* buf) {
  if (!buf->ReadUInt32(bits_))
    return false;
  return true;
}

void StunUInt32Attribute::Write(ByteBuffer* buf) const {
  buf->WriteUInt32(bits_);
}

StunByteStringAttribute::StunByteStringAttribute(uint16 type, uint16 length)
    : StunAttribute(type, length), bytes_(0) {
}

StunByteStringAttribute::~StunByteStringAttribute() {
  delete [] bytes_;
}

void StunByteStringAttribute::SetBytes(char* bytes, uint16 length) {
  delete [] bytes_;
  bytes_ = bytes;
  SetLength(length);
}

void StunByteStringAttribute::CopyBytes(const char* bytes) {
  CopyBytes(bytes, (uint16)strlen(bytes));
}

void StunByteStringAttribute::CopyBytes(const void* bytes, uint16 length) {
  char* new_bytes = new char[length];
  std::memcpy(new_bytes, bytes, length);
  SetBytes(new_bytes, length);
}

uint8 StunByteStringAttribute::GetByte(int index) const {
  assert(bytes_);
  assert((0 <= index) && (index < length()));
  return static_cast<uint8>(bytes_[index]);
}

void StunByteStringAttribute::SetByte(int index, uint8 value) {
  assert(bytes_);
  assert((0 <= index) && (index < length()));
  bytes_[index] = value;
}

bool StunByteStringAttribute::Read(ByteBuffer* buf) {
  bytes_ = new char[length()];
  if (!buf->ReadBytes(bytes_, length()))
    return false;
  return true;
}

void StunByteStringAttribute::Write(ByteBuffer* buf) const {
  buf->WriteBytes(bytes_, length());
}

StunErrorCodeAttribute::StunErrorCodeAttribute(uint16 type, uint16 length)
    : StunAttribute(type, length), class_(0), number_(0) {
}

StunErrorCodeAttribute::~StunErrorCodeAttribute() {
}

void StunErrorCodeAttribute::SetErrorCode(uint32 code) {
  class_ = (uint8)((code >> 8) & 0x7);
  number_ = (uint8)(code & 0xff);
}

void StunErrorCodeAttribute::SetReason(const std::string& reason) {
  SetLength(MIN_SIZE + (uint16)reason.size());
  reason_ = reason;
}

bool StunErrorCodeAttribute::Read(ByteBuffer* buf) {
  uint32 val;
  if (!buf->ReadUInt32(val))
    return false;

  if ((val >> 11) != 0)
    LOG(LERROR) << "error-code bits not zero";

  SetErrorCode(val);

  if (!buf->ReadString(reason_, length() - 4))
    return false;

  return true;
}

void StunErrorCodeAttribute::Write(ByteBuffer* buf) const {
  buf->WriteUInt32(error_code());
  buf->WriteString(reason_);
}

StunUInt16ListAttribute::StunUInt16ListAttribute(uint16 type, uint16 length)
    : StunAttribute(type, length) {
  attr_types_ = new std::vector<uint16>();
}

StunUInt16ListAttribute::~StunUInt16ListAttribute() {
  delete attr_types_;
}

size_t StunUInt16ListAttribute::Size() const {
  return attr_types_->size();
}

uint16 StunUInt16ListAttribute::GetType(int index) const {
  return (*attr_types_)[index];
}

void StunUInt16ListAttribute::SetType(int index, uint16 value) {
  (*attr_types_)[index] = value;
}

void StunUInt16ListAttribute::AddType(uint16 value) {
  attr_types_->push_back(value);
  SetLength((uint16)attr_types_->size() * 2);
}

bool StunUInt16ListAttribute::Read(ByteBuffer* buf) {
  for (int i = 0; i < length() / 2; i++) {
    uint16 attr;
    if (!buf->ReadUInt16(attr))
      return false;
    attr_types_->push_back(attr);
  }
  return true;
}

void StunUInt16ListAttribute::Write(ByteBuffer* buf) const {
  for (unsigned i = 0; i < attr_types_->size(); i++)
    buf->WriteUInt16((*attr_types_)[i]);
}

StunTransportPrefsAttribute::StunTransportPrefsAttribute(
    uint16 type, uint16 length)
    : StunAttribute(type, length), preallocate_(false), prefs_(0), addr_(0) { 
}

StunTransportPrefsAttribute::~StunTransportPrefsAttribute() {
  delete addr_;
}

void StunTransportPrefsAttribute::SetPreallocateAddress(
    StunAddressAttribute* addr) {
  if (!addr) {
    preallocate_ = false;
    addr_ = 0;
    SetLength(SIZE1);
  } else {
    preallocate_ = true;
    addr_ = addr;
    SetLength(SIZE2);
  }
}

bool StunTransportPrefsAttribute::Read(ByteBuffer* buf) {
  uint32 val;
  if (!buf->ReadUInt32(val))
    return false;

  if ((val >> 3) != 0)
    LOG(LERROR) << "transport-preferences bits not zero";

  preallocate_ = static_cast<bool>((val >> 2) & 0x1);
  prefs_ = (uint8)(val & 0x3);

  if (preallocate_ && (prefs_ == 3))
    LOG(LERROR) << "transport-preferences imcompatible P and Typ";

  if (!preallocate_) {
    if (length() != StunUInt32Attribute::SIZE)
      return false;
  } else {
    if (length() != StunUInt32Attribute::SIZE + StunAddressAttribute::SIZE)
      return false;

    addr_ = new StunAddressAttribute(STUN_ATTR_SOURCE_ADDRESS);
    addr_->Read(buf);
  }

  return true;
}

void StunTransportPrefsAttribute::Write(ByteBuffer* buf) const {
  buf->WriteUInt32((preallocate_ ? 4 : 0) | prefs_);

  if (preallocate_)
    addr_->Write(buf);
}

StunMessageType GetStunResponseType(StunMessageType request_type) {
  switch (request_type) {
  case STUN_SHARED_SECRET_REQUEST:
    return STUN_SHARED_SECRET_RESPONSE;
  case STUN_ALLOCATE_REQUEST:
    return STUN_ALLOCATE_RESPONSE;
  case STUN_SEND_REQUEST:
    return STUN_SEND_RESPONSE;
  default:
    return STUN_BINDING_RESPONSE;
  }
}

StunMessageType GetStunErrorResponseType(StunMessageType request_type) {
  switch (request_type) {
  case STUN_SHARED_SECRET_REQUEST:
    return STUN_SHARED_SECRET_ERROR_RESPONSE;
  case STUN_ALLOCATE_REQUEST:
    return STUN_ALLOCATE_ERROR_RESPONSE;
  case STUN_SEND_REQUEST:
    return STUN_SEND_ERROR_RESPONSE;
  default:
    return STUN_BINDING_ERROR_RESPONSE;
  }
}

} // namespace cricket

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

#ifndef __STUN_H__
#define __STUN_H__

// This file contains classes for dealing with the STUN and TURN protocols.
// Both protocols use the same wire format.

#include "talk/base/basictypes.h"
#include "talk/base/bytebuffer.h"
#include <string>
#include <vector>

namespace cricket {

// These are the types of STUN & TURN messages as of last check.
enum StunMessageType {
  STUN_BINDING_REQUEST              = 0x0001,
  STUN_BINDING_RESPONSE             = 0x0101,
  STUN_BINDING_ERROR_RESPONSE       = 0x0111,
  STUN_SHARED_SECRET_REQUEST        = 0x0002,
  STUN_SHARED_SECRET_RESPONSE       = 0x0102,
  STUN_SHARED_SECRET_ERROR_RESPONSE = 0x0112,
  STUN_ALLOCATE_REQUEST             = 0x0003,
  STUN_ALLOCATE_RESPONSE            = 0x0103,
  STUN_ALLOCATE_ERROR_RESPONSE      = 0x0113,
  STUN_SEND_REQUEST                 = 0x0004,
  STUN_SEND_RESPONSE                = 0x0104,
  STUN_SEND_ERROR_RESPONSE          = 0x0114,
  STUN_DATA_INDICATION              = 0x0115
};

// These are the types of attributes defined in STUN & TURN.  Next to each is
// the name of the class (T is StunTAttribute) that implements that type.
enum StunAttributeType {
  STUN_ATTR_MAPPED_ADDRESS        = 0x0001, // Address
  STUN_ATTR_RESPONSE_ADDRESS      = 0x0002, // Address
  STUN_ATTR_CHANGE_REQUEST        = 0x0003, // UInt32
  STUN_ATTR_SOURCE_ADDRESS        = 0x0004, // Address
  STUN_ATTR_CHANGED_ADDRESS       = 0x0005, // Address
  STUN_ATTR_USERNAME              = 0x0006, // ByteString, multiple of 4 bytes
  STUN_ATTR_PASSWORD              = 0x0007, // ByteString, multiple of 4 bytes
  STUN_ATTR_MESSAGE_INTEGRITY     = 0x0008, // ByteString, 20 bytes
  STUN_ATTR_ERROR_CODE            = 0x0009, // ErrorCode
  STUN_ATTR_UNKNOWN_ATTRIBUTES    = 0x000a, // UInt16List
  STUN_ATTR_REFLECTED_FROM        = 0x000b, // Address
  STUN_ATTR_TRANSPORT_PREFERENCES = 0x000c, // TransportPrefs
  STUN_ATTR_LIFETIME              = 0x000d, // UInt32
  STUN_ATTR_ALTERNATE_SERVER      = 0x000e, // Address
  STUN_ATTR_MAGIC_COOKIE          = 0x000f, // ByteString, 4 bytes
  STUN_ATTR_BANDWIDTH             = 0x0010, // UInt32
  STUN_ATTR_DESTINATION_ADDRESS   = 0x0011, // Address
  STUN_ATTR_SOURCE_ADDRESS2       = 0x0012, // Address
  STUN_ATTR_DATA                  = 0x0013, // ByteString
  STUN_ATTR_OPTIONS               = 0x8001  // UInt32
};

enum StunErrorCodes {
  STUN_ERROR_BAD_REQUEST          = 400,
  STUN_ERROR_UNAUTHORIZED         = 401,
  STUN_ERROR_UNKNOWN_ATTRIBUTE    = 420,
  STUN_ERROR_STALE_CREDENTIALS    = 430,
  STUN_ERROR_INTEGRITY_CHECK_FAILURE = 431,
  STUN_ERROR_MISSING_USERNAME     = 432,
  STUN_ERROR_USE_TLS              = 433,
  STUN_ERROR_SERVER_ERROR         = 500,
  STUN_ERROR_GLOBAL_FAILURE       = 600
};

extern const std::string STUN_ERROR_REASON_BAD_REQUEST;
extern const std::string STUN_ERROR_REASON_UNAUTHORIZED;
extern const std::string STUN_ERROR_REASON_UNKNOWN_ATTRIBUTE;
extern const std::string STUN_ERROR_REASON_STALE_CREDENTIALS;
extern const std::string STUN_ERROR_REASON_INTEGRITY_CHECK_FAILURE;
extern const std::string STUN_ERROR_REASON_MISSING_USERNAME;
extern const std::string STUN_ERROR_REASON_USE_TLS;
extern const std::string STUN_ERROR_REASON_SERVER_ERROR;
extern const std::string STUN_ERROR_REASON_GLOBAL_FAILURE;

class StunAttribute;
class StunAddressAttribute;
class StunUInt32Attribute;
class StunByteStringAttribute;
class StunErrorCodeAttribute;
class StunUInt16ListAttribute;
class StunTransportPrefsAttribute;

// Records a complete STUN/TURN message.  Each message consists of a type and
// any number of attributes.  Each attribute is parsed into an instance of an
// appropriate class (see above).  The Get* methods will return instances of
// that attribute class.
class StunMessage {
public:
  StunMessage();
  ~StunMessage();

  StunMessageType type() const { return static_cast<StunMessageType>(type_); }
  uint16 length() const { return length_; }
  const std::string& transaction_id() const { return transaction_id_; }

  void SetType(StunMessageType type) { type_ = type; }
  void SetTransactionID(const std::string& str);

  const StunAddressAttribute* GetAddress(StunAttributeType type) const;
  const StunUInt32Attribute* GetUInt32(StunAttributeType type) const;
  const StunByteStringAttribute* GetByteString(StunAttributeType type) const;
  const StunErrorCodeAttribute* GetErrorCode() const;
  const StunUInt16ListAttribute* GetUnknownAttributes() const;
  const StunTransportPrefsAttribute* GetTransportPrefs() const;

  void AddAttribute(StunAttribute* attr);

  // Parses the STUN/TURN packet in the given buffer and records it here.  The
  // return value indicates whether this was successful.
  bool Read(ByteBuffer* buf);

  // Writes this object into a STUN/TURN packet.  Return value is true if
  // successful.
  void Write(ByteBuffer* buf) const;

private:
  uint16 type_;
  uint16 length_;
  std::string transaction_id_;
  std::vector<StunAttribute*>* attrs_;

  const StunAttribute* GetAttribute(StunAttributeType type) const;
};

// Base class for all STUN/TURN attributes.
class StunAttribute {
public:
  virtual ~StunAttribute() {}

  StunAttributeType type() const {
    return static_cast<StunAttributeType>(type_);
  } 
  uint16 length() const { return length_; }

  // Reads the body (not the type or length) for this type of attribute from
  // the given buffer.  Return value is true if successful.
  virtual bool Read(ByteBuffer* buf) = 0;

  // Writes the body (not the type or length) to the given buffer.  Return
  // value is true if successful.
  virtual void Write(ByteBuffer* buf) const = 0;

  // Creates an attribute object with the given type and len.
  static StunAttribute* Create(uint16 type, uint16 length);

  // Creates an attribute object with the given type and smallest length.
  static StunAddressAttribute* CreateAddress(uint16 type);
  static StunUInt32Attribute* CreateUInt32(uint16 type);
  static StunByteStringAttribute* CreateByteString(uint16 type);
  static StunErrorCodeAttribute* CreateErrorCode();
  static StunUInt16ListAttribute* CreateUnknownAttributes();
  static StunTransportPrefsAttribute* CreateTransportPrefs();

protected:
  StunAttribute(uint16 type, uint16 length);

  void SetLength(uint16 length) { length_ = length; }

private:
  uint16 type_;
  uint16 length_;
};

// Implements STUN/TURN attributes that record an Internet address.
class StunAddressAttribute : public StunAttribute {
public:
  StunAddressAttribute(uint16 type);

#if (_MSC_VER < 1300)
  enum { SIZE = 8 };
#else
  static const uint16 SIZE = 8;
#endif

  uint8 family() const { return family_; }
  uint16 port() const { return port_; }
  uint32 ip() const { return ip_; }

  void SetFamily(uint8 family) { family_ = family; }
  void SetIP(uint32 ip) { ip_ = ip; }
  void SetPort(uint16 port) { port_ = port; }

  bool Read(ByteBuffer* buf);
  void Write(ByteBuffer* buf) const;

private:
  uint8 family_;
  uint16 port_;
  uint32 ip_;
};

// Implements STUN/TURN attributs that record a 32-bit integer.
class StunUInt32Attribute : public StunAttribute {
public:
  StunUInt32Attribute(uint16 type);

#if (_MSC_VER < 1300)
  enum { SIZE = 4 };
#else
  static const uint16 SIZE = 4;
#endif

  uint32 value() const { return bits_; }

  void SetValue(uint32 bits) { bits_ = bits; }

  bool GetBit(int index) const;
  void SetBit(int index, bool value);

  bool Read(ByteBuffer* buf);
  void Write(ByteBuffer* buf) const;

private:
  uint32 bits_;
};

// Implements STUN/TURN attributs that record an arbitrary byte string
class StunByteStringAttribute : public StunAttribute {
public:
  StunByteStringAttribute(uint16 type, uint16 length);
  ~StunByteStringAttribute();

  const char* bytes() const { return bytes_; }

  void SetBytes(char* bytes, uint16 length);

  void CopyBytes(const char* bytes); // uses strlen
  void CopyBytes(const void* bytes, uint16 length);

  uint8 GetByte(int index) const;
  void SetByte(int index, uint8 value);

  bool Read(ByteBuffer* buf);
  void Write(ByteBuffer* buf) const;

private:
  char* bytes_;
};

// Implements STUN/TURN attributs that record an error code.
class StunErrorCodeAttribute : public StunAttribute {
public:
  StunErrorCodeAttribute(uint16 type, uint16 length);
  ~StunErrorCodeAttribute();

#if (_MSC_VER < 1300)
  enum { MIN_SIZE = 4 };
#else
  static const uint16 MIN_SIZE = 4;
#endif

  uint32 error_code() const { return (class_ << 8) | number_; }
  uint8 error_class() const { return class_; }
  uint8 number() const { return number_; }
  const std::string& reason() const { return reason_; }

  void SetErrorCode(uint32 code);
  void SetErrorClass(uint8 eclass) { class_ = eclass; }
  void SetNumber(uint8 number) { number_ = number; }
  void SetReason(const std::string& reason);

  bool Read(ByteBuffer* buf);
  void Write(ByteBuffer* buf) const;

private:
  uint8 class_;
  uint8 number_;
  std::string reason_;
};

// Implements STUN/TURN attributs that record a list of attribute names.
class StunUInt16ListAttribute : public StunAttribute {
public:
  StunUInt16ListAttribute(uint16 type, uint16 length);
  ~StunUInt16ListAttribute();

  size_t Size() const;
  uint16 GetType(int index) const;
  void SetType(int index, uint16 value);
  void AddType(uint16 value);

  bool Read(ByteBuffer* buf);
  void Write(ByteBuffer* buf) const;

private:
  std::vector<uint16>* attr_types_;
};

// Implements the TURN TRANSPORT-PREFS attribute, which provides information
// about the ports to allocate.
class StunTransportPrefsAttribute : public StunAttribute {
public:
  StunTransportPrefsAttribute(uint16 type, uint16 length);
  ~StunTransportPrefsAttribute();

#if (_MSC_VER < 1300)
  enum { SIZE1 = 4, SIZE2 = 12 };
#else
  static const uint16 SIZE1 = 4;
  static const uint16 SIZE2 = 12;
#endif

  bool preallocate() const { return preallocate_; }
  uint8 preference_type() const { return prefs_; }
  const StunAddressAttribute* address() const { return addr_; }

  void SetPreferenceType(uint8 prefs) { prefs_ = prefs; }

  // Sets the preallocate address to the given value, or if 0 is given, it sets
  // to not preallocate.
  void SetPreallocateAddress(StunAddressAttribute* addr);

  bool Read(ByteBuffer* buf);
  void Write(ByteBuffer* buf) const;

private:
  bool preallocate_;
  uint8 prefs_;
  StunAddressAttribute* addr_;
};

// The special MAGIC-COOKIE attribute is used to distinguish TURN packets from
// other kinds of traffic.
const char STUN_MAGIC_COOKIE_VALUE[] = { 0x72, char(0xc6), 0x4b, char(0xc6) };

// Returns the (successful) response type for the given request type.
StunMessageType GetStunResponseType(StunMessageType request_type);

// Returns the error response type for the given request type.
StunMessageType GetStunErrorResponseType(StunMessageType request_type);

} // namespace cricket

#endif // __STUN_H__

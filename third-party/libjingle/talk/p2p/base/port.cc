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

#if defined(_MSC_VER) && _MSC_VER < 1300
#pragma warning(disable:4786)
#endif
#include "talk/base/logging.h"
#include "talk/base/asyncudpsocket.h"
#include "talk/base/asynctcpsocket.h"
#include "talk/base/socketadapters.h"
#include "talk/p2p/base/port.h"
#include "talk/p2p/base/helpers.h"
#include "talk/base/scoped_ptr.h"
#include <errno.h>
#include <algorithm>
#include <iostream>
#include <cassert>
#include <vector>

#if defined(_MSC_VER) && _MSC_VER < 1300
namespace std {
  using ::memcmp;
}
#endif

namespace {

// The length of time we wait before timing out readability on a connection.
const uint32 CONNECTION_READ_TIMEOUT = 30 * 1000; // 30 seconds

// The length of time we wait before timing out writability on a connection.
const uint32 CONNECTION_WRITE_TIMEOUT = 15 * 1000; // 15 seconds

// The length of time we wait before we become unwritable.
const uint32 CONNECTION_WRITE_CONNECT_TIMEOUT = 5 * 1000; // 5 seconds

// The number of pings that must fail to respond before we become unwritable.
const uint32 CONNECTION_WRITE_CONNECT_FAILURES = 5;

// This is the length of time that we wait for a ping response to come back.
const int CONNECTION_RESPONSE_TIMEOUT = 5 * 1000; // 5 seconds

// Determines whether we have seen at least the given maximum number of
// pings fail to have a response.
inline bool TooManyFailures(
    const std::vector<uint32>& pings_since_last_response,
    uint32 maximum_failures,
    uint32 rtt_estimate,
    uint32 now) {

  // If we haven't sent that many pings, then we can't have failed that many.
  if (pings_since_last_response.size() < maximum_failures)
    return false;

  // Check if the window in which we would expect a response to the ping has
  // already elapsed.
  return pings_since_last_response[maximum_failures - 1] + rtt_estimate < now;
}

// Determines whether we have gone too long without seeing any response.
inline bool TooLongWithoutResponse(
    const std::vector<uint32>& pings_since_last_response,
    uint32 maximum_time,
    uint32 now) {

  if (pings_since_last_response.size() == 0)
    return false;

  return pings_since_last_response[0] + maximum_time < now;
}

// We will restrict RTT estimates (when used for determining state) to be
// within a reasonable range.
const uint32 MINIMUM_RTT = 100;  // 0.1 seconds
const uint32 MAXIMUM_RTT = 3000; // 3 seconds

// When we don't have any RTT data, we have to pick something reasonable.  We
// use a large value just in case the connection is really slow.
const uint32 DEFAULT_RTT = MAXIMUM_RTT;

// Computes our estimate of the RTT given the current estimate and the number
// of data points on which it is based.
inline uint32 ConservativeRTTEstimate(uint32 rtt, uint32 rtt_data_points) {
  if (rtt_data_points == 0)
    return DEFAULT_RTT;
  else
    return cricket::_max(MINIMUM_RTT, cricket::_min(MAXIMUM_RTT, 2 * rtt));
}

// Weighting of the old rtt value to new data.
const int RTT_RATIO = 3; // 3 : 1

// The delay before we begin checking if this port is useless.
const int kPortTimeoutDelay = 30 * 1000; // 30 seconds

const uint32 MSG_CHECKTIMEOUT = 1;
const uint32 MSG_DELETE = 1;

}

namespace cricket {

static const char * const PROTO_NAMES[PROTO_LAST+1] = { "udp", "tcp", "ssltcp" };

const char * ProtoToString(ProtocolType proto) {
  return PROTO_NAMES[proto];
}

bool StringToProto(const char * value, ProtocolType& proto) {
  for (size_t i=0; i<=PROTO_LAST; ++i) {
    if (strcmp(PROTO_NAMES[i], value) == 0) {
      proto = static_cast<ProtocolType>(i);
      return true;
    }
  }
  return false;
}

std::string Port::agent_;
ProxyInfo Port::proxy_;

Port::Port(Thread* thread, const std::string& type, SocketFactory* factory,
           Network* network)
  : thread_(thread), factory_(factory), type_(type), network_(network),
    preference_(-1), lifetime_(LT_PRESTART) {

  if (factory_ == NULL)
    factory_ = thread_->socketserver();

  set_username_fragment(CreateRandomString(16));
  set_password(CreateRandomString(16));
}

Port::~Port() {
  // Delete all of the remaining connections.  We copy the list up front
  // because each deletion will cause it to be modified.

  std::vector<Connection*> list;

  AddressMap::iterator iter = connections_.begin();
  while (iter != connections_.end()) {
    list.push_back(iter->second);
    ++iter;
  }

  for (uint32 i = 0; i < list.size(); i++)
    delete list[i];
}

Connection* Port::GetConnection(const SocketAddress& remote_addr) {
  AddressMap::const_iterator iter = connections_.find(remote_addr);
  if (iter != connections_.end())
    return iter->second;
  else
    return NULL;
}

void Port::set_username_fragment(const std::string& username_fragment) {
  username_frag_ = username_fragment;
}

void Port::set_password(const std::string& password) {
  password_ = password;
}

void Port::add_address(const SocketAddress& address, const std::string& protocol, bool final) {
  Candidate c;
  c.set_name(name_);
  c.set_type(type_);
  c.set_protocol(protocol);
  c.set_address(address);
  c.set_preference(preference_);
  c.set_username(username_frag_);
  c.set_password(password_);
  c.set_network_name(network_->name());
  c.set_generation(generation_);
  candidates_.push_back(c);

  if (final)
    SignalAddressReady(this);
}

void Port::AddConnection(Connection* conn) {
  connections_[conn->remote_candidate().address()] = conn;
  conn->SignalDestroyed.connect(this, &Port::OnConnectionDestroyed);
  SignalConnectionCreated(this, conn);
}

void Port::OnReadPacket(
    const char* data, size_t size, const SocketAddress& addr) {

  // If this is an authenticated STUN request, then signal unknown address and
  // send back a proper binding response.
  StunMessage* msg;
  std::string remote_username;
  if (!GetStunMessage(data, size, addr, msg, remote_username)) {
    LOG(LERROR) << "Received non-STUN packet from unknown address: "
               << addr.ToString();
  } else if (!msg) {
    // STUN message handled already
  } else if (msg->type() == STUN_BINDING_REQUEST) {
    SignalUnknownAddress(this, addr, msg, remote_username);
  } else {
    LOG(LERROR) << "Received unexpected STUN message type (" << msg->type()
               << ") from unknown address: " << addr.ToString();
    delete msg;
  }
}

void Port::SendBindingRequest(Connection* conn) {

  // Construct the request message.

  StunMessage request;
  request.SetType(STUN_BINDING_REQUEST);
  request.SetTransactionID(CreateRandomString(16));

  StunByteStringAttribute* username_attr =
      StunAttribute::CreateByteString(STUN_ATTR_USERNAME);
  std::string username = conn->remote_candidate().username();
  username.append(username_frag_);
  username_attr->CopyBytes(username.c_str(), (uint16)username.size());
  request.AddAttribute(username_attr);

  // Send the request message.
  // NOTE: If we wanted to, this is where we would add the HMAC.
  ByteBuffer buf;
  request.Write(&buf);
  SendTo(buf.Data(), buf.Length(), conn->remote_candidate().address(), false);
}

bool Port::GetStunMessage(const char* data, size_t size,
                          const SocketAddress& addr, StunMessage *& msg,
                          std::string& remote_username) {
  // NOTE: This could clearly be optimized to avoid allocating any memory.
  //       However, at the data rates we'll be looking at on the client side,
  //       this probably isn't worth worrying about.

  msg = 0;

  // Parse the request message.  If the packet is not a complete and correct
  // STUN message, then ignore it.
  buzz::scoped_ptr<StunMessage> stun_msg(new StunMessage());
  ByteBuffer buf(data, size);
  if (!stun_msg->Read(&buf) || (buf.Length() > 0)) {
    return false;
  }

  // The packet must include a username that either begins or ends with our
  // fragment.  It should begin with our fragment if it is a request and it
  // should end with our fragment if it is a response.
  const StunByteStringAttribute* username_attr =
      stun_msg->GetByteString(STUN_ATTR_USERNAME);

  int remote_frag_len = (username_attr ? username_attr->length() : 0);
  remote_frag_len -= static_cast<int>(username_frag_.size());

  if (stun_msg->type() == STUN_BINDING_REQUEST) {
    if ((remote_frag_len < 0)
        || (std::memcmp(username_attr->bytes(),
                        username_frag_.c_str(), username_frag_.size()) != 0)) {
      LOG(LERROR) << "Received STUN request with bad username";
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_BAD_REQUEST,
        STUN_ERROR_REASON_BAD_REQUEST);
      return true;
    }

    remote_username.assign(username_attr->bytes() + username_frag_.size(),
      username_attr->bytes() + username_attr->length());
  } else if ((stun_msg->type() == STUN_BINDING_RESPONSE)
      || (stun_msg->type() == STUN_BINDING_ERROR_RESPONSE)) {
    if ((remote_frag_len < 0)
        || (std::memcmp(username_attr->bytes() + remote_frag_len,
                        username_frag_.c_str(), username_frag_.size()) != 0)) {
      LOG(LERROR) << "Received STUN response with bad username";
      // Do not send error response to a response
      return true;
    }

    remote_username.assign(username_attr->bytes(),
      username_attr->bytes() + remote_frag_len);

    if (stun_msg->type() == STUN_BINDING_ERROR_RESPONSE) {
      if (const StunErrorCodeAttribute* error_code = stun_msg->GetErrorCode()) {
        LOG(LERROR) << "Received STUN binding error:"
                   << " class=" << error_code->error_class()
                   << " number=" << error_code->number()
                   << " reason='" << error_code->reason() << "'";
        // Return message to allow error-specific processing
      } else {
        LOG(LERROR) << "Received STUN error response with no error code";
        // Drop corrupt message
        return true;
      }
    }
  } else {
    LOG(LERROR) << "Received STUN packet with invalid type: "
               << stun_msg->type();
    return true;
  }

  // Return the STUN message found.
  msg = stun_msg.release();
  return true;
}

void Port::SendBindingResponse(
    StunMessage* request, const SocketAddress& addr) {

  assert(request->type() == STUN_BINDING_REQUEST);

  // Retrieve the username from the request.
  const StunByteStringAttribute* username_attr =
      request->GetByteString(STUN_ATTR_USERNAME);
  assert(username_attr);

  // Fill in the response message.

  StunMessage response;
  response.SetType(STUN_BINDING_RESPONSE);
  response.SetTransactionID(request->transaction_id());

  StunByteStringAttribute* username2_attr =
      StunAttribute::CreateByteString(STUN_ATTR_USERNAME);
  username2_attr->CopyBytes(username_attr->bytes(), username_attr->length());
  response.AddAttribute(username2_attr);

  StunAddressAttribute* addr_attr =
      StunAttribute::CreateAddress(STUN_ATTR_MAPPED_ADDRESS);
  addr_attr->SetFamily(1);
  addr_attr->SetPort(addr.port());
  addr_attr->SetIP(addr.ip());
  response.AddAttribute(addr_attr);

  // Send the response message.
  // NOTE: If we wanted to, this is where we would add the HMAC.
  ByteBuffer buf;
  response.Write(&buf);
  SendTo(buf.Data(), buf.Length(), addr, false);

  // The fact that we received a successful request means that this connection
  // (if one exists) should now be readable.
  Connection* conn = GetConnection(addr);
  assert(conn);
  if (conn)
    conn->ReceivedPing();
}

void Port::SendBindingErrorResponse(
    StunMessage* request, const SocketAddress& addr, int error_code,
    const std::string& reason) {

  assert(request->type() == STUN_BINDING_REQUEST);

  // Retrieve the username from the request.  If it didn't have one, we
  // shouldn't be responding at all.
  const StunByteStringAttribute* username_attr =
      request->GetByteString(STUN_ATTR_USERNAME);
  assert(username_attr);

  // Fill in the response message.

  StunMessage response;
  response.SetType(STUN_BINDING_ERROR_RESPONSE);
  response.SetTransactionID(request->transaction_id());

  StunByteStringAttribute* username2_attr =
      StunAttribute::CreateByteString(STUN_ATTR_USERNAME);
  username2_attr->CopyBytes(username_attr->bytes(), username_attr->length());
  response.AddAttribute(username2_attr);

  StunErrorCodeAttribute* error_attr = StunAttribute::CreateErrorCode();
  error_attr->SetErrorCode(error_code);
  error_attr->SetReason(reason);
  response.AddAttribute(error_attr);

  // Send the response message.
  // NOTE: If we wanted to, this is where we would add the HMAC.
  ByteBuffer buf;
  response.Write(&buf);
  SendTo(buf.Data(), buf.Length(), addr, false);
}

AsyncPacketSocket * Port::CreatePacketSocket(ProtocolType proto) {
  if (proto == PROTO_UDP) {
    return new AsyncUDPSocket(factory_->CreateAsyncSocket(SOCK_DGRAM));
  } else if ((proto == PROTO_TCP) || (proto == PROTO_SSLTCP)) {
    AsyncSocket * socket = factory_->CreateAsyncSocket(SOCK_STREAM);
    switch (proxy().type) {
    case PROXY_NONE:
      break;
    case PROXY_SOCKS5:
      socket = new AsyncSocksProxySocket(socket, proxy().address,
                                         proxy().username, proxy().password);
      break;
    case PROXY_HTTPS:
    default:
      socket = new AsyncHttpsProxySocket(socket, user_agent(), proxy().address,
                                         proxy().username, proxy().password);
      break;
    }
    if (proto == PROTO_SSLTCP) {
      socket = new AsyncSSLSocket(socket);
    }
    return new AsyncTCPSocket(socket);
  } else {
    LOG(INFO) << "Unknown protocol: " << proto;
    return 0;
  }
}

void Port::OnMessage(Message *pmsg) {
  assert(pmsg->message_id == MSG_CHECKTIMEOUT);
  assert(lifetime_ == LT_PRETIMEOUT);
  lifetime_ = LT_POSTTIMEOUT;
  CheckTimeout();
}

void Port::Start() {
  // The port sticks around for a minimum lifetime, after which
  // we destroy it when it drops to zero connections.
  if (lifetime_ == LT_PRESTART) {
    lifetime_ = LT_PRETIMEOUT;
    thread_->PostDelayed(kPortTimeoutDelay, this, MSG_CHECKTIMEOUT);
  } else {
    LOG(WARNING) << "Port restart attempted";
  }
}

void Port::OnConnectionDestroyed(Connection* conn) {
  AddressMap::iterator iter = connections_.find(conn->remote_candidate().address());
  assert(iter != connections_.end());
  connections_.erase(iter);

  CheckTimeout();
}

void Port::CheckTimeout() {
  // If this port has no connections, then there's no reason to keep it around.
  // When the connections time out (both read and write), they will delete
  // themselves, so if we have any connections, they are either readable or
  // writable (or still connecting).
  if ((lifetime_ == LT_POSTTIMEOUT) && connections_.empty()) {
    LOG(INFO) << "Destroying port: " << name_ << "-" << type_;
    SignalDestroyed(this);
    delete this;
  }
}

// A ConnectionRequest is a simple STUN ping used to determine writability.
class ConnectionRequest : public StunRequest {
public:
  ConnectionRequest(Connection* connection) : connection_(connection) {
  }

  virtual ~ConnectionRequest() {
  }

  virtual void Prepare(StunMessage* request) {
    request->SetType(STUN_BINDING_REQUEST);
    StunByteStringAttribute* username_attr =
        StunAttribute::CreateByteString(STUN_ATTR_USERNAME);
    std::string username = connection_->remote_candidate().username();
    username.append(connection_->port()->username_fragment());
    username_attr->CopyBytes(username.c_str(), (uint16)username.size());
    request->AddAttribute(username_attr);
  }

  virtual void OnResponse(StunMessage* response) {
    connection_->OnConnectionRequestResponse(response, Elapsed());
  }

  virtual void OnErrorResponse(StunMessage* response) {
    connection_->OnConnectionRequestErrorResponse(response, Elapsed());
  }

  virtual void OnTimeout() {
  }

  virtual int GetNextDelay() {
    // Each request is sent only once.  After a single delay , the request will
    // time out.
    timeout_ = true;
    return CONNECTION_RESPONSE_TIMEOUT;
  }

private:
  Connection* connection_;
};

//
// Connection
//

Connection::Connection(Port* port, size_t index, const Candidate& remote_candidate)
  : requests_(port->thread()), port_(port), local_candidate_index_(index),
    remote_candidate_(remote_candidate), read_state_(STATE_READ_TIMEOUT),
    write_state_(STATE_WRITE_CONNECT), connected_(true), pruned_(false),
    rtt_(0), rtt_data_points_(0), last_ping_sent_(0), last_ping_received_(0),
    recv_total_bytes_(0), recv_bytes_second_(0),
    last_recv_bytes_second_time_((uint32)-1), last_recv_bytes_second_calc_(0),
    sent_total_bytes_(0), sent_bytes_second_(0),
    last_sent_bytes_second_time_((uint32)-1), last_sent_bytes_second_calc_(0) {

  // Wire up to send stun packets
  requests_.SignalSendPacket.connect(this, &Connection::OnSendStunPacket);
}

Connection::~Connection() {
}

const Candidate& Connection::local_candidate() const {
  if (local_candidate_index_ < port_->candidates().size())
    return port_->candidates()[local_candidate_index_];
  assert(false);
  static Candidate foo;
  return foo;
}

void Connection::set_read_state(ReadState value) {
  ReadState old_value = read_state_;
  read_state_ = value;
  if (value != old_value) {
    SignalStateChange(this);
    CheckTimeout();
  }
}

void Connection::set_write_state(WriteState value) {
  WriteState old_value = write_state_;
  write_state_ = value;
  if (value != old_value) {
    SignalStateChange(this);
    CheckTimeout();
  }
}

void Connection::set_connected(bool value) {
  bool old_value = connected_;
  connected_ = value;

  // When connectedness is turned off, this connection is done.
  if (old_value && !value)
    set_write_state(STATE_WRITE_TIMEOUT);
}

void Connection::OnSendStunPacket(const void* data, size_t size) {
  port_->SendTo(data, size, remote_candidate_.address(), false);
}

void Connection::OnReadPacket(const char* data, size_t size) {
  StunMessage* msg;
  std::string remote_username;
  const SocketAddress& addr(remote_candidate_.address());
  if (!port_->GetStunMessage(data, size, addr, msg, remote_username)) {
    // The packet did not parse as a valid STUN message
  
    // If this connection is readable, then pass along the packet.
    if (read_state_ == STATE_READABLE) {
      // readable means data from this address is acceptable
      // Send it on!

      recv_total_bytes_ += size;
      SignalReadPacket(this, data, size);

      // If timed out sending writability checks, start up again
      if (!pruned_ && (write_state_ == STATE_WRITE_TIMEOUT))
        set_write_state(STATE_WRITE_CONNECT);
    } else {
      // Not readable means the remote address hasn't send a valid
      // binding request yet.

      LOG(WARNING) << "Received non-STUN packet from an unreadable connection.";
    }
  } else if (!msg) {
    // The packet was STUN, but was already handled
  } else if (remote_username != remote_candidate_.username()) {
    // Not destined this connection
    LOG(LERROR) << "Received STUN packet on wrong address.";
    if (msg->type() == STUN_BINDING_REQUEST) {
      port_->SendBindingErrorResponse(msg, addr, STUN_ERROR_BAD_REQUEST,
                                      STUN_ERROR_REASON_BAD_REQUEST);
    }
    delete msg;
  } else {
    // The packet is STUN, with the current username
    // If this is a STUN request, then update the readable bit and respond.
    // If this is a STUN response, then update the writable bit.

    switch (msg->type()) {
    case STUN_BINDING_REQUEST:
      // Incoming, validated stun request from remote peer.
      // This call will also set the connection readable.

      port_->SendBindingResponse(msg, addr);

      // If timed out sending writability checks, start up again
      if (!pruned_ && (write_state_ == STATE_WRITE_TIMEOUT))
        set_write_state(STATE_WRITE_CONNECT);
      break;

    case STUN_BINDING_RESPONSE:
    case STUN_BINDING_ERROR_RESPONSE:
      // Response from remote peer. Does it match request sent?
      // This doesn't just check, it makes callbacks if transaction
      // id's match
      requests_.CheckResponse(msg);
      break;

    default:
      assert(false);
      break;
    }

    // Done with the message; delete

    delete msg;
  }
}

void Connection::Prune() {
  pruned_ = true;
  requests_.Clear();
  set_write_state(STATE_WRITE_TIMEOUT);
}

void Connection::Destroy() {
  set_read_state(STATE_READ_TIMEOUT);
  set_write_state(STATE_WRITE_TIMEOUT);
}

void Connection::UpdateState(uint32 now) {
  // Check the readable state.
  //
  // Since we don't know how many pings the other side has attempted, the best
  // test we can do is a simple window.

  if ((read_state_ == STATE_READABLE) &&
      (last_ping_received_ + CONNECTION_READ_TIMEOUT <= now)) {
    set_read_state(STATE_READ_TIMEOUT);
  }

  // Check the writable state.  (The order of these checks is important.)
  //
  // Before becoming unwritable, we allow for a fixed number of pings to fail
  // (i.e., receive no response).  We also have to give the response time to
  // get back, so we include a conservative estimate of this.
  //
  // Before timing out writability, we give a fixed amount of time.  This is to
  // allow for changes in network conditions.

  uint32 rtt = ConservativeRTTEstimate(rtt_, rtt_data_points_);

  if ((write_state_ == STATE_WRITABLE) &&
      TooManyFailures(pings_since_last_response_,
                      CONNECTION_WRITE_CONNECT_FAILURES,
                      rtt,
                      now) &&
      TooLongWithoutResponse(pings_since_last_response_,
                             CONNECTION_WRITE_CONNECT_TIMEOUT,
                             now)) {
    set_write_state(STATE_WRITE_CONNECT);
  }

  if ((write_state_ == STATE_WRITE_CONNECT) &&
      TooLongWithoutResponse(pings_since_last_response_,
                             CONNECTION_WRITE_TIMEOUT,
                             now)) {
    set_write_state(STATE_WRITE_TIMEOUT);
  }
}

void Connection::Ping(uint32 now) {
  assert(connected_);
  last_ping_sent_ = now;
  pings_since_last_response_.push_back(now);
  requests_.Send(new ConnectionRequest(this));
}

void Connection::ReceivedPing() {
  last_ping_received_ = Time();
  set_read_state(STATE_READABLE);
}

void Connection::OnConnectionRequestResponse(StunMessage *response, uint32 rtt) {
  // We have a potentially valid reply from the remote address.
  // The packet must include a username that ends with our fragment,
  // since it is a response.

  // Check exact message type
  bool valid = true;
  if (response->type() != STUN_BINDING_RESPONSE)
    valid = false;

  // Must have username attribute
  const StunByteStringAttribute* username_attr =
      response->GetByteString(STUN_ATTR_USERNAME);
  if (valid) {
    if (!username_attr) {
      LOG(LERROR) << "Received likely STUN packet with no username";
      valid = false;
    }
  }

  // Length must be at least the size of our fragment (actually, should
  // be bigger since our fragment is at the end!)
  if (valid) {
    if (username_attr->length() <= port_->username_fragment().size()) {
      LOG(LERROR) << "Received likely STUN packet with short username";
      valid = false;
    }
  }

  // Compare our fragment with the end of the username - must be exact match
  if (valid) {
    std::string username_fragment = port_->username_fragment();
    int offset = (int)(username_attr->length() - username_fragment.size());
    if (std::memcmp(username_attr->bytes() + offset,
        username_fragment.c_str(), username_fragment.size()) != 0) {
      LOG(LERROR) << "Received STUN response with bad username";
      valid = false;
    }
  }

  if (valid) {
    // Valid response. If we're not already, become writable.  We may be
    // bringing a pruned connection back to life, but if we don't really want
    // it, we can always prune it again.
    set_write_state(STATE_WRITABLE);

    pings_since_last_response_.clear();
    rtt_ = (RTT_RATIO * rtt_ + rtt) / (RTT_RATIO + 1);
    rtt_data_points_ += 1;
  }
}

void Connection::OnConnectionRequestErrorResponse(StunMessage *response, uint32 rtt) {
  const StunErrorCodeAttribute* error = response->GetErrorCode();
  uint32 error_code = error ? error->error_code() : STUN_ERROR_GLOBAL_FAILURE;

  if ((error_code == STUN_ERROR_UNKNOWN_ATTRIBUTE)
      || (error_code == STUN_ERROR_SERVER_ERROR)
      || (error_code == STUN_ERROR_UNAUTHORIZED)) {
    // Recoverable error, retry
  } else if (error_code == STUN_ERROR_STALE_CREDENTIALS) {
    // Race failure, retry
  } else {
    // This is not a valid connection.
    set_connected(false);
  }
}

void Connection::CheckTimeout() {
  // If both read and write have timed out, then this connection can contribute
  // no more to p2p socket unless at some later date readability were to come
  // back.  However, we gave readability a long time to timeout, so at this
  // point, it seems fair to get rid of this connectoin.
  if ((read_state_ == STATE_READ_TIMEOUT) &&
      (write_state_ == STATE_WRITE_TIMEOUT)) {
    port_->thread()->Post(this, MSG_DELETE);
  }
}

void Connection::OnMessage(Message *pmsg) {
  assert(pmsg->message_id == MSG_DELETE);

  LOG(INFO) << "Destroying connection: from "
            << local_candidate().address().ToString()
            << " to " << remote_candidate_.address().ToString();

  SignalDestroyed(this);
  delete this;
}

size_t Connection::recv_bytes_second() {
  // Snapshot bytes / second calculator

  uint32 current_time = Time();
  if (last_recv_bytes_second_time_ != (uint32)-1) {
    int delta = TimeDiff(current_time, last_recv_bytes_second_time_);
    if (delta >= 1000) {
      int fraction_time = delta % 1000;
      int seconds_time = delta - fraction_time;
      int fraction_bytes = (int)(recv_total_bytes_ - last_recv_bytes_second_calc_) * fraction_time / delta;
      recv_bytes_second_ = (recv_total_bytes_ - last_recv_bytes_second_calc_ - fraction_bytes) * seconds_time / delta;
      last_recv_bytes_second_time_ = current_time - fraction_time;
      last_recv_bytes_second_calc_ = recv_total_bytes_ - fraction_bytes;
    }
  }
  if (last_recv_bytes_second_time_ == (uint32)-1) {
    last_recv_bytes_second_time_ = current_time;
    last_recv_bytes_second_calc_ = recv_total_bytes_;
  }

  return recv_bytes_second_;
}

size_t Connection::recv_total_bytes() {
  return recv_total_bytes_;
}

size_t Connection::sent_bytes_second() {
  // Snapshot bytes / second calculator

  uint32 current_time = Time();
  if (last_sent_bytes_second_time_ != (uint32)-1) {
    int delta = TimeDiff(current_time, last_sent_bytes_second_time_);
    if (delta >= 1000) {
      int fraction_time = delta % 1000;
      int seconds_time = delta - fraction_time;
      int fraction_bytes = (int)(sent_total_bytes_ - last_sent_bytes_second_calc_) * fraction_time / delta;
      sent_bytes_second_ = (sent_total_bytes_ - last_sent_bytes_second_calc_ - fraction_bytes) * seconds_time / delta;
      last_sent_bytes_second_time_ = current_time - fraction_time;
      last_sent_bytes_second_calc_ = sent_total_bytes_ - fraction_bytes;
    }
  }
  if (last_sent_bytes_second_time_ == (uint32)-1) {
    last_sent_bytes_second_time_ = current_time;
    last_sent_bytes_second_calc_ = sent_total_bytes_;
  }

  return sent_bytes_second_;
}

size_t Connection::sent_total_bytes() {
  return sent_total_bytes_;
}

ProxyConnection::ProxyConnection(Port* port, size_t index, const Candidate& candidate)
  : Connection(port, index, candidate), error_(0) {
}

int ProxyConnection::Send(const void* data, size_t size) {
  if (write_state() != STATE_WRITABLE) {
    error_ = EWOULDBLOCK;
    return SOCKET_ERROR;
  }
  int sent = port_->SendTo(data, size, remote_candidate_.address(), true);
  if (sent <= 0) {
    assert(sent < 0);
    error_ = port_->GetError();
  } else {
    sent_total_bytes_ += sent;
  }
  return sent;
}

} // namespace cricket

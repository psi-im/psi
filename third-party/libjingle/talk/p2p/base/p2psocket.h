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

// P2PSocket wraps up the state management of the connection between two
// P2P clients.  Clients have candidate ports for connecting, and connections
// which are combinations of candidates from each end (Alice and Bob each
// have candidates, one candidate from Alice and one candidate from Bob are
// used to make a connection, repeat to make many connections).
//
// When all of the available connections become invalid (non-writable), we
// kick off a process of determining more candidates and more connections.
//
#ifndef _CRICKET_P2P_BASE_P2PSOCKET_H_
#define _CRICKET_P2P_BASE_P2PSOCKET_H_

#include <vector>
#include <string>
#include "talk/base/sigslot.h"
#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/port.h"
#include "talk/p2p/base/portallocator.h"

namespace cricket {

// Adds the port on which the candidate originated.
class RemoteCandidate: public Candidate {
 public:
  RemoteCandidate(const Candidate& c, Port* origin_port)
    : Candidate(c), origin_port_(origin_port) {}

  Port* origin_port() { return origin_port_; }

 private:
  Port* origin_port_;
};

// P2PSocket manages the candidates and connection process to keep two P2P
// clients connected to each other.
class P2PSocket : public MessageHandler, public sigslot::has_slots<> {
 public:
  enum State {
    STATE_CONNECTING = 0,    // establishing writability
    STATE_WRITABLE,          // connected - ready for writing
  };

  P2PSocket(const std::string &name, PortAllocator *allocator);
  virtual ~P2PSocket();

  // Typically SocketManager calls these

  const std::string &name() const;
  void StartProcessingCandidates();
  void AddRemoteCandidates(const std::vector<Candidate> &remote_candidates);
  void OnSignalingReady();
  void Reset();

  // Typically the Session Client calls these

  int Send(const char *data, size_t len);
  int SetOption(Socket::Option opt, int value);
  int GetError();

  State state();
  bool writable();
  const std::vector<Connection *>& connections();
  Connection* best_connection() { return best_connection_; }
  Thread *thread();

  sigslot::signal2<P2PSocket *, State> SignalState;
  sigslot::signal0<> SignalRequestSignaling;
  sigslot::signal3<P2PSocket *, const char *, size_t> SignalReadPacket;
  sigslot::signal2<P2PSocket *, const SocketAddress &> SignalConnectionChanged;
  sigslot::signal2<P2PSocket *, const std::vector<Candidate>&> 
      SignalCandidatesReady;
  sigslot::signal1<P2PSocket *> SignalConnectionMonitor;

  // Handler for internal messages.
  virtual void OnMessage(Message *pmsg);

 private:
  void set_state(State state);
  void UpdateConnectionStates();
  void RequestSort();
  void SortConnections();
  void SwitchBestConnectionTo(Connection* conn);
  void HandleWritable();
  void HandleNotWritable();
  void HandleAllTimedOut();
  Connection* GetBestConnectionOnNetwork(Network* network);
  bool CreateConnections(const Candidate &remote_candidate, Port* origin_port, 
                         bool readable);
  bool CreateConnection(Port* port, const Candidate& remote_candidate, 
                        Port* origin_port, bool readable);
  void RememberRemoteCandidate(const Candidate& remote_candidate, 
                               Port* origin_port);
  void OnUnknownAddress(Port *port, const SocketAddress &addr, 
                        StunMessage *stun_msg, 
                        const std::string &remote_username);
  void OnPortReady(PortAllocatorSession *session, Port* port);
  void OnCandidatesReady(PortAllocatorSession *session, 
                         const std::vector<Candidate>& candidates);
  void OnConnectionStateChange(Connection *connection);
  void OnConnectionDestroyed(Connection *connection);
  void OnPortDestroyed(Port* port);
  void OnAllocate();
  void OnReadPacket(Connection *connection, const char *data, size_t len);
  void OnSort();
  void OnPing();
  bool IsPingable(Connection* conn);
  Connection* FindNextPingableConnection();
  uint32 NumPingableConnections();
  PortAllocatorSession* allocator_session() {
    return allocator_sessions_.back();
  }
  void AddAllocatorSession(PortAllocatorSession* session);

  Thread *worker_thread_;
  State state_;
  bool waiting_for_signaling_;
  int error_;
  std::string name_;
  PortAllocator *allocator_;
  std::vector<PortAllocatorSession*> allocator_sessions_;
  std::vector<Port *> ports_;
  std::vector<Connection *> connections_;
  Connection *best_connection_;
  std::vector<RemoteCandidate> remote_candidates_;
  bool pinging_started_;  // indicates whether StartGetAllCandidates has been called
  bool sort_dirty_; // indicates whether another sort is needed right now
  bool was_writable_;
  bool was_timed_out_;
  typedef std::map<Socket::Option, int> OptionMap;
  OptionMap options_;

  DISALLOW_EVIL_CONSTRUCTORS(P2PSocket);
};

} // namespace cricket

#endif // _CRICKET_P2P_BASE_P2PSOCKET_H_

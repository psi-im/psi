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
#include "talk/base/host.h"
#include "talk/base/logging.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/p2p/base/port.h"
#include "talk/p2p/base/udpport.h"
#include "talk/p2p/base/tcpport.h"
#include "talk/p2p/base/stunport.h"
#include "talk/p2p/base/relayport.h"
#include "talk/p2p/base/helpers.h"
#include <cassert>

namespace {

const uint32 MSG_CONFIG_START = 1;
const uint32 MSG_CONFIG_READY = 2;
const uint32 MSG_ALLOCATE = 3;
const uint32 MSG_ALLOCATION_PHASE = 4;
const uint32 MSG_SHAKE = 5;

const uint32 ALLOCATE_DELAY = 250;
const uint32 ALLOCATION_STEP_DELAY = 1 * 1000;

const int PHASE_UDP = 0;
const int PHASE_RELAY = 1;
const int PHASE_TCP = 2;
const int PHASE_SSLTCP = 3;
const int kNumPhases = 4;

const float PREF_LOCAL_UDP = 1.0f;
const float PREF_LOCAL_STUN = 0.9f;
const float PREF_LOCAL_TCP = 0.8f;
const float PREF_RELAY = 0.5f;

const float RELAY_PRIMARY_PREF_MODIFIER = 0.0f; // modifiers of the above constants
const float RELAY_BACKUP_PREF_MODIFIER = -0.2f;


// Returns the phase in which a given local candidate (or rather, the port that
// gave rise to that local candidate) would have been created.
int LocalCandidateToPhase(const cricket::Candidate& candidate) {
  cricket::ProtocolType proto;
  bool result = cricket::StringToProto(candidate.protocol().c_str(), proto);
  if (result) {
    if (candidate.type() == cricket::LOCAL_PORT_TYPE) {
      switch (proto) {
      case cricket::PROTO_UDP: return PHASE_UDP;
      case cricket::PROTO_TCP: return PHASE_TCP;
      default: assert(false);
      }
    } else if (candidate.type() == cricket::STUN_PORT_TYPE) {
      return PHASE_UDP;
    } else if (candidate.type() == cricket::RELAY_PORT_TYPE) {
      switch (proto) {
      case cricket::PROTO_UDP: return PHASE_RELAY;
      case cricket::PROTO_TCP: return PHASE_TCP;
      case cricket::PROTO_SSLTCP: return PHASE_SSLTCP;
      default: assert(false);
      }
    } else {
      assert(false);
    }
  } else {
    assert(false);
  }
  return PHASE_UDP; // reached only with assert failure
}

const int SHAKE_MIN_DELAY = 45 * 1000; // 45 seconds
const int SHAKE_MAX_DELAY = 90 * 1000; // 90 seconds

int ShakeDelay() {
  int range = SHAKE_MAX_DELAY - SHAKE_MIN_DELAY + 1;
  return SHAKE_MIN_DELAY + cricket::CreateRandomId() % range;
}

}

namespace cricket {

// Performs the allocation of ports, in a sequenced (timed) manner, for a given
// network and IP address.
class AllocationSequence: public MessageHandler {
public:
  AllocationSequence(BasicPortAllocatorSession* session,
                     Network* network,
                     PortConfiguration* config);
  ~AllocationSequence();

  // Determines whether this sequence is operating on an equivalent network
  // setup to the one given.
  bool IsEquivalent(Network* network);

  // Starts and stops the sequence.  When started, it will continue allocating
  // new ports on its own timed schedule.
  void Start();
  void Stop();

  // MessageHandler:
  void OnMessage(Message* msg);

  void EnableProtocol(ProtocolType proto);
  bool ProtocolEnabled(ProtocolType proto) const;

private:
  BasicPortAllocatorSession* session_;
  Network* network_;
  uint32 ip_;
  PortConfiguration* config_;
  bool running_;
  int step_;
  int step_of_phase_[kNumPhases];

  typedef std::vector<ProtocolType> ProtocolList;
  ProtocolList protocols_;

  void CreateUDPPorts();
  void CreateTCPPorts();
  void CreateStunPorts();
  void CreateRelayPorts();
};


// BasicPortAllocator

BasicPortAllocator::BasicPortAllocator(NetworkManager* network_manager)
  : network_manager_(network_manager), best_writable_phase_(-1), stun_address_(NULL), relay_address_(NULL) {
}

BasicPortAllocator::BasicPortAllocator(NetworkManager* network_manager, SocketAddress* stun_address, SocketAddress *relay_address)
  : network_manager_(network_manager), best_writable_phase_(-1), stun_address_(stun_address), relay_address_(relay_address) {
}

BasicPortAllocator::~BasicPortAllocator() {
}

int BasicPortAllocator::best_writable_phase() const {
  // If we are configured with an HTTP proxy, the best bet is to use the relay
  if ((best_writable_phase_ == -1)
      && ((proxy().type == PROXY_HTTPS) || (proxy().type == PROXY_UNKNOWN))) {
    return PHASE_RELAY;
  }
  return best_writable_phase_;
}

PortAllocatorSession *BasicPortAllocator::CreateSession(const std::string &name) {
  return new BasicPortAllocatorSession(this, name, stun_address_, relay_address_);
}

void BasicPortAllocator::AddWritablePhase(int phase) {
  if ((best_writable_phase_ == -1) || (phase < best_writable_phase_))
    best_writable_phase_ = phase;
}

// BasicPortAllocatorSession

BasicPortAllocatorSession::BasicPortAllocatorSession(
    BasicPortAllocator *allocator,
    const std::string &name)
  : allocator_(allocator), name_(name), network_thread_(NULL),
    config_thread_(NULL), allocation_started_(false), running_(false),
    stun_address_(NULL), relay_address_(NULL) {
}

BasicPortAllocatorSession::BasicPortAllocatorSession(
    BasicPortAllocator *allocator,
    const std::string &name,
    SocketAddress *stun_address,
    SocketAddress *relay_address)
  : allocator_(allocator), name_(name), network_thread_(NULL),
    config_thread_(NULL), allocation_started_(false), running_(false),
    stun_address_(stun_address), relay_address_(relay_address) {
}

BasicPortAllocatorSession::~BasicPortAllocatorSession() {
  if (config_thread_ != NULL)
    config_thread_->Clear(this);
  if (network_thread_ != NULL)
    network_thread_->Clear(this);

  std::vector<PortData>::iterator it;
  for (it = ports_.begin(); it != ports_.end(); it++)
    delete it->port;

  for (uint32 i = 0; i < configs_.size(); ++i)
    delete configs_[i];

  for (uint32 i = 0; i < sequences_.size(); ++i)
    delete sequences_[i];
}

void BasicPortAllocatorSession::GetInitialPorts() {
  network_thread_ = Thread::Current();
  if (!config_thread_)
    config_thread_ = network_thread_;

  config_thread_->Post(this, MSG_CONFIG_START);

  if (allocator()->flags() & PORTALLOCATOR_ENABLE_SHAKER)
    network_thread_->PostDelayed(ShakeDelay(), this, MSG_SHAKE);
}

void BasicPortAllocatorSession::StartGetAllPorts() {
  assert(Thread::Current() == network_thread_);
  running_ = true;
  if (allocation_started_)
    network_thread_->PostDelayed(ALLOCATE_DELAY, this, MSG_ALLOCATE);
  for (uint32 i = 0; i < sequences_.size(); ++i)
    sequences_[i]->Start();
  for (size_t i = 0; i < ports_.size(); ++i)
    ports_[i].port->Start();
}

void BasicPortAllocatorSession::StopGetAllPorts() {
  assert(Thread::Current() == network_thread_);
  running_ = false;
  network_thread_->Clear(this, MSG_ALLOCATE);
  for (uint32 i = 0; i < sequences_.size(); ++i)
    sequences_[i]->Stop();
}

void BasicPortAllocatorSession::OnMessage(Message *message) {
  switch (message->message_id) {
  case MSG_CONFIG_START:
    assert(Thread::Current() == config_thread_);
    GetPortConfigurations();
    break;

  case MSG_CONFIG_READY:
    assert(Thread::Current() == network_thread_);
    OnConfigReady(static_cast<PortConfiguration*>(message->pdata));
    break;

  case MSG_ALLOCATE:
    assert(Thread::Current() == network_thread_);
    OnAllocate();
    break;

  case MSG_SHAKE:
    assert(Thread::Current() == network_thread_);
    OnShake();
    break;

  default:
    assert(false);
  }
}

void BasicPortAllocatorSession::GetPortConfigurations() {
  PortConfiguration* config = NULL;
  if (stun_address_ != NULL)
    config = new PortConfiguration(*stun_address_,
				   CreateRandomString(16),
				   CreateRandomString(16),
				   "");
  PortConfiguration::PortList ports;
  if (relay_address_ != NULL) {
    ports.push_back(ProtocolAddress(*relay_address_, PROTO_UDP));
    config->AddRelay(ports, RELAY_PRIMARY_PREF_MODIFIER);
  }

  ConfigReady(config);
}

void BasicPortAllocatorSession::ConfigReady(PortConfiguration* config) {
  network_thread_->Post(this, MSG_CONFIG_READY, config);
}

// Adds a configuration to the list.
void BasicPortAllocatorSession::OnConfigReady(PortConfiguration* config) {
  if (config)
    configs_.push_back(config);

  AllocatePorts();
}

void BasicPortAllocatorSession::AllocatePorts() {
  assert(Thread::Current() == network_thread_);

  if (allocator_->proxy().type != PROXY_NONE)
    Port::set_proxy(allocator_->user_agent(), allocator_->proxy());

  network_thread_->Post(this, MSG_ALLOCATE);
}

// For each network, see if we have a sequence that covers it already.  If not,
// create a new sequence to create the appropriate ports.
void BasicPortAllocatorSession::OnAllocate() {
  std::vector<Network*> networks;
  allocator_->network_manager()->GetNetworks(networks);

  for (uint32 i = 0; i < networks.size(); ++i) {
    if (HasEquivalentSequence(networks[i]))
      continue;

    PortConfiguration* config = NULL;
    if (configs_.size() > 0)
      config = configs_.back();

    AllocationSequence* sequence =
        new AllocationSequence(this, networks[i], config);
    if (running_)
      sequence->Start();

    sequences_.push_back(sequence);
  }

  allocation_started_ = true;
  if (running_)
    network_thread_->PostDelayed(ALLOCATE_DELAY, this, MSG_ALLOCATE);
}

bool BasicPortAllocatorSession::HasEquivalentSequence(Network* network) {
  for (uint32 i = 0; i < sequences_.size(); ++i)
    if (sequences_[i]->IsEquivalent(network))
      return true;
  return false;
}

void BasicPortAllocatorSession::AddAllocatedPort(Port* port,
                                                 AllocationSequence * seq,
                                                 float pref,
                                                 bool prepare_address) {
  if (!port)
    return;

  port->set_name(name_);
  port->set_preference(pref);
  port->set_generation(generation());
  PortData data;
  data.port = port;
  data.sequence = seq;
  data.ready = false;
  ports_.push_back(data);
  port->SignalAddressReady.connect(this, &BasicPortAllocatorSession::OnAddressReady);
  port->SignalConnectionCreated.connect(this, &BasicPortAllocatorSession::OnConnectionCreated);
  port->SignalDestroyed.connect(this, &BasicPortAllocatorSession::OnPortDestroyed);
  if (prepare_address)
    port->PrepareAddress();
  if (running_)
    port->Start();
}

void BasicPortAllocatorSession::OnAddressReady(Port *port) {
  assert(Thread::Current() == network_thread_);
  std::vector<PortData>::iterator it = std::find(ports_.begin(), ports_.end(), port);
  assert(it != ports_.end());
  assert(!it->ready);
  it->ready = true;
  SignalPortReady(this, port);

  // Only accumulate the candidates whose protocol has been enabled
  std::vector<Candidate> candidates;
  const std::vector<Candidate>& potentials = port->candidates();
  for (size_t i=0; i<potentials.size(); ++i) {
    ProtocolType pvalue;
    if (!StringToProto(potentials[i].protocol().c_str(), pvalue))
      continue;
    if (it->sequence->ProtocolEnabled(pvalue)) {
      candidates.push_back(potentials[i]);
    }
  }
  if (!candidates.empty()) {
    SignalCandidatesReady(this, candidates);
  }
}

void BasicPortAllocatorSession::OnProtocolEnabled(AllocationSequence * seq, ProtocolType proto) {
  std::vector<Candidate> candidates;
  for (std::vector<PortData>::iterator it = ports_.begin(); it != ports_.end(); ++it) {
    if (!it->ready || (it->sequence != seq))
      continue;

    const std::vector<Candidate>& potentials = it->port->candidates();
    for (size_t i=0; i<potentials.size(); ++i) {
      ProtocolType pvalue;
      if (!StringToProto(potentials[i].protocol().c_str(), pvalue))
        continue;
      if (pvalue == proto) {
        candidates.push_back(potentials[i]);
      }
    }
  }
  if (!candidates.empty()) {
    SignalCandidatesReady(this, candidates);
  }
}

void BasicPortAllocatorSession::OnPortDestroyed(Port* port) {
  assert(Thread::Current() == network_thread_);
  std::vector<PortData>::iterator iter =
      find(ports_.begin(), ports_.end(), port);
  assert(iter != ports_.end());
  ports_.erase(iter);

  LOG(INFO) << "Removed port from allocator: "
            << static_cast<int>(ports_.size()) << " remaining";
}

void BasicPortAllocatorSession::OnConnectionCreated(Port* port, Connection* conn) {
  conn->SignalStateChange.connect(this, &BasicPortAllocatorSession::OnConnectionStateChange);
}

void BasicPortAllocatorSession::OnConnectionStateChange(Connection* conn) {
  if (conn->write_state() == Connection::STATE_WRITABLE)
    allocator_->AddWritablePhase(LocalCandidateToPhase(conn->local_candidate()));
}

void BasicPortAllocatorSession::OnShake() {
  LOG(INFO) << ">>>>> SHAKE <<<<< >>>>> SHAKE <<<<< >>>>> SHAKE <<<<<";

  std::vector<Port*> ports;
  std::vector<Connection*> connections;

  for (size_t i = 0; i < ports_.size(); ++i) {
    if (ports_[i].ready)
      ports.push_back(ports_[i].port);
  }

  for (size_t i = 0; i < ports.size(); ++i) {
    Port::AddressMap::const_iterator iter;
    for (iter = ports[i]->connections().begin();
         iter != ports[i]->connections().end();
         ++iter) {
      connections.push_back(iter->second);
    }
  }

  LOG(INFO) << ">>>>> Destroying " << (int)ports.size() << " ports and "
            << (int)connections.size() << " connections";

  for (size_t i = 0; i < connections.size(); ++i)
    connections[i]->Destroy();

  if (running_ || (ports.size() > 0) || (connections.size() > 0))
    network_thread_->PostDelayed(ShakeDelay(), this, MSG_SHAKE);
}

// AllocationSequence

AllocationSequence::AllocationSequence(BasicPortAllocatorSession* session,
                                       Network* network,
                                       PortConfiguration* config)
  : session_(session), network_(network), ip_(network->ip()), config_(config),
    running_(false), step_(0) {

  // All of the phases up until the best-writable phase so far run in step 0.
  // The other phases follow sequentially in the steps after that.  If there is
  // no best-writable so far, then only phase 0 occurs in step 0.
  int last_phase_in_step_zero =
      _max(0, session->allocator()->best_writable_phase());
  for (int phase = 0; phase < kNumPhases; ++phase)
    step_of_phase_[phase] = _max(0, phase - last_phase_in_step_zero);

  // Immediately perform phase 0.
  OnMessage(NULL);
}

AllocationSequence::~AllocationSequence() {
  session_->network_thread()->Clear(this);
}

bool AllocationSequence::IsEquivalent(Network* network) {
  return (network == network_) && (ip_ == network->ip());
}

void AllocationSequence::Start() {
  running_ = true;
  session_->network_thread()->PostDelayed(ALLOCATION_STEP_DELAY,
                                          this,
                                          MSG_ALLOCATION_PHASE);
}

void AllocationSequence::Stop() {
  running_ = false;
  session_->network_thread()->Clear(this, MSG_ALLOCATION_PHASE);
}

void AllocationSequence::OnMessage(Message* msg) {
  assert(Thread::Current() == session_->network_thread());
  if (msg)
    assert(msg->message_id == MSG_ALLOCATION_PHASE);

  // Perform all of the phases in the current step.
  for (int phase = 0; phase < kNumPhases; phase++) {
    if (step_of_phase_[phase] != step_)
      continue;

    switch (phase) {
    case PHASE_UDP:
      LOG(INFO) << "Phase=UDP Step=" << step_;
      CreateUDPPorts();
      CreateStunPorts();
      EnableProtocol(PROTO_UDP);
      break;

    case PHASE_RELAY:
      LOG(INFO) << "Phase=RELAY Step=" << step_;
      CreateRelayPorts();
      break;

    case PHASE_TCP:
      LOG(INFO) << "Phase=TCP Step=" << step_;
      CreateTCPPorts();
      EnableProtocol(PROTO_TCP);
      break;

    case PHASE_SSLTCP:
      LOG(INFO) << "Phase=SSLTCP Step=" << step_;
      EnableProtocol(PROTO_SSLTCP);
      break;

    default:
      // Nothing else we can do.
      return;
    }
  }

  // TODO: use different delays for each stage
  step_ += 1;
  if (running_) {
    session_->network_thread()->PostDelayed(ALLOCATION_STEP_DELAY,
                                            this,
                                            MSG_ALLOCATION_PHASE);
  }
}

void AllocationSequence::EnableProtocol(ProtocolType proto) {
  if (!ProtocolEnabled(proto)) {
    protocols_.push_back(proto);
    session_->OnProtocolEnabled(this, proto);
  }
}

bool AllocationSequence::ProtocolEnabled(ProtocolType proto) const {
  for (ProtocolList::const_iterator it = protocols_.begin(); it != protocols_.end(); ++it) {
    if (*it == proto)
      return true;
  }
  return false;
}

void AllocationSequence::CreateUDPPorts() {
  if (session_->allocator()->flags() & PORTALLOCATOR_DISABLE_UDP)
    return;

  Port* port = new UDPPort(session_->network_thread(), NULL, network_,
                           SocketAddress(ip_, 0));
  session_->AddAllocatedPort(port, this, PREF_LOCAL_UDP);
}

void AllocationSequence::CreateTCPPorts() {
  if (session_->allocator()->flags() & PORTALLOCATOR_DISABLE_TCP)
    return;

  Port* port = new TCPPort(session_->network_thread(), NULL, network_,
                           SocketAddress(ip_, 0));
  session_->AddAllocatedPort(port, this, PREF_LOCAL_TCP); 
}

void AllocationSequence::CreateStunPorts() {
  if (session_->allocator()->flags() & PORTALLOCATOR_DISABLE_STUN)
    return;

  if (!config_ || config_->stun_address.IsAny())
    return;

  Port* port = new StunPort(session_->network_thread(), NULL, network_,
                            SocketAddress(ip_, 0), config_->stun_address);
  session_->AddAllocatedPort(port, this, PREF_LOCAL_STUN); 
}

void AllocationSequence::CreateRelayPorts() {
  if (session_->allocator()->flags() & PORTALLOCATOR_DISABLE_RELAY)
    return;

  if (!config_)
    return;

  PortConfiguration::RelayList::const_iterator relay;
  for (relay = config_->relays.begin();
       relay != config_->relays.end();
       ++relay) {

    RelayPort *port = new RelayPort(session_->network_thread(), NULL, network_,
                                    SocketAddress(ip_, 0),
                                    config_->username, config_->password,
                                    config_->magic_cookie);
    // Note: We must add the allocated port before we add addresses because
    //       the latter will create candidates that need name and preference
    //       settings.  However, we also can't prepare the address (normally
    //       done by AddAllocatedPort) until we have these addresses.  So we
    //       wait to do that until below.
    session_->AddAllocatedPort(port, this, PREF_RELAY + relay->pref_modifier, false);

    // Add the addresses of this protocol.
    PortConfiguration::PortList::const_iterator relay_port;
    for (relay_port = relay->ports.begin();
          relay_port != relay->ports.end();
          ++relay_port) {
      port->AddServerAddress(*relay_port);
      port->AddExternalAddress(*relay_port);
    }

    // Start fetching an address for this port.
    port->PrepareAddress();
  }
}

// PortConfiguration

PortConfiguration::PortConfiguration(const SocketAddress& sa,
                                     const std::string& un,
                                     const std::string& pw,
                                     const std::string& mc)
  : stun_address(sa), username(un), password(pw), magic_cookie(mc) {
}

void PortConfiguration::AddRelay(const PortList& ports, float pref_modifier) {
  RelayServer relay;
  relay.ports = ports;
  relay.pref_modifier = pref_modifier;
  relays.push_back(relay);
}

bool PortConfiguration::SupportsProtocol(
    const PortConfiguration::RelayServer& relay, ProtocolType type) {
  PortConfiguration::PortList::const_iterator relay_port;
  for (relay_port = relay.ports.begin();
        relay_port != relay.ports.end();
        ++relay_port) {
    if (relay_port->proto == type)
      return true;
  }
  return false;
}

} // namespace cricket

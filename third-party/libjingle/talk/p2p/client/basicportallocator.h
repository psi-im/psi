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

#ifndef _BASICPORTALLOCATOR_H_
#define _BASICPORTALLOCATOR_H_

#include "talk/base/thread.h"
#include "talk/base/messagequeue.h"
#include "talk/base/network.h"
#include "talk/p2p/base/portallocator.h"
#include <string>
#include <vector>

namespace cricket {

class BasicPortAllocator: public PortAllocator {
public:
  BasicPortAllocator(NetworkManager* network_manager);
  BasicPortAllocator(NetworkManager* network_manager, SocketAddress *stun_server, SocketAddress *relay_server);
  virtual ~BasicPortAllocator();

  NetworkManager* network_manager() { return network_manager_; }

  // Returns the best (highest preference) phase that has produced a port that
  // produced a writable connection.  If no writable connections have been
  // produced, this returns -1.
  int best_writable_phase() const;

  virtual PortAllocatorSession *CreateSession(const std::string &name);

  // Called whenever a connection becomes writable with the argument being the
  // phase that the corresponding port was created in.
  void AddWritablePhase(int phase);

private:
  NetworkManager* network_manager_;
  SocketAddress* stun_address_;
  SocketAddress* relay_address_;
  int best_writable_phase_;
};

struct PortConfiguration;
class AllocationSequence;

class BasicPortAllocatorSession: public PortAllocatorSession, public MessageHandler {
public:
  BasicPortAllocatorSession(BasicPortAllocator *allocator,
			    const std::string &name);
  BasicPortAllocatorSession(BasicPortAllocator *allocator,
                            const std::string &name,
			    SocketAddress *stun_address,
			    SocketAddress *relay_address);
  ~BasicPortAllocatorSession();

  BasicPortAllocator* allocator() { return allocator_; }
  const std::string& name() const { return name_; }
  Thread* network_thread() { return network_thread_; }

  Thread* config_thread() { return config_thread_; }
  void set_config_thread(Thread* thread) { config_thread_ = thread; }

  virtual void GetInitialPorts();
  virtual void StartGetAllPorts();
  virtual void StopGetAllPorts();
  virtual bool IsGettingAllPorts() { return running_; }

protected:
  // Starts the process of getting the port configurations.
  virtual void GetPortConfigurations();

  // Adds a port configuration that is now ready.  Once we have one for each
  // network (or a timeout occurs), we will start allocating ports.
  void ConfigReady(PortConfiguration* config);

  // MessageHandler.  Can be overriden if message IDs do not conflict.
  virtual void OnMessage(Message *message);

private:
  void OnConfigReady(PortConfiguration* config);
  void OnConfigTimeout();
  void AllocatePorts();
  void OnAllocate();
  bool HasEquivalentSequence(Network* network);
  void AddAllocatedPort(Port* port, AllocationSequence * seq, float pref, bool prepare_address = true);
  void OnAddressReady(Port *port);
  void OnProtocolEnabled(AllocationSequence * seq, ProtocolType proto);
  void OnPortDestroyed(Port* port);
  void OnConnectionCreated(Port* port, Connection* conn);
  void OnConnectionStateChange(Connection* conn);
  void OnShake();

  BasicPortAllocator *allocator_;
  std::string name_;
  Thread* network_thread_;
  Thread* config_thread_;
  bool configuration_done_;
  bool allocation_started_;
  bool running_; // set when StartGetAllPorts is called
  std::vector<PortConfiguration*> configs_;
  std::vector<AllocationSequence*> sequences_;
  SocketAddress *stun_address_;
  SocketAddress *relay_address_;

  struct PortData {
    Port * port;
    AllocationSequence * sequence;
    bool ready;

    bool operator==(Port * rhs) const { return (port == rhs); }
  };
  std::vector<PortData> ports_;

  friend class AllocationSequence;
};

// Records configuration information useful in creating ports.
struct PortConfiguration: public MessageData {
  SocketAddress stun_address;
  std::string username;
  std::string password;
  std::string magic_cookie;

  typedef std::vector<ProtocolAddress> PortList;
  struct RelayServer {
    PortList ports;
    float pref_modifier; // added to the protocol modifier to get the
                         // preference for this particular server
  };

  typedef std::vector<RelayServer> RelayList;
  RelayList relays;

  PortConfiguration(const SocketAddress& stun_address,
                    const std::string& username,
                    const std::string& password,
                    const std::string& magic_cookie);

  // Adds another relay server, with the given ports and modifier, to the list.
  void AddRelay(const PortList& ports, float pref_modifier);

  // Determines whether the given relay server supports the given protocol.
  static bool SupportsProtocol(const PortConfiguration::RelayServer& relay,
                               ProtocolType type);
};

} // namespace cricket

#endif // _BASICPORTALLOCATOR_H_

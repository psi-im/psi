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

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include "talk/base/basictypes.h"

#include <deque>
#include <map>
#include <string>
#include <vector>

namespace cricket {

class Network;
class NetworkSession;

// Keeps track of the available network interfaces over time so that quality
// information can be aggregated and recorded.
class NetworkManager {
public:

  // Updates and returns the current list of networks available on this machine.
  // This version will make sure that repeated calls return the same object for
  // a given network, so that quality is tracked appropriately.
  void GetNetworks(std::vector<Network*>& networks);

  // Reads and writes the state of the quality database in a string format.
  std::string GetState();
  void SetState(std::string str);

  // Creates a network object for each network available on the machine.
  static void CreateNetworks(std::vector<Network*>& networks);

private:
  typedef std::map<std::string,Network*> NetworkMap;

  NetworkMap networks_;
};

// Represents a Unix-type network interface, with a name and single address.
// It also includes the ability to track and estimate quality.
class Network {
public:
  Network(const std::string& name, uint32 ip);

  // Returns the OS name of this network.  This is considered the primary key
  // that identifies each network.
  const std::string& name() const { return name_; }

  // Identifies the current IP address used by this network.
  uint32 ip() const { return ip_; }
  void set_ip(uint32 ip) { ip_ = ip; }

  // Updates the list of sessions that are ongoing.
  void StartSession(NetworkSession* session);
  void StopSession(NetworkSession* session);

  // Re-computes the estimate of near-future quality based on the information
  // as of this exact moment.
  void EstimateQuality();

  // Returns the current estimate of the near-future quality of connections
  // that use this local interface.
  double quality() { return quality_; }

private:
  typedef std::vector<NetworkSession*> SessionList;

  std::string name_;
  uint32 ip_;
  SessionList sessions_;
  double uniform_numerator_;
  double uniform_denominator_;
  double exponential_numerator_;
  double exponential_denominator_;
  uint32 last_data_time_;
  double quality_;

  // Updates the statistics maintained to include the given estimate.
  void AddDataPoint(uint32 time, double quality);

  // Converts the internal state to and from a string.  This is used to record
  // quality information into a permanent store.
  void SetState(std::string str);
  std::string GetState();

  friend class NetworkManager;
};

// Represents a session that is in progress using a particular network and can
// provide data about the quality of the network at any given moment.
class NetworkSession {
public:
  // Determines whether this session has an estimate at this moment.  We will
  // only call GetCurrentQuality when this returns true.
  virtual bool HasQuality() = 0;

  // Returns an estimate of the quality at this exact moment.  The result should
  // be a MOS (mean opinion score) value.
  virtual float GetCurrentQuality() = 0;

};

const double QUALITY_BAD  = 3.0;
const double QUALITY_FAIR = 3.35;
const double QUALITY_GOOD = 3.7;

} // namespace cricket

#endif // __NETWORK_H__

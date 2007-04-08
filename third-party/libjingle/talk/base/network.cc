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

#include "talk/base/host.h"
#include "talk/base/logging.h"
#include "talk/base/network.h"
#include "talk/base/socket.h" // this includes something that makes windows happy
#include "talk/base/time.h"
#include "talk/base/basicdefs.h"

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <sstream>

#ifdef POSIX
extern "C" {
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <errno.h>
}
#endif // POSIX

#ifdef WIN32
#include <Iphlpapi.h>
#endif

namespace {

const double kAlpha = 0.5; // weight for data infinitely far in the past
const double kHalfLife = 2000; // half life of exponential decay (in ms)
const double kLog2 = 0.693147180559945309417;
const double kLambda = kLog2 / kHalfLife;

// assume so-so quality unless data says otherwise
const double kDefaultQuality = cricket::QUALITY_FAIR;

typedef std::map<std::string,std::string> StrMap;

void BuildMap(const StrMap& map, std::string& str) {
  str.append("{");
  bool first = true;
  for (StrMap::const_iterator i = map.begin(); i != map.end(); ++i) {
    if (!first) str.append(",");
    str.append(i->first);
    str.append("=");
    str.append(i->second);
    first = false;
  }
  str.append("}");
}

void ParseCheck(std::istringstream& ist, char ch) {
  if (ist.get() != ch)
    LOG(LERROR) << "Expecting '" << ch << "'";
}

std::string ParseString(std::istringstream& ist) {
  std::string str;
  int count = 0;
  while (ist) {
    char ch = ist.peek();
    if ((count == 0) && ((ch == '=') || (ch == ',') || (ch == '}'))) {
      break;
    } else if (ch == '{') {
      count += 1;
    } else if (ch == '}') {
      count -= 1;
      if (count < 0)
        LOG(LERROR) << "mismatched '{' and '}'";
    }
    str.append(1, static_cast<char>(ist.get()));
  }
  return str;
}

void ParseMap(const std::string& str, StrMap& map) {
  if (str.size() == 0)
    return;
  std::istringstream ist(str);
  ParseCheck(ist, '{');
  for (;;) {
    std::string key = ParseString(ist);
    ParseCheck(ist, '=');
    std::string val = ParseString(ist);
    map[key] = val;
    if (ist.peek() == ',')
      ist.get();
    else
      break;
  }
  ParseCheck(ist, '}');
  if (ist.rdbuf()->in_avail() != 0)
    LOG(LERROR) << "Unexpected characters at end";
}

#if 0
const std::string TEST_MAP0_IN = "";
const std::string TEST_MAP0_OUT = "{}";
const std::string TEST_MAP1 = "{a=12345}";
const std::string TEST_MAP2 = "{a=12345,b=67890}";
const std::string TEST_MAP3 = "{a=12345,b=67890,c=13579}";
const std::string TEST_MAP4 = "{a={d=12345,e=67890}}";
const std::string TEST_MAP5 = "{a={d=12345,e=67890},b=67890}";
const std::string TEST_MAP6 = "{a=12345,b={d=12345,e=67890}}";
const std::string TEST_MAP7 = "{a=12345,b={d=12345,e=67890},c=13579}";

class MyTest {
public:
  MyTest() {
    test(TEST_MAP0_IN, TEST_MAP0_OUT);
    test(TEST_MAP1, TEST_MAP1);
    test(TEST_MAP2, TEST_MAP2);
    test(TEST_MAP3, TEST_MAP3);
    test(TEST_MAP4, TEST_MAP4);
    test(TEST_MAP5, TEST_MAP5);
    test(TEST_MAP6, TEST_MAP6);
    test(TEST_MAP7, TEST_MAP7);
  }
  void test(const std::string& input, const std::string& exp_output) {
    StrMap map;
    ParseMap(input, map);
    std::string output;
    BuildMap(map, output);
    LOG(INFO) << "  ********  " << (output == exp_output);
  }
};

static MyTest myTest;
#endif

template <typename T>
std::string ToString(T val) {
  std::ostringstream ost;
  ost << val;
  return ost.str();
}

template <typename T>
T FromString(std::string str) {
  std::istringstream ist(str);
  T val;
  ist >> val;
  return val;
}

}

namespace cricket {

#ifdef POSIX
void NetworkManager::CreateNetworks(std::vector<Network*>& networks) {
  int fd;
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    PLOG(LERROR, errno) << "socket";
    return;
  }

  struct ifconf ifc;
  ifc.ifc_len = 64 * sizeof(struct ifreq);
  ifc.ifc_buf = new char[ifc.ifc_len];

  if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
    PLOG(LERROR, errno) << "ioctl";
    return;
  }
  assert(ifc.ifc_len < static_cast<int>(64 * sizeof(struct ifreq)));

  struct ifreq* ptr = reinterpret_cast<struct ifreq*>(ifc.ifc_buf);
  struct ifreq* end =
      reinterpret_cast<struct ifreq*>(ifc.ifc_buf + ifc.ifc_len);

  while (ptr < end) {
    struct sockaddr_in* inaddr =
        reinterpret_cast<struct sockaddr_in*>(&ptr->ifr_ifru.ifru_addr);
    if (inaddr->sin_family == AF_INET) {
      uint32 ip = ntohl(inaddr->sin_addr.s_addr);
      networks.push_back(new Network(std::string(ptr->ifr_name), ip));
    }
#ifdef _SIZEOF_ADDR_IFREQ
    ptr = reinterpret_cast<struct ifreq*>(
        reinterpret_cast<char*>(ptr) + _SIZEOF_ADDR_IFREQ(*ptr));
#else
    ptr++;
#endif
  }

  delete [] ifc.ifc_buf;
  close(fd);
}
#endif

#ifdef WIN32
void NetworkManager::CreateNetworks(std::vector<Network*>& networks) {
  IP_ADAPTER_INFO info_temp;
  ULONG len = 0;
  
  if (GetAdaptersInfo(&info_temp, &len) != ERROR_BUFFER_OVERFLOW)
    return;
  IP_ADAPTER_INFO *infos = new IP_ADAPTER_INFO[len];
  if (GetAdaptersInfo(infos, &len) != NO_ERROR)
    return;

  int count = 0;
  for (IP_ADAPTER_INFO *info = infos; info != NULL; info = info->Next) {
    if (info->Type == MIB_IF_TYPE_LOOPBACK)
      continue;
    if (strcmp(info->IpAddressList.IpAddress.String, "0.0.0.0") == 0)
      continue;

    // In production, don't transmit the network name because of
    // privacy concerns. Transmit a number instead.

    std::string name;
#if defined(PRODUCTION)
    std::ostringstream ost;
    ost << count;
    name = ost.str();
    count++;
#else
    name = info->Description;
#endif

    networks.push_back(new Network(name,
        SocketAddress::StringToIP(info->IpAddressList.IpAddress.String)));
  }

  delete infos;
}
#endif

void NetworkManager::GetNetworks(std::vector<Network*>& result) {
  std::vector<Network*> list;
  CreateNetworks(list);

  for (uint32 i = 0; i < list.size(); ++i) {
    NetworkMap::iterator iter = networks_.find(list[i]->name());

    Network* network;
    if (iter == networks_.end()) {
      network = list[i];
    } else {
      network = iter->second;
      network->set_ip(list[i]->ip());
      delete list[i];
    }

    networks_[network->name()] = network;
    result.push_back(network);
  }
}

std::string NetworkManager::GetState() {
  StrMap map;
  for (NetworkMap::iterator i = networks_.begin(); i != networks_.end(); ++i)
    map[i->first] = i->second->GetState();

  std::string str;
  BuildMap(map, str);
  return str;
}

void NetworkManager::SetState(std::string str) {
  StrMap map;
  ParseMap(str, map);

  for (StrMap::iterator i = map.begin(); i != map.end(); ++i) {
    std::string name = i->first;
    std::string state = i->second;

    Network* network = new Network(name, 0);
    network->SetState(state);
    networks_[name] = network;
  }
}

Network::Network(const std::string& name, uint32 ip)
  : name_(name), ip_(ip), uniform_numerator_(0), uniform_denominator_(0),
    exponential_numerator_(0), exponential_denominator_(0),
    quality_(kDefaultQuality) {

  last_data_time_ = Time();

  // TODO: seed the historical data with one data point based on the link speed
  //       metric from XP (4.0 if < 50, 3.0 otherwise).
}

void Network::StartSession(NetworkSession* session) {
  assert(std::find(sessions_.begin(), sessions_.end(), session) == sessions_.end());
  sessions_.push_back(session);
}

void Network::StopSession(NetworkSession* session) {
  SessionList::iterator iter = std::find(sessions_.begin(), sessions_.end(), session);
  if (iter != sessions_.end())
    sessions_.erase(iter);
}

void Network::EstimateQuality() {
  uint32 now = Time();

  // Add new data points for the current time.
  for (uint32 i = 0; i < sessions_.size(); ++i) {
    if (sessions_[i]->HasQuality())
      AddDataPoint(now, sessions_[i]->GetCurrentQuality());
  }

  // Construct the weighted average using both uniform and exponential weights.

  double exp_shift = exp(-kLambda * (now - last_data_time_));
  double numerator = uniform_numerator_ + exp_shift * exponential_numerator_;
  double denominator = uniform_denominator_ + exp_shift * exponential_denominator_;

  if (denominator < DBL_EPSILON)
    quality_ = kDefaultQuality;
  else
    quality_ = numerator / denominator;
}

void Network::AddDataPoint(uint32 time, double quality) {
  uniform_numerator_ += kAlpha * quality;
  uniform_denominator_ += kAlpha;

  double exp_shift = exp(-kLambda * (time - last_data_time_));
  exponential_numerator_ = (1 - kAlpha) * quality + exp_shift * exponential_numerator_;
  exponential_denominator_ = (1 - kAlpha) + exp_shift * exponential_denominator_;

  last_data_time_ = time;
}

std::string Network::GetState() {
  StrMap map;
  map["lt"] = ToString<uint32>(last_data_time_);
  map["un"] = ToString<double>(uniform_numerator_);
  map["ud"] = ToString<double>(uniform_denominator_);
  map["en"] = ToString<double>(exponential_numerator_);
  map["ed"] = ToString<double>(exponential_denominator_);

  std::string str;
  BuildMap(map, str);
  return str;
}

void Network::SetState(std::string str) {
  StrMap map;
  ParseMap(str, map);

  last_data_time_ = FromString<uint32>(map["lt"]);
  uniform_numerator_ = FromString<double>(map["un"]);
  uniform_denominator_ = FromString<double>(map["ud"]);
  exponential_numerator_ = FromString<double>(map["en"]);
  exponential_denominator_ = FromString<double>(map["ed"]);
}

} // namespace cricket

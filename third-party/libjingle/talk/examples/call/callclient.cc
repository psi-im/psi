/*
 * Jingle call example
 * Copyright 2004--2005, Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string>
#include <vector>

#include "talk/xmpp/constants.h"
#include "talk/base/thread.h"
#include "talk/base/network.h"
#include "talk/base/socketaddress.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/base/helpers.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/session/receiver.h"
#include "talk/session/sessionsendtask.h"
#include "talk/session/phone/phonesessionclient.h"
#include "talk/examples/call/callclient.h"
#include "talk/examples/call/console.h"
#include "talk/examples/call/presencepushtask.h"
#include "talk/examples/call/presenceouttask.h"

namespace {

const char* DescribeStatus(buzz::Status::Show show, const std::string& desc) {
  switch (show) {
  case buzz::Status::SHOW_XA:      return desc.c_str();
  case buzz::Status::SHOW_ONLINE:  return "online";
  case buzz::Status::SHOW_AWAY:    return "away";
  case buzz::Status::SHOW_DND:     return "do not disturb";
  case buzz::Status::SHOW_CHAT:    return "ready to chat";
  default:                         return "offline";
  }
}

} // namespace

const char* CALL_COMMANDS =
"Available commands:\n"
"\n"
"  hangup  Ends the call.\n"
"  mute    Stops sending voice.\n"
"  unmute  Re-starts sending voice.\n"
"  quit    Quits the application.\n"
"";

const char* RECEIVE_COMMANDS =
"Available commands:\n"
"\n"
"  accept  Accepts the incoming call and switches to it.\n"
"  reject  Rejects the incoming call and stays with the current call.\n"
"  quit    Quits the application.\n"
"";

const char* CONSOLE_COMMANDS =
"Available commands:\n"
"\n"
"  roster       Prints the online friends from your roster.\n"
"  call <name>  Initiates a call to the friend with the given name.\n"
"  quit         Quits the application.\n"
"";

void CallClient::ParseLine(const std::string& line) {
  std::vector<std::string> words;
  int start = -1;
  int state = 0;
  for (int index = 0; index <= static_cast<int>(line.size()); ++index) {
    if (state == 0) {
      if (!isspace(line[index])) {
        start = index;
        state = 1;
      }
    } else {
      assert(state == 1);
      assert(start >= 0);
      if (isspace(line[index])) {
        std::string word(line, start, index - start);
		words.push_back(word);
        start = -1;
        state = 0;
      }
    }
  }

  // Global commands
  if ((words.size() == 1) && (words[0] == "quit")) {
      exit(0);
  }

  if (call_ && incoming_call_) {
    if ((words.size() == 1) && (words[0] == "accept")) {
      assert(call_->sessions().size() == 1);
      call_->AcceptSession(call_->sessions()[0]);
	  phone_client()->SetFocus(call_);
      incoming_call_ = false;
    } else if ((words.size() == 1) && (words[0] == "reject")) {
      call_->RejectSession(call_->sessions()[0]);
      incoming_call_ = false;
    } else {
      console_->Print(RECEIVE_COMMANDS);
    }    
  } else if (call_) {
    if ((words.size() == 1) && (words[0] == "hangup")) {
      call_->Terminate();
	  call_ = NULL;
	  session_ = NULL;
	  console_->SetPrompt(NULL);
    } else if ((words.size() == 1) && (words[0] == "mute")) {
      call_->Mute(true);
    } else if ((words.size() == 1) && (words[0] == "unmute")) {
      call_->Mute(false);
    } else {
      console_->Print(CALL_COMMANDS);
    }
  } else {
    if ((words.size() == 1) && (words[0] == "roster")) {
      PrintRoster();
    } else if ((words.size() == 2) && (words[0] == "call")) {
      MakeCallTo(words[1]);
    } else {
      console_->Print(CONSOLE_COMMANDS);
    }
  }
}

CallClient::CallClient(buzz::XmppClient* xmpp_client)
    : xmpp_client_(xmpp_client), roster_(new RosterMap), call_(NULL),
      incoming_call_(false) {
  xmpp_client_->SignalStateChange.connect(this, &CallClient::OnStateChange);
}

CallClient::~CallClient() {
  delete roster_;
}

const std::string CallClient::strerror(buzz::XmppEngine::Error err) {
  switch (err) {
   case  buzz::XmppEngine::ERROR_NONE: 
     return "";
   case  buzz::XmppEngine::ERROR_XML:  
     return "Malformed XML or encoding error";
   case  buzz::XmppEngine::ERROR_STREAM: 
     return "XMPP stream error";
   case  buzz::XmppEngine::ERROR_VERSION:
     return "XMPP version error";
   case  buzz::XmppEngine::ERROR_UNAUTHORIZED:
     return "User is not authorized (Check your username and password)";
   case  buzz::XmppEngine::ERROR_TLS:
     return "TLS could not be negotiated";
   case	 buzz::XmppEngine::ERROR_AUTH:
     return "Authentication could not be negotiated";
   case  buzz::XmppEngine::ERROR_BIND:
     return "Resource or session binding could not be negotiated";
   case  buzz::XmppEngine::ERROR_CONNECTION_CLOSED:
     return "Connection closed by output handler.";
   case  buzz::XmppEngine::ERROR_DOCUMENT_CLOSED:
     return "Closed by </stream:stream>";
   case  buzz::XmppEngine::ERROR_SOCKET:
     return "Socket error";
   default:
	 return "Unknown error";
  }
}

void CallClient::OnCallDestroy(cricket::Call* call) {
  if (call == call_) {
	console_->SetPrompt(NULL);
    console_->Print("call destroyed");
    call_ = NULL;
	session_ = NULL;
  }
}

void CallClient::OnStateChange(buzz::XmppEngine::State state) {
  switch (state) {
  case buzz::XmppEngine::STATE_START:
    console_->Print("connecting...");
    break;

  case buzz::XmppEngine::STATE_OPENING:
    console_->Print("logging in...");
    break;

  case buzz::XmppEngine::STATE_OPEN:
    console_->Print("logged in...");
    InitPhone();
    InitPresence();
    break;

  case buzz::XmppEngine::STATE_CLOSED:
    buzz::XmppEngine::Error error = xmpp_client_->GetError();
    console_->Print("logged out..." + strerror(error));
    exit(0);
  }
}

void CallClient::InitPhone() {
  std::string client_unique = xmpp_client_->jid().Str();
  cricket::InitRandom(client_unique.c_str(), client_unique.size());

  worker_thread_ = new cricket::Thread();

  network_manager_ = new cricket::NetworkManager();
  
  cricket::SocketAddress *stun_addr = new cricket::SocketAddress("64.233.167.126", 19302);
  port_allocator_ = new cricket::BasicPortAllocator(network_manager_, stun_addr, NULL);

  session_manager_ = new cricket::SessionManager(
      port_allocator_, worker_thread_);
  session_manager_->SignalRequestSignaling.connect(
      this, &CallClient::OnRequestSignaling);
  session_manager_->OnSignalingReady();

  phone_client_ = new cricket::PhoneSessionClient(
      xmpp_client_->jid(),session_manager_);
  phone_client_->SignalCallCreate.connect(this, &CallClient::OnCallCreate);
  phone_client_->SignalSendStanza.connect(this, &CallClient::OnSendStanza);

  receiver_ = new cricket::Receiver(xmpp_client_, phone_client_);
  receiver_->Start();

  worker_thread_->Start();
}

void CallClient::OnRequestSignaling() {
  session_manager_->OnSignalingReady();
}

void CallClient::OnCallCreate(cricket::Call* call) {
  call->SignalSessionState.connect(this, &CallClient::OnSessionState);
}

void CallClient::OnSessionState(cricket::Call* call,
                                cricket::Session* session,
                                cricket::Session::State state) {
  if (state == cricket::Session::STATE_RECEIVEDINITIATE) {
    buzz::Jid jid(session->remote_address());
    console_->Printf("Incoming call from '%s'", jid.Str().c_str());
    call_ = call;
	session_ = session;
	incoming_call_ = true;
  } else if (state == cricket::Session::STATE_SENTINITIATE) {
    console_->Print("calling...");
  } else if (state == cricket::Session::STATE_RECEIVEDACCEPT) {
    console_->Print("call answered");
  } else if (state == cricket::Session::STATE_RECEIVEDREJECT) {
    console_->Print("call not answered");
  } else if (state == cricket::Session::STATE_INPROGRESS) {
    console_->Print("call in progress");
  } else if (state == cricket::Session::STATE_RECEIVEDTERMINATE) {
    console_->Print("other side hung up");
  }
 }

void CallClient::OnSendStanza(cricket::SessionClient *client, const buzz::XmlElement* stanza, bool is_response) {
  cricket::SessionSendTask* sender =
      new cricket::SessionSendTask(xmpp_client_, phone_client_);
  sender->Send(stanza);
  sender->Start();
}

void CallClient::InitPresence() {
  presence_push_ = new buzz::PresencePushTask(xmpp_client_);
  presence_push_->SignalStatusUpdate.connect(
    this, &CallClient::OnStatusUpdate);
  presence_push_->Start();

  buzz::Status my_status;
  my_status.set_jid(xmpp_client_->jid());
  my_status.set_available(true);
  my_status.set_invisible(false);
  my_status.set_show(buzz::Status::SHOW_ONLINE);
  my_status.set_priority(0);
  my_status.set_know_capabilities(true);
  my_status.set_phone_capability(true);
  my_status.set_is_google_client(true);
  my_status.set_version("1.0.0.66");

  buzz::PresenceOutTask* presence_out_ =
      new buzz::PresenceOutTask(xmpp_client_);
  presence_out_->Send(my_status);
  presence_out_->Start();
}

void CallClient::OnStatusUpdate(const buzz::Status& status) {
  RosterItem item;
  item.jid = status.jid();
  item.show = status.show();
  item.status = status.status();

  std::string key = item.jid.Str();

  if (status.available() && status.phone_capability()) {
     console_->Printf("Adding to roster: %s", key.c_str());
    (*roster_)[key] = item;
  } else {
    console_->Printf("Removing from roster: %s", key.c_str());
    RosterMap::iterator iter = roster_->find(key);
    if (iter != roster_->end())
      roster_->erase(iter);
  }
}

void CallClient::PrintRoster() {	
 console_->SetPrompting(false);
 console_->Printf("Roster contains %d callable", roster_->size());
 RosterMap::iterator iter = roster_->begin();
 while (iter != roster_->end()) {
   console_->Printf("%s - %s",
                    iter->second.jid.BareJid().Str().c_str(),
                    DescribeStatus(iter->second.show, iter->second.status));
    iter++;
  }
 console_->SetPrompting(true);
}

void CallClient::MakeCallTo(const std::string& name) {
  bool found = false;
  buzz::Jid found_jid;

  RosterMap::iterator iter = roster_->begin();
  while (iter != roster_->end()) {
    if (iter->second.jid.node() == name) {
      found = true;
      found_jid = iter->second.jid;
      break;
    }
    ++iter;
  }

  if (found) {
    console_->Printf("Found online friend '%s'", found_jid.Str().c_str());
    phone_client()->SignalCallDestroy.connect(
        this, &CallClient::OnCallDestroy);
    if (!call_) {
      call_ = phone_client()->CreateCall();
	  console_->SetPrompt(found_jid.Str().c_str());
      call_->SignalSessionState.connect(this, &CallClient::OnSessionState);
      session_ = call_->InitiateSession(found_jid);
    }
    phone_client()->SetFocus(call_);
  } else {
    console_->Printf("Could not find online friend '%s'", name.c_str());
  } 
}

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

#include <sstream>
#include <time.h>
#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/examples/call/presenceouttask.h"

namespace buzz {

// string helper functions -----------------------------------------------------
template <class T> static
bool FromString(const std::string& s,
                 T * t) {
  std::istringstream iss(s);
  return !(iss>>*t).fail();
}

template <class T> static
bool ToString(const T &t,
               std::string* s) {
  std::ostringstream oss;
  oss << t;
  *s = oss.str();
  return !oss.fail();
}

XmppReturnStatus
PresenceOutTask::Send(const Status & s) {
  if (GetState() != STATE_INIT)
    return XMPP_RETURN_BADSTATE;

  stanza_.reset(TranslateStatus(s));
  return XMPP_RETURN_OK;
}

XmppReturnStatus
PresenceOutTask::SendDirected(const Jid & j, const Status & s) {
  if (GetState() != STATE_INIT)
    return XMPP_RETURN_BADSTATE;

  XmlElement * presence = TranslateStatus(s);
  presence->AddAttr(QN_TO, j.Str());
  stanza_.reset(presence);
  return XMPP_RETURN_OK;
}

XmppReturnStatus PresenceOutTask::SendProbe(const Jid & jid) {
  if (GetState() != STATE_INIT)
    return XMPP_RETURN_BADSTATE;

  XmlElement * presence = new XmlElement(QN_PRESENCE);
  presence->AddAttr(QN_TO, jid.Str());
  presence->AddAttr(QN_TYPE, "probe");

  stanza_.reset(presence);
  return XMPP_RETURN_OK;
}

int
PresenceOutTask::ProcessStart() {
  if (SendStanza(stanza_.get()) != XMPP_RETURN_OK)
    return STATE_ERROR;
  return STATE_DONE;
}

XmlElement *
PresenceOutTask::TranslateStatus(const Status & s) {
  XmlElement * result = new XmlElement(QN_PRESENCE);
  if (!s.available()) {
    result->AddAttr(QN_TYPE, STR_UNAVAILABLE);
  }
  else {
    if (s.invisible()) {
      result->AddAttr(QN_TYPE, STR_INVISIBLE);
    }

    if (s.show() != Status::SHOW_ONLINE && s.show() != Status::SHOW_OFFLINE) {
      result->AddElement(new XmlElement(QN_SHOW));
      switch (s.show()) {
        default:
          result->AddText(STR_SHOW_AWAY, 1);
          break;
        case Status::SHOW_XA:
          result->AddText(STR_SHOW_XA, 1);
          break;
        case Status::SHOW_DND:
          result->AddText(STR_SHOW_DND, 1);
          break;
        case Status::SHOW_CHAT:
          result->AddText(STR_SHOW_CHAT, 1);
          break;
      }
    }

    result->AddElement(new XmlElement(QN_STATUS));
    result->AddText(s.status(), 1);

    std::string pri;
    ToString(s.priority(), &pri);

    result->AddElement(new XmlElement(QN_PRIORITY));
    result->AddText(pri, 1);

    if (s.know_capabilities() && s.is_google_client()) {
      result->AddElement(new XmlElement(QN_CAPS_C, true));
      result->AddAttr(QN_NODE, GOOGLE_CLIENT_NODE, 1);
      result->AddAttr(QN_VER, s.version(), 1);
      result->AddAttr(QN_EXT, s.phone_capability() ? "voice-v1" : "", 1);
    }

    // Put the delay mark on the presence according to JEP-0091
    {
      result->AddElement(new XmlElement(kQnDelayX, true));

      // This here is why we *love* the C runtime
      time_t current_time_seconds;
      time(&current_time_seconds);
      struct tm* current_time = gmtime(&current_time_seconds);
      char output[256];
      strftime(output, ARRAY_SIZE(output), "%Y%m%dT%H:%M:%S", current_time);
      result->AddAttr(kQnStamp, output, 1);
    }

  }

  return result;
}


}

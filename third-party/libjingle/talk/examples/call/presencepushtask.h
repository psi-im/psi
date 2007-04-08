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

#ifndef _PRESENCEPUSHTASK_H_
#define _PRESENCEPUSHTASK_H_

#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"
#include "talk/base/sigslot.h"
#include "talk/examples/call/status.h"

namespace buzz {

class PresencePushTask : public XmppTask {

public:
  PresencePushTask(Task * parent) : XmppTask(parent, XmppEngine::HL_TYPE) {}
  virtual int ProcessStart();
  sigslot::signal1<const Status &>SignalStatusUpdate;

protected:
  virtual bool HandleStanza(const XmlElement * stanza);
};

  
}

#endif

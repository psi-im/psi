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

#define _CRT_SECURE_NO_DEPRECATE 1

#include <cassert>
#include "talk/base/messagequeue.h"
#include "talk/base/stringutils.h"
#include "talk/examples/call/console.h"
#include "talk/examples/call/callclient.h"

Console::Console(cricket::Thread *thread, CallClient *client) : 
client_thread_(thread), client_(client), prompt_(std::string("call")),
prompting_(true) {}

void Console::StartConsole() {
  char input_buffer[64];
  for (;;) {
	fgets(input_buffer, sizeof(input_buffer), stdin);
	client_thread_->Post(this, MSG_INPUT, 
		            new cricket::TypedMessageData<std::string>(input_buffer));
  }
}

void Console::OnMessage(cricket::Message *msg) {
  switch (msg->message_id) {
    case MSG_START:
      StartConsole();
	  break;
	case MSG_INPUT:
	  cricket::TypedMessageData<std::string> *data = 
	    static_cast<cricket::TypedMessageData<std::string>*>(msg->pdata);
	  client_->ParseLine(data->data());
	  break;
  }
}

void Console::Print(const char* str) {
  printf("\n%s", str);
  if (prompting_)
    printf("\n(%s) ", prompt_.c_str());
}

void Console::Print(const std::string& str) {
  Print(str.c_str());
}

void Console::Printf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);

  char buf[4096];
  int size = vsnprintf(buf, sizeof(buf), format, ap);
  assert(size >= 0);
  assert(size < static_cast<int>(sizeof(buf)));
  buf[size] = '\0';
  Print(buf);

  va_end(ap);
}
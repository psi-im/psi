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

#ifndef CRICKET_EXAMPLES_CALL_CONSOLE_H__
#define CRICKET_EXAMPLES_CALL_CONSOLE_H__

#include "talk/base/thread.h"
#include "talk/base/messagequeue.h"

class CallClient;

enum {
	MSG_START,
	MSG_INPUT,
};

class Console : public cricket::MessageHandler {
 public:
  Console(cricket::Thread *thread, CallClient *client);
  virtual void OnMessage(cricket::Message *msg);
  void SetPrompt(const char *prompt) {
	  prompt_ = prompt ? std::string(prompt) : std::string("call");
  }
  void SetPrompting(bool prompting) {
    prompting_ = prompting;
    if (prompting)
	  printf("\n(%s) ", prompt_.c_str());
    }
  bool prompting() {return prompting_;}

  void Print(const char* str);
  void Print(const std::string& str);
  void Printf(const char* format, ...);
 private:
  CallClient *client_;
  cricket::Thread *client_thread_;
  void StartConsole();
  void ParseLine(std::string &str);
  std::string prompt_;
  bool prompting_;
};

#endif // CRICKET_EXAMPLES_CALL_CONSOLE_H__
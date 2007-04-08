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

#include "talk/base/physicalsocketserver.h"
#include "talk/base/ssladapter.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/examples/login/xmppthread.h"
#include "talk/examples/login/xmppauth.h"
#include "talk/examples/call/callclient.h"
#include "talk/examples/call/console.h"

class InsecurePasswordImpl : public buzz::XmppPasswordImpl {
 public:
  std::string& password() { return password_; }
  const std::string& password() const { return password_; }

  virtual ~InsecurePasswordImpl() {}
  virtual size_t GetLength() const { return password_.size(); }
  virtual void CopyTo(char * dest, bool nullterminate) const {
    memcpy(dest, password_.data(), password_.size());
    if (nullterminate) dest[password_.size()] = 0;
  }
  virtual std::string UrlEncode() const { return password_; }
  virtual XmppPasswordImpl * Copy() const {
    InsecurePasswordImpl * copy = new InsecurePasswordImpl;
    copy->password() = password_;
    return copy;
  }
 private:
  std::string password_;
};

void SetConsoleEcho(bool on) {
#ifdef WIN32
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  if ((hIn == INVALID_HANDLE_VALUE) || (hIn == NULL))
    return;

  DWORD mode;
  if (!GetConsoleMode(hIn, &mode))
    return;

  if (on) {
    mode = mode | ENABLE_ECHO_INPUT;
  } else {
    mode = mode & ~ENABLE_ECHO_INPUT;
  }

  SetConsoleMode(hIn, mode);
#else
  if (on)
    system("stty echo");
  else
    system("stty -echo");
#endif
}


int main(int argc, char **argv) {
  // This app has three threads. The main thread will run the XMPP client, 
  // which will print to the screen in its own thread. A second thread 
  // will get input from the console, parse it, and pass the appropriate
  // message back to the XMPP client's thread. A third thread is used
  // by PhoneSessionClient as its worker thread.
  InitializeSSL();   
  XmppPump pump;

  buzz::XmppClientSettings xcs;
  InsecurePasswordImpl pass;
  std::string username;

  std::cout << "Username: ";
  std::cin >> username;
  SetConsoleEcho(false);
  std::cout << "Password: ";
  std::cin >> pass.password();
  SetConsoleEcho(true);
  std::cout << std::endl;

  xcs.set_user(username);
  xcs.set_host("gmail.com");
  xcs.set_use_tls(true);
  xcs.set_pass(buzz::XmppPassword(pass));
  xcs.set_server(cricket::SocketAddress("talk.google.com", 5222));
  printf("Logging in as %s@gmail.com\n", username.c_str());

  cricket::PhysicalSocketServer ss;

  CallClient *client = new CallClient(pump.client());
  
  cricket::Thread main_thread(&ss);
  cricket::ThreadManager::SetCurrent(&main_thread);
  Console *console = new Console(&main_thread, client);
  client->SetConsole(console);
  cricket::Thread *console_thread = new cricket::Thread(&ss);
  console_thread->Start();
  console_thread->Post(console, MSG_START);

  pump.DoLogin(xcs, new XmppSocket(true), 0);

  main_thread.Loop();

  return 0;
}

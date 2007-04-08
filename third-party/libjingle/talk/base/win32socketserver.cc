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

#include "talk/base/common.h"
#include "win32socketserver.h"

namespace cricket {

#define WM_WAKEUP (WM_USER+0)

LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT wm, WPARAM wp, LPARAM lp);

// A socket server that provides cricket base services on top of a win32 gui thread

Win32SocketServer::Win32SocketServer(MessageQueue *message_queue) {
  message_queue_ = message_queue;
  hwnd_ = NULL;
  CreateDummyWindow();
}

Win32SocketServer::~Win32SocketServer() {
  if (hwnd_ != NULL) {
    KillTimer(hwnd_, 1);
    ::DestroyWindow(hwnd_);
  }
}

bool Win32SocketServer::Wait(int cms, bool process_io) {
  ASSERT(!process_io || (cms == 0));  // Should only be used for Thread::Send, or in Pump, below
  if (cms == -1) {
    MSG msg;
    GetMessage(&msg, NULL, WM_WAKEUP, WM_WAKEUP);
  } else if (cms != 0) {
    Sleep(cms);
  }
  return true;
}

void Win32SocketServer::WakeUp() {
  // Always post for every wakeup, so there are no
  // critical sections
  if (hwnd_ != NULL)
    PostMessage(hwnd_, WM_WAKEUP, 0, 0);
}

void Win32SocketServer::Pump() {
  // Process messages
  Message msg;
  while (message_queue_->Get(&msg, 0))
    message_queue_->Dispatch(&msg);

  // Anything remaining?
  int delay = message_queue_->GetDelay();
  if (delay == -1) {
    KillTimer(hwnd_, 1);
  } else {
    SetTimer(hwnd_, 1, delay, NULL);
  }
}

void Win32SocketServer::CreateDummyWindow()
{
  static bool s_registered;
  if (!s_registered) {
    ::WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbWndExtra = sizeof(this);
    wc.lpszClassName = L"Dummy";
    wc.lpfnWndProc = DummyWndProc;
    ::RegisterClassW(&wc);
    s_registered = true;
  }

  hwnd_ = ::CreateWindowW(L"Dummy", L"", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
  SetWindowLong(hwnd_, GWL_USERDATA, (LONG)(LONG_PTR)this);
}

LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT wm, WPARAM wp, LPARAM lp)
{
  if (wm == WM_WAKEUP || (wm == WM_TIMER && wp == 1)) {
    Win32SocketServer *ss = (Win32SocketServer *)(LONG_PTR)GetWindowLong(hwnd, GWL_USERDATA);
    ss->Pump();
    return 0;
  }
  return ::DefWindowProc(hwnd, wm, wp, lp);
}

}

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

#ifdef WIN32
#include <windows.h>
#define snprintf _snprintf
#undef ERROR  // wingdi.h
#endif

#include <iostream>
#include <iomanip>

#include "talk/base/logging.h"
#include "talk/base/stream.h"
#include "talk/base/time.h"

/////////////////////////////////////////////////////////////////////////////
// Constant Labels
/////////////////////////////////////////////////////////////////////////////

const char * FindLabel(int value, const ConstantLabel entries[]) {
  for (int i=0; entries[i].label; ++i) {
    if (value == entries[i].value) {
      return entries[i].label;
    }
  }
  return 0;
}

std::string ErrorName(int err, const ConstantLabel * err_table) {
	const char * value = 0;
	if (err == 0)
		return "No error";

  if (err_table != 0) {
    if (const char * value = FindLabel(err, err_table))
      return value;
  }
  
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "0x%08lx", err);  
  return buffer;
}

/////////////////////////////////////////////////////////////////////////////

#if _DEBUG
const int LOG_DEFAULT = LS_INFO;
#else  // !_DEBUG
const int LOG_DEFAULT = LogMessage::NO_LOGGING;
#endif  // !_DEBUG

// By default, release builds don't log, debug builds at info level
int LogMessage::min_sev_ = LOG_DEFAULT;
int LogMessage::dbg_sev_ = LOG_DEFAULT;

// No file logging by default
int LogMessage::stream_sev_ = NO_LOGGING;

// Don't bother printing context for the ubiquitous INFO log messages
int LogMessage::ctx_sev_ = LS_WARNING;

// stream_ defaults to NULL
// Note: we explicitly do not clean this up, because of the uncertain ordering
// of destructors at program exit.  Let the person who sets the stream trigger
// cleanup by setting to NULL, or let it leak (safe at program exit).
StreamInterface * LogMessage::stream_;

// Boolean options default to false (0)
bool LogMessage::thread_, LogMessage::timestamp_;

// Close to program execution time
uint32 LogMessage::start_ = cricket::Time();

LogMessage::LogMessage(const char* file, int line, LoggingSeverity sev,
                       LogErrorContext err_ctx, int err, const char* module)
    : severity_(sev) {
  if (timestamp_) {
    uint32 time = cricket::Time() - start_;
    print_stream_ << "[" << std::setfill('0') << std::setw(3) << (time / 1000)
                  << ":" << std::setw(3) << (time % 1000) << std::setfill(' ')
                  << "] ";
  }

  if (thread_) {
#ifdef WIN32
    DWORD id = GetCurrentThreadId();
    print_stream_ << "[" << std::hex << id << std::dec << "] ";
#endif  // WIN32
  }

  if (severity_ >= ctx_sev_) {
    print_stream_ << Describe(sev) << "(" << DescribeFile(file)
                  << ":" << line << "): ";
  }

  if (err_ctx != ERRCTX_NONE) {
    std::ostringstream tmp;
    tmp << "[0x" << std::setfill('0') << std::hex << std::setw(8) << err << "]";
    switch (err_ctx) {
      case ERRCTX_ERRNO:
        tmp << " " << strerror(err);
        break;
  #ifdef WIN32
      case ERRCTX_HRESULT: {
        char msgbuf[256];
        DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM;
        HMODULE hmod = GetModuleHandleA(module);
        if (hmod)
          flags |= FORMAT_MESSAGE_FROM_HMODULE;
        if (DWORD len = FormatMessageA(
            flags, hmod, err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            msgbuf, sizeof(msgbuf) / sizeof(msgbuf[0]), NULL)) {
          while ((len > 0) && isspace(msgbuf[len-1]))
            msgbuf[--len] = 0;
          tmp << " " << msgbuf;
        }
        break; }
  #endif  // WIN32
      default:
        break;
    }
    extra_ = tmp.str();
  }
}

LogMessage::~LogMessage() {
  if (!extra_.empty())
    print_stream_ << " : " << extra_;
  print_stream_ << std::endl;
  const std::string& str = print_stream_.str();

  if (severity_ >= dbg_sev_) {
    bool log_to_stderr = true;
#ifdef WIN32
    static bool debugger_present = (IsDebuggerPresent() != FALSE);
    if (debugger_present) {
      log_to_stderr = false;
      OutputDebugStringA(str.c_str());
    }
#endif  // WIN32
    if (log_to_stderr) {
      std::cerr << str;
      std::cerr.flush();
    }
  }

  if (severity_ >= stream_sev_) {
    // If write isn't fully successful, what are we going to do, log it? :)
    stream_->WriteAll(str.data(), str.size(), NULL, NULL);
  }
}

void LogMessage::LogContext(int min_sev) {
  ctx_sev_ = min_sev;
}

void LogMessage::LogThreads(bool on) {
  thread_ = on;
}

void LogMessage::LogTimestamps(bool on) {
  timestamp_ = on;
}

void LogMessage::ResetTimestamps() {
  start_ = cricket::Time();
}

void LogMessage::LogToDebug(int min_sev) {
  dbg_sev_ = min_sev;
  min_sev_ = cricket::_min(dbg_sev_, stream_sev_);
}

void LogMessage::LogToStream(StreamInterface* stream, int min_sev) {
  delete stream_;
  stream_ = stream;
  stream_sev_ = (stream_ == 0) ? NO_LOGGING : min_sev;
  min_sev_ = cricket::_min(dbg_sev_, stream_sev_);
}

const char* LogMessage::Describe(LoggingSeverity sev) {
  switch (sev) {
  case LS_SENSITIVE: return "Sensitive";
  case LS_VERBOSE:   return "Verbose";
  case LS_INFO:      return "Info";
  case LS_WARNING:   return "Warning";
  case LS_ERROR:     return "Error";
  default:           return "<unknown>";
  }
}

const char* LogMessage::DescribeFile(const char* file) {
  const char* end1 = ::strrchr(file, '/');
  const char* end2 = ::strrchr(file, '\\');
  if (!end1 && !end2)
    return file;
  else
    return (end1 > end2) ? end1 + 1 : end2 + 1;
}

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

//   LOG(...) an ostream target that can be used to send formatted
// output to a variety of logging targets, such as debugger console, stderr,
// file, or any StreamInterface.
//   The severity level passed as the first argument to the the LOGging
// functions is used as a filter, to limit the verbosity of the logging.
//   Static members of LogMessage documented below are used to control the
// verbosity and target of the output.
//   There are several variations on the LOG macro which facilitate logging
// of common error conditions, detailed below.

#ifndef TALK_BASE_LOGGING_H__
#define TALK_BASE_LOGGING_H__

#include <sstream>
#include "talk/base/basictypes.h"
class StreamInterface;

///////////////////////////////////////////////////////////////////////////////
// ConstantLabel can be used to easily generate string names from constant
// values.  This can be useful for logging descriptive names of error messages.
// Usage:
//   const ConstantLabel LIBRARY_ERRORS[] = {
//     KLABEL(SOME_ERROR),
//     KLABEL(SOME_OTHER_ERROR),
//     ...
//     LASTLABEL
//   }
//
//   int err = LibraryFunc();
//   LOG(LS_ERROR) << "LibraryFunc returned: "
//                 << ErrorName(err, LIBRARY_ERRORS);

struct ConstantLabel { int value; const char * label; };
#define KLABEL(x) { x, #x }
#define TLABEL(x,y) { x, y }
#define LASTLABEL { 0, 0 }

const char * FindLabel(int value, const ConstantLabel entries[]);
std::string ErrorName(int err, const ConstantLabel * err_table);

//////////////////////////////////////////////////////////////////////

// Note that the non-standard LoggingSeverity aliases exist because they are
// still in broad use.  The meanings of the levels are:
//  LS_SENSITIVE: Information which should only be logged with the consent
//   of the user, due to privacy concerns.
//  LS_VERBOSE: This level is for data which we do not want to appear in the
//   normal debug log, but should appear in diagnostic logs.
//  LS_INFO: Chatty level used in debugging for all sorts of things, the default
//   in debug builds.
//  LS_WARNING: Something that may warrant investigation.
//  LS_ERROR: Something that should not have occurred.
enum LoggingSeverity { LS_SENSITIVE, LS_VERBOSE, LS_INFO, LS_WARNING, LS_ERROR,
                       INFO = LS_INFO,
                       WARNING = LS_WARNING,
                       LERROR = LS_ERROR };

// LogErrorContext assists in interpreting the meaning of an error value.
//  ERRCTX_ERRNO: the value was read from global 'errno'
//  ERRCTX_HRESULT: the value is a Windows HRESULT
enum LogErrorContext { ERRCTX_NONE, ERRCTX_ERRNO, ERRCTX_HRESULT };

// If LOGGING is not explicitly defined, default to enabled in debug mode
#if !defined(LOGGING)
#if defined(_DEBUG) && !defined(NDEBUG)
#define LOGGING 1
#else
#define LOGGING 0
#endif
#endif  // !defined(LOGGING)

#if LOGGING

#define LOG(sev) \
  if (LogMessage::Loggable(sev)) \
    LogMessage(__FILE__, __LINE__, sev).stream()

// PLOG and LOG_ERR attempt to provide a string description of an errno derived
// error.  LOG_ERR reads errno directly, so care must be taken to call it before
// errno is reset.
#define PLOG(sev, err) \
  if (LogMessage::Loggable(sev)) \
    LogMessage(__FILE__, __LINE__, sev, ERRCTX_ERRNO, err).stream()
#define LOG_ERR(sev) \
  if (LogMessage::Loggable(sev)) \
    LogMessage(__FILE__, __LINE__, sev, ERRCTX_ERRNO, errno).stream()

// LOG_GLE(M) attempt to provide a string description of the HRESULT returned
// by GetLastError.  The second variant allows searching of a dll's string
// table for the error description.
#ifdef WIN32
#define LOG_GLE(sev) \
  if (LogMessage::Loggable(sev)) \
    LogMessage(__FILE__, __LINE__, sev, ERRCTX_HRESULT, GetLastError()).stream()
#define LOG_GLEM(sev, mod) \
  if (LogMessage::Loggable(sev)) \
    LogMessage(__FILE__, __LINE__, sev, ERRCTX_HRESULT, GetLastError(), mod) \
      .stream()
#endif  // WIN32

// TODO: Add an "assert" wrapper that logs in the same manner.

#else  // !LOGGING

// Hopefully, the compiler will optimize away some of this code.
// Note: syntax of "1 ? (void)0 : LogMessage" was causing errors in g++,
//   converted to "while (false)"
#define LOG(sev) \
  while (false) LogMessage(NULL, 0, sev).stream()
#define PLOG(sev, err) \
  while (false) LogMessage(NULL, 0, sev, ERRCTX_ERRNO, 0).stream()
#define LOG_ERR(sev) \
  while (false) LogMessage(NULL, 0, sev, ERRCTX_ERRNO, 0).stream()
#ifdef WIN32
#define LOG_GLE(sev) \
  while (false) LogMessage(NULL, 0, sev, ERRCTX_HRESULT, 0).stream()
#define LOG_GLEM(sev, mod) \
  while (false) LogMessage(NULL, 0, sev, ERRCTX_HRESULT, 0).stream()
#endif  // WIN32

#endif  // !LOGGING

class LogMessage {
 public:
  LogMessage(const char* file, int line, LoggingSeverity sev,
             LogErrorContext err_ctx = ERRCTX_NONE, int err = 0,
             const char* module = NULL);
  ~LogMessage();

  static inline bool Loggable(LoggingSeverity sev) { return (sev >= min_sev_); }
  std::ostream& stream() { return print_stream_; }

  enum { NO_LOGGING = LS_ERROR + 1 };

  // These are attributes which apply to all logging channels
  //  LogContext: Display the file and line number of the message
  static void LogContext(int min_sev);
  //  LogThreads: Display the thread identifier of the current thread
  static void LogThreads(bool on = true);
  //  LogTimestamps: Display the elapsed time of the program
  static void LogTimestamps(bool on = true);

  // Timestamps begin with program execution, but can be reset with this
  // function for measuring the duration of an activity, or to synchronize
  // timestamps between multiple instances.
  static void ResetTimestamps();

  // These are the available logging channels
  //  Debug: Debug console on Windows, otherwise stderr
  static void LogToDebug(int min_sev);
  static int GetLogToDebug() { return dbg_sev_; }
  //  Stream: Any non-blocking stream interface.  LogMessage takes ownership of
  //   the stream.
  static void LogToStream(StreamInterface* stream, int min_sev);
  static int GetLogToStream() { return stream_sev_; }

  // Testing against MinLogSeverity allows code to avoid potentially expensive
  // logging operations by pre-checking the logging level.
  static int GetMinLogSeverity() { return min_sev_; }

 private:
  // These assist in formatting some parts of the debug output.
  static const char* Describe(LoggingSeverity sev);
  static const char* DescribeFile(const char* file);

  // The ostream that buffers the formatted message before output
  std::ostringstream print_stream_;

  // The severity level of this message
  LoggingSeverity severity_;

  // String data generated in the constructor, that should be appended to
  // the message before output.
  std::string extra_;

  // dbg_sev_ and stream_sev_ are the thresholds for those output targets
  // min_sev_ is the minimum (most verbose) of those levels, and is used
  //  as a short-circuit in the logging macros to identify messages that won't
  //  be logged.
  // ctx_sev_ is the minimum level at which file context is displayed
  static int min_sev_, dbg_sev_, stream_sev_, ctx_sev_;

  // The output stream, if any
  static StreamInterface * stream_;

  // Flags for formatting options
  static bool thread_, timestamp_;

  // The timestamp at which logging started.
  static uint32 start_;

  DISALLOW_EVIL_CONSTRUCTORS(LogMessage);
};

#endif  // TALK_BASE_LOGGING_H__

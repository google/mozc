// Copyright 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZC_BASE_LOGGING_H_
#define MOZC_BASE_LOGGING_H_

#ifdef OS_WINDOWS
#include <windows.h>
// windows.h defines ERROR as 0 somewhere!
#endif

#undef INFO
#undef WARNING
#undef ERROR
#undef FATAL

#include <stdlib.h>

#include <iostream>
#include <string>

namespace mozc {

enum LogSeverity {
  LOG_INFO = 0,
  LOG_WARNING = 1,
  LOG_ERROR = 2,
  LOG_FATAL = 3,
  LOG_SEVERITY_SIZE = 4,
};

class NullLogStream;

class Logging {
 public:
  // Initialize log stream. argv0 is the program name commonly
  // storead as argv[0].
  // The default log file is <USER_PROFILE>/<program_name>.log
  static void           InitLogStream(const char *argv0 = "UNKNOWN");

  // Close the logging stream
  static void           CloseLogStream();

  // Get logging stream. The log message can be written to the stream
  static ostream       &GetLogStream();

  // Get NullLogStream for NO_LOGGING mode
  static NullLogStream &GetNullLogStream();

  // Convert LogSeverity to the string name
  static const char    *GetLogSeverityName(LogSeverity severity);

  // return "YYYY-MM-DD HH:MM:SS PID TID", e.g. "2008 11-16 19:40:21 100 20"
  static string         GetLogMessageHeader();

  // return FLAGS_v
  static int            GetVerboseLevel();

  // set FLAGS_v
  static void           SetVerboseLevel(int verboselevel);

  // set Verbose Level for Config.
  // Since Config dialog will overwrite -v option, we separate
  // config_verbose_level and FLAGS_v.
  // real_config_level = max(FLAGS_v, config_verbose_level);
  static void           SetConfigVerboseLevel(int verboselevel);
};

class LogFinalizer {
 public:
  explicit LogFinalizer(LogSeverity severity)
      : severity_(severity) {}

  ~LogFinalizer() {
    mozc::Logging::GetLogStream() << endl;
    if (severity_ >= LOG_FATAL) {
      // On windows, call exception handler to
      // make stack trace and minidump
#ifdef OS_WINDOWS
      ::RaiseException(::GetLastError(),
                       EXCEPTION_NONCONTINUABLE,
                       NULL, NULL);
#else
      mozc::Logging::CloseLogStream();
      exit(-1);
#endif
    }
  }

  void operator&(ostream&) {}

 public:
  LogSeverity  severity_;
};

// When using NullLogStream, all debug message will be stripped
class NullLogStream {
 public:
  template <typename T>
  NullLogStream& operator<<(const T &value) {
    return *this;
  }
  NullLogStream& operator<<(ostream& (*pfunc)(ostream&) ) {
    return *this;
  }
};

class NullLogFinalizer {
 public:
  explicit NullLogFinalizer(LogSeverity severity)
      : severity_(severity) {}
  ~NullLogFinalizer() {
    if (severity_ >= LOG_FATAL) {
#ifdef OS_WINDOWS
      ::RaiseException(::GetLastError(),
                       EXCEPTION_NONCONTINUABLE,
                       NULL, NULL);
#else
      exit(-1);
#endif
    }
  }

  void operator&(NullLogStream&) {}

 private:
  LogSeverity severity_;
};
}  // mozc

// ad-hoc porting of google-glog
#ifdef NO_LOGGING   // don't use logging feature.

#define LOG(severity) \
  mozc::NullLogFinalizer(mozc::LOG_##severity) & \
  mozc::Logging::GetNullLogStream()

// To suppress the "statement has no effect" waring, (void) is
// inserted.  This technique is suggested by the gcc manual
// -Wunused-variable section.
#define LOG_IF(severity, condition) \
  (!(condition)) ? (void) 0 : \
  mozc::NullLogFinalizer(mozc::LOG_##severity) & \
  mozc::Logging::GetNullLogStream()

#define CHECK(condition) \
  (condition) ? (void) 0 : \
  mozc::NullLogFinalizer(mozc::LOG_FATAL) & mozc::Logging::GetNullLogStream()

#else   // NO_LOGGING

#define LOG(severity) \
  mozc::LogFinalizer(mozc::LOG_##severity) & mozc::Logging::GetLogStream() \
  << mozc::Logging::GetLogMessageHeader() \
  << " LOG(" << mozc::Logging::GetLogSeverityName(mozc::LOG_##severity) << ") " \
  << __FILE__ << "(" << __LINE__ << ") "

#define LOG_IF(severity, condition) \
  (!(condition)) ? (void) 0 : \
  mozc::LogFinalizer(mozc::LOG_##severity) & mozc::Logging::GetLogStream() \
  << mozc::Logging::GetLogMessageHeader() \
  << " LOG(" << mozc::Logging::GetLogSeverityName(mozc::LOG_##severity) << ") " \
  << __FILE__ << "(" << __LINE__ << ") [" << #condition << "] "

#define CHECK(condition) \
  (condition) ? (void) 0 : \
  mozc::LogFinalizer(mozc::LOG_FATAL) & \
  mozc::Logging::GetLogStream() << mozc::Logging::GetLogMessageHeader() \
  << " CHECK " << __FILE__ << "(" << __LINE__ << ") [" << #condition << "] "

#endif  // end NO_LOGGING

#define VLOG_IS_ON(verboselevel) \
(mozc::Logging::GetVerboseLevel() >= verboselevel)

#define VLOG(verboselevel) LOG_IF(INFO, VLOG_IS_ON(verboselevel))

#define VLOG_IF(verboselevel, condition) \
  LOG_IF(INFO, ((condition) && VLOG_IS_ON(verboselevel)))

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))
#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_LT(a, b) CHECK((a) < (b))

// Debug build
#if defined(DEBUG) || defined(_DEBUG)

#define DLOG(severity) LOG(severity)
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#define DCHECK(condition) CHECK(condition)
#define DCHECK_EQ(a, b) CHECK_EQ(a, b)
#define DCHECK_NE(a, b) CHECK_NE(a, b)
#define DCHECK_GE(a, b) CHECK_GE(a, b)
#define DCHECK_LE(a, b) CHECK_LE(a, b)
#define DCHECK_GT(a, b) CHECK_GT(a, b)
#define DCHECK_LT(a, b) CHECK_LT(a, b)

#else   // opt build

#define DLOG(severity) \
  mozc::NullLogFinalizer(mozc::LOG_##severity) & \
  mozc::Logging::GetNullLogStream()

#define DLOG_IF(severity, condition) \
  (true || !(condition)) ? (void) 0 : \
  mozc::NullLogFinalizer(mozc::LOG_##severity)  \
  & mozc::Logging::GetNullLogStream()

#define DCHECK(condition) while (false) CHECK(condition)
#define DCHECK_EQ(a, b) while (false) CHECK_EQ(a, b)
#define DCHECK_NE(a, b) while (false) CHECK_NE(a, b)
#define DCHECK_GE(a, b) while (false) CHECK_GE(a, b)
#define DCHECK_LE(a, b) while (false) CHECK_LE(a, b)
#define DCHECK_GT(a, b) while (false) CHECK_GT(a, b)
#define DCHECK_LT(a, b) while (false) CHECK_LT(a, b)

#endif  // DEBUG
#endif  // MOZC_BASE_LOGGING_H_

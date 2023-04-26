// Copyright 2010-2021, Google Inc.
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

#include "base/logging.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif  // __ANDROID__

#ifdef _WIN32
#define NO_MINMAX
#include <windows.h>
#else  // _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif  // _WIN32

#include <algorithm>
#ifdef _WIN32
#include <codecvt>
#endif  // _WIN32
#include <cstdlib>
#include <cstring>
#include <fstream>
#ifdef _WIN32
#include <locale>
#endif  // _WIN32
#include <memory>
#include <sstream>
#include <string>

#ifdef __ANDROID__
#include "base/const.h"
#endif  // __ANDROID__
#include "base/clock.h"
#include "base/singleton.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"

ABSL_FLAG(bool, colored_log, true,
          "Enables colored log messages on tty devices");
ABSL_FLAG(bool, logtostderr, false,
          "log messages go to stderr instead of logfiles")
    .OnUpdate([] {
      mozc::Logging::SetLogToStderr(absl::GetFlag(FLAGS_logtostderr));
    });
ABSL_FLAG(int32_t, v, 0, "verbose level");

namespace mozc {

#ifdef __ANDROID__
namespace {
// In order to make logging.h independent from <android/log.h>, we use the
// raw number to define the following constants. Check the equality here
// just in case.
#define COMPARE_LOG_LEVEL(mozc_log_level, android_log_level)                   \
  static_assert(                                                               \
      static_cast<int>(mozc_log_level) == static_cast<int>(android_log_level), \
      "Checking Android log level constants.")
COMPARE_LOG_LEVEL(LOG_UNKNOWN, ANDROID_LOG_UNKNOWN);
COMPARE_LOG_LEVEL(LOG_DEFAULT, ANDROID_LOG_DEFAULT);
COMPARE_LOG_LEVEL(LOG_VERBOSE, ANDROID_LOG_VERBOSE);
COMPARE_LOG_LEVEL(LOG_DEBUG, ANDROID_LOG_DEBUG);
COMPARE_LOG_LEVEL(LOG_INFO, ANDROID_LOG_INFO);
COMPARE_LOG_LEVEL(LOG_WARNING, ANDROID_LOG_WARN);
COMPARE_LOG_LEVEL(LOG_ERROR, ANDROID_LOG_ERROR);
COMPARE_LOG_LEVEL(LOG_FATAL, ANDROID_LOG_FATAL);
COMPARE_LOG_LEVEL(LOG_SILENT, ANDROID_LOG_SILENT);
#undef COMPARE_LOG_LEVEL
}  // namespace
#endif  // __ANDROID__

// Use the same implementation both for Opt and Debug.
std::string Logging::GetLogMessageHeader() {
#ifdef __ANDROID__
  // On Android, other records are not needed because they are added by
  // Android's logging framework.
  return absl::StrCat(pthread_self(), " ");  // returns unsigned long.

#else  // __ANDROID__

  const absl::Time at = Clock::GetAbslTime();
  const absl::TimeZone tz = Clock::GetTimeZone();
  const std::string timestamp = absl::FormatTime("%Y-%m-%d %H:%M:%S ", at, tz);

#if defined(__wasm__)
  return absl::StrCat(timestamp, ::getpid(), " ",
                      static_cast<unsigned int>(pthread_self()));
#elif defined(__linux__)
  return absl::StrCat(timestamp, ::getpid(), " ",
                      // It returns unsigned long.
                      pthread_self());
#elif defined(__APPLE__)
#ifdef __LP64__
  return absl::StrCat(timestamp, ::getpid(), " ",
                      reinterpret_cast<uint64_t>(pthread_self()));
#else   // __LP64__
  return absl::StrCat(timestamp, ::getpid(), " ", ::getpid(),
                      reinterpret_cast<uint32_t>(pthread_self()));
#endif  // __LP64__
#elif defined(_WIN32)
  return absl::StrCat(timestamp, ::GetCurrentProcessId(), " ",
                      ::GetCurrentThreadId());
#endif  // _WIN32
#endif  // __ANDROID__
}

#ifdef MOZC_NO_LOGGING

void Logging::InitLogStream(const std::string &log_file_path) {}

void Logging::CloseLogStream() {}

std::ostream &Logging::GetWorkingLogStream() {
  // Never called.
  return *(new std::ostringstream);
}

void Logging::FinalizeWorkingLogStream(LogSeverity severity,
                                       std::ostream *working_stream) {
  // Never called.
  delete working_stream;
}

NullLogStream &Logging::GetNullLogStream() {
  return *(Singleton<NullLogStream>::get());
}

const char *Logging::GetLogSeverityName(LogSeverity severity) { return ""; }

const char *Logging::GetBeginColorEscapeSequence(LogSeverity severity) {
  return "";
}

const char *Logging::GetEndColorEscapeSequence() { return ""; }

int Logging::GetVerboseLevel() { return 0; }

void Logging::SetVerboseLevel(int verboselevel) {}

void Logging::SetConfigVerboseLevel(int verboselevel) {}

void Logging::SetLogToStderr(bool log_to_stderr) {}

#else  // MOZC_NO_LOGGING

namespace {

class LogStreamImpl {
 public:
  LogStreamImpl();
  ~LogStreamImpl();

  void Init(const std::string &log_file_path);
  void Reset();

  int verbose_level() const {
    return std::max(absl::GetFlag(FLAGS_v), config_verbose_level_);
  }

  void set_verbose_level(int level) {
    absl::MutexLock l(&mutex_);
    absl::SetFlag(&FLAGS_v, level);
  }

  void set_config_verbose_level(int level) {
    absl::MutexLock l(&mutex_);
    config_verbose_level_ = level;
  }

  bool support_color() const { return support_color_; }

  void Write(LogSeverity, const std::string &log);
  void set_log_to_stderr(bool log_to_stderr) {
#if defined(__ANDROID__)
    // Android uses Android's log library.
    use_cerr_ = false;
#else   // __ANDROID__
    absl::MutexLock l(&mutex_);
    use_cerr_ = log_to_stderr;
#endif  // __ANDROID__
  }

 private:
  void ResetUnlocked();

  // Real backing log stream.
  // This is not thread-safe so must be guarded.
  // If std::cerr is real log stream, this is empty.
  std::unique_ptr<std::ostream> real_log_stream_;
  int config_verbose_level_;
  bool support_color_ = false;
  bool use_cerr_ = false;
  absl::Mutex mutex_;
};

void LogStreamImpl::Write(LogSeverity severity, const std::string &log) {
  absl::MutexLock l(&mutex_);
  if (use_cerr_) {
    std::cerr << log;
  } else {
#if defined(__ANDROID__)
    __android_log_write(severity, kProductPrefix,
                        const_cast<char *>(log.c_str()));
#else   // __ANDROID__
    // Since our logging mechanism is essentially singleton, it is indeed
    // possible that this method is called before |Logging::InitLogStream()|.
    // b/32360767 is an example, where |SystemUtil::GetLoggingDirectory()|
    // called as a preparation for |Logging::InitLogStream()| internally
    // triggered |LOG(ERROR)|.
    if (real_log_stream_) {
      *real_log_stream_ << log;
      real_log_stream_->flush();
    }
#endif  // __ANDROID__
  }
}

LogStreamImpl::LogStreamImpl() { Reset(); }

// Initializes real log stream.
// After initialization, use_cerr_ and real_log_stream_ become like following:
// OS,      --logtostderr => use_cerr_, real_log_stream_
// Android, *             => false,     empty
// Others,  true          => true,      empty
// Others,  false         => true,      initialized
void LogStreamImpl::Init(const std::string &log_file_path) {
  absl::MutexLock l(&mutex_);
  ResetUnlocked();

  if (use_cerr_) {
    return;
  }
#if defined(_WIN32)
  // On Windows, just create a stream.
  // Since Windows uses UTF-16 for internationalized file names, we should
  // convert the encoding of the given |log_file_path| from UTF-8 to UTF-16.
  // NOTE: To avoid circular dependency, |Util::Utf8ToWide| shouldn't be used
  // here.
  DCHECK_NE(log_file_path.size(), 0);
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_to_wide;
  real_log_stream_ = std::make_unique<std::ofstream>(
      utf8_to_wide.from_bytes(log_file_path).c_str(), std::ios::app);
#elif !defined(__ANDROID__)
  // On non-Android platform, change file mode in addition.
  // Android uses logcat instead of log file.
  DCHECK_NE(log_file_path.size(), 0);
  real_log_stream_ =
      std::make_unique<std::ofstream>(log_file_path.c_str(), std::ios::app);
  ::chmod(log_file_path.c_str(), 0600);
#endif  // __ANDROID__
  DCHECK(!use_cerr_ || !real_log_stream_);
}

void LogStreamImpl::Reset() {
  absl::MutexLock l(&mutex_);
  ResetUnlocked();
}

void LogStreamImpl::ResetUnlocked() {
  real_log_stream_.reset();
  config_verbose_level_ = 0;
#if defined(__ANDROID__) || defined(_WIN32)
  // On Android, the standard log library is used.
  // On Windows, coloring is disabled
  // because cmd.exe doesn't support ANSI color escape sequences.
  // TODO(team): Considers to use SetConsoleTextAttribute on Windows.
  support_color_ = false;
#else   // __ANDROID__, _WIN32
  support_color_ = (use_cerr_ && absl::GetFlag(FLAGS_colored_log) &&
                    ::isatty(::fileno(stderr)));
#endif  // __ANDROID__, _WIN32
  // use_cerr_ is updated by ABSL_FLAG.OnUpdate().
}

LogStreamImpl::~LogStreamImpl() { Reset(); }
}  // namespace

void Logging::InitLogStream(const std::string &log_file_path) {
  Singleton<LogStreamImpl>::get()->Init(log_file_path);
  std::ostream &stream = GetWorkingLogStream();
  stream << "Log file created at: " << Logging::GetLogMessageHeader();
  FinalizeWorkingLogStream(LogSeverity::LOG_INFO, &stream);
}

void Logging::CloseLogStream() { Singleton<LogStreamImpl>::get()->Reset(); }

std::ostream &Logging::GetWorkingLogStream() {
  return *(new std::ostringstream);
}

void Logging::FinalizeWorkingLogStream(LogSeverity severity,
                                       std::ostream *working_stream) {
  *working_stream << std::endl;
  Singleton<LogStreamImpl>::get()->Write(
      severity, static_cast<std::ostringstream *>(working_stream)->str());
  // The working stream is new'd in LogStreamImpl::GetWorkingLogStream().
  // Must be deleted by finalizer.
  delete working_stream;
}

NullLogStream &Logging::GetNullLogStream() {
  return *(Singleton<NullLogStream>::get());
}

namespace {
// ANSI Color escape sequences.
// FYI: Other escape sequences are here.
// Black:   "\x1b[30m"
// Green    "\x1b[32m"
// Blue:    "\x1b[34m"
// Magenta: "\x1b[35m"
// White    "\x1b[37m"
constexpr const char kClearEscapeSequence[] = "\x1b[0m";
constexpr const char kRedEscapeSequence[] = "\x1b[31m";
constexpr const char kYellowEscapeSequence[] = "\x1b[33m";
constexpr const char kCyanEscapeSequence[] = "\x1b[36m";

constexpr struct SeverityProperty {
 public:
  const char *label;
  const char *color_escape_sequence;
} kSeverityProperties[] = {
#ifdef __ANDROID__
    {"UNKNOWN", kCyanEscapeSequence}, {"DEFAULT", kCyanEscapeSequence},
    {"VERBOSE", kCyanEscapeSequence}, {"DEBUG", kCyanEscapeSequence},
    {"INFO", kCyanEscapeSequence},    {"WARNING", kYellowEscapeSequence},
    {"ERROR", kRedEscapeSequence},    {"FATAL", kRedEscapeSequence},
    {"SILENT", kCyanEscapeSequence},
#else   // __ANDROID__
    {"INFO", kCyanEscapeSequence},
    {"WARNING", kYellowEscapeSequence},
    {"ERROR", kRedEscapeSequence},
    {"FATAL", kRedEscapeSequence},
#endif  // __ANDROID__
};
}  // namespace

const char *Logging::GetLogSeverityName(LogSeverity severity) {
  return kSeverityProperties[severity].label;
}

const char *Logging::GetBeginColorEscapeSequence(LogSeverity severity) {
  if (Singleton<LogStreamImpl>::get()->support_color()) {
    return kSeverityProperties[severity].color_escape_sequence;
  }
  return "";
}

const char *Logging::GetEndColorEscapeSequence() {
  if (Singleton<LogStreamImpl>::get()->support_color()) {
    return kClearEscapeSequence;
  }
  return "";
}

int Logging::GetVerboseLevel() {
  return Singleton<LogStreamImpl>::get()->verbose_level();
}

void Logging::SetVerboseLevel(int verboselevel) {
  Singleton<LogStreamImpl>::get()->set_verbose_level(verboselevel);
}

void Logging::SetConfigVerboseLevel(int verboselevel) {
  Singleton<LogStreamImpl>::get()->set_config_verbose_level(verboselevel);
}

void Logging::SetLogToStderr(bool log_to_stderr) {
  Singleton<LogStreamImpl>::get()->set_log_to_stderr(log_to_stderr);
}
#endif  // MOZC_NO_LOGGING

LogFinalizer::LogFinalizer(LogSeverity severity) : severity_(severity) {}

LogFinalizer::~LogFinalizer() {
  Logging::FinalizeWorkingLogStream(severity_, working_stream_);
  if (severity_ >= LOG_FATAL) {
    // On windows, call exception handler to
    // make stack trace and minidump
#ifdef _WIN32
    ::RaiseException(::GetLastError(), EXCEPTION_NONCONTINUABLE, 0, nullptr);
#else   // _WIN32
    mozc::Logging::CloseLogStream();
    exit(-1);
#endif  // _WIN32
  }
}

void LogFinalizer::operator&(std::ostream &working_stream) {
  working_stream_ = &working_stream;
}

void NullLogFinalizer::OnFatal() {
#ifdef _WIN32
  ::RaiseException(::GetLastError(), EXCEPTION_NONCONTINUABLE, 0, nullptr);
#else   // _WIN32
  exit(-1);
#endif  // _WIN32
}

}  // namespace mozc

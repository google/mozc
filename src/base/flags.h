// Copyright 2010-2011, Google Inc.
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

// This is a simple port of google flags

#ifndef MOZC_BASE_FLAGS_H_
#define MOZC_BASE_FLAGS_H_


#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include "base/crash_report_handler.h"

#include <string>
#include "base/port.h"
#include "base/stats_config_util.h"

namespace mozc_flags {

enum { I, B, I64, U64, D, S };

struct Flag;

class FlagRegister {
 public:
  FlagRegister(const char *name,
               void *storage,
               const void *default_storage,
               int shorttpe,
               const char *help);
  virtual ~FlagRegister();
 private:
  Flag *flag_;
};

uint32 ParseCommandLineFlags(int *argc, char*** argv,
                             bool remove_flags);
}  // mozc_flags

namespace mozc {

extern void InitGoogleInternal(const char *arg0,
                               int *argc, char ***argv,
                               bool remove_flags);

namespace {
// We define InstallBreakpad() here,
// so that CrashReportHandler() is resovled in link time
inline void InstallBreakpad() {
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
#ifndef OS_LINUX
  if (StatsConfigUtil::IsEnabled()) {
    CrashReportHandler::Initialize(false);
  }
#else  // OS_LINUX
  // TODO(taku): install breakpad
#endif  // OS_LINUX
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
}
}  // namespace
}  // mozc

#ifdef OS_WINDOWS
namespace {
LONG CALLBACK ExitProcessExceptionFilter(EXCEPTION_POINTERS *ExceptionInfo) {
  // Currently, we haven't found good way to perform both
  // "send mininump" and "exit the process gracefully".
  ::ExitProcess(static_cast<UINT>(-1));
  return EXCEPTION_EXECUTE_HANDLER;
}
}  // namespace
#endif  // OS_WINDOWS

// class for holding argc, and argv
// This class just copies the pointer of argv given at
// the entory point
namespace mozc {
class Flags {
 public:
  static int argc;
  static char **argv;
};
}

namespace {
// not install breakpad
inline void InitGoogle(const char *arg0,
                       int *argc, char ***argv,
                       bool remove_flags) {
#ifdef OS_WINDOWS
  // InitGoogle() is supposed to be used for code generator or
  // other programs which are not included in the production code.
  // In these code, we don't want to show any error messages when
  // exceptions are raised. This is important to keep
  // our continuous build stable.
  ::SetUnhandledExceptionFilter(ExitProcessExceptionFilter);
#endif  // OS_WINDOWS

  mozc::InitGoogleInternal(arg0, argc, argv, remove_flags);
}

// install breakpad
inline void InitGoogleWithBreakPad(const char *arg0,
                                   int *argc, char ***argv,
                                   bool remove_flags) {
  mozc::InstallBreakpad();
  mozc::InitGoogleInternal(arg0, argc, argv, remove_flags);
}
}  // namespace

#define DEFINE_VARIABLE(type, shorttype, name, value, help) \
namespace fL##shorttype { \
  using namespace mozc_flags; \
  type FLAGS_##name = value; \
  static const type FLAGS_DEFAULT_##name = value; \
  static const mozc_flags::FlagRegister \
  fL##name(#name,                                                       \
           reinterpret_cast<void *>(&FLAGS_##name),                     \
           reinterpret_cast<const void *>(&FLAGS_DEFAULT_##name),       \
           shorttype, help);                                            \
} \
using fL##shorttype::FLAGS_##name

#define DECLARE_VARIABLE(type, shorttype, name)      \
namespace fL##shorttype {                            \
  extern type FLAGS_##name;                          \
}                                                    \
using fL##shorttype::FLAGS_##name

#define DEFINE_int32(name, value, help) \
DEFINE_VARIABLE(int32, I, name, value, help)
#define DECLARE_int32(name) \
DECLARE_VARIABLE(int32, I, name)

#define DEFINE_int64(name, value, help)  \
DEFINE_VARIABLE(int64, I64, name, value, help)
#define DECLARE_int64(name) \
DECLARE_VARIABLE(int64, I64, name)

#define DEFINE_uint64(name, value, help) \
DEFINE_VARIABLE(uint64, U64, name, value, help)
#define DECLARE_uint64(name) \
DECLARE_VARIABLE(uint64, U64, name)

#define DEFINE_double(name, value, help) \
DEFINE_VARIABLE(double, D, name, value, help)
#define DECLARE_double(name) \
DECLARE_VARIABLE(double, D, name)

#define DEFINE_bool(name, value, help)    \
DEFINE_VARIABLE(bool, B, name, value, help)
#define DECLARE_bool(name) \
DECLARE_VARIABLE(bool, B, name)

#define DEFINE_string(name, value, help)  \
DEFINE_VARIABLE(string, S, name, value, help)
#define DECLARE_string(name) \
DECLARE_VARIABLE(string, S, name)

#endif  // MOZC_BASE_FLAGS_H_

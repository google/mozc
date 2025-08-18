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

#include "base/init_mozc.h"

#include <string>

#include "absl/flags/flag.h"

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

#include "absl/flags/parse.h"
#include "base/file_util.h"
#include "base/log_file.h"

#ifndef MOZC_BUILDTOOL_BUILD
#include "base/system_util.h"
#endif  // MOZC_BUILDTOOL_BUILD

// Even if log_dir is modified in the middle of the process, the
// logging directory will not be changed because the logging stream is
// initialized in the very early initialization stage.
ABSL_FLAG(std::string, log_dir, "",
          "If specified, logfiles are written into this directory "
          "instead of the default logging directory.");

ABSL_RETIRED_FLAG(
    bool, logtostderr, false,
    "[Deprecated; no-op] Log messages only go to stderr, not log files.");

ABSL_RETIRED_FLAG(
    bool, colored_log, true,
    "[Deprecated; no-op] Enables colored log messages on tty devices");


ABSL_FLAG(std::string, program_invocation_name, "",
          "Program name copied from argv[0].");

namespace mozc {
namespace {

#ifdef _WIN32
LONG CALLBACK ExitProcessExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo) {
  // Currently, we haven't found a good way to perform both
  // "send mininump" and "exit the process gracefully".
  ::ExitProcess(static_cast<UINT>(-1));
  return EXCEPTION_EXECUTE_HANDLER;
}
#endif  // _WIN32

std::string GetLogFilePathFromProgramName(const std::string& program_name) {
  const std::string basename = FileUtil::Basename(program_name) + ".log";
  if (absl::GetFlag(FLAGS_log_dir).empty()) {
#ifdef MOZC_BUILDTOOL_BUILD
    return basename;
#else   // MOZC_BUILDTOOL_BUILD
    return FileUtil::JoinPath(SystemUtil::GetLoggingDirectory(), basename);
#endif  // !MOZC_BUILDTOOL_BUILD
  }
  return FileUtil::JoinPath(absl::GetFlag(FLAGS_log_dir), basename);
}

void ParseCommandLineFlags(int argc, char** argv) {
  absl::flags_internal::ParseCommandLineImpl(
      argc, argv,
      // Suppress help messages invoked by --help and others.
      // Use UsageFlagsAction::kHandleUsage to enable it.
      absl::flags_internal::UsageFlagsAction::kIgnoreUsage,
      absl::flags_internal::OnUndefinedFlag::kIgnoreUndefined);
}

}  // namespace

void InitMozc(const char* arg0, int* argc, char*** argv) {
  absl::SetFlag(&FLAGS_program_invocation_name, *argv[0]);
#ifdef _WIN32
  // InitMozc() is supposed to be used for code generator or
  // other programs which are not included in the production code.
  // In these code, we don't want to show any error messages when
  // exceptions are raised. This is important to keep
  // our continuous build stable.
  ::SetUnhandledExceptionFilter(ExitProcessExceptionFilter);
#endif  // _WIN32
  ParseCommandLineFlags(*argc, *argv);

  const std::string program_name = *argc > 0 ? (*argv)[0] : "UNKNOWN";
  RegisterLogFileSink(GetLogFilePathFromProgramName(program_name));
}

}  // namespace mozc

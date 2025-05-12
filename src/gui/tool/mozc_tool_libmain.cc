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

#include "gui/tool/mozc_tool_libmain.h"

#include <QtGui>
#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "base/init_mozc.h"
#include "base/run_level.h"
#include "gui/base/debug_util.h"

#ifdef __APPLE__
#include <cstdlib>
#ifndef IGNORE_INVALID_FLAG
#include <iostream>
#endif  // IGNORE_INVALID_FLAG

#include "base/const.h"
#include "base/environ.h"
#include "base/file_util.h"
#endif  // __APPLE__

#ifdef _WIN32
#include <windows.h>

#include "gui/base/win_util.h"
#endif  // _WIN32

ABSL_FLAG(std::string, mode, "about_dialog", "mozc_tool mode");
ABSL_DECLARE_FLAG(std::string, error_type);

// Run* are defined in each qt module
int RunAboutDialog(int argc, char *argv[]);
int RunConfigDialog(int argc, char *argv[]);
int RunDictionaryTool(int argc, char *argv[]);
int RunWordRegisterDialog(int argc, char *argv[]);
int RunErrorMessageDialog(int argc, char *argv[]);

#ifdef _WIN32
// (PostInstall|RunAdministartion)Dialog are used for Windows only.
int RunPostInstallDialog(int argc, char *argv[]);
int RunAdministrationDialog(int argc, char *argv[]);
#endif  // _WIN32

#ifdef __APPLE__
int RunPrelaunchProcesses(int argc, char *argv[]);
#endif  // __APPLE__

#ifdef __APPLE__
namespace {

void SetFlagsFromEnv() {
  const std::string mode = mozc::Environ::GetEnv("FLAGS_mode");
  if (!mode.empty()) {
    absl::SetFlag(&FLAGS_mode, mode);
  }

  const std::string error_type = mozc::Environ::GetEnv("FLAGS_error_type");
  if (!error_type.empty()) {
    absl::SetFlag(&FLAGS_error_type, error_type);
  }
}

}  // namespace
#endif  // __APPLE__

int RunMozcTool(int argc, char *argv[]) {
#ifdef __APPLE__
  // OSX's app won't accept command line flags.  Here we preset flags from
  // environment variables.
  SetFlagsFromEnv();
#endif  // __APPLE__
  mozc::InitMozc(argv[0], &argc, &argv);

#ifdef __APPLE__
  // In Mac, we shares the same binary but changes the application
  // name.
  std::string binary_name = mozc::FileUtil::Basename(argv[0]);
  if (binary_name == "AboutDialog") {
    absl::SetFlag(&FLAGS_mode, "about_dialog");
  } else if (binary_name == "ConfigDialog") {
    absl::SetFlag(&FLAGS_mode, "config_dialog");
  } else if (binary_name == "DictionaryTool") {
    absl::SetFlag(&FLAGS_mode, "dictionary_tool");
  } else if (binary_name == "ErrorMessageDialog") {
    absl::SetFlag(&FLAGS_mode, "error_message_dialog");
  } else if (binary_name == "WordRegisterDialog") {
    absl::SetFlag(&FLAGS_mode, "word_register_dialog");
  } else if (binary_name == kProductPrefix "Prelauncher") {
    // The binary name of prelauncher is user visible in
    // "System Preferences" -> "Accounts" -> "Login items".
    // So we set kProductPrefix to the binary name.
    absl::SetFlag(&FLAGS_mode, "prelauncher");
  }
#endif  // __APPLE__

  if (absl::GetFlag(FLAGS_mode) != "administration_dialog" &&
      !mozc::RunLevel::IsValidClientRunLevel()) {
    return -1;
  }

  // install Qt debug handler
  qInstallMessageHandler(mozc::gui::DebugUtil::MessageHandler);

#ifdef _WIN32
  // Update JumpList if available.
  mozc::gui::WinUtil::KeepJumpListUpToDate();
#endif  // _WIN32

  if (absl::GetFlag(FLAGS_mode) == "config_dialog") {
    return RunConfigDialog(argc, argv);
  } else if (absl::GetFlag(FLAGS_mode) == "dictionary_tool") {
    return RunDictionaryTool(argc, argv);
  } else if (absl::GetFlag(FLAGS_mode) == "word_register_dialog") {
    return RunWordRegisterDialog(argc, argv);
  } else if (absl::GetFlag(FLAGS_mode) == "error_message_dialog") {
    return RunErrorMessageDialog(argc, argv);
  } else if (absl::GetFlag(FLAGS_mode) == "about_dialog") {
    return RunAboutDialog(argc, argv);
#ifdef _WIN32
  } else if (absl::GetFlag(FLAGS_mode) == "post_install_dialog") {
    // post_install_dialog is used on Windows only.
    return RunPostInstallDialog(argc, argv);
  } else if (absl::GetFlag(FLAGS_mode) == "administration_dialog") {
    // administration_dialog is used on Windows only.
    return RunAdministrationDialog(argc, argv);
#endif  // _WIN32
#ifdef __APPLE__
  } else if (absl::GetFlag(FLAGS_mode) == "prelauncher") {
    // Prelauncher is used on Mac only.
    return RunPrelaunchProcesses(argc, argv);
#endif  // __APPLE__
  } else {
    LOG(ERROR) << "Unknown mode: " << absl::GetFlag(FLAGS_mode);
    return -1;
  }

  return 0;
}

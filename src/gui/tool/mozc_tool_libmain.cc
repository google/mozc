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

#ifdef OS_WIN
#include <windows.h>
#endif

#include <QtGui/QtGui>

#ifdef __APPLE__
#include <cstdlib>
#ifndef IGNORE_INVALID_FLAG
#include <iostream>
#endif  // IGNORE_INVALID_FLAG
#endif  // __APPLE__

#include "base/crash_report_handler.h"
#include "base/file_util.h"
#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/run_level.h"
#include "base/util.h"
#include "config/stats_config_util.h"
#include "gui/base/debug_util.h"

#ifdef __APPLE__
#include "base/const.h"
#endif  // __APPLE__

#ifdef OS_WIN
#include "gui/base/win_util.h"
#endif  // OS_WIN

DEFINE_string(mode, "about_dialog", "mozc_tool mode");

// Run* are defiend in each qt module
int RunAboutDialog(int argc, char *argv[]);
int RunConfigDialog(int argc, char *argv[]);
int RunDictionaryTool(int argc, char *argv[]);
int RunWordRegisterDialog(int argc, char *argv[]);
int RunErrorMessageDialog(int argc, char *argv[]);

#ifdef OS_WIN
// (SetDefault|PostInstall|RunAdministartion)Dialog are used for Windows only.
int RunSetDefaultDialog(int argc, char *argv[]);
int RunPostInstallDialog(int argc, char *argv[]);
int RunAdministrationDialog(int argc, char *argv[]);
#endif  // OS_WIN

#ifdef __APPLE__
// Confirmation Dialog is used for the update dialog on Mac only.
int RunConfirmationDialog(int argc, char *argv[]);
int RunPrelaunchProcesses(int argc, char *argv[]);
#endif  // __APPLE__

#ifdef __APPLE__
namespace {

void SetFlagFromEnv(const string &key) {
  const string flag_name = "mozc::GetFlag(FLAGS_)" + key;
  const char *env = getenv(flag_name.c_str());
  if (env == nullptr) {
    return;
  }
  if (!mozc_flags::SetFlag(key, env)) {
#ifndef IGNORE_INVALID_FLAG
    std::cerr << "Unknown/Invalid flag " << key << std::endl;
#endif
  }
}

}  // namespace
#endif  // __APPLE__

int RunMozcTool(int argc, char *argv[]) {
  if (mozc::config::StatsConfigUtil::IsEnabled()) {
    mozc::CrashReportHandler::Initialize(false);
  }
#ifdef __APPLE__
  // OSX's app won't accept command line flags.  Here we preset flags from
  // environment variables.
  SetFlagFromEnv("mode");
  SetFlagFromEnv("error_type");
  SetFlagFromEnv("confirmation_type");
  SetFlagFromEnv("register_prelauncher");
#endif  // __APPLE__
  mozc::InitMozc(argv[0], &argc, &argv);

#ifdef __APPLE__
  // In Mac, we shares the same binary but changes the application
  // name.
  string binary_name = mozc::FileUtil::Basename(argv[0]);
  if (binary_name == "AboutDialog") {
    mozc::SetFlag(&FLAGS_mode, "about_dialog");
  } else if (binary_name == "ConfigDialog") {
    mozc::SetFlag(&FLAGS_mode, "config_dialog");
  } else if (binary_name == "DictionaryTool") {
    mozc::SetFlag(&FLAGS_mode, "dictionary_tool");
  } else if (binary_name == "ErrorMessageDialog") {
    mozc::SetFlag(&FLAGS_mode, "error_message_dialog");
  } else if (binary_name == "WordRegisterDialog") {
    mozc::SetFlag(&FLAGS_mode, "word_register_dialog");
  } else if (binary_name == kProductPrefix "Prelauncher") {
    // The binary name of prelauncher is user visible in
    // "System Preferences" -> "Accounts" -> "Login items".
    // So we set kProductPrefix to the binary name.
    mozc::SetFlag(&FLAGS_mode, "prelauncher");
  }
#endif

  if (mozc::GetFlag(FLAGS_mode) != "administration_dialog" &&
      !mozc::RunLevel::IsValidClientRunLevel()) {
    return -1;
  }

  // install Qt debug handler
  qInstallMessageHandler(mozc::gui::DebugUtil::MessageHandler);

#ifdef OS_WIN
  // Update JumpList if available.
  mozc::gui::WinUtil::KeepJumpListUpToDate();
#endif  // OS_WIN

  if (mozc::GetFlag(FLAGS_mode) == "config_dialog") {
    return RunConfigDialog(argc, argv);
  } else if (mozc::GetFlag(FLAGS_mode) == "dictionary_tool") {
    return RunDictionaryTool(argc, argv);
  } else if (mozc::GetFlag(FLAGS_mode) == "word_register_dialog") {
    return RunWordRegisterDialog(argc, argv);
  } else if (mozc::GetFlag(FLAGS_mode) == "error_message_dialog") {
    return RunErrorMessageDialog(argc, argv);
  } else if (mozc::GetFlag(FLAGS_mode) == "about_dialog") {
    return RunAboutDialog(argc, argv);
#ifdef OS_WIN
  } else if (mozc::GetFlag(FLAGS_mode) == "set_default_dialog") {
    // set_default_dialog is used on Windows only.
    return RunSetDefaultDialog(argc, argv);
  } else if (mozc::GetFlag(FLAGS_mode) == "post_install_dialog") {
    // post_install_dialog is used on Windows only.
    return RunPostInstallDialog(argc, argv);
  } else if (mozc::GetFlag(FLAGS_mode) == "administration_dialog") {
    // administration_dialog is used on Windows only.
    return RunAdministrationDialog(argc, argv);
#endif  // OS_WIN
#ifdef __APPLE__
  } else if (mozc::GetFlag(FLAGS_mode) == "confirmation_dialog") {
    // Confirmation Dialog is used for the update dialog on Mac only.
    return RunConfirmationDialog(argc, argv);
  } else if (mozc::GetFlag(FLAGS_mode) == "prelauncher") {
    // Prelauncher is used on Mac only.
    return RunPrelaunchProcesses(argc, argv);
#endif  // __APPLE__
  } else {
    LOG(ERROR) << "Unknown mode: " << mozc::GetFlag(FLAGS_mode);
    return -1;
  }

  return 0;
}

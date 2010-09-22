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

#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include <QtGui/QtGui>
#include "base/base.h"
#include "base/password_manager.h"
#include "base/util.h"
#include "gui/base/debug_util.h"
#include "base/run_level.h"
#include "base/winmain.h"

DEFINE_string(mode, "about_dialog", "mozc_tool mode");

// Run* are defiend in each qt module
int RunAboutDialog(int argc, char *argv[]);
int RunConfigDialog(int argc, char *argv[]);
int RunDictionaryTool(int argc, char *argv[]);
int RunErrorMessageDialog(int argc, char *argv[]);

#ifdef OS_WINDOWS
// (SetDefault|PostInstall|RunAdministartion)Dialog are used for Windows only.
int RunSetDefaultDialog(int argc, char *argv[]);
int RunPostInstallDialog(int argc, char *argv[]);
int RunAdministrationDialog(int argc, char *argv[]);
#endif  // OS_WINDOWS

#ifdef OS_MACOSX
// Confirmation Dialog is used for the update dialog on Mac only.
int RunConfirmationDialog(int argc, char *argv[]);
#endif  // OS_MACOSX

#ifdef OS_MACOSX
namespace {
char *strdup_with_new(const char *str) {
  const size_t len = strlen(str);
  char *v = new char [len + 1];
  memcpy(v, str, len);
  v[len] = '\0';
  return v;
}

}  // namespace
#endif  // OS_MACOSX

int main(int argc, char *argv[]) {
#ifdef OS_MACOSX
  // OSX's app won't accept command line flags.
  // Here we read the flags by using --fromenv option
  scoped_array<char *> tmp(new char * [2]);
  tmp[0] = strdup_with_new(argv[0]);
  tmp[1] = strdup_with_new("--fromenv=mode,error_type,confirmation_type");
  int new_argc = 2;
  char **new_argv = tmp.get();
  InitGoogleWithBreakPad(new_argv[0], &new_argc, &new_argv, false);
  delete [] tmp[0];
  delete [] tmp[1];
#else  // OS_MACOSX
  InitGoogleWithBreakPad(argv[0], &argc, &argv, false);
#endif  // OS_MACOSX

#ifdef OS_MACOSX
  // In Mac, we shares the same binary but changes the application
  // name.
  string binary_name = mozc::Util::Basename(argv[0]);
  if (binary_name == "AboutDialog") {
    FLAGS_mode = "about_dialog";
  } else if (binary_name == "ConfigDialog") {
    FLAGS_mode = "config_dialog";
  } else if (binary_name == "DictionaryTool") {
    FLAGS_mode = "dictionary_tool";
  } else if (binary_name =="ErrorMessageDialog") {
    FLAGS_mode = "error_message_dialog";
  }
#endif

  if (FLAGS_mode != "administration_dialog" &&
      !mozc::RunLevel::IsValidClientRunLevel()) {
    return -1;
  }

  // install Qt debug handler
  qInstallMsgHandler(mozc::gui::DebugUtil::MessageHandler);

  Q_INIT_RESOURCE(qrc_mozc_tool);

  // we cannot install the translation of qt_ja_JP here.
  // as Qpplication is initialized inside Run* function

  if (FLAGS_mode == "config_dialog") {
    return RunConfigDialog(argc, argv);
  } else if (FLAGS_mode == "dictionary_tool") {
    return RunDictionaryTool(argc, argv);
  } else if (FLAGS_mode == "error_message_dialog") {
    return RunErrorMessageDialog(argc, argv);
  } else if (FLAGS_mode == "about_dialog") {
    return RunAboutDialog(argc, argv);
#ifdef OS_WINDOWS
  } else if (FLAGS_mode == "set_default_dialog") {
    // set_default_dialog is used on Windows only.
    return RunSetDefaultDialog(argc, argv);
  } else if (FLAGS_mode == "post_install_dialog") {
    // post_install_dialog is used on Windows only.
    return RunPostInstallDialog(argc, argv);
  } else if (FLAGS_mode == "administration_dialog") {
    // administration_dialog is used on Windows only.
    return RunAdministrationDialog(argc, argv);
#endif  // OS_WINDOWS
#ifdef OS_MACOSX
  } else if (FLAGS_mode == "confirmation_dialog") {
    // Confirmation Dialog is used for the update dialog on Mac only.
    return RunConfirmationDialog(argc, argv);
#endif  // OS_MACOSX
  } else {
    LOG(ERROR) << "Unknown mode: " << FLAGS_mode;
    return -1;
  }

  return 0;
}

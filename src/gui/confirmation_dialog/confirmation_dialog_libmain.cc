// Copyright 2010-2013, Google Inc.
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

#include <QtGui/QApplication>
#include <QtGui/QtGui>
#include "base/logging.h"
#include "base/process_mutex.h"
#include "base/system_util.h"
#include "base/util.h"
#include "gui/base/locale_util.h"
#include "gui/confirmation_dialog/confirmation_dialog.h"

DECLARE_string(confirmation_type);
DEFINE_int32(confirmation_wait_time, 3600,
             "The interval time to pop-up the confirmation again, in sec.");

int RunConfirmationDialog(int argc, char *argv[]) {
  Q_INIT_RESOURCE(qrc_confirmation_dialog);

  mozc::SystemUtil::DisableIME();
  QApplication app(argc, argv);

  string name = "confirmation_dialog.";
  name += mozc::SystemUtil::GetDesktopNameAsString();

  mozc::ProcessMutex mutex(name.c_str());
  if (!mutex.Lock()) {
    LOG(INFO) << "confirmation_dialog is already running";
    return -1;
  }

  mozc::gui::LocaleUtil::InstallTranslationMessageAndFont
      ("confirmation_dialog");

  if (FLAGS_confirmation_type != "log_out") {
    if (mozc::gui::ConfirmationDialog::Show()) {
      return 0;  // Yes.
    }
    return 1;  // No.
  }

  // confirmation_type == "log_out"
  for (;;) {
    if (mozc::gui::ConfirmationDialog::Show()) {
      return 0;  // Yes.
    }
    mozc::Util::Sleep(FLAGS_confirmation_wait_time * 1000 /* msec */);
  }

  // Code will not reach here but put the return value just in case.
  return 1;
}

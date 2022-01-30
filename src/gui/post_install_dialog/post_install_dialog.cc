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

#include "gui/post_install_dialog/post_install_dialog.h"

#ifdef OS_WIN
#include <windows.h>
#endif  // OS_WIN

#include <QtGui>

#include "base/logging.h"
#include "base/process.h"
#include "base/run_level.h"
#include "base/util.h"
#include "gui/base/setup_util.h"
#include "gui/base/util.h"
#include "usage_stats/usage_stats.h"

#ifdef OS_WIN
#include "base/win_util.h"
#include "win32/base/imm_util.h"
#endif  // OS_WIN

namespace mozc {
namespace gui {
PostInstallDialog::PostInstallDialog() : setuputil_(new SetupUtil()) {
  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint |
                 Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint);
  setWindowModality(Qt::NonModal);

  QObject::connect(okButton, SIGNAL(clicked()), this, SLOT(OnOk()));
  QObject::connect(setAsDefaultCheckBox, SIGNAL(stateChanged(int)), this,
                   SLOT(OnsetAsDefaultCheckBoxToggled(int)));

  // set the default state of migrateDefaultIMEUserDictionaryCheckBox
  const bool status =
      (!RunLevel::IsElevatedByUAC() && setuputil_->LockUserDictionary());
  migrateDefaultIMEUserDictionaryCheckBox->setVisible(status);

  // import MS-IME by default
  migrateDefaultIMEUserDictionaryCheckBox->setChecked(true);

  GuiUtil::ReplaceWidgetLabels(this);
}

PostInstallDialog::~PostInstallDialog() {}

void PostInstallDialog::OnOk() {
  usage_stats::UsageStats::IncrementCount("PostInstallOkButton");
  ApplySettings();
  done(QDialog::Accepted);
}

void PostInstallDialog::reject() {
  usage_stats::UsageStats::IncrementCount("PostInstallRejectButton");
  done(QDialog::Rejected);
}

void PostInstallDialog::ApplySettings() {
#ifdef OS_WIN
  uint32 flags = SetupUtil::NONE;
  if (setAsDefaultCheckBox->isChecked()) {
    flags |= SetupUtil::IME_DEFAULT;
  }
  if (IMEHotKeyDisabledCheckBox->isEnabled() &&
      IMEHotKeyDisabledCheckBox->isChecked()) {
    flags |= SetupUtil::DISABLE_HOTKEY;
  }
  if (migrateDefaultIMEUserDictionaryCheckBox->isChecked() &&
      migrateDefaultIMEUserDictionaryCheckBox->isVisible()) {
    flags |= SetupUtil::IMPORT_MSIME_DICTIONARY;
  }
  setuputil_->SetDefaultProperty(flags);
#else  // OS_WIN
  // not supported on Mac and Linux
#endif  // OS_WIN
}

void PostInstallDialog::OnsetAsDefaultCheckBoxToggled(int state) {
#ifdef OS_WIN
  // IMEHotKey is only activated when setAsDefaultCheckBox is checked.
  IMEHotKeyDisabledCheckBox->setChecked(state);
  IMEHotKeyDisabledCheckBox->setEnabled(static_cast<bool>(state));
#endif  // OS_WIN
}

}  // namespace gui
}  // namespace mozc

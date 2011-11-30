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

#include "gui/post_install_dialog/post_install_dialog.h"

#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include <QtGui/QtGui>
#include "base/base.h"
#include "base/process.h"
#include "base/run_level.h"
#include "base/util.h"
#include "gui/base/setup_util.h"
#include "usage_stats/usage_stats.h"

#ifdef OS_WINDOWS
#include "win32/base/imm_util.h"
#include "base/win_util.h"
#endif

namespace mozc {
namespace gui {
PostInstallDialog::PostInstallDialog()
    : setuputil_(new SetupUtil()) {
  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint |
                 Qt::MSWindowsFixedSizeDialogHint |
                 Qt::WindowStaysOnTopHint);
  setWindowModality(Qt::NonModal);

  QObject::connect(logoffNowButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(OnLogoffNow()));
  QObject::connect(logoffLaterButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(OnLogoffLater()));
  QObject::connect(okButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(OnOk()));
  QObject::connect(setAsDefaultCheckBox,
                   SIGNAL(stateChanged(int)),
                   this,
                   SLOT(OnsetAsDefaultCheckBoxToggled(int)));

  // We change buttons to be displayed depending on the condition this dialog
  // is launched.
  // The following table summarizes which buttons are displayed by conditions.
  //
  // ----------------------------------------------
  // |   OK   | Later  |  Now   |  help  | logoff |
  // ----------------------------------------------
  // |   D    |   N    |   N    |  true  |  true  |
  // |   D    |   N    |   N    |  true  |  false |
  // |   N    |   D    |   D    |  false |  true  |
  // |   D    |   N    |   N    |  false |  false |
  // ----------------------------------------------
  //
  // The followings are meanings of the words used in the table.
  // OK     : okButton
  // Later  : logoffLaterButton
  // Now    : logoffNowButton
  // help   : The result of IsShowHelpPageRequired()
  // logoff : The result of IsLogoffRequired()
  // N      : not displayed
  // D      : displayed
  //
  if (IsShowHelpPageRequired()) {
    usage_stats::UsageStats::IncrementCount("PostInstallShowPageRequired");
    thanksLabel->setText(tr("Thanks for installing.\n"
                            "You need to configure your computer before using "
                            "Mozc. Please follow the "
                            "instructions on the help page."));
    logoffNowButton->setVisible(false);
    logoffLaterButton->setVisible(false);
  } else {
    // Currently, |logoff_required()| always returns false so the following
    // conditional section, which was originally been used for rebooting system
    // to enable CUAS on Windows XP, is now dead code.  If you enable this
    // section again for some reason, please note that OnLogoffNow has not
    // supported Mac nor Linux yet.
    DCHECK(!logoff_required());
    if (logoff_required()) {
      usage_stats::UsageStats::IncrementCount("PostInstallLogoffRequired");
      thanksLabel->setText(tr("Thanks for installing.\nYou must log off before "
                              "using Mozc."));
      // remove OK button and move the other buttons to right.
      const int rows = gridLayout->rowCount();
      const int cols = gridLayout->columnCount();
      okButton->setVisible(false);
      gridLayout->removeWidget(okButton);
      gridLayout->addWidget(logoffNowButton, rows - 1, cols - 2);
      gridLayout->addWidget(logoffLaterButton, rows - 1, cols - 1);
    } else {
      usage_stats::UsageStats::IncrementCount("PostInstallNothingRequired");
      logoffNowButton->setVisible(false);
      logoffLaterButton->setVisible(false);
      gridLayout->removeWidget(logoffNowButton);
      gridLayout->removeWidget(logoffLaterButton);
    }
  }

  // set the default state of migrateDefaultIMEUserDictionaryCheckBox
  const bool status = (!RunLevel::IsElevatedByUAC() &&
                       setuputil_->LockUserDictionary());
  migrateDefaultIMEUserDictionaryCheckBox->setVisible(status);

  // import MS-IME by default
  migrateDefaultIMEUserDictionaryCheckBox->setChecked(true);
}

PostInstallDialog::~PostInstallDialog() {
}

bool PostInstallDialog::logoff_required() {
  // This function always returns false in all platforms.  See b/2899762 for
  // details.
  return false;
}

bool PostInstallDialog::ShowHelpPageIfRequired() {
  if (PostInstallDialog::IsShowHelpPageRequired()) {
    const char kHelpPageUrl[] =
        "http://www.google.com/support/ime/japanese/bin/answer.py?hl=jp&answer="
        "166771";
    return mozc::Process::OpenBrowser(kHelpPageUrl);
  }
  return false;
}

// NOTE(mazda): UsageStats class is not currently multi-process safe so it is
// possible that usagestats is incorrectly collected.
// For example if the user activate Mozc in another application before closing
// this dialog, the usagestats collected in the application can be overwritten
// when this dialog is closed.
// But this case is very rare since this dialog is launched immediately after
// installation.
// So we accept the potential error until this limitation is fixed.
void PostInstallDialog::OnLogoffNow() {
  usage_stats::UsageStats::IncrementCount("PostInstallLogoffNowButton");
  ApplySettings();
#ifdef OS_WINDOWS
  mozc::WinUtil::Logoff();
#else
  // not supported on Mac and Linux
#endif  // OS_WINDOWS
  done(QDialog::Accepted);
}

void PostInstallDialog::OnLogoffLater() {
  usage_stats::UsageStats::IncrementCount("PostInstallLogoffLaterButton");
  ApplySettings();
  done(QDialog::Rejected);
}

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
#ifdef OS_WINDOWS
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
#else
  // not supported on Mac and Linux
#endif  // OS_WINDOWS
}

void PostInstallDialog::OnsetAsDefaultCheckBoxToggled(int state) {
#ifdef OS_WINDOWS
  // IMEHotKey is only activated when setAsDefaultCheckBox is checked.
  IMEHotKeyDisabledCheckBox->setChecked(state);
  IMEHotKeyDisabledCheckBox->setEnabled(static_cast<bool>(state));
#endif
}

bool PostInstallDialog::IsShowHelpPageRequired() {
#ifdef OS_WINDOWS
  return !win32::ImeUtil::IsCtfmonRunning();
#else
  // not supported on Mac and Linux
  return false;
#endif  // OS_WINDOWS
}

}  // namespace gui
}  // namespace mozc

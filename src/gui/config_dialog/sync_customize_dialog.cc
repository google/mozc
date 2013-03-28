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

#include "gui/config_dialog/sync_customize_dialog.h"

#include <string>
#include "base/base.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_util.h"

namespace mozc {
namespace gui {

SyncCustomizeDialog::SyncCustomizeDialog(QWidget *parent)
    : QDialog(parent) {
  setupUi(this);
#ifdef OS_WIN
  Qt::WindowFlags flags = windowFlags();
  // Remove Context help button. b/5579590.
  flags &= ~Qt::WindowContextHelpButtonHint;
  setWindowFlags(flags);
#endif  // OS_WIN

  QObject::connect(syncEverythingCheckbox,
                   SIGNAL(clicked(bool)),
                   this,
                   SLOT(syncAllClicked(bool)));

  // This dialog is not visible when the instantiation timing.  The
  // parent window (ConfigDialog) always has the pointer to this
  // dialog, and emit 'show' signal only when necessary.
}

void SyncCustomizeDialog::syncAllClicked(bool checked) {
  if (checked) {
    // Disable all checkbox and all checked.
    syncConfigCheckbox->setEnabled(false);
    syncUserDictionaryCheckbox->setEnabled(false);
    syncConfigCheckbox->setChecked(true);
    syncUserDictionaryCheckbox->setChecked(true);
  } else {
    // Enable all check box.
    syncConfigCheckbox->setEnabled(true);
    syncUserDictionaryCheckbox->setEnabled(true);
  }
}

void SyncCustomizeDialog::Load(const config::Config &config) {
  if (!config.has_sync_config()) {
    syncEverythingCheckbox->setChecked(false);
    syncAllClicked(false);
    return;
  }

  const config::SyncConfig &sync_config = config.sync_config();

  if (sync_config.use_config_sync() &&
      sync_config.use_user_dictionary_sync()) {
    syncEverythingCheckbox->setChecked(true);
    syncAllClicked(true);
    return;
  }

  syncEverythingCheckbox->setChecked(false);

  syncConfigCheckbox->setChecked(sync_config.use_config_sync());
  syncUserDictionaryCheckbox->setChecked(
      sync_config.use_user_dictionary_sync());
}

void SyncCustomizeDialog::Save(bool force_save, config::Config *config) const {
  if (!force_save && !config->has_sync_config()) {
    // do nothing.
    return;
  }

  // All checkbox status should be checked when syncEverything is
  // checked in syncAllClicked() slot, so here just use 'isChecked' method.
  config->mutable_sync_config()->set_use_config_sync(
      syncConfigCheckbox->isChecked());
  config->mutable_sync_config()->set_use_user_dictionary_sync(
      syncUserDictionaryCheckbox->isChecked());
}

}  // namespace mozc::gui
}  // namespace mozc

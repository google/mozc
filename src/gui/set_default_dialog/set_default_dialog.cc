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

#include "gui/set_default_dialog/set_default_dialog.h"

#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include <QtGui/QtGui>
#include "base/base.h"
#include "base/util.h"
#include "client/session.h"
#include "session/config_handler.h"
#include "session/config.pb.h"

#ifdef OS_WINDOWS
#include "win32/base/imm_util.h"
#endif


namespace mozc {
namespace gui {

SetDefaultDialog::SetDefaultDialog() {
  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint |
                 Qt::MSWindowsFixedSizeDialogHint |
                 Qt::WindowStaysOnTopHint);
  setWindowModality(Qt::NonModal);
}

SetDefaultDialog::~SetDefaultDialog() {
}

void SetDefaultDialog::accept() {
// TODO(mazda): Implement SetDefault on Mac and Linux.
#ifdef OS_WINDOWS
  if (!mozc::ImeUtil::SetDefault()) {
    LOG(ERROR) << "Failed to set Mozc as the default IME";
  }
#endif  // OS_WINDOWS

  if(dontAskAgainCheckBox->checkState() == Qt::Checked) {
    if (!SetCheckDefault(false)) {
      LOG(ERROR) << "Failed to set check_default";
    }
  }
  done(QDialog::Accepted);
}

void SetDefaultDialog::reject() {
  if(dontAskAgainCheckBox->checkState() == Qt::Checked) {
    if (!SetCheckDefault(false)) {
      LOG(ERROR) << "Failed to set check_default";
    }
  }
  done(QDialog::Rejected);
}

bool SetDefaultDialog::SetCheckDefault(bool check_default) {
  mozc::client::Session client;
  if (!client.PingServer() && !client.EnsureConnection()) {
    LOG(ERROR) << "Cannot connect to server";
    return false;
  }
  mozc::config::Config config;
  if(!mozc::config::ConfigHandler::GetConfig(&config)) {
    LOG(ERROR) << "Cannot get config";
    return false;
  }
  config.set_check_default(check_default);
  if (!client.SetConfig(config)) {
    LOG(ERROR) << "Cannot set config";
    return false;
  }
  return true;
}

}  // namespace gui
}  // namespace mozc

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

#include "gui/set_default_dialog/set_default_dialog.h"

#ifdef OS_WIN
#include <windows.h>
#endif

#include <QtGui/QtGui>
#include <memory>

#include "base/logging.h"
#include "base/util.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "gui/base/util.h"
#include "protocol/config.pb.h"

#ifdef OS_WIN
#include "win32/base/migration_util.h"
#endif

namespace mozc {
namespace gui {

SetDefaultDialog::SetDefaultDialog() {
  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint |
                 Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint);
  setWindowModality(Qt::NonModal);
  GuiUtil::ReplaceWidgetLabels(this);
}

SetDefaultDialog::~SetDefaultDialog() {}

void SetDefaultDialog::accept() {
  // TODO(mazda): Implement SetDefault on Mac and Linux.
  const bool dont_ask_again =
      (dontAskAgainCheckBox->checkState() == Qt::Checked);
#ifdef OS_WIN
  // LaunchBrokerForSetDefault is responsible to do the same task of
  // |SetCheckDefault(false)| if |dont_ask_again| is true.
  if (!win32::MigrationUtil::LaunchBrokerForSetDefault(dont_ask_again)) {
    LOG(ERROR) << "Failed to set Mozc as the default IME";
  }
#else
  if (dont_ask_again) {
    if (!SetCheckDefault(false)) {
      LOG(ERROR) << "Failed to set check_default";
    }
  }
#endif
  done(QDialog::Accepted);
}

void SetDefaultDialog::reject() {
  if (dontAskAgainCheckBox->checkState() == Qt::Checked) {
    if (!SetCheckDefault(false)) {
      LOG(ERROR) << "Failed to set check_default";
    }
  }
  done(QDialog::Rejected);
}

bool SetDefaultDialog::SetCheckDefault(bool check_default) {
  std::unique_ptr<mozc::client::ClientInterface> client(
      mozc::client::ClientFactory::NewClient());
  if (!client->PingServer() && !client->EnsureConnection()) {
    LOG(ERROR) << "Cannot connect to server";
    return false;
  }
  mozc::config::Config config;
  if (!mozc::config::ConfigHandler::GetConfig(&config)) {
    LOG(ERROR) << "Cannot get config";
    return false;
  }
  config.set_check_default(check_default);
  if (!client->SetConfig(config)) {
    LOG(ERROR) << "Cannot set config";
    return false;
  }
  return true;
}

}  // namespace gui
}  // namespace mozc

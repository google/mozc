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

#include "gui/config_dialog/auth_dialog.h"

#include <string>
#include "base/base.h"
#include "base/process.h"
#include "base/util.h"
#include "gui/config_dialog/auth_code_detector.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_util.h"

namespace mozc {
namespace gui {

AuthDialog::AuthDialog(QWidget *parent)
    : QDialog(parent),
      detector_timer_(new QTimer(this)),
      auth_code_detector_(new AuthCodeDetector) {
  setupUi(this);
  Qt::WindowFlags flags = windowFlags() | Qt::WindowStaysOnTopHint;
#ifdef OS_WINDOWS
  // Remove Context help button. b/5579590.
  flags &= ~Qt::WindowContextHelpButtonHint;
#endif  // OS_WINDOWS
  setWindowFlags(flags);

  QObject::connect(openBrowserButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(browserButtonClicked()));
  // The input code is disabled when the window appears.  Will be
  // enabled when the user clicks the open browser button.
  authCodeLineEdit->setEnabled(false);

  // The ok button is disabled when the window appears.  Will be
  // enabled when the user types something into the text edit.
  QPushButton *ok_button = buttonBox->button(QDialogButtonBox::Ok);
  DCHECK(ok_button);
  ok_button->setEnabled(false);
  QObject::connect(authCodeLineEdit,
                   SIGNAL(textChanged(const QString &)),
                   this,
                   SLOT(handleTextChange(const QString &)));
  QObject::connect(auth_code_detector_.get(),
                   SIGNAL(setAuthCode(const QString &)),
                   authCodeLineEdit,
                   SLOT(setText(const QString &)),
                   Qt::QueuedConnection);
  QObject::connect(detector_timer_.get(), SIGNAL(timeout()),
                   auth_code_detector_.get(), SLOT(startFetchingAuthCode()),
                   Qt::QueuedConnection);
  auth_code_detector_->start();
  // detector_timer_ does not start at the constructor timing.  Rather
  // it should start at opening browser.
}

AuthDialog::~AuthDialog() {
  auth_code_detector_->quit();
  auth_code_detector_->wait();
  detector_timer_->stop();
}

string AuthDialog::GetAuthCode() const {
  return auth_code_;
}

void AuthDialog::browserButtonClicked() {
  sync::OAuth2Util oauth2_util(sync::OAuth2Client::GetDefaultClient());
  Process::OpenBrowser(oauth2_util.GetAuthenticateUri());
  openBrowserButton->setDefault(false);
  // Enables the line edit once clicked.
  authCodeLineEdit->setEnabled(true);
  // Checks the auth code every 1 sec.
  detector_timer_->start(1000);
}

void AuthDialog::handleTextChange(const QString &new_text) {
  detector_timer_->stop();

  QByteArray code_bytes = new_text.toUtf8();
  Util::StripWhiteSpaces(string(code_bytes.data(), code_bytes.length()),
                         &auth_code_);

  // Accepts the result only when there are something in the text edit.
  QPushButton *ok_button = buttonBox->button(QDialogButtonBox::Ok);
  DCHECK(ok_button);
  ok_button->setEnabled(!auth_code_.empty());
  ok_button->setDefault(true);
}

bool AuthDialog::Show(QWidget *parent, string *auth_code) {
  DCHECK(auth_code);
  AuthDialog window(parent);
  window.setWindowModality(Qt::ApplicationModal);

  // open modal mode
  bool result = (QDialog::Accepted == window.exec());
  if (result) {
    auth_code->assign(window.GetAuthCode());
  }
  return result;
}

}  // namespace mozc::gui
}  // namespace mozc

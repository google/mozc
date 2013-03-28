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

#include "gui/confirmation_dialog/confirmation_dialog.h"

#include <QtGui/QtGui>
#include <QtGui/QMessageBox>
#include "base/base.h"

DEFINE_string(confirmation_type, "", "type of confirmation");

namespace mozc {
namespace gui {

// static
bool ConfirmationDialog::Show() {
  QMessageBox message_box(
      QMessageBox::Question,
      // Title
      QObject::tr("Mozc"),
      // Message
      QObject::tr("Invalid confirmation dialog.  "
                  "You specified less arguments."),
      QMessageBox::Yes | QMessageBox::No,
      NULL,
      Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint);


  if (FLAGS_confirmation_type == "update") {
    message_box.setText(QObject::tr(
        "Mozc has been updated.  "
        "Would you like to activate the new version now?  "
        "(Note: some features will not be available "
        "until you log out and log back in.)"));
    QAbstractButton *yes_button = message_box.button(QMessageBox::Yes);
    if (yes_button != NULL) {
      yes_button->setText(QObject::tr("Activate now"));
    }
    QAbstractButton *no_button = message_box.button(QMessageBox::No);
    if (no_button != NULL) {
      no_button->setText(QObject::tr("Wait until logout"));
    }
  } else if (FLAGS_confirmation_type == "log_out") {
    message_box.setText(QObject::tr(
        "Mozc has been updated.  "
        "Please log out and back in to enable the new version."));
    QAbstractButton *yes_button = message_box.button(QMessageBox::Yes);
    if (yes_button != NULL) {
      yes_button->setText(QObject::tr("Log out"));
    }
    QAbstractButton *no_button = message_box.button(QMessageBox::No);
    if (no_button != NULL) {
      no_button->setText(QObject::tr("Remind me in 1 hour"));
    }
  }

  const int result = message_box.exec();
  return (result == QMessageBox::Yes);
}
}  // namespace gui
}  // namespace mozc

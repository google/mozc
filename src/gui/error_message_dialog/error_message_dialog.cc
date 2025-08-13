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

#include "gui/error_message_dialog/error_message_dialog.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QTimer>
#include <QtGui>
#include <string>

#include "absl/flags/flag.h"
#include "gui/base/util.h"

ABSL_FLAG(std::string, error_type, "", "type of error");

namespace mozc {
namespace gui {
namespace {

void OnFatal(const QString &message) {
  // we don't use QMessageBox::critical() here
  // to set WindowStaysOnTopHint
  QMessageBox message_box(
      QMessageBox::Critical, QObject::tr("[ProductName] Fatal Error"), message,
      QMessageBox::Ok, nullptr,
      Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint);
  GuiUtil::ReplaceWidgetLabels(&message_box);
  DeleyedMessageDialogHandler handler(&message_box);
  handler.Exec();
}
}  // namespace

DeleyedMessageDialogHandler::DeleyedMessageDialogHandler(
    QMessageBox *message_box)
    : message_box_(message_box) {}

DeleyedMessageDialogHandler::~DeleyedMessageDialogHandler() = default;

void DeleyedMessageDialogHandler::Exec() {
  constexpr int kDisableInterval = 3000;
  QTimer::singleShot(kDisableInterval, this, SLOT(EnableOkButton()));
  QAbstractButton *button = message_box_->button(QMessageBox::Ok);
  if (button != nullptr) {
    button->setEnabled(false);
  }
  message_box_->exec();
}

void DeleyedMessageDialogHandler::EnableOkButton() {
  QAbstractButton *button = message_box_->button(QMessageBox::Ok);
  if (button != nullptr) {
    button->setEnabled(true);
  }
}

void ErrorMessageDialog::Show() {
  // defining all literal messages inside Show() method
  // for easy i18n/i10n
  if (absl::GetFlag(FLAGS_error_type) == "server_timeout") {
    OnFatal(
        QObject::tr("Conversion engine is not responding. "
                    "Please restart this application."));
  } else if (absl::GetFlag(FLAGS_error_type) == "server_broken_message") {
    OnFatal(
        QObject::tr("Connecting to an incompatible conversion engine. "
                    "Please restart your computer to enable [ProductName]. "
                    "If this problem persists, please uninstall [ProductName] "
                    "and install it again."));
  } else if (absl::GetFlag(FLAGS_error_type) == "server_version_mismatch") {
    OnFatal(QObject::tr(
        "Conversion engine has been upgraded. "
        "Please restart this application to enable conversion engine. "
        "If the problem persists, please restart your computer."));
  } else if (absl::GetFlag(FLAGS_error_type) == "server_shutdown") {
    OnFatal(
        QObject::tr("Conversion engine is killed unexceptionally. "
                    "Restarting the engine..."));
  } else if (absl::GetFlag(FLAGS_error_type) == "server_fatal") {
    OnFatal(
        QObject::tr("Cannot start conversion engine. "
                    "Please restart your computer."));
  } else if (absl::GetFlag(FLAGS_error_type) == "renderer_version_mismatch") {
    OnFatal(
        QObject::tr("Candidate window renderer has been upgraded. "
                    "Please restart this application to enable new candidate "
                    "window renderer. "
                    "If the problem persists, please restart your computer."));
  } else if (absl::GetFlag(FLAGS_error_type) == "renderer_fatal") {
    OnFatal(
        QObject::tr("Cannot start candidate window renderer. "
                    "Please restart your computer."));
  }
}
}  // namespace gui
}  // namespace mozc

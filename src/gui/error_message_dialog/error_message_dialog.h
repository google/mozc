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

#ifndef MOZC_GUI_ERROR_MESSAGE_DIALOG_H_
#define MOZC_GUI_ERROR_MESSAGE_DIALOG_H_

#include <QtCore/QObject>
#include <QtGui/QMessageBox>

namespace mozc {
namespace gui {

// out-proc error message dialog.
class ErrorMessageDialog {
 public:
  static void Show();
};

// disable OK button for 3 seconds to prevent user
// from pushing OK button accidentally. Since the error
// dialog will be displayed while user is activating IME,
// user will push space or enter key before reading
// the message of dialog.
class DeleyedMessageDialogHandler : public QObject {
 Q_OBJECT

 public:
  DeleyedMessageDialogHandler(QMessageBox *message_box);
  virtual ~DeleyedMessageDialogHandler();

  void Exec();

 private slots:
  void EnableOkButton();

 private:
  QMessageBox *message_box_;
};
}  // namespace mozc::gui
}  // namespace mozc
#endif  // MOZC_GUI_ERROR_MESSAGE_DIALOG_H_

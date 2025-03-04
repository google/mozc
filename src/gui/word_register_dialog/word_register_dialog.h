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

#ifndef MOZC_GUI_WORD_REGISTER_DIALOG_WORD_REGISTER_DIALOG_H_
#define MOZC_GUI_WORD_REGISTER_DIALOG_WORD_REGISTER_DIALOG_H_

#include <QDialog>
#include <QString>
#include <QtGui>
#include <memory>

#include "data_manager/pos_list_provider.h"
#include "gui/word_register_dialog/ui_word_register_dialog.h"

namespace mozc {

namespace client {
class ClientInterface;
}  // namespace client

namespace user_dictionary {
class UserDictionarySession;
}  // namespace user_dictionary

namespace gui {
class WordRegisterDialog : public QDialog, private Ui::WordRegisterDialog {
  Q_OBJECT;

 public:
  WordRegisterDialog();
  ~WordRegisterDialog() override;

  bool IsAvailable() const;

 protected slots:
  void Clicked(QAbstractButton *button);
  void LineEditChanged(const QString &str);
  void CompleteReading();
  void LaunchDictionaryTool();

 private:
  enum ErrorCode {
    SAVE_SUCCESS,
    SAVE_FAILURE,
    INVALID_KEY,
    INVALID_VALUE,
    EMPTY_KEY,
    EMPTY_VALUE,
    FATAL_ERROR
  };

  ErrorCode SaveEntry();

  void UpdateUIStatus();

  // Copy the current selected text on the foreground window
  // to clipboard. This method should be invoked before
  // the word register form is activated.
  // Seems that both ATOK and MS-IME use a clipboard to
  // copy the selected text to the word register dialog.
  // Clipboard seems to be the most robust mechanism to know
  // the selected text. It works on almost all applications.
  // TODO(all): Mac version is not available.
  void CopyCurrentSelectionToClipboard();

  // Load text from clipboard.
  void SetDefaultEntryFromClipboard();

  // Load text from environment variable.  Currently this method is
  // tested only on Mac OSX and Windows.
  // Return false if source environment variable is not found.
  bool SetDefaultEntryFromEnvironmentVariable();

  // Return reading of value with reverse conversion feature.
  QString GetReading(const QString &str);

  // remove "\n" "\r" from |value|.
  // remove whitespace from the start and the end.
  QString TrimValue(const QString &str) const;

  // turn on IME.
  // When the dialog is shown, it is better to turn on IME.
  void EnableIME();

  bool is_available_;
  std::unique_ptr<mozc::user_dictionary::UserDictionarySession> session_;
  std::unique_ptr<client::ClientInterface> client_;
  QString window_title_;
  const PosListProvider pos_list_provider_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_WORD_REGISTER_DIALOG_WORD_REGISTER_DIALOG_H_

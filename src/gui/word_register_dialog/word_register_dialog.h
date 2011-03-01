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

#ifndef MOZC_GUI_WORD_REGISTER_DIALOG_H_
#define MOZC_GUI_WORD_REGISTER_DIALOG_H_

#include <QtCore/QString>
#include <QtGui/QtGui>
#include <QtGui/QDialog>
#include "base/base.h"
#include "dictionary/user_dictionary_storage.h"
#include "gui/word_register_dialog/ui_word_register_dialog.h"

namespace mozc {

namespace client {
class Session;
}

namespace gui {
class WordRegisterDialog : public QDialog,
                           private Ui::WordRegisterDialog {
  Q_OBJECT;

 public:
  WordRegisterDialog();
  virtual ~WordRegisterDialog();

  bool IsAvailable() const;

 protected slots:
  void Clicked(QAbstractButton *button);
  void LineEditChanged(const QString &str);
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

  // Load text from environment variable.  Currently this method works
  // only with Mac OSX.
  void SetDefaultEntryFromEnvironmentVariable();

  // Return reading of value with reverse conversion feature.
  const QString GetReading(const QString &value);

  // remove "\n" "\r" from |value|.
  // remove whitespace from the start and the end.
  const QString TrimValue(const QString &value) const;

  bool is_available_;
  scoped_ptr<UserDictionaryStorage> storage_;
  scoped_ptr<client::Session> session_;
  QString window_title_;
};
}  // namespace mozc::gui
}  // namespace mozc
#endif  // MOZC_GUI_WORD_REGISTER_DIALOG_H_

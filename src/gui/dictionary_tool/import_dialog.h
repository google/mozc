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

// ImportDialog is a dialog that is shown to user when importing a
// dictionary from a file. The dialog lets the user to select an
// imported file, a IME type and character encoding. The class is
// intended to be created and used by DictionaryTool application.

#ifndef MOZC_GUI_DICTIONARY_TOOL_IMPORT_DIALOG_H_
#define MOZC_GUI_DICTIONARY_TOOL_IMPORT_DIALOG_H_

#include <QDialog>

#include "dictionary/user_dictionary_importer.h"
#include "gui/dictionary_tool/ui_import_dialog.h"

namespace mozc {
namespace gui {

class ImportDialog : public QDialog, private Ui::ImportDialog {
  Q_OBJECT

 public:
  explicit ImportDialog(QWidget* parent = nullptr);

  // Accessor methods to get form values.
  QString file_name() const;
  QString dic_name() const;

  user_dictionary::IMEType ime_type() const;
  user_dictionary::EncodingType encoding_type() const;

  // Show this dialog synchronously. The field for name of a new
  // dictionary is shown.
  int ExecInCreateMode();
  // Show this dialog synchronously. The field for name of a new
  // dictionary is hidden.
  int ExecInAppendMode();

 private slots:
  // Check input fields and update state of the import button.
  void OnFormValueChanged();
  // Open a dialog that lets the user to select a file to be imported.
  void SelectFile();
  void Clicked(QAbstractButton* button);

 private:
  // return true if accept button can be enabled
  bool IsAcceptButtonEnabled() const;

  // Reset value of all forms to default.
  void Reset();

  enum Mode {
    CREATE,
    APPEND,
  };
  Mode mode_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_DICTIONARY_TOOL_IMPORT_DIALOG_H_

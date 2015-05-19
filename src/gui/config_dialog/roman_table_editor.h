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

#ifndef MOZC_GUI_CONFIG_DIALOG_ROMAN_TABLE_EDITOR_H_
#define MOZC_GUI_CONFIG_DIALOG_ROMAN_TABLE_EDITOR_H_

#include <QtGui/QWidget>
#include <string>
#include "base/base.h"
#include "gui/config_dialog/generic_table_editor.h"

class QAbstractButton;

namespace mozc {
namespace gui {

class RomanTableEditorDialog : public GenericTableEditorDialog {
 Q_OBJECT;
 public:
  explicit RomanTableEditorDialog(QWidget *parent);
  virtual ~RomanTableEditorDialog();

  // show a modal dialog
  static bool Show(QWidget *parent,
                   const string &current_keymap,
                   string *new_keymap);

 protected slots:
  virtual void UpdateMenuStatus();
  virtual void OnEditMenuAction(QAction *action);

 protected:
  virtual string GetDefaultFilename() const {
    return "romantable.txt";
  }
  virtual bool LoadFromStream(istream *is);
  virtual bool Update();

 private:
  bool LoadDefaultRomanTable();
  static string GetDefaultRomanTable();

 private:
  scoped_array<QAction *> actions_;
};
}  // namespace gui
}  // namespace mozc
#endif  // MOZC_GUI_CONFIG_DIALOG_ROMAN_TABLE_EDITOR_H_

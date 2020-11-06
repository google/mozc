// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_GUI_CONFIG_DIALOG_KEYMAP_EDITOR_H_
#define MOZC_GUI_CONFIG_DIALOG_KEYMAP_EDITOR_H_

#include <QtWidgets/QWidget>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/port.h"
#include "gui/config_dialog/generic_table_editor.h"

class QAbstractButton;

namespace mozc {
namespace gui {

class ComboBoxDelegate;
class KeyBindingEditorDelegate;

class KeyMapEditorDialog : public GenericTableEditorDialog {
  Q_OBJECT;

 public:
  explicit KeyMapEditorDialog(QWidget *parent);
  ~KeyMapEditorDialog() override;

  // show a modal dialog
  static bool Show(QWidget *parent, const std::string &current_keymap,
                   std::string *new_keymap);

 protected slots:
  void UpdateMenuStatus() override;
  void OnEditMenuAction(QAction *action) override;

 protected:
  std::string GetDefaultFilename() const override { return "keymap.txt"; }
  bool LoadFromStream(std::istream *is) override;
  bool Update() override;

 private:
  std::string invisible_keymap_table_;
  // This is used for deciding whether the user has changed the settings that
  // are valid only for new applications.
  std::set<std::string> direct_mode_commands_;
  std::unique_ptr<QAction *[]> actions_;
  std::unique_ptr<QAction *[]> import_actions_;
  std::unique_ptr<ComboBoxDelegate> status_delegate_;
  std::unique_ptr<ComboBoxDelegate> commands_delegate_;
  std::unique_ptr<KeyBindingEditorDelegate> keybinding_delegate_;

  std::map<std::string, std::string> normalized_command_map_;
  std::map<std::string, std::string> normalized_status_map_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CONFIG_DIALOG_KEYMAP_EDITOR_H_

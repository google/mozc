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

#ifndef MOZC_GUI_CONFIG_DIALOG_KEYBINDING_EDITOR_H_
#define MOZC_GUI_CONFIG_DIALOG_KEYBINDING_EDITOR_H_

#include <QGuiApplication>
#include <QtGui>
#include <memory>

#include "gui/config_dialog/ui_keybinding_editor.h"

namespace mozc {
namespace gui {
namespace key_binding_editor_internal {

class KeyBindingFilter : public QObject {
 public:
  KeyBindingFilter(QLineEdit *line_edit, QPushButton *ok_button);

  enum KeyState { DENY_KEY, ACCEPT_KEY, SUBMIT_KEY };

 protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

 private:
  void Reset();

  // add new "qt_key" to the filter.
  // return true if the current key_bindings the KeyBindingFilter holds
  // is valid. Composed key_bindings are stored to "result"
  KeyState AddKey(const QKeyEvent &key_event, QString *result);

  // encode the current key binding
  KeyState Encode(QString *result) const;

  bool committed_;
  bool ctrl_pressed_;
  bool alt_pressed_;
  bool shift_pressed_;
  QString modifier_required_key_;
  QString modifier_non_required_key_;
  QString unknown_key_;
  QLineEdit *line_edit_;
  QPushButton *ok_button_;
};

}  // namespace key_binding_editor_internal

class KeyBindingEditor : public QDialog, private Ui::KeyBindingEditor {
  Q_OBJECT;

 public:
  // |parent| is the parent object of KeyBindingEditor.
  // |trigger_parent| is the object who was the trigger of launching the editor.
  // QPushButton can be a trigger_parent.
  KeyBindingEditor(QWidget *parent, QWidget *trigger_parent);

  QWidget *mutable_trigger_parent() const { return trigger_parent_; }

  // return current binding in QString
  QString GetBinding() const;
  void SetBinding(const QString &binding);

  // For some reason, KeyBindingEditor lanuched by
  // QItemDelegate loses focus. We overwrite
  // setVisible() function to call raise() and activateWindow().
  void setVisible(bool visible) override {
    QWidget::setVisible(visible);
    if (visible) {
      raise();
      activateWindow();
    }
  }

 private slots:
  void Clicked(QAbstractButton *button);

 private:
  QWidget *trigger_parent_;
  std::unique_ptr<key_binding_editor_internal::KeyBindingFilter> filter_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CONFIG_DIALOG_KEYBINDING_EDITOR_H_

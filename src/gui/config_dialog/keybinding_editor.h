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

#ifndef MOZC_GUI_CONFIG_DIALOG_KEYBINDING_EDITOR_H_
#define MOZC_GUI_CONFIG_DIALOG_KEYBINDING_EDITOR_H_

#include <QtGui/QtGui>
#include <QtGui/QDialog>
#include <string>
#include "base/base.h"
#include "gui/config_dialog/ui_keybinding_editor.h"

namespace mozc {
namespace gui {
class KeyBindingFilter;
class KeyBindingEditor : public QDialog,
                         private Ui::KeyBindingEditor {
  Q_OBJECT;

 public:
  // |parent| is the parent object of KeyBindingEditor.
  // |trigger_parent| is the object who was the trigger of launching the editor.
  // QPushButton can be a trigger_parent.
  KeyBindingEditor(QWidget *parent, QWidget *trigger_parent);
  virtual ~KeyBindingEditor();

  QWidget *mutable_trigger_parent() const {
    return trigger_parent_;
  }

  // return current binding in QString
  const QString GetBinding() const;
  void SetBinding(const QString &binding);

  // For some reason, KeyBindingEditor lanuched by
  // QItemDelegate looses focus. We overwrite
  // setVisible() function to call raise() and activateWindow().
  virtual void setVisible(bool visible) {
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
  scoped_ptr<KeyBindingFilter> filter_;
};
}  // namespace mozc::gui
}  // namespace mozc
#endif  // MOZC_GUI_CONFIG_DIALOG_KEYBINDING_EDITOR_H_

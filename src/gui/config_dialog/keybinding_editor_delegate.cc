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

#include "gui/config_dialog/keybinding_editor_delegate.h"

#include <QtGui/QtGui>
#include <QtGui/QPushButton>

#include "base/base.h"
#include "base/logging.h"
#include "gui/config_dialog/keybinding_editor.h"

namespace mozc {
namespace gui {

class KeyBindingEditorTriggerButton : public QPushButton {
 public:
  KeyBindingEditorTriggerButton(QWidget *parent,
                                QWidget *modal_parent) :
      QPushButton(parent),
      editor_(new KeyBindingEditor(modal_parent, this)) {
    editor_->setModal(true);   // create a modal dialog
    setFocusProxy(editor_.get());
    connect(this, SIGNAL(clicked()),
            editor_.get(), SLOT(show()));
  }

  KeyBindingEditor *mutable_editor() {
    return editor_.get();
  }

 private:
  scoped_ptr<KeyBindingEditor> editor_;
};

KeyBindingEditorDelegate::KeyBindingEditorDelegate(QObject *parent)
    : QItemDelegate(parent),
      modal_parent_(static_cast<QWidget *>(parent)) {}

KeyBindingEditorDelegate::~KeyBindingEditorDelegate() {}

QWidget *KeyBindingEditorDelegate::createEditor(
    QWidget *parent,
    const QStyleOptionViewItem &option,
    const QModelIndex &index) const {
  KeyBindingEditorTriggerButton *button
      = new KeyBindingEditorTriggerButton(parent, modal_parent_);
  CHECK(button);
  connect(button->mutable_editor(), SIGNAL(accepted()),
          this, SLOT(CommitAndCloseEditor()));
  connect(button->mutable_editor(), SIGNAL(rejected()),
          this, SLOT(CloseEditor()));
  return button;
}

void KeyBindingEditorDelegate::setEditorData(
    QWidget *editor,
    const QModelIndex &index) const {
  const QString str = index.model()->data(index, Qt::EditRole).toString();
  KeyBindingEditorTriggerButton *button
      = static_cast<KeyBindingEditorTriggerButton *>(editor);
  if (button == NULL) {
    return;
  }
  button->setText(str);
  button->mutable_editor()->SetBinding(str);
}

void KeyBindingEditorDelegate::setModelData(
    QWidget *editor, QAbstractItemModel *model,
    const QModelIndex &index) const {
  KeyBindingEditorTriggerButton *button
      = static_cast<KeyBindingEditorTriggerButton *>(editor);
  if (model == NULL || button == NULL) {
    return;
  }
  model->setData(index,
                 button->mutable_editor()->GetBinding(),
                 Qt::EditRole);
}

void KeyBindingEditorDelegate::updateEditorGeometry(
    QWidget *editor,
    const QStyleOptionViewItem &option,
    const QModelIndex &index) const {
  if (editor == NULL) {
    return;
  }
  editor->setGeometry(option.rect);
}

void KeyBindingEditorDelegate::CommitAndCloseEditor() {
  KeyBindingEditor *editor = qobject_cast<KeyBindingEditor *>(sender());
  KeyBindingEditorTriggerButton *button =
      static_cast<KeyBindingEditorTriggerButton *>(
          editor->mutable_trigger_parent());
  if (button == NULL) {
    return;
  }
  emit commitData(button);
  emit closeEditor(button);
}

void KeyBindingEditorDelegate::CloseEditor() {
  KeyBindingEditor *editor = qobject_cast<KeyBindingEditor *>(sender());
  KeyBindingEditorTriggerButton *button =
      static_cast<KeyBindingEditorTriggerButton *>(
          editor->mutable_trigger_parent());
  if (button == NULL) {
    return;
  }
  emit closeEditor(button);
}
}  // namespace gui
}  // namespace mozc

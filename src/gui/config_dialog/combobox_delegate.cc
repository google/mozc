// Copyright 2010-2014, Google Inc.
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

#include "gui/config_dialog/combobox_delegate.h"
#include <QtGui/QtGui>
#include <QtGui/QComboBox>

namespace mozc {
namespace gui {

ComboBoxDelegate::ComboBoxDelegate(QObject *parent)
    : QItemDelegate(parent) {}

ComboBoxDelegate::~ComboBoxDelegate() {}

QWidget *ComboBoxDelegate::createEditor(
    QWidget *parent,
    const QStyleOptionViewItem &option,
    const QModelIndex &index) const {
  QComboBox *editor = new QComboBox(parent);
  connect(editor, SIGNAL(currentIndexChanged(const QString &)),
          this, SLOT(CommitAndCloseEditor(const QString &)));
  for (int i = 0; i < item_list_.size(); ++i) {
    editor->addItem(item_list_.at(i));
  }
  return editor;
}

void ComboBoxDelegate::SetItemList(const QStringList &item_list) {
  item_list_ = item_list;
}

void ComboBoxDelegate::setEditorData(
    QWidget *editor,
    const QModelIndex &index) const {
  QString str = index.model()->data(index, Qt::EditRole).toString();
  QComboBox *comboBox =
      static_cast<QComboBox*>(editor);
  if (comboBox == NULL) {
    return;
  }
  comboBox->setCurrentIndex(comboBox->findText(str));
}

void ComboBoxDelegate::setModelData(
    QWidget *editor, QAbstractItemModel *model,
    const QModelIndex &index) const {
  QComboBox *comboBox =
      static_cast<QComboBox*>(editor);
  if (comboBox == NULL || model == NULL) {
    return;
  }
  model->setData(index, comboBox->currentText(), Qt::EditRole);
}

void ComboBoxDelegate::updateEditorGeometry(
    QWidget *editor,
    const QStyleOptionViewItem &option,
    const QModelIndex &index) const {
  if (editor == NULL) {
    return;
  }
  editor->setGeometry(option.rect);
}

void ComboBoxDelegate::CommitAndCloseEditor(const QString &) {
  QComboBox *editor = qobject_cast<QComboBox *>(sender());
  emit commitData(editor);
  emit closeEditor(editor);
}
}  // gui
}  // mozc

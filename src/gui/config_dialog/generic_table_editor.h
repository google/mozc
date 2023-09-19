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

#ifndef MOZC_GUI_CONFIG_DIALOG_GENERIC_TABLE_EDITOR_H_
#define MOZC_GUI_CONFIG_DIALOG_GENERIC_TABLE_EDITOR_H_

#include <QAbstractButton>
#include <QDialog>
#include <QWidget>
#include <cstddef>
#include <istream>
#include <string>

#include "gui/config_dialog/ui_generic_table_editor.h"

namespace mozc {
namespace gui {

class GenericTableEditorDialog : public QDialog,
                                 private Ui::GenericTableEditorDialog {
  Q_OBJECT;

 public:
  explicit GenericTableEditorDialog(QWidget *parent, size_t column_size);

  bool LoadFromString(const std::string &str);
  const std::string &table() const;

 protected slots:
  virtual void AddNewItem();
  virtual void InsertItem();
  virtual void DeleteSelectedItems();
  virtual void OnContextMenuRequested(const QPoint &pos);
  virtual void Clicked(QAbstractButton *button);
  virtual void InsertEmptyItem(int row);
  virtual void UpdateOKButton(bool status);
  virtual void Import();
  virtual void Export();

  virtual void UpdateMenuStatus();
  virtual void OnEditMenuAction(QAction *action);

 protected:
  std::string *mutable_table();
  QTableWidget *mutable_table_widget();
  QMenu *mutable_edit_menu();

  // implements a method which returns default fileame
  virtual QLatin1String GetDefaultFilename() const = 0;

  // implements a method which loads
  // internal data from istream
  virtual bool LoadFromStream(std::istream *is) = 0;

  // implements a method which called when
  // the current view is updated.
  virtual bool Update() = 0;

  // return maximum size of entry.
  virtual size_t max_entry_size() const;

 private:
  QMenu *edit_menu_;
  std::string table_;
  size_t column_size_;
};
}  // namespace gui
}  // namespace mozc
#endif  // MOZC_GUI_CONFIG_DIALOG_GENERIC_TABLE_EDITOR_H_

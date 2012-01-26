// Copyright 2010-2012, Google Inc.
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

#include "gui/config_dialog/generic_table_editor.h"

#include <QtCore/QFile>
#include <QtGui/QFileDialog>
#include <QtGui/QtGui>
#include <algorithm>  // for unique
#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "session/commands.pb.h"

namespace mozc {
namespace gui {
namespace {
const size_t kMaxEntrySize = 10000;

int GetTableHeight(QTableWidget *widget) {
  // Dragon Hack:
  // Here we use "龍" to calc font size, as it looks almsot square
  // const char kHexBaseChar[]= "龍";
  const char kHexBaseChar[]= "\xE9\xBE\x8D";
  const QRect rect =
      QFontMetrics(widget->font()).boundingRect(
          QObject::trUtf8(kHexBaseChar));
#ifdef OS_WINDOWS
  return static_cast<int>(rect.height() * 1.3);
#else
  return static_cast<int>(rect.height() * 1.4);
#endif   // OS_WINDOWS
}
}  // namespace

GenericTableEditorDialog::GenericTableEditorDialog(QWidget *parent,
                                                   size_t column_size)
    : QDialog(parent),
      edit_menu_(new QMenu(this)),
      column_size_(column_size) {
  setupUi(this);
  editorTableWidget->setAlternatingRowColors(true);
  setWindowFlags(Qt::WindowSystemMenuHint | Qt::Tool);
  editorTableWidget->setColumnCount(column_size_);

  CHECK_GT(column_size_, 0);

  // Mac style
#ifdef OS_MACOSX
  editorTableWidget->setShowGrid(false);
  layout()->setContentsMargins(0, 0, 0, 4);
  gridLayout->setHorizontalSpacing(12);
  gridLayout->setVerticalSpacing(12);
#endif  // OS_MACOSX

  editButton->setText(tr("Edit"));
  editButton->setMenu(edit_menu_);

  QObject::connect(edit_menu_,
                   SIGNAL(triggered(QAction *)),
                   this,
                   SLOT(OnEditMenuAction(QAction *)));

  QObject::connect(editorButtonBox,
                   SIGNAL(clicked(QAbstractButton *)),
                   this,
                   SLOT(Clicked(QAbstractButton *)));

  editorTableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(editorTableWidget, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(OnContextMenuRequested(const QPoint &)));

  editorTableWidget->horizontalHeader()->setStretchLastSection(true);
  editorTableWidget->horizontalHeader()->setSortIndicatorShown(true);
  editorTableWidget->horizontalHeader()->setHighlightSections(false);
  editorTableWidget->setEditTriggers(QAbstractItemView::AllEditTriggers);
  editorTableWidget->setSortingEnabled(true);

  editorTableWidget->verticalHeader()->setResizeMode(QHeaderView::Fixed);
  editorTableWidget->verticalHeader()->setDefaultSectionSize(
      GetTableHeight(editorTableWidget));

  UpdateMenuStatus();
}

GenericTableEditorDialog::~GenericTableEditorDialog() {}

QTableWidget *GenericTableEditorDialog::mutable_table_widget() {
  return editorTableWidget;
}

const string &GenericTableEditorDialog::table() const {
  return table_;
}

string *GenericTableEditorDialog::mutable_table() {
  return &table_;
}

QMenu *GenericTableEditorDialog::mutable_edit_menu() {
  return edit_menu_;
}

bool GenericTableEditorDialog::LoadFromString(const string &str) {
  istringstream istr(str);
  return LoadFromStream(&istr);
}

void GenericTableEditorDialog::DeleteSelectedItems() {
  vector<int> rows;
  QList<QTableWidgetItem *> selected =
      editorTableWidget->selectedItems();

  // remember the last column as user chooses the
  // last rows from top to bottom in general.
  const int column = selected.isEmpty() ? 0 :
      selected.back()->column();

  for (int i = 0; i < selected.size(); ++i) {
    rows.push_back(selected[i]->row());
  }

  vector<int>::iterator last = unique(rows.begin(), rows.end());
  rows.erase(last, rows.end());

  if (rows.empty()) {
    QMessageBox::warning(this,
                         tr("Mozc settings"),
                         tr("No entry is selected"));
    return;
  }

  // Choose next or prev item.
  QTableWidgetItem *item = editorTableWidget->item(rows.back() + 1, column);
  if (item == NULL) {
    item = editorTableWidget->item(rows.back() - 1, column);
  }

  // select item
  if (item != NULL) {
    editorTableWidget->setCurrentItem(item);
  }

  // remove from the buttom
  for (int i = rows.size() - 1; i >= 0; --i) {
    editorTableWidget->removeRow(rows[i]);
  }

  UpdateMenuStatus();
}

void GenericTableEditorDialog::InsertEmptyItem(int row) {
  editorTableWidget->verticalHeader()->hide();
  editorTableWidget->insertRow(row);
  for (size_t i = 0; i < column_size_; ++i) {
    editorTableWidget->setItem(row, i, new QTableWidgetItem(""));
  }
  QTableWidgetItem *item = editorTableWidget->item(row, 0);
  if (item != NULL) {
    editorTableWidget->setCurrentItem(item);
    editorTableWidget->scrollToItem(item,
                                    QAbstractItemView::PositionAtCenter);
    editorTableWidget->editItem(item);
  }

  UpdateMenuStatus();
}

void GenericTableEditorDialog::InsertItem() {
  QTableWidgetItem *current = editorTableWidget->currentItem();
  if (current == NULL) {
    QMessageBox::warning(this,
                         tr("Mozc settings"),
                         tr("No entry is selected"));
    return;
  }
  InsertEmptyItem(current->row() + 1);
}

void GenericTableEditorDialog::AddNewItem() {
  if (editorTableWidget->rowCount() >= max_entry_size()) {
    QMessageBox::warning(
        this,
        tr("Mozc settings"),
        tr("You can't have more than %1 entries").arg(max_entry_size()));
    return;
  }

  editorTableWidget->insertRow(editorTableWidget->rowCount());
  InsertEmptyItem(editorTableWidget->rowCount() - 1);
  editorTableWidget->setRowCount(editorTableWidget->rowCount() - 1);
  UpdateMenuStatus();
}

void GenericTableEditorDialog::Import() {
  const QString filename =
  QFileDialog::getOpenFileName(this, tr("import from file"),
                               QDir::homePath());
  if (filename.isEmpty()) {
    return;
  }

  QFile file(filename);
  if (!file.exists()) {
    QMessageBox::warning(this,
                         tr("Mozc settings"),
                         tr("File not found"));
    return;
  }

  const qint64 kMaxSize = 100 * 1024;
  if (file.size() >= kMaxSize) {
    QMessageBox::warning(this,
                         tr("Mozc settings"),
                         tr("The specified file is too large (>=100K byte)"));
    return;
  }

  InputFileStream ifs(filename.toStdString().c_str());
  if (!LoadFromStream(&ifs)) {
    QMessageBox::warning(this,
                         tr("Mozc settings"),
                         tr("Import failed"));
    return;
  }
}

void GenericTableEditorDialog::Export() {
  if (!Update()) {
    return;
  }

  const QString filename =
      QFileDialog::getSaveFileName(this, tr("export to file"),
          QDir::homePath() + QDir::separator() +
          QString(GetDefaultFilename().c_str()));
  if (filename.isEmpty()) {
    return;
  }

  OutputFileStream ofs(filename.toStdString().c_str());
  if (!ofs) {
    QMessageBox::warning(this,
                         tr("Mozc settings"),
                         tr("Export failed"));
    return;
  }

  ofs << table();
}

void GenericTableEditorDialog::Clicked(QAbstractButton *button) {
  // Workaround for http://b/242686
  // By changing the foucs, incomplete entries in QTableView are
  // submitted to the model.
  editButton->setFocus(Qt::MouseFocusReason);

  switch (editorButtonBox->buttonRole(button)) {
    /// number of role might be increased in the future.
    case QDialogButtonBox::AcceptRole:
      if (Update()) {
        QDialog::accept();
      }
      break;
    default:
      QDialog::reject();
      break;
  }
}

void GenericTableEditorDialog::OnContextMenuRequested(const QPoint &pos) {
  QTableWidgetItem *item = editorTableWidget->itemAt(pos);
  if (item == NULL) {
    return;
  }

  QMenu *menu = new QMenu(this);
  QAction *rename_action = menu->addAction(tr("New entry"));
  QAction *delete_action = menu->addAction(tr("Remove entry"));
  QAction *selected_action = menu->exec(QCursor::pos());

  if (selected_action == rename_action) {
    AddNewItem();
  } else if (selected_action == delete_action) {
    DeleteSelectedItems();
  }
}

void GenericTableEditorDialog::UpdateOKButton(bool status) {
  QPushButton *button = editorButtonBox->button(QDialogButtonBox::Ok);
  if (button != NULL) {
    button->setEnabled(status);
  }
}

size_t GenericTableEditorDialog::max_entry_size() {
  return kMaxEntrySize;
}

bool GenericTableEditorDialog::LoadFromStream(istream *is) {
  return true;
}

bool GenericTableEditorDialog::Update() {
  return true;
}

void GenericTableEditorDialog::UpdateMenuStatus() {}

void GenericTableEditorDialog::OnEditMenuAction(QAction *action) {}
}  // namespace gui
}  // namespace mozc

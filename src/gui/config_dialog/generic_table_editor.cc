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

#include "gui/config_dialog/generic_table_editor.h"

#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QtGui>
#include <cctype>
#include <cstddef>
#include <istream>
#include <sstream>
#include <string>

#include "base/file_stream.h"
#include "base/logging.h"
#include "protocol/commands.pb.h"
#include "absl/container/btree_set.h"
#include "gui/base/util.h"

namespace mozc {
namespace gui {
namespace {
constexpr size_t kMaxEntrySize = 10000;

int GetTableHeight(QTableWidget *widget) {
  // Dragon Hack:
  // Here we use "龍" to calc font size, as it looks almost square
  const QRect rect =
      QFontMetrics(widget->font()).boundingRect(QString::fromUtf8("龍"));
#ifdef _WIN32
  return static_cast<int>(rect.height() * 1.3);
#else  // _WIN32
  return static_cast<int>(rect.height() * 1.4);
#endif  // _WIN32
}
}  // namespace

GenericTableEditorDialog::GenericTableEditorDialog(QWidget *parent,
                                                   size_t column_size)
    : QDialog(parent), edit_menu_(new QMenu(this)), column_size_(column_size) {
  setupUi(this);
  editorTableWidget->setAlternatingRowColors(true);
  setWindowFlags(Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint |
                 Qt::Tool);
  editorTableWidget->setColumnCount(column_size_);

  CHECK_GT(column_size_, 0);

  // Mac style
#ifdef __APPLE__
  editorTableWidget->setShowGrid(false);
  layout()->setContentsMargins(0, 0, 0, 4);
  gridLayout->setHorizontalSpacing(12);
  gridLayout->setVerticalSpacing(12);
#endif  // __APPLE__

  editButton->setText(tr("Edit"));
  editButton->setMenu(edit_menu_);

  QObject::connect(edit_menu_, SIGNAL(triggered(QAction *)), this,
                   SLOT(OnEditMenuAction(QAction *)));

  QObject::connect(editorButtonBox, SIGNAL(clicked(QAbstractButton *)), this,
                   SLOT(Clicked(QAbstractButton *)));

  editorTableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(editorTableWidget, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(OnContextMenuRequested(const QPoint &)));

  editorTableWidget->horizontalHeader()->setStretchLastSection(true);
  editorTableWidget->horizontalHeader()->setSortIndicatorShown(true);
  editorTableWidget->horizontalHeader()->setHighlightSections(false);
  // Do not use QAbstractItemView::AllEditTriggers so that user can easily
  // select multiple items. See b/6488800.
  editorTableWidget->setEditTriggers(QAbstractItemView::AnyKeyPressed |
                                     QAbstractItemView::DoubleClicked |
                                     QAbstractItemView::SelectedClicked);
  editorTableWidget->setSortingEnabled(true);

  editorTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  editorTableWidget->verticalHeader()->setDefaultSectionSize(
      GetTableHeight(editorTableWidget));

  GuiUtil::ReplaceWidgetLabels(this);
  UpdateMenuStatus();
}

GenericTableEditorDialog::~GenericTableEditorDialog() {}

QTableWidget *GenericTableEditorDialog::mutable_table_widget() {
  return editorTableWidget;
}

const std::string &GenericTableEditorDialog::table() const { return table_; }

std::string *GenericTableEditorDialog::mutable_table() { return &table_; }

QMenu *GenericTableEditorDialog::mutable_edit_menu() { return edit_menu_; }

bool GenericTableEditorDialog::LoadFromString(const std::string &str) {
  std::istringstream istr(str);
  return LoadFromStream(&istr);
}

void GenericTableEditorDialog::DeleteSelectedItems() {
  absl::btree_set<int> rows;
  QList<QTableWidgetItem *> selected = editorTableWidget->selectedItems();

  for (auto item : selected) {
    rows.insert(item->row());
  }

  if (rows.empty()) {
    QMessageBox::warning(this, windowTitle(), tr("No entry is selected"));
    return;
  }

  // Keep the current cursor position after the deletion.
  {
    // remember the last column as user chooses the
    // last rows from top to bottom in general.
    const int cur_col = selected.back()->column();
    const int cur_row = selected.back()->row();
    QTableWidgetItem *cur_item = editorTableWidget->item(cur_row + 1, cur_col);
    if (cur_item == nullptr) {
      cur_item = editorTableWidget->item(cur_row - 1, cur_col);
    }

    // select item
    if (cur_item != nullptr) {
      editorTableWidget->setCurrentItem(cur_item);
    }
  }

  // remove from the bottom
  for (auto rit = rows.rbegin(); rit != rows.rend(); ++rit) {
    editorTableWidget->removeRow(*rit);
  }

  UpdateMenuStatus();
}

void GenericTableEditorDialog::InsertEmptyItem(int row) {
  editorTableWidget->verticalHeader()->hide();

  // It is important to disable auto-sorting before we programmatically edit
  // multiple items. Otherwise, one single edit of cell such as
  //   editorTableWidget->setItem(row, col, data);
  // will cause auto-sorting and the target row will be moved to different
  // place.
  const bool sorting_enabled = editorTableWidget->isSortingEnabled();
  if (sorting_enabled) {
    editorTableWidget->setSortingEnabled(false);
  }

  editorTableWidget->insertRow(row);
  for (size_t i = 0; i < column_size_; ++i) {
    editorTableWidget->setItem(row, i, new QTableWidgetItem(QLatin1String("")));
  }
  QTableWidgetItem *item = editorTableWidget->item(row, 0);
  if (item != nullptr) {
    editorTableWidget->setCurrentItem(item);
    editorTableWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
    editorTableWidget->editItem(item);
  }

  // Restore auto-sorting setting if necessary.
  if (sorting_enabled) {
    // From the usability perspective, auto-sorting should be disabled until
    // a user explicitly enables it again by clicking the table header.
    // To achieve it, set -1 to the |logicalIndex| in setSortIndicator.
    editorTableWidget->horizontalHeader()->setSortIndicator(-1,
                                                            Qt::AscendingOrder);
    editorTableWidget->setSortingEnabled(true);
  }

  UpdateMenuStatus();
}

void GenericTableEditorDialog::InsertItem() {
  QTableWidgetItem *current = editorTableWidget->currentItem();
  if (current == nullptr) {
    QMessageBox::warning(this, windowTitle(), tr("No entry is selected"));
    return;
  }
  InsertEmptyItem(current->row() + 1);
}

void GenericTableEditorDialog::AddNewItem() {
  if (editorTableWidget->rowCount() >= max_entry_size()) {
    QMessageBox::warning(
        this, windowTitle(),
        tr("You can't have more than %1 entries").arg(max_entry_size()));
    return;
  }

  InsertEmptyItem(editorTableWidget->rowCount());
}

void GenericTableEditorDialog::Import() {
  const QString filename = QFileDialog::getOpenFileName(
      this, tr("import from file"), QDir::homePath());
  if (filename.isEmpty()) {
    return;
  }

  QFile file(filename);
  if (!file.exists()) {
    QMessageBox::warning(this, windowTitle(), tr("File not found"));
    return;
  }

  const qint64 kMaxSize = 100 * 1024;
  if (file.size() >= kMaxSize) {
    QMessageBox::warning(this, windowTitle(),
                         tr("The specified file is too large (>=100K byte)"));
    return;
  }

  InputFileStream ifs(filename.toStdString());
  if (!LoadFromStream(&ifs)) {
    QMessageBox::warning(this, windowTitle(), tr("Import failed"));
    return;
  }
}

void GenericTableEditorDialog::Export() {
  if (!Update()) {
    return;
  }

  const QString filename = QFileDialog::getSaveFileName(
      this, tr("export to file"),
      QDir::homePath() + QDir::separator() +
          QString::fromUtf8(GetDefaultFilename().c_str()));
  if (filename.isEmpty()) {
    return;
  }

  OutputFileStream ofs(filename.toStdString());
  if (!ofs) {
    QMessageBox::warning(this, windowTitle(), tr("Export failed"));
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
  if (item == nullptr) {
    return;
  }

  QMenu *menu = new QMenu(this);
  QAction *edit_action = nullptr;
  const QList<QTableWidgetItem *> selected_items =
      editorTableWidget->selectedItems();
  if (selected_items.count() == 1) {
    edit_action = menu->addAction(tr("Edit entry"));
  }
  QAction *rename_action = menu->addAction(tr("New entry"));
  QAction *delete_action = menu->addAction(tr("Remove entry"));
  QAction *selected_action = menu->exec(QCursor::pos());

  if (edit_action != nullptr && selected_action == edit_action) {
    editorTableWidget->editItem(selected_items[0]);
  } else if (selected_action == rename_action) {
    AddNewItem();
  } else if (selected_action == delete_action) {
    DeleteSelectedItems();
  }
}

void GenericTableEditorDialog::UpdateOKButton(bool status) {
  QPushButton *button = editorButtonBox->button(QDialogButtonBox::Ok);
  if (button != nullptr) {
    button->setEnabled(status);
  }
}

size_t GenericTableEditorDialog::max_entry_size() const {
  return kMaxEntrySize;
}

bool GenericTableEditorDialog::LoadFromStream(std::istream *is) { return true; }

bool GenericTableEditorDialog::Update() { return true; }

void GenericTableEditorDialog::UpdateMenuStatus() {}

void GenericTableEditorDialog::OnEditMenuAction(QAction *action) {}
}  // namespace gui
}  // namespace mozc

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

#include "gui/dictionary_tool/find_dialog.h"

#include <QtGui/QtGui>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTableWidget>

#include "base/logging.h"
#include "gui/base/util.h"

namespace mozc {
namespace gui {
namespace {
const char kYellowSelectionStyleSheet[] =
    "selection-background-color : yellow;";
}

FindDialog::FindDialog(QWidget *parent, QTableWidget *table)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      table_(table),
      last_item_(nullptr) {
  setupUi(this);
  setModal(false);

  connect(QuerylineEdit, SIGNAL(textChanged(const QString &)), this,
          SLOT(LineEditChanged(const QString &)));
  connect(FindForwardpushButton, SIGNAL(clicked()), this, SLOT(FindForward()));
  connect(FindBackwardpushButton, SIGNAL(clicked()), this,
          SLOT(FindBackward()));
  connect(CancelpushButton, SIGNAL(clicked()), this, SLOT(close()));
  GuiUtil::ReplaceWidgetLabels(this);
}

FindDialog::~FindDialog() {}

void FindDialog::LineEditChanged(const QString &str) { UpdateUIStatus(); }

void FindDialog::showEvent(QShowEvent *event) {
  QuerylineEdit->setFocus(Qt::OtherFocusReason);
  if (!QuerylineEdit->text().isEmpty()) {
    QuerylineEdit->selectAll();
  }
  FindForwardpushButton->setDefault(true);
  last_item_ = nullptr;
  UpdateUIStatus();
}

void FindDialog::closeEvent(QCloseEvent *event) {
  table_->setStyleSheet(QLatin1String(""));
  last_item_ = nullptr;
}

void FindDialog::UpdateUIStatus() {
  const bool enabled = !QuerylineEdit->text().isEmpty();
  FindForwardpushButton->setEnabled(enabled);
  FindBackwardpushButton->setEnabled(enabled);
}

bool FindDialog::Match(const QString &query, int row, int column) {
  if (last_item_ != nullptr && last_item_ == table_->item(row, column)) {
    return false;
  }
  const QString &value = table_->item(row, column)->text();
  return value.contains(query, Qt::CaseInsensitive);
}

void FindDialog::FindForward() {
  FindForwardpushButton->setDefault(true);
  Find(FORWARD);
}

void FindDialog::FindBackward() {
  FindBackwardpushButton->setDefault(true);
  Find(BACKWARD);
}

void FindDialog::Find(FindDialog::Direction direction) {
  const QString &query = QuerylineEdit->text();
  const int start_row = std::max(0, table_->currentRow());
  int start_column = std::min(1, std::max(0, table_->currentColumn()));
  int matched_column = -1;
  int matched_row = -1;

  switch (direction) {
    case FORWARD:
      for (int row = start_row; row < table_->rowCount(); ++row) {
        for (int column = start_column; column < 2; ++column) {
          start_column = 0;
          if (Match(query, row, column)) {
            matched_row = row;
            matched_column = column;
            goto FOUND;
          }
        }
      }
      break;
    case BACKWARD:
      for (int row = start_row; row >= 0; --row) {
        for (int column = start_column; column >= 0; --column) {
          start_column = 1;
          if (Match(query, row, column)) {
            matched_row = row;
            matched_column = column;
            goto FOUND;
          }
        }
      }
      break;
    default:
      LOG(FATAL) << "Unknown direction: " << static_cast<int>(direction);
  }

FOUND:

  if (matched_row >= 0 && matched_column >= 0) {
    QTableWidgetItem *item = table_->item(matched_row, matched_column);
    DCHECK(item);
    last_item_ = item;
    table_->setStyleSheet(QLatin1String(kYellowSelectionStyleSheet));
    table_->setCurrentItem(item);
    table_->scrollToItem(item);
  } else {
    last_item_ = nullptr;
    QMessageBox::information(this, this->windowTitle(),
                             tr("Cannot find pattern %1").arg(query));
  }
}
}  // namespace gui
}  // namespace mozc

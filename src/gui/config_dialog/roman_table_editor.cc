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

#include "gui/config_dialog/roman_table_editor.h"

#include <QtGui/QtGui>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/util.h"
#include "session/commands.pb.h"

namespace mozc {
namespace gui {
namespace {
enum {
  NEW_INDEX              = 0,
  REMOVE_INDEX           = 1,
  IMPORT_FROM_FILE_INDEX = 2,
  EXPORT_TO_FILE_INDEX   = 3,
  RESET_INDEX            = 4,
  MENU_SIZE              = 5
};
const char kRomanTableFile[] = "system://romanji-hiragana.tsv";
}  // namespace

RomanTableEditorDialog::RomanTableEditorDialog(QWidget *parent)
    : GenericTableEditorDialog(parent, 3) {
  actions_.reset(new QAction * [MENU_SIZE]);
  actions_[NEW_INDEX] = mutable_edit_menu()->addAction(tr("New entry"));
  actions_[REMOVE_INDEX] =
      mutable_edit_menu()->addAction(tr("Remove selected entries"));
  mutable_edit_menu()->addSeparator();
  actions_[IMPORT_FROM_FILE_INDEX] =
      mutable_edit_menu()->addAction(tr("Import from file..."));
  actions_[EXPORT_TO_FILE_INDEX] =
      mutable_edit_menu()->addAction(tr("Export to file..."));
  mutable_edit_menu()->addSeparator();
  actions_[RESET_INDEX] =
      mutable_edit_menu()->addAction(tr("Reset to defaults"));

  setWindowTitle(tr("Mozc Romaji table editor"));
  CHECK(mutable_table_widget());
  CHECK_EQ(mutable_table_widget()->columnCount(), 3);
  QStringList headers;
  headers << tr("Input") << tr("Output") << tr("Next input");
  mutable_table_widget()->setHorizontalHeaderLabels(headers);

  resize(330, 350);

  UpdateMenuStatus();
}

RomanTableEditorDialog::~RomanTableEditorDialog() {}

string RomanTableEditorDialog::GetDefaultRomanTable() {
  scoped_ptr<istream> ifs(ConfigFileStream::Open(kRomanTableFile));
  CHECK(ifs.get() != NULL);  // should never happen
  string line, result;
  vector<string> fields;
  while (getline(*ifs.get(), line)) {
    if (line.empty()) {
      continue;
    }
    Util::ChopReturns(&line);
    fields.clear();
    Util::SplitStringAllowEmpty(line, "\t", &fields);
    if (fields.size() < 2) {
      VLOG(3) << "field size < 2";
      continue;
    }
    result += fields[0];
    result += '\t';
    result += fields[1];
    if (fields.size() >= 3) {
      result += '\t';
      result += fields[2];
    }
    result += '\n';
  }
  return result;
}

bool RomanTableEditorDialog::LoadFromStream(istream *is) {
  CHECK(is);
  string line;
  vector<string> fields;
  mutable_table_widget()->setRowCount(0);
  mutable_table_widget()->verticalHeader()->hide();

  int row = 0;
  while (getline(*is, line)) {
    if (line.empty()) {
      continue;
    }
    Util::ChopReturns(&line);

    fields.clear();
    Util::SplitStringAllowEmpty(line, "\t", &fields);
    if (fields.size() < 2) {
      VLOG(3) << "field size < 2";
      continue;
    }

    if (fields.size() == 2) {
      fields.push_back("");
    }

    QTableWidgetItem *input
        = new QTableWidgetItem(QString::fromUtf8(fields[0].c_str()));
    QTableWidgetItem *output
        = new QTableWidgetItem(QString::fromUtf8(fields[1].c_str()));
    QTableWidgetItem *pending
        = new QTableWidgetItem(QString::fromUtf8(fields[2].c_str()));

    mutable_table_widget()->insertRow(row);
    mutable_table_widget()->setItem(row, 0, input);
    mutable_table_widget()->setItem(row, 1, output);
    mutable_table_widget()->setItem(row, 2, pending);
    ++row;

    if (row >= GenericTableEditorDialog::max_entry_size()) {
      QMessageBox::warning(
          this,
          tr("Mozc settings"),
          tr("You can't have more than %1 entries").arg(
              GenericTableEditorDialog::max_entry_size()));
      break;
    }
  }

  UpdateMenuStatus();

  return true;
}

bool RomanTableEditorDialog::LoadDefaultRomanTable() {
  scoped_ptr<istream> ifs(ConfigFileStream::Open(kRomanTableFile));
  CHECK(ifs.get() != NULL);  // should never happen
  CHECK(LoadFromStream(ifs.get()));
  return true;
}

bool RomanTableEditorDialog::Update() {
  if (mutable_table_widget()->rowCount() == 0) {
    QMessageBox::warning(this,
                         tr("Mozc settings"),
                         tr("Romaji to Kana table is empty."));
    return false;
  }

  string *table = mutable_table();
  table->clear();
  for (int i = 0; i < mutable_table_widget()->rowCount(); ++i) {
    const string input =
        mutable_table_widget()->item(i, 0)->text().toStdString();
    const string output =
        mutable_table_widget()->item(i, 1)->text().toStdString();
    const string pending =
        mutable_table_widget()->item(i, 2)->text().toStdString();
    if (input.empty() || (output.empty() && pending.empty())) {
      continue;
    }
    *table += input;
    *table += '\t';
    *table += output;
    if (!pending.empty()) {
      *table += '\t';
      *table += pending;
    }
    *table += '\n';
  }

  return true;
}

void RomanTableEditorDialog::UpdateMenuStatus() {
  const bool status = (mutable_table_widget()->rowCount() > 0);
  actions_[RESET_INDEX]->setEnabled(status);
  actions_[REMOVE_INDEX]->setEnabled(status);
  UpdateOKButton(status);
}

void RomanTableEditorDialog::OnEditMenuAction(QAction *action) {
  if (action == actions_[NEW_INDEX]) {
    AddNewItem();
  } else if (action == actions_[REMOVE_INDEX]) {
    DeleteSelectedItems();
  } else if (action == actions_[IMPORT_FROM_FILE_INDEX] ||
             action == actions_[RESET_INDEX]) {   // import or reset
    if (mutable_table_widget()->rowCount() > 0 &&
        QMessageBox::Ok !=
        QMessageBox::question(
            this,
            tr("Mozc settings"),
            tr("Do you want to overwrite the current roman table?"),
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Cancel)) {
      return;
    }

    if (action == actions_[IMPORT_FROM_FILE_INDEX]) {
      Import();
    } else if (action == actions_[RESET_INDEX]) {
      LoadDefaultRomanTable();
    }
  } else if (action == actions_[EXPORT_TO_FILE_INDEX]) {
    Export();
  }

  return;
}

// static
bool RomanTableEditorDialog::Show(QWidget *parent,
                                  const string &current_roman_table,
                                  string *new_roman_table) {
  RomanTableEditorDialog window(parent);
  if (current_roman_table.empty()) {
    window.LoadDefaultRomanTable();
  } else {
    window.LoadFromString(current_roman_table);
  }

  // open modal mode
  const bool result = (QDialog::Accepted == window.exec());
  new_roman_table->clear();

  if (result &&
      window.table() != window.GetDefaultRomanTable()) {
    *new_roman_table = window.table();
  }

  return result;
}
}  // namespace gui
}  // namespace mozc

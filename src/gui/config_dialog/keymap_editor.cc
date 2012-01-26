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

#include "gui/config_dialog/keymap_editor.h"

#include <QtCore/QFile>
#include <QtGui/QFileDialog>
#include <QtGui/QtGui>
#include <algorithm>  // for unique
#include <cctype>
#include <set>
#include <string>
#include <sstream>
#include <vector>
#include "base/base.h"
#include "base/singleton.h"
#include "base/config_file_stream.h"
#include "base/util.h"
#include "gui/config_dialog/combobox_delegate.h"
#include "gui/config_dialog/keybinding_editor_delegate.h"
#include "session/commands.pb.h"
#include "session/key_parser.h"
#include "session/internal/keymap.h"
// TODO(komatsu): internal files should not be used from external modules.

namespace mozc {
namespace gui {
namespace {

config::Config::SessionKeymap kKeyMaps[] = {
  config::Config::ATOK,
  config::Config::MSIME,
  config::Config::KOTOERI,
};

const char *kKeyMapStatus[] = {
  "DirectInput",
  "Precomposition",
  "Composition",
  "Conversion",
  "Suggestion",
  "Prediction",
};

const char kAbortCommand[] = "Abort";
const char kInsertCharacterCommand[] = "InsertCharacter";
const char kIMEOnCommand[] = "IMEOn";
const char kIMEOffCommand[] = "IMEOff";
const char kReconvertCommand[] = "Reconvert";
const char kReportBugCommand[] = "ReportBug";
// Old command name
const char kEditInsertCommand[] = "EditInsert";

enum {
  NEW_INDEX              = 0,
  REMOVE_INDEX           = 1,
  IMPORT_FROM_FILE_INDEX = 2,
  EXPORT_TO_FILE_INDEX   = 3,
  MENU_SIZE              = 4
};

// Keymap validator for deciding that input is configurable
class KeyMapValidator {
 public:
  KeyMapValidator() {
    invisible_commands_.insert(kAbortCommand);
    invisible_commands_.insert(kInsertCharacterCommand);
    invisible_commands_.insert(kReportBugCommand);
    // Old command name.
    invisible_commands_.insert(kEditInsertCommand);
#if defined(OS_MACOSX)
    // On Mac, we cannot customize keybindings for IME ON/OFF
    // So we do not show them.
    // TODO(toshiyuki): remove them after implimenting IME ON/OFF for Mac
    invisible_commands_.insert(kIMEOnCommand);
    invisible_commands_.insert(kIMEOffCommand);
#endif  // OS_MACOSX

    invisible_modifiers_.insert(mozc::commands::KeyEvent::KEY_DOWN);
    invisible_modifiers_.insert(mozc::commands::KeyEvent::KEY_UP);

    invisible_key_events_.insert(mozc::commands::KeyEvent::KANJI);
    invisible_key_events_.insert(mozc::commands::KeyEvent::ON);
    invisible_key_events_.insert(mozc::commands::KeyEvent::OFF);
    invisible_key_events_.insert(mozc::commands::KeyEvent::ASCII);
  }

  bool IsVisibleKey(const string &key) {
    mozc::commands::KeyEvent key_event;
    const bool parse_success = mozc::KeyParser::ParseKey(key, &key_event);
    if (!parse_success) {
      VLOG(3) << "key parse failed";
      return false;
    }
    for (size_t i = 0; i < key_event.modifier_keys_size(); ++i) {
      if (invisible_modifiers_.find(key_event.modifier_keys(i))
          != invisible_modifiers_.end()) {
        VLOG(3) << "invisible modifiers: " << key_event.modifier_keys(i);
        return false;
      }
    }
    if (key_event.has_special_key() &&
        (invisible_key_events_.find(key_event.special_key())
         != invisible_key_events_.end())) {
      VLOG(3) << "invisible special key: " << key_event.special_key();
      return false;
    }
    return true;
  }

  bool IsVisibleStatus(const string &status) {
    // no validation for now.
    return true;
  }

  bool IsVisibleCommand(const string &command) {
    if (invisible_commands_.find(command) == invisible_commands_.end()) {
      return true;
    }
    VLOG(3) << "invisible command: " << command;
    return false;
  }

  // Returns true if the key map entry is valid
  // invalid keymaps are not exported/imported.
  bool IsValidEntry(const vector<string> &fields) {
    if (fields.size() < 3) {
      return false;
    }

#ifndef _DEBUG
    if (fields[2] == kAbortCommand) {
      return false;
    }
#endif

#ifdef NO_LOGGING
    if (fields[2] == kReportBugCommand) {
      return false;
    }
#endif
    return true;
  }

  // Returns true if the key map entry is configurable and
  // we want to show them.
  bool IsVisibleEntry(const vector<string> &fields) {
    if (fields.size() < 3) {
      return false;
    }
    const string &key = fields[1];
    const string &command = fields[2];
    if (!IsVisibleKey(key)) {
      return false;
    }
    if (!IsVisibleCommand(command)) {
      return false;
    }

    return true;
  }
 private:
  set<uint32> invisible_modifiers_;
  set<uint32> invisible_key_events_;
  set<string> invisible_commands_;
};

class KeyMapTableLoader {
 public:
  KeyMapTableLoader() {
    string line;
    vector<string> fields;
    set<string> status;
    set<string> commands;
    KeyMapValidator *validator = mozc::Singleton<KeyMapValidator>::get();

    // get all command names
    set<string> command_names;
    mozc::keymap::KeyMapManager manager;
    manager.GetAvailableCommandNameDirect(&command_names);
    manager.GetAvailableCommandNamePrecomposition(&command_names);
    manager.GetAvailableCommandNameComposition(&command_names);
    manager.GetAvailableCommandNameConversion(&command_names);
    manager.GetAvailableCommandNameSuggestion(&command_names);
    manager.GetAvailableCommandNamePrediction(&command_names);
    for (set<string>::const_iterator itr = command_names.begin();
         itr != command_names.end(); ++itr) {
      if (validator->IsVisibleCommand(*itr)) {
        commands.insert(*itr);
      }
    }

    for (size_t i = 0; i < arraysize(kKeyMapStatus); ++i) {
      status_ << QString::fromUtf8(kKeyMapStatus[i]);
    }

    for (set<string>::const_iterator it = commands.begin();
         it != commands.end(); ++it) {
      commands_ << QString::fromUtf8(it->c_str());
    }
  }

  const QStringList &status() { return status_; }
  const QStringList &commands() { return commands_; }

 private:
  QStringList status_;
  QStringList commands_;
};
}  // namespace

KeyMapEditorDialog::KeyMapEditorDialog(QWidget *parent)
    : GenericTableEditorDialog(parent, 3),
      status_delegate_(new ComboBoxDelegate),
      commands_delegate_(new ComboBoxDelegate),
      keybinding_delegate_(new KeyBindingEditorDelegate) {
  actions_.reset(new QAction * [MENU_SIZE]);
  import_actions_.reset(new QAction * [arraysize(kKeyMaps)]);

  actions_[NEW_INDEX] =  mutable_edit_menu()->addAction(tr("New entry"));
  actions_[REMOVE_INDEX] =
    mutable_edit_menu()->addAction(tr("Remove selected entries"));
  mutable_edit_menu()->addSeparator();

  QMenu *sub_menu =
               mutable_edit_menu()->addMenu(tr("Import predefined mapping"));
  DCHECK(sub_menu);

  // Make sure that the order should be the same as kKeyMapTableFiles
  import_actions_[0] = sub_menu->addAction(tr("ATOK"));
  import_actions_[1] = sub_menu->addAction(tr("MS-IME"));
  import_actions_[2] = sub_menu->addAction(tr("Kotoeri"));

  mutable_edit_menu()->addSeparator();
  actions_[IMPORT_FROM_FILE_INDEX] =
      mutable_edit_menu()->addAction(tr("Import from file..."));
  actions_[EXPORT_TO_FILE_INDEX] =
      mutable_edit_menu()->addAction(tr("Export to file..."));

  // expand the last "Command" column
  mutable_table_widget()->setColumnWidth
      (0, static_cast<int>(mutable_table_widget()->columnWidth(0) * 1.5));
  mutable_table_widget()->setColumnWidth
      (1, static_cast<int>(mutable_table_widget()->columnWidth(1) * 1.1));
  mutable_table_widget()->horizontalHeader()->setStretchLastSection(true);

  KeyMapTableLoader *loader = Singleton<KeyMapTableLoader>::get();

  // Generate i18n status list
  const QStringList &statuses = loader->status();
  QStringList i18n_statuses;
  for (size_t i = 0; i < statuses.size(); ++i) {
    const QString i18n_status = tr(statuses[i].toStdString().data());
    i18n_statuses.append(i18n_status);
    normalized_status_map_.insert(
        make_pair(i18n_status.toStdString(), statuses[i].toStdString()));
  }
  status_delegate_->SetItemList(i18n_statuses);

  // Generate i18n command list.
  const QStringList &commands = loader->commands();
  QStringList i18n_commands;
  for (size_t i = 0; i < commands.size(); ++i) {
    const QString i18n_command = tr(commands[i].toStdString().data());
    i18n_commands.append(i18n_command);
    normalized_command_map_.insert(
        make_pair(i18n_command.toStdString(), commands[i].toStdString()));
  }
  i18n_commands.sort();
  commands_delegate_->SetItemList(i18n_commands);

  mutable_table_widget()->setItemDelegateForColumn(0,
                                                   status_delegate_.get());
  mutable_table_widget()->setItemDelegateForColumn(1,
                                                   keybinding_delegate_.get());
  mutable_table_widget()->setItemDelegateForColumn(2,
                                                   commands_delegate_.get());
  mutable_table_widget()->setEditTriggers(QAbstractItemView::AllEditTriggers);

  setWindowTitle(tr("Mozc keymap editor"));
  CHECK(mutable_table_widget());
  CHECK_EQ(mutable_table_widget()->columnCount(), 3);
  QStringList headers;
  headers << tr("Mode") << tr("Key") << tr("Command");
  mutable_table_widget()->setHorizontalHeaderLabels(headers);

  resize(500, 350);

  UpdateMenuStatus();
}

KeyMapEditorDialog::~KeyMapEditorDialog() {}

bool KeyMapEditorDialog::LoadFromStream(istream *is) {
  if (is == NULL) {
    return false;
  }

  string line;
  if (!getline(*is, line)) {   // must have 1st line
    return false;
  }

  vector<string> fields;
  int row = 0;
  mutable_table_widget()->setRowCount(0);
  mutable_table_widget()->verticalHeader()->hide();

  invisible_keymap_table_.clear();
  ime_switch_keymap_.clear();
  while (getline(*is, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    Util::ChopReturns(&line);

    fields.clear();
    Util::SplitStringUsing(line, "\t", &fields);
    if (fields.size() < 3) {
      VLOG(3) << "field size < 3";
      continue;
    }

    // don't accept invalid keymap entries.
    if (!Singleton<KeyMapValidator>::get()->IsValidEntry(fields)) {
      VLOG(3) << "invalid entry.";
      continue;
    }

    // don't show invisible (not configurable) keymap entries.
    if (!Singleton<KeyMapValidator>::get()->IsVisibleEntry(fields)) {
      VLOG(3) << "invalid entry to show. add to invisible_keymap_table_";
      invisible_keymap_table_ += fields[0];
      invisible_keymap_table_ += '\t';
      invisible_keymap_table_ += fields[1];
      invisible_keymap_table_ += '\t';
      invisible_keymap_table_ += fields[2];
      invisible_keymap_table_ += '\n';
      continue;
    }

    if (fields[2] == kIMEOnCommand || fields[2] == kReconvertCommand) {
      ime_switch_keymap_.insert(line);
    }

    QTableWidgetItem *status
        = new QTableWidgetItem(tr(fields[0].c_str()));
    QTableWidgetItem *key
        = new QTableWidgetItem(QString::fromUtf8(fields[1].c_str()));
    QTableWidgetItem *command
        = new QTableWidgetItem(tr(fields[2].c_str()));

    mutable_table_widget()->insertRow(row);
    mutable_table_widget()->setItem(row, 0, status);
    mutable_table_widget()->setItem(row, 1, key);
    mutable_table_widget()->setItem(row, 2, command);
    ++row;
  }

  UpdateMenuStatus();

  return true;
}

bool KeyMapEditorDialog::Update() {
  if (mutable_table_widget()->rowCount() == 0) {
    QMessageBox::warning(this,
                         tr("Mozc settings"),
                         tr("Current keymap table is empty. "
                            "You might want to import a pre-defined "
                            "keymap table first"));
    return false;
  }

  set<string> new_ime_switch_keymap;

  KeyMapValidator *validator = Singleton<KeyMapValidator>::get();
  string *keymap_table = mutable_table();

  *keymap_table = "status\tkey\tcommand\n";

  for (int i = 0; i < mutable_table_widget()->rowCount(); ++i) {
    const string i18n_status =
        mutable_table_widget()->item(i, 0)->text().toStdString();
    const string key =
        mutable_table_widget()->item(i, 1)->text().toStdString();
    const string i18n_command =
        mutable_table_widget()->item(i, 2)->text().toStdString();

    const map<string, string>::const_iterator status_it =
        normalized_status_map_.find(i18n_status);
    if (status_it == normalized_status_map_.end()) {
      LOG(ERROR) << "Unsupported i18n status name: " << i18n_status;
      continue;
    }
    const string &status = status_it->second;

    const map<string, string>::const_iterator command_it =
        normalized_command_map_.find(i18n_command);
    if (command_it == normalized_command_map_.end()) {
      LOG(ERROR) << "Unsupported i18n command name:" << i18n_command;
      continue;
    }
    const string &command = command_it->second;

    if (!validator->IsVisibleKey(key)) {
      QMessageBox::warning(this,
                           tr("Mozc settings"),
                           (tr("Invalid key:\n%1")
                            .arg(QString::fromUtf8(key.c_str()))));
      return false;
    }
    const string keymap_line = status + "\t" + key + "\t" + command;
    *keymap_table += keymap_line;
    *keymap_table += '\n';

    if (command == kIMEOnCommand || command == kReconvertCommand) {
      new_ime_switch_keymap.insert(keymap_line);
    }
  }
  *keymap_table += invisible_keymap_table_;

  if (new_ime_switch_keymap != ime_switch_keymap_) {
#if defined(OS_WINDOWS) || defined(OS_LINUX)
    QMessageBox::information(
        this,
        tr("Mozc settings"),
        tr("The keymaps for IME ON and Reconversion will be "
           "applied after new applications."));
#endif  // OS_WINDOWS || OS_LINUX
    ime_switch_keymap_ = new_ime_switch_keymap;
  }

  return true;
}

void KeyMapEditorDialog::UpdateMenuStatus() {
  const bool status = (mutable_table_widget()->rowCount() > 0);
  actions_[REMOVE_INDEX]->setEnabled(status);
  actions_[EXPORT_TO_FILE_INDEX]->setEnabled(status);
  UpdateOKButton(status);
}

void KeyMapEditorDialog::OnEditMenuAction(QAction *action) {
  int import_index = -1;
  for (size_t i = 0; i < arraysize(kKeyMaps); ++i) {
    if (import_actions_[i] == action) {
      import_index = i;
      break;
    }
  }

  if (action == actions_[NEW_INDEX]) {
    AddNewItem();
  } else if (action == actions_[REMOVE_INDEX]) {
    DeleteSelectedItems();
  } else if (import_index != -1 ||
             action == actions_[IMPORT_FROM_FILE_INDEX]) {
    if (mutable_table_widget()->rowCount() > 0 &&
        QMessageBox::Ok !=
        QMessageBox::question(
            this,
            tr("Mozc settings"),
            tr("Do you want to overwrite the current keymaps?"),
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Cancel)) {
      return;
    }

    // import_category_index means Import from file
    if (action == actions_[IMPORT_FROM_FILE_INDEX]) {
      Import();
    // otherwise, load from predefined tables
    } else if (import_index >= 0 &&
               import_index < arraysize(kKeyMaps)) {
      const char *keymap_file =
          keymap::KeyMapManager::GetKeyMapFileName(kKeyMaps[import_index]);
      scoped_ptr<istream> ifs(
          ConfigFileStream::Open(keymap_file));
      CHECK(ifs.get() != NULL);  // should never happen
      CHECK(LoadFromStream(ifs.get()));
    }
  } else if (action == actions_[EXPORT_TO_FILE_INDEX]) {
    Export();
  }

  return;
}

// static
bool KeyMapEditorDialog::Show(QWidget *parent,
                              const string &current_keymap,
                              string *new_keymap) {
  KeyMapEditorDialog window(parent);
  window.LoadFromString(current_keymap);

  // open modal mode
  const bool result = (QDialog::Accepted == window.exec());
  new_keymap->clear();
  if (result) {
    *new_keymap = window.table();
  }
  return result;
}
}  // namespace gui
}  // namespace mozc

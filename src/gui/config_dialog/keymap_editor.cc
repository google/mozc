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

#include "gui/config_dialog/keymap_editor.h"

#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QtGui>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/config_file_stream.h"
#include "base/singleton.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/key_parser.h"
#include "gui/base/table_util.h"
#include "gui/base/util.h"
#include "gui/config_dialog/combobox_delegate.h"
#include "gui/config_dialog/generic_table_editor.h"
#include "gui/config_dialog/keybinding_editor_delegate.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/keymap.h"

#if defined(__ANDROID__) || defined(__wasm__)
#error "This platform is not supported."
#endif  // __ANDROID__ || __wasm__

namespace mozc {
namespace gui {
namespace {

constexpr config::Config::SessionKeymap kKeyMaps[] = {
    config::Config::ATOK,
    config::Config::MSIME,
    config::Config::KOTOERI,
};

constexpr absl::string_view kKeyMapStatus[] = {
    "DirectInput", "Precomposition", "Composition",
    "Conversion",  "Suggestion",     "Prediction",
};

constexpr char kInsertCharacterCommand[] = "InsertCharacter";
constexpr char kDirectMode[] = "DirectInput";
constexpr char kReportBugCommand[] = "ReportBug";
// Old command name
constexpr char kEditInsertCommand[] = "EditInsert";

#if defined(__APPLE__)
constexpr char kIMEOnCommand[] = "IMEOn";
constexpr char kIMEOffCommand[] = "IMEOff";
#endif  // __APPLE__

enum {
  NEW_INDEX = 0,
  REMOVE_INDEX = 1,
  IMPORT_FROM_FILE_INDEX = 2,
  EXPORT_TO_FILE_INDEX = 3,
  MENU_SIZE = 4
};

// Keymap validator for deciding that input is configurable
class KeyMapValidator {
 public:
  KeyMapValidator() {
    invisible_commands_.insert(kInsertCharacterCommand);
    invisible_commands_.insert(kReportBugCommand);
    // Old command name.
    invisible_commands_.insert(kEditInsertCommand);
#if defined(__APPLE__)
    // On Mac, we cannot customize keybindings for IME ON/OFF
    // So we do not show them.
    // TODO(toshiyuki): remove them after implementing IME ON/OFF for Mac
    invisible_commands_.insert(kIMEOnCommand);
    invisible_commands_.insert(kIMEOffCommand);
#endif  // __APPLE__

    invisible_modifiers_.insert(mozc::commands::KeyEvent::KEY_DOWN);
    invisible_modifiers_.insert(mozc::commands::KeyEvent::KEY_UP);

    invisible_key_events_.insert(mozc::commands::KeyEvent::KANJI);
    invisible_key_events_.insert(mozc::commands::KeyEvent::ON);
    invisible_key_events_.insert(mozc::commands::KeyEvent::OFF);
    invisible_key_events_.insert(mozc::commands::KeyEvent::TEXT_INPUT);
  }

  bool IsVisibleKey(const absl::string_view key) {
    mozc::commands::KeyEvent key_event;
    const bool parse_success = mozc::KeyParser::ParseKey(key, &key_event);
    if (!parse_success) {
      MOZC_VLOG(3) << "key parse failed";
      return false;
    }
    for (size_t i = 0; i < key_event.modifier_keys_size(); ++i) {
      if (invisible_modifiers_.find(key_event.modifier_keys(i)) !=
          invisible_modifiers_.end()) {
        MOZC_VLOG(3) << "invisible modifiers: " << key_event.modifier_keys(i);
        return false;
      }
    }
    if (key_event.has_special_key() &&
        (invisible_key_events_.find(key_event.special_key()) !=
         invisible_key_events_.end())) {
      MOZC_VLOG(3) << "invisible special key: " << key_event.special_key();
      return false;
    }
    return true;
  }

  bool IsVisibleStatus(const absl::string_view status) {
    // no validation for now.
    return true;
  }

  bool IsVisibleCommand(const absl::string_view command) {
    if (invisible_commands_.contains(command)) {
      MOZC_VLOG(3) << "invisible command: " << command;
      return false;
    }
    return true;
  }

  // Returns true if the key map entry is valid
  // invalid keymaps are not exported/imported.
  bool IsValidEntry(absl::Span<const std::string> fields) {
    if (fields.size() < 3) {
      return false;
    }

#ifdef NDEBUG
    if (fields[2] == kReportBugCommand) {
      return false;
    }
#endif  // NDEBUG
    return true;
  }

  // Returns true if the key map entry is configurable and
  // we want to show them.
  bool IsVisibleEntry(absl::Span<const std::string> fields) {
    if (fields.size() < 3) {
      return false;
    }
    const std::string &key = fields[1];
    const std::string &command = fields[2];
    if (!IsVisibleKey(key)) {
      return false;
    }
    if (!IsVisibleCommand(command)) {
      return false;
    }

    return true;
  }

 private:
  absl::flat_hash_set<uint32_t> invisible_modifiers_;
  absl::flat_hash_set<uint32_t> invisible_key_events_;
  absl::flat_hash_set<std::string> invisible_commands_;
};

class KeyMapTableLoader {
 public:
  KeyMapTableLoader() {
    absl::flat_hash_set<std::string> commands;
    KeyMapValidator *validator = mozc::Singleton<KeyMapValidator>::get();

    // get all command names
    absl::flat_hash_set<std::string> command_names;
    mozc::keymap::KeyMapManager manager;
    manager.AppendAvailableCommandNameDirect(command_names);
    manager.AppendAvailableCommandNamePrecomposition(command_names);
    manager.AppendAvailableCommandNameComposition(command_names);
    manager.AppendAvailableCommandNameConversion(command_names);
    manager.AppendAvailableCommandNameZeroQuerySuggestion(command_names);
    manager.AppendAvailableCommandNameSuggestion(command_names);
    manager.AppendAvailableCommandNamePrediction(command_names);
    for (auto it = std::make_move_iterator(command_names.begin());
         it != std::make_move_iterator(command_names.end()); ++it) {
      if (validator->IsVisibleCommand(*it)) {
        commands.insert(*it);  // implied move
      }
    }
    for (const std::string &command : commands) {
      commands_ << QString::fromStdString(command);
    }

    for (const absl::string_view status : kKeyMapStatus) {
      status_ << QString::fromUtf8(status.data(), status.size());
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
      actions_(MENU_SIZE),
      import_actions_(std::size(kKeyMaps)),
      status_delegate_(std::make_unique<ComboBoxDelegate>()),
      commands_delegate_(std::make_unique<ComboBoxDelegate>()),
      keybinding_delegate_(std::make_unique<KeyBindingEditorDelegate>()) {
  actions_[NEW_INDEX] = mutable_edit_menu()->addAction(tr("New entry"));
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
  mutable_table_widget()->setColumnWidth(
      0, static_cast<int>(mutable_table_widget()->columnWidth(0) * 1.5));
  mutable_table_widget()->setColumnWidth(
      1, static_cast<int>(mutable_table_widget()->columnWidth(1) * 1.1));
  mutable_table_widget()->horizontalHeader()->setStretchLastSection(true);

  KeyMapTableLoader *loader = Singleton<KeyMapTableLoader>::get();

  // Generate i18n status list
  const QStringList &statuses = loader->status();
  QStringList i18n_statuses;
  for (size_t i = 0; i < statuses.size(); ++i) {
    const QString i18n_status = tr(statuses[i].toStdString().data());
    i18n_statuses.append(i18n_status);
    normalized_status_map_.insert(
        std::make_pair(i18n_status.toStdString(), statuses[i].toStdString()));
  }
  status_delegate_->SetItemList(i18n_statuses);

  // Generate i18n command list.
  const QStringList &commands = loader->commands();
  QStringList i18n_commands;
  for (size_t i = 0; i < commands.size(); ++i) {
    const QString i18n_command = tr(commands[i].toStdString().data());
    i18n_commands.append(i18n_command);
    normalized_command_map_.insert(
        std::make_pair(i18n_command.toStdString(), commands[i].toStdString()));
  }
  i18n_commands.sort();
  commands_delegate_->SetItemList(i18n_commands);

  mutable_table_widget()->setItemDelegateForColumn(0, status_delegate_.get());
  mutable_table_widget()->setItemDelegateForColumn(1,
                                                   keybinding_delegate_.get());
  mutable_table_widget()->setItemDelegateForColumn(2, commands_delegate_.get());

  setWindowTitle(tr("[ProductName] keymap editor"));
  GuiUtil::ReplaceWidgetLabels(this);

  CHECK(mutable_table_widget());
  CHECK_EQ(mutable_table_widget()->columnCount(), 3);
  QStringList headers;
  headers << tr("Mode") << tr("Key") << tr("Command");
  mutable_table_widget()->setHorizontalHeaderLabels(headers);

  resize(500, 350);

  UpdateMenuStatus();
}

bool KeyMapEditorDialog::LoadFromStream(std::istream *is) {
  if (is == nullptr) {
    return false;
  }

  std::string line;
  if (!std::getline(*is, line)) {  // must have 1st line
    return false;
  }

  int row = 0;
  mutable_table_widget()->setRowCount(0);
  mutable_table_widget()->verticalHeader()->hide();

  invisible_keymap_table_.clear();
  direct_mode_commands_.clear();
  while (std::getline(*is, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    Util::ChopReturns(&line);

    std::vector<std::string> fields =
        absl::StrSplit(line, '\t', absl::SkipEmpty());
    if (fields.size() < 3) {
      MOZC_VLOG(3) << "field size < 3";
      continue;
    }

    const std::string &status = fields[0];
    const std::string &key = fields[1];
    const std::string &command = fields[2];

    // don't accept invalid keymap entries.
    if (!Singleton<KeyMapValidator>::get()->IsValidEntry(fields)) {
      MOZC_VLOG(3) << "invalid entry.";
      continue;
    }

    // don't show invisible (not configurable) keymap entries.
    if (!Singleton<KeyMapValidator>::get()->IsVisibleEntry(fields)) {
      MOZC_VLOG(3) << "invalid entry to show. add to invisible_keymap_table_";
      absl::StrAppend(&invisible_keymap_table_, status, "\t", key, "\t",
                      command, "\n");
      continue;
    }

    if (status == kDirectMode) {
      direct_mode_commands_.insert(key);
    }

    QTableWidgetItem *status_item = new QTableWidgetItem(tr(status.c_str()));
    QTableWidgetItem *key_item =
        new QTableWidgetItem(QString::fromStdString(key));
    QTableWidgetItem *command_item = new QTableWidgetItem(tr(command.c_str()));

    mutable_table_widget()->insertRow(row);
    mutable_table_widget()->setItem(row, 0, status_item);
    mutable_table_widget()->setItem(row, 1, key_item);
    mutable_table_widget()->setItem(row, 2, command_item);
    ++row;
  }

  UpdateMenuStatus();

  return true;
}

bool KeyMapEditorDialog::Update() {
  if (mutable_table_widget()->rowCount() == 0) {
    QMessageBox::warning(this, windowTitle(),
                         tr("Current keymap table is empty. "
                            "You might want to import a pre-defined "
                            "keymap table first."));
    return false;
  }

  absl::flat_hash_set<std::string> new_direct_mode_commands;

  KeyMapValidator *validator = Singleton<KeyMapValidator>::get();
  std::string *keymap_table = mutable_table();

  *keymap_table = "status\tkey\tcommand\n";

  for (int i = 0; i < mutable_table_widget()->rowCount(); ++i) {
    const std::string &i18n_status =
        TableUtil::SafeGetItemText(mutable_table_widget(), i, 0).toStdString();
    const std::string &key =
        TableUtil::SafeGetItemText(mutable_table_widget(), i, 1).toStdString();
    const std::string &i18n_command =
        TableUtil::SafeGetItemText(mutable_table_widget(), i, 2).toStdString();

    const auto status_it = normalized_status_map_.find(i18n_status);
    if (status_it == normalized_status_map_.end()) {
      LOG(ERROR) << "Unsupported i18n status name: " << i18n_status;
      continue;
    }
    const std::string &status = status_it->second;

    const auto command_it = normalized_command_map_.find(i18n_command);
    if (command_it == normalized_command_map_.end()) {
      LOG(ERROR) << "Unsupported i18n command name:" << i18n_command;
      continue;
    }
    const std::string &command = command_it->second;

    if (!validator->IsVisibleKey(key)) {
      QMessageBox::warning(
          this, windowTitle(),
          (tr("Invalid key:\n%1")
               .arg(QString::fromUtf8(key.data(), key.size()))));
      return false;
    }
    absl::StrAppend(keymap_table, status, "\t", key, "\t", command, "\n");

    if (status == kDirectMode) {
      new_direct_mode_commands.insert(key);
    }
  }
  absl::StrAppend(keymap_table, invisible_keymap_table_);

  if (new_direct_mode_commands != direct_mode_commands_) {
#if defined(_WIN32) || defined(__linux__)
    QMessageBox::information(
        this, windowTitle(),
        tr("Changes of keymaps for direct input mode will apply only to "
           "applications that are launched after making your "
           "modifications."));
#endif  // _WIN32 || __linux__
    direct_mode_commands_ = std::move(new_direct_mode_commands);
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
  for (size_t i = 0; i < std::size(kKeyMaps); ++i) {
    if (import_actions_[i] == action) {
      import_index = i;
      break;
    }
  }

  if (action == actions_[NEW_INDEX]) {
    AddNewItem();
  } else if (action == actions_[REMOVE_INDEX]) {
    DeleteSelectedItems();
  } else if (import_index != -1 || action == actions_[IMPORT_FROM_FILE_INDEX]) {
    if (mutable_table_widget()->rowCount() > 0 &&
        QMessageBox::Ok !=
            QMessageBox::question(
                this, windowTitle(),
                tr("Do you want to overwrite the current keymaps?"),
                QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel)) {
      return;
    }

    // import_category_index means Import from file
    if (action == actions_[IMPORT_FROM_FILE_INDEX]) {
      Import();
      // otherwise, load from predefined tables
    } else if (import_index >= 0 && import_index < std::size(kKeyMaps)) {
      const char *keymap_file =
          keymap::KeyMapManager::GetKeyMapFileName(kKeyMaps[import_index]);
      std::unique_ptr<std::istream> ifs(
          ConfigFileStream::LegacyOpen(keymap_file));
      CHECK(ifs.get() != nullptr);  // should never happen
      CHECK(LoadFromStream(ifs.get()));
    }
  } else if (action == actions_[EXPORT_TO_FILE_INDEX]) {
    Export();
  }
}

// static
bool KeyMapEditorDialog::Show(QWidget *parent,
                              const std::string &current_keymap,
                              std::string *new_keymap) {
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

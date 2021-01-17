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

#ifndef MOZC_GUI_DICTIONARY_TOOL_DICTIONARY_TOOL_H_
#define MOZC_GUI_DICTIONARY_TOOL_DICTIONARY_TOOL_H_

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QSplitterHandle>
#include <memory>
#include <string>
#include <vector>

#include "base/port.h"
#include "dictionary/user_dictionary_importer.h"
#include "gui/dictionary_tool/ui_dictionary_tool.h"

namespace mozc {

class POSListProviderInterface;

namespace client {
class ClientInterface;
}  // namespace client

namespace user_dictionary {
class UserDictionarySession;
}  // namespace user_dictionary

namespace gui {

class ImportDialog;
class FindDialog;

class DictionaryTool : public QMainWindow, private Ui::DictionaryTool {
  Q_OBJECT

 public:
  explicit DictionaryTool(QWidget *parent = nullptr);
  ~DictionaryTool() override;

  // return true DictionaryTool is available.
  bool IsAvailable() const { return is_available_; }

 protected:
  // Override the default implementation to check unsaved
  // modifications on data before closing the window.
  void closeEvent(QCloseEvent *event) override;

  bool eventFilter(QObject *obj, QEvent *event) override;

 private slots:
  void CreateDictionary();
  void DeleteDictionary();
  void RenameDictionary();
  void ImportAndCreateDictionary();
  void ImportAndAppendDictionary();
  void ImportFromDefaultIME();
  void ExportDictionary();
  void AddWord();
  void DeleteWord();
  void CloseWindow();
  void UpdateUIStatus();

  // Signals to be connected with a particular action by the user.
  void OnDictionarySelectionChanged();
  void OnItemChanged(QTableWidgetItem *unused_item);
  void OnHeaderClicked(int logicalIndex);
  void OnDeactivate();

  // We customize the default behavior of context menu on the table
  // widget for dictionary contents so that the menu is shown only
  // when there is an item under the mouse cursor.
  void OnContextMenuRequestedForContent(const QPoint &pos);
  void OnContextMenuRequestedForList(const QPoint &pos);

 private:
  // Data type to provide information on a dictionary.
  struct DictionaryInfo {
    int row;                // Row in the list widget.
    uint64 id;              // ID of the dictionary.
    QListWidgetItem *item;  // Item object for the dictionary.
  };

  // Returns information on the current dictionary.
  DictionaryInfo current_dictionary() const;

  // Save content of the current dictionary to the UserDictionaryStorage
  void SyncToStorage();

  // Setup GUI components to edit dictionary contents for a given
  // dictionary.
  void SetupDicContentEditor(const DictionaryInfo &dic_info);

  // It's used internally to create a dictionary.
  void CreateDictionaryHelper(const QString &dic_name);

  bool InitDictionaryList();

  // Show a dialog and get text for dictionary name from the user. The
  // first parameter is default text printed in a form. The second is
  // message printed on the dialog. It returns an empty stirng
  // whenever a proper value for dictionary name is input.
  QString PromptForDictionaryName(const QString &text, const QString &label);

  // Check storage_->GetLastError() and displays an
  // appropriate error message.
  void ReportError();

  // These two functions are to start/stop monitoring data on the
  // table widget being changed. We validate the value on the widget
  // when the user edit it but the data can be modified
  // programatically and validation is not necessary.
  void StartMonitoringUserEdit();
  void StopMonitoringUserEdit();

  // Show a special dialog message according to the result
  // of UserDictionaryImporter.
  void ReportImportError(UserDictionaryImporter::ErrorType error,
                         const std::string &dic_name, int added_entries_size);

  void ImportHelper(uint64 dic_id, const std::string &dic_name,
                    const std::string &file_name,
                    UserDictionaryImporter::IMEType,
                    UserDictionaryImporter::EncodingType encoding_type);

  // Save storage contents into the disk and
  // send Reload command to the server.
  void SaveAndReloadServer();

  // 1. Shows a dialog box and get new |commnet|.
  // 2. Changes the comemnt of all selected.
  void EditComment();

  // Changes the POS of all selected items to |pos|.
  void EditPOS(const std::string &pos);

  // Moves selected items to the dictionary whose row is |dictionary_row|.
  void MoveTo(int dictionary_row);

  // Helper functions to check if a file with given name is readable
  // to import or writable to export without trying to open it.
  static bool IsWritableToExport(const std::string &file_name);
  static bool IsReadableToImport(const std::string &file_name);

  // Helper function for DeleteWord and MoveTo.
  // Fills selected word entry rows as a unique sorted sequence.
  void GetSortedSelectedRows(std::vector<int> *rows) const;

  // Returns a pointer to the first selected dictionary.
  // Returns nullptr if no dictionary is selected.
  QListWidgetItem *GetFirstSelectedDictionary() const;

  ImportDialog *import_dialog_;
  FindDialog *find_dialog_;
  std::unique_ptr<mozc::user_dictionary::UserDictionarySession> session_;

  // ID of current selected dictionary. This needs to be maintained
  // separate from selection on the list widget because data is saved
  // on the previous dictionary after the selection on the widget is
  // changed. It takes -1 when no dictionary is selected.
  uint64 current_dic_id_;

  // Whether any change has been made on the current dictionary and
  // not been saved.
  bool modified_;

  // Holds information on whether dictionary entires are sorted, key
  // column of sort and order of sort.
  //
  // Current implementation of sort may not be perfect. It doesn't
  // check if entries are already sorted when they are loaded nor
  // whether modification is made keeping sorted entires sorted.
  struct SortState {
    bool sorted;
    int column;
    Qt::SortOrder order;
  } sort_state_;

  // See comment above StartMonitoringUserEdit() for role of the
  // variable.
  bool monitoring_user_edit_;

  // POS shown in the combo box by default.
  QString default_pos_;

  QString window_title_;

  // Buttons
  QPushButton *dic_menu_button_;
  QPushButton *new_word_button_;
  QPushButton *delete_word_button_;

  // Menu for managing dictionary.
  QMenu *dic_menu_;

  // Action inside menu.
  // Sine we have to disable/enable the actions according to the
  // numbers of active dictionary, we define them as a member.
  QAction *new_action_;
  QAction *rename_action_;
  QAction *delete_action_;
  QAction *find_action_;
  QAction *import_create_action_;
  QAction *import_append_action_;
  QAction *export_action_;
  QAction *import_default_ime_action_;

  // status message
  QString statusbar_message_;

  std::unique_ptr<client::ClientInterface> client_;

  bool is_available_;

  // The maximum number of entries for a dictionary currently selected.
  int max_entry_size_;

  std::unique_ptr<const POSListProviderInterface> pos_list_provider_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_DICTIONARY_TOOL_DICTIONARY_TOOL_H_

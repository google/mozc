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

#include "gui/dictionary_tool/dictionary_tool.h"

#include <QtCore/QTimer>
#include <QtGui/QtGui>
#include <QtGui/QProgressDialog>

#ifdef OS_WIN
#include <Windows.h>
#endif  // OS_WIN

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/run_level.h"
#include "base/system_util.h"
#include "base/util.h"
#include "client/client.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/user_dictionary_importer.h"
#include "dictionary/user_dictionary_session.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_storage.pb.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_pos.h"
#include "gui/base/win_util.h"
#include "gui/config_dialog/combobox_delegate.h"
#include "gui/dictionary_tool/find_dialog.h"
#include "gui/dictionary_tool/import_dialog.h"

namespace mozc {
namespace gui {

using mozc::user_dictionary::UserDictionary;
using mozc::user_dictionary::UserDictionaryCommandStatus;
using mozc::user_dictionary::UserDictionarySession;
using mozc::user_dictionary::UserDictionaryStorage;

namespace {

// set longer timeout because it takes longer time
// to reload all user dictionary.
const int kSessionTimeout = 100000;

int GetTableHeight(QTableWidget *widget) {
  // Dragon Hack:
  // Here we use "龍" to calc font size, as it looks almsot square
  // const char kHexBaseChar[]= "龍";
  const char kHexBaseChar[]= "\xE9\xBE\x8D";
  const QRect rect =
      QFontMetrics(widget->font()).boundingRect(
          QObject::trUtf8(kHexBaseChar));
  return static_cast<int>(rect.height() * 1.4);
}

QProgressDialog *CreateProgressDialog(
    QString message, QWidget *parent, int size) {
  QProgressDialog *progress =
      new QProgressDialog(message, "", 0, size, parent);
  CHECK(progress);
  progress->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
  progress->setWindowModality(Qt::WindowModal);
  // This cancel button is invisible to users.
  // We don't accept any cancel operation
  QPushButton *cancel_button = new QPushButton;
  CHECK(cancel_button);
  progress->setAutoClose(true);
  progress->setCancelButton(cancel_button);
  progress->setFixedSize(QSize(400, 100));
  cancel_button->setVisible(false);

  return progress;
}

#if defined(OS_WIN) || defined(OS_MACOSX)
void InstallStyleSheet(const QString &filename) {
  QFile file(filename);
  file.open(QFile::ReadOnly);
  qApp->setStyleSheet(QLatin1String(file.readAll()));
}
#endif

// Use QTextStream to read UTF16 text -- we can't use ifstream,
// since ifstream cannot handle Wide character.
class UTF16TextLineIterator
    : public UserDictionaryImporter::TextLineIteratorInterface {
 public:
  UTF16TextLineIterator(
      UserDictionaryImporter::EncodingType encoding_type,
      const string &filename,
      const QString &message, QWidget *parent)
      : stream_(new QTextStream) {
    CHECK_EQ(UserDictionaryImporter::UTF16, encoding_type);
    file_.setFileName(QString::fromUtf8(filename.c_str()));
    if (!file_.open(QIODevice::ReadOnly)) {
      LOG(ERROR) << "Cannot open: " << filename;
    }
    stream_->setDevice(&file_);
    stream_->setCodec("UTF-16");
    progress_.reset(CreateProgressDialog(message, parent, file_.size()));
  }

  bool IsAvailable() const {
    return file_.error() == QFile::NoError;
  }

  bool Next(string *line) {
    if (stream_->atEnd()) {
      return false;
    }

    // Can't use ReadLine as ReadLine doesn't support CR only text.
    QChar ch;
    QString output_line;
    while (!stream_->atEnd()) {
      *(stream_.get()) >> ch;
      if (output_line.isEmpty() && ch == QLatin1Char('\n')) {
        // no harm to skip empty line
        continue;
      }
      if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
        break;
      } else {
        output_line += ch;
      }
    }

    progress_->setValue(file_.pos());

    *line = output_line.toUtf8().data();
    return true;
  }

  void Reset() {
    file_.seek(0);
    stream_.reset(new QTextStream);
    stream_->setDevice(&file_);
    stream_->setCodec("UTF-16");
  }

 private:
  QFile file_;
  scoped_ptr<QTextStream> stream_;
  scoped_ptr<QProgressDialog> progress_;
};

class MultiByteTextLineIterator
    : public UserDictionaryImporter::TextLineIteratorInterface {
 public:
  MultiByteTextLineIterator(
      UserDictionaryImporter::EncodingType encoding_type,
      const string &filename,
      const QString &message, QWidget *parent)
      : encoding_type_(encoding_type),
        ifs_(new InputFileStream(filename.c_str())),
        first_line_(true) {
    const streampos begin = ifs_->tellg();
    ifs_->seekg(0, ios::end);
    const size_t size = static_cast<size_t>(ifs_->tellg() - begin);
    ifs_->seekg(0, ios::beg);
    progress_.reset(CreateProgressDialog(message, parent, size));
  }

  bool IsAvailable() const {
    return *(ifs_.get());
  }

  bool Next(string *line)  {
    if (!*(ifs_.get())) {
      return false;
    }

    // Can't use getline as getline doesn't support CR only text.
    char ch;
    string output_line;
    while (*(ifs_.get())) {
      ifs_->get(ch);
      if (output_line.empty() && ch == '\n') {
        continue;
      }
      if (ch == '\n' || ch == '\r') {
        break;
      } else {
        output_line += ch;
      }
    }

    progress_->setValue(ifs_->tellg());

    *line = output_line;

    // We can't use QTextCodec as QTextCodec is not enabled by default.
    // We won't enable it as it increases the binary size.
    if (encoding_type_ == UserDictionaryImporter::SHIFT_JIS) {
      const string input = *line;
      Util::SJISToUTF8(input, line);
    }

    // strip UTF8 BOM
    if (first_line_ &&
        encoding_type_ == UserDictionaryImporter::UTF8) {
      Util::StripUTF8BOM(line);
    }

    Util::ChopReturns(line);

    first_line_ = false;
    return true;
  }

  void Reset() {
    ifs_->seekg(0, ios_base::beg);
    first_line_ = true;
  }

 private:
  UserDictionaryImporter::EncodingType encoding_type_;
  scoped_ptr<InputFileStream> ifs_;
  scoped_ptr<QProgressDialog> progress_;
  bool first_line_;
};

UserDictionaryImporter::TextLineIteratorInterface *CreateTextLineIterator(
    UserDictionaryImporter::EncodingType encoding_type,
    const string &filename,
    QWidget *parent) {
  if (encoding_type == UserDictionaryImporter::ENCODING_AUTO_DETECT) {
    encoding_type = UserDictionaryImporter::GuessFileEncodingType(filename);
  }

  if (encoding_type == UserDictionaryImporter::NUM_ENCODINGS) {
    LOG(ERROR) << "GuessFileEncodingType() returns UNKNOWN encoding.";
    // set default encoding
#ifdef OS_WIN
    encoding_type = UserDictionaryImporter::SHIFT_JIS;
#else
    encoding_type = UserDictionaryImporter::UTF16;
#endif
  }

  VLOG(1) << "Setting Encoding to: " << static_cast<int>(encoding_type);

  const QString message = QObject::tr("Importing new words...");

  switch (encoding_type) {
    case UserDictionaryImporter::UTF8:
    case UserDictionaryImporter::SHIFT_JIS:
      return new MultiByteTextLineIterator(encoding_type, filename,
                                           message, parent);
      break;
    case UserDictionaryImporter::UTF16:
      return new UTF16TextLineIterator(encoding_type, filename,
                                       message, parent);
      break;
    default:
      return NULL;
  }

  return NULL;
}
}  // namespace

DictionaryTool::DictionaryTool(QWidget *parent)
    : QMainWindow(parent),
      import_dialog_(NULL), find_dialog_(NULL),
      session_(new UserDictionarySession(
          UserDictionaryUtil::GetUserDictionaryFileName())),
      current_dic_id_(0), modified_(false), monitoring_user_edit_(false),
      window_title_(tr("Mozc")),
      dic_menu_(new QMenu),
      new_action_(NULL), rename_action_(NULL),
      delete_action_(NULL), find_action_(NULL),
      import_create_action_(NULL), import_append_action_(NULL),
      export_action_(NULL), import_default_ime_action_(NULL),
      client_(client::ClientFactory::NewClient()),
      is_available_(true),
      max_entry_size_(mozc::UserDictionaryStorage::max_entry_size()),
      user_pos_(new UserPOS(
          UserPosManager::GetUserPosManager()->GetUserPOSData())) {
  setupUi(this);

  // Create and set up ImportDialog object.
  import_dialog_ = new ImportDialog(this);

  // Create and set up FindDialog object.
  find_dialog_ = new FindDialog(this, dic_content_);

  client_->set_timeout(kSessionTimeout);

  if (session_->Load() !=
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    LOG(WARNING) << "UserDictionarySession::Load() failed";
  }

  if (!session_->mutable_storage()->Lock()) {
    QMessageBox::information(
        this, window_title_,
        tr("Another process is accessing the user dictionary file."));
    is_available_ = false;
    return;
  }

#ifndef ENABLE_CLOUD_SYNC
  if (session_->mutable_storage()
      ->ConvertSyncDictionariesToNormalDictionaries()) {
    LOG(INFO) << "Syncable dictionaries are converted to normal dictionaries";
    session_->mutable_storage()->Save();
  }
#endif  // !ENABLE_CLOUD_SYNC

  // main window
#ifndef OS_LINUX
  // For some reason setCentralWidget crashes the dictionary_tool on Linux
  // TODO(taku): investigate the cause of the crashes
  setCentralWidget(splitter_);
#endif

  setContextMenuPolicy(Qt::NoContextMenu);

  // toolbar
  dic_menu_button_ = new QPushButton(tr("Tools"), this);
  new_word_button_ = new QPushButton(tr("Add"), this);
  delete_word_button_ = new QPushButton(tr("Remove"), this);
  toolbar_->addWidget(dic_menu_button_);
  toolbar_->addWidget(new_word_button_);
  toolbar_->addWidget(delete_word_button_);

  // Cosmetic tweaks for Mac.
#ifdef OS_MACOSX
  dic_list_->setAttribute(Qt::WA_MacShowFocusRect, false);
  dic_list_->setAutoFillBackground(true);
  dic_list_->setFrameStyle(QFrame::NoFrame);

  dic_content_->setFrameStyle(QFrame::NoFrame);
  dic_content_->setShowGrid(false);
#endif

  dic_content_->setWordWrap(false);
  dic_content_->verticalHeader()->hide();
  dic_content_->horizontalHeader()->setStretchLastSection(true);
  dic_content_->horizontalHeader()->setSortIndicatorShown(true);
  dic_content_->horizontalHeader()->setHighlightSections(false);
  dic_content_->setAlternatingRowColors(true);

  // We fix the row height so that paint() is executed faster.
  // This changes allows DictionaryTool handle about 1M words.
  dic_content_->verticalHeader()->setResizeMode(QHeaderView::Fixed);
  dic_content_->verticalHeader()->setDefaultSectionSize(
      GetTableHeight(dic_content_));

  // Get a list of POS and set a custom delagate that holds the list.
  vector<string> tmp_pos_vec;
  user_pos_->GetPOSList(&tmp_pos_vec);
  QStringList pos_list;
  for (size_t i = 0; i < tmp_pos_vec.size(); ++i) {
    pos_list.append(tmp_pos_vec[i].c_str());
  }
  ComboBoxDelegate *delegate = new ComboBoxDelegate;
  delegate->SetItemList(pos_list);
  dic_content_->setItemDelegateForColumn(2, delegate);
  if (!pos_list.isEmpty()) {
    default_pos_ = pos_list[0];
  } else {
    LOG(WARNING) << "No POS is given.";
  }

  // Set up the main table widget for dictionary contents.
  dic_content_->setColumnCount(4);
  QStringList header_labels;
  header_labels << tr("Reading") << tr("Word")
                << tr("Category") << tr("Comment");
  dic_content_->setHorizontalHeaderLabels(header_labels);

#ifdef OS_MACOSX
  // This is a workaround for MacOSX.
  // QKeysequence::Delete doesn't work on Mac,
  // so we set the key sequence directly.
  QShortcut *shortcut1 = new QShortcut(QKeySequence(Qt::Key_Backspace),
                                       dic_content_);
  // Qt::CTRL = Command key
  // Command+Backspace = delete
  QShortcut *shortcut2 = new QShortcut(QKeySequence(Qt::CTRL +
                                                    Qt::Key_Backspace),
                                       dic_content_);
  connect(shortcut1, SIGNAL(activated()), this, SLOT(DeleteWord()));
  connect(shortcut2, SIGNAL(activated()), this, SLOT(DeleteWord()));
#else
  QShortcut *shortcut = new QShortcut(QKeySequence(QKeySequence::Delete),
                                      dic_content_);
  connect(shortcut, SIGNAL(activated()), this, SLOT(DeleteWord()));
#endif

  dic_content_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(dic_content_, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(OnContextMenuRequestedForContent(const QPoint &)));
  connect(dic_content_->horizontalHeader(), SIGNAL(sectionClicked(int)),
          this, SLOT(OnHeaderClicked(int)));

  dic_list_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(dic_list_, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(OnContextMenuRequestedForList(const QPoint &)));

  // Set up the menu for dictionary related operations.
  new_action_    = dic_menu_->addAction(tr("New dictionary..."));
  rename_action_ = dic_menu_->addAction(tr("Rename dictionary..."));
  delete_action_ = dic_menu_->addAction(tr("Delete dictionary"));
  find_action_   = dic_menu_->addAction(tr("Find..."));
  dic_menu_->addSeparator();
  import_create_action_ =
      dic_menu_->addAction(tr("Import as new dictionary..."));
  import_append_action_ =
      dic_menu_->addAction(tr("Import to current dictionary..."));
  export_action_ = dic_menu_->addAction(tr("Export current dictionary..."));

  // add Import from MS-IME's dictionary
#ifdef OS_WIN
  dic_menu_->addSeparator();
  import_default_ime_action_ =
      dic_menu_->addAction(
          tr("Import from %1's user dictionary...").arg("Microsoft IME"));
#endif  // OS_WIN

  dic_menu_button_->setMenu(dic_menu_);
  connect(new_action_, SIGNAL(triggered()),
          this, SLOT(CreateDictionary()));
  connect(rename_action_, SIGNAL(triggered()),
          this, SLOT(RenameDictionary()));
  connect(delete_action_, SIGNAL(triggered()),
          this, SLOT(DeleteDictionary()));
  connect(find_action_, SIGNAL(triggered()),
          find_dialog_, SLOT(show()));
  connect(import_create_action_, SIGNAL(triggered()),
          this, SLOT(ImportAndCreateDictionary()));
  connect(import_append_action_, SIGNAL(triggered()),
          this, SLOT(ImportAndAppendDictionary()));
  connect(export_action_, SIGNAL(triggered()),
          this, SLOT(ExportDictionary()));

#ifdef OS_WIN
  connect(import_default_ime_action_, SIGNAL(triggered()),
          this, SLOT(ImportFromDefaultIME()));
#endif  // OS_WIN

  // Signals and slots to connect buttons to actions.
  connect(new_word_button_, SIGNAL(clicked()),
          this, SLOT(AddWord()));
  connect(delete_word_button_, SIGNAL(clicked()),
          this, SLOT(DeleteWord()));

  // When empty area of Dictionary Table is clicked,
  // insert a new word. This is only enabled on Mac
  connect(dic_content_, SIGNAL(emptyAreaClicked()),
          this, SLOT(AddWord()));

  // Signals and slots to update state of objects on the window in
  // response to change of selection/data of items.
  connect(dic_list_, SIGNAL(itemSelectionChanged()),
          this, SLOT(OnDictionarySelectionChanged()));

  // Initialize the list widget for a list of dictionaries.
  InitDictionaryList();
  if (dic_list_->count() != 0) {
    dic_list_->setCurrentRow(0);
  } else {
    // Make sure that the table widget is initialized when there is no
    // dictionary.
    OnDictionarySelectionChanged();
  }

  // Adjust the splitter.
  splitter_->setSizes(QList<int>()
                      << static_cast<int>(width() * 0.25)
                      << static_cast<int>(width() * 0.75));

  // adjust column width
  dic_content_->setColumnWidth(0, static_cast<int>(width() * 0.18));
  dic_content_->setColumnWidth(1, static_cast<int>(width() * 0.18));
  dic_content_->setColumnWidth(2, static_cast<int>(width() * 0.18));

  sort_state_.sorted = false;

  // If this is the first time for the user dictionary is used, create
  // a defautl dictionary.
  if (!session_->mutable_storage()->Exists()) {
    CreateDictionaryHelper(tr("User Dictionary 1"));
  }

  // for Mac-like style
#ifdef OS_MACOSX
  setUnifiedTitleAndToolBarOnMac(true);
  InstallStyleSheet(":mac_style.qss");
#endif

  // for Window Aero Glass support
#ifdef OS_WIN
  if (SystemUtil::IsVistaOrLater()) {
    setContentsMargins(0, 0, 0, 0);
    WinUtil::InstallStyleSheetsFiles(":win_aero_style.qss",
                                     ":win_style.qss");
    if (gui::WinUtil::IsCompositionEnabled()) {
      WinUtil::ExtendFrameIntoClientArea(this);
      InstallStyleSheet(":win_aero_style.qss");
    } else {
      InstallStyleSheet(":win_style.qss");
    }
  }
#endif

  StartMonitoringUserEdit();
  UpdateUIStatus();

  qApp->installEventFilter(this);
}

DictionaryTool::~DictionaryTool() {}

void DictionaryTool::closeEvent(QCloseEvent *event) {
  // QEvent::ApplicationDeactivate is not emitted here.

  // Change the focus so that the last incomplete itmes on
  // TableView is submitted to the model
  dic_menu_button_->setFocus(Qt::MouseFocusReason);

  SyncToStorage();
  SaveAndReloadServer();

  if (session_->mutable_storage()->GetLastError()
      == mozc::UserDictionaryStorage::TOO_BIG_FILE_BYTES) {
    QMessageBox::warning(
        this, window_title_,
        tr("Making dangerously large user dictionary file. "
           "If the dictionary file turns out to be larger than 256Mbyte, "
           "the dictionary loader skips to handle all the words to prevent "
           "the converter from being halted."));
  }
}

void DictionaryTool::OnDeactivate() {
  SyncToStorage();
  SaveAndReloadServer();
}

bool DictionaryTool::eventFilter(QObject *obj, QEvent *event) {
  // When application focus leaves, reload the server so
  // that new dictionary can be loaded.
  // This is an approximation of dynamic relading.
  //
  // We cannot call OnDeactivate() inside event filter,
  // as pending changes are not correctly synced to the disk.
  // Seems that all pending changes are committed to the UI
  // AFTER DictionaryTool receives ApplicationDeactivate event.
  // Here we delayed the execution of OnDeactivate() using QTimer.
  // This is an workaround for http://b/2190275.
  // TODO(taku): Find out a better way.
  if (event->type() == QEvent::ApplicationDeactivate) {
    const int kDelayOnDeactivateTime = 200;
    QTimer::singleShot(kDelayOnDeactivateTime, this, SLOT(OnDeactivate()));
  }
  return QWidget::eventFilter(obj, event);
}

void DictionaryTool::OnDictionarySelectionChanged() {
  SyncToStorage();

  DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item == NULL) {
    current_dic_id_ = 0;
    StopMonitoringUserEdit();
    dic_content_->clearContents();
    dic_content_->setRowCount(0);
    dic_content_->setEnabled(false);
    StartMonitoringUserEdit();
    new_word_button_->setEnabled(false);
    delete_word_button_->setEnabled(false);
    rename_action_->setEnabled(false);
    delete_action_->setEnabled(false);
    import_append_action_->setEnabled(false);
    export_action_->setEnabled(false);
  } else {
    current_dic_id_ = dic_info.id;
    max_entry_size_ = mozc::UserDictionaryStorage::max_entry_size();
    SetupDicContentEditor(dic_info);
  }
}

void DictionaryTool::SetupDicContentEditor(
    const DictionaryInfo &dic_info) {
  UserDictionary *dic =
      session_->mutable_storage()->GetUserDictionary(dic_info.id);

  if (dic == NULL) {
    LOG(ERROR) << "Failed to load the dictionary: " << dic_info.id;
    ReportError();
    return;
  }

  // Update the main table widget for dictionary contents.
  StopMonitoringUserEdit();

  rename_action_->setEnabled(true);
  delete_action_->setEnabled(true);
  import_append_action_->setEnabled(true);
  export_action_->setEnabled(true);

  setUpdatesEnabled(false);

  dic_content_->clearContents();
  dic_content_->setRowCount(dic->entries_size());

  {
    scoped_ptr<QProgressDialog> progress(CreateProgressDialog(
        tr("Updating the current view data..."),
        this,
        dic->entries_size()));

    for (size_t i = 0; i < dic->entries_size(); ++i) {
      dic_content_->setItem(
          i, 0, new QTableWidgetItem(dic->entries(i).key().c_str()));
      dic_content_->setItem(
          i, 1, new QTableWidgetItem(dic->entries(i).value().c_str()));
      dic_content_->setItem(
          i, 2, new QTableWidgetItem(
              UserDictionaryUtil::GetStringPosType(dic->entries(i).pos())));
      dic_content_->setItem(
          i, 3, new QTableWidgetItem(dic->entries(i).comment().c_str()));
      progress->setValue(i);
    }
  }

  setUpdatesEnabled(true);
  dic_content_->repaint();
  dic_content_->setEnabled(true);

  StartMonitoringUserEdit();

  // Update state of other GUI components.
  UpdateUIStatus();

  const bool dictionary_is_full = dic_content_->rowCount() >= max_entry_size_;
  new_word_button_->setEnabled(!dictionary_is_full);

  modified_ = false;
}

void DictionaryTool::CreateDictionary() {
  const int max_size =
      static_cast<int>(mozc::UserDictionaryStorage::max_dictionary_size());
  if (dic_list_->count() >= max_size) {
    QMessageBox::critical(
        this, window_title_,
        tr("You can't have more than %1 dictionaries.").arg(max_size));
    return;
  }

  const QString dic_name =
      PromptForDictionaryName("", tr("Name of the new dictionary"));
  if (dic_name.isEmpty()) {
    return;  // canceld by user
  }

  SyncToStorage();

  CreateDictionaryHelper(dic_name);
}

void DictionaryTool::DeleteDictionary() {
  DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item == NULL) {
    QMessageBox::information(this, window_title_,
                             tr("No dictionary is selected."));
    return;
  }

  if (QMessageBox::question(
          this, window_title_,
          tr("Do you want to delete %1?").arg(dic_info.item->text()),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
      != QMessageBox::Yes) {
    return;
  }

  if (!session_->mutable_storage()->DeleteDictionary(dic_info.id)) {
    LOG(ERROR) << "Failed to delete the dictionary.";
    ReportError();
    return;
  }

  modified_ = false;
  QListWidgetItem *item = dic_list_->takeItem(dic_info.row);
  delete item;

  UpdateUIStatus();
}

void DictionaryTool::RenameDictionary() {
  DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item == NULL) {
    QMessageBox::information(this, window_title_,
                             tr("No dictionary is selected."));
    return;
  }

  const QString dic_name =
      PromptForDictionaryName(dic_info.item->text(),
                              tr("New name of the dictionary"));
  if (dic_name.isEmpty()) {
    return;
  }

  if (!session_->mutable_storage()->RenameDictionary(
          dic_info.id, dic_name.toStdString())) {
    LOG(ERROR) << "Failed to rename the dictionary.";
    ReportError();
    return;
  }

  dic_info.item->setText(dic_name);

  UpdateUIStatus();
}

void DictionaryTool::ImportAndCreateDictionary() {
  const int max_size = static_cast<int>(
      mozc::UserDictionaryStorage::max_dictionary_size());
  if (dic_list_->count() >= max_size) {
    QMessageBox::critical(
        this, window_title_,
        tr("You can't have more than %1 dictionaries.").arg(max_size));
    return;
  }

  // Get necessary information from the user.
  if (import_dialog_->ExecInCreateMode() != QDialog::Accepted) {
    LOG(WARNING) << "Cancele by the user.";
    return;
  }

  ImportHelper(0,   // dic_id == 0 means that "CreateNewDictonary" mode
               import_dialog_->dic_name().toStdString(),
               import_dialog_->file_name().toStdString(),
               import_dialog_->ime_type(),
               import_dialog_->encoding_type());
}

void DictionaryTool::ImportAndAppendDictionary() {
  DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item == NULL) {
    LOG(WARNING) << "No dictionary to import is selected";
    QMessageBox::information(this, window_title_,
                             tr("No dictionary is selected."));
    return;
  }

  if (dic_content_->rowCount() >= max_entry_size_) {
    QMessageBox::critical(this, window_title_,
                          tr("You can't have more than %1 "
                             "words in one dictionary.").arg(max_entry_size_));
    return;
  }

  if (import_dialog_->ExecInAppendMode() != QDialog::Accepted) {
    LOG(WARNING) << "Cancele by the user.";
    return;
  }

  ImportHelper(dic_info.id,
               dic_info.item->text().toStdString(),
               import_dialog_->file_name().toStdString(),
               import_dialog_->ime_type(),
               import_dialog_->encoding_type());
}

void DictionaryTool::ReportImportError(UserDictionaryImporter::ErrorType error,
                                       const string &dic_name,
                                       int added_entries_size) {
  switch (error) {
    case UserDictionaryImporter::IMPORT_NO_ERROR:
      QMessageBox::information(
          this, window_title_,
          tr("%1 entries are imported to %2.")
          .arg(added_entries_size).arg(dic_name.c_str()));
      break;
    case UserDictionaryImporter::IMPORT_NOT_SUPPORTED:
      QMessageBox::information(
          this, window_title_,
          tr("You have imported a file in an invalid or "
             "unsupported file format.\n\n"
             "Please check the file format. "
             "ATOK11 or older format is not supported by "
             "Mozc."));
      break;
    case UserDictionaryImporter::IMPORT_TOO_MANY_WORDS:
      QMessageBox::information(
          this, window_title_,
          tr("%1 doesn't have enough space to import all words in "
             "the file. First %2 entries "
             "are imported.").arg(dic_name.c_str()).arg(added_entries_size));
      break;
    case UserDictionaryImporter::IMPORT_INVALID_ENTRIES:
      QMessageBox::information(
          this, window_title_,
          tr("%1 entries are imported to %2.\n\nSome imported "
             "words were not recognized by %3. "
             "Please check the original import file.").
          arg(added_entries_size).arg(dic_name.c_str()).arg(window_title_));
      break;
    case UserDictionaryImporter::IMPORT_FATAL:
      QMessageBox::critical(this, window_title_,
                            tr("Failed to open user dictionary"));
      break;
    default:
      break;
  }
}

void DictionaryTool::ImportHelper(
    uint64 dic_id,
    const string &dic_name,
    const string &file_name,
    UserDictionaryImporter::IMEType ime_type,
    UserDictionaryImporter::EncodingType encoding_type) {
  if (!IsReadableToImport(file_name)) {
    LOG(ERROR) << "File is not readable to import.";
    QMessageBox::critical(this, window_title_,
                          tr("Can't open %1.").arg(file_name.c_str()));
    return;
  }

  if (dic_id == 0 &&
      !session_->mutable_storage()->CreateDictionary(dic_name, &dic_id)) {
    LOG(ERROR) << "Failed to create the dictionary.";
    ReportError();
    return;
  }

  UserDictionary *dic = session_->mutable_storage()->GetUserDictionary(dic_id);

  if (dic == NULL) {
    LOG(ERROR) << "Cannot find dictionary id: " << dic_id;
    ReportError();
    return;
  }

  if (dic->name() != dic_name) {
    LOG(ERROR) << "Dictionary name is inconsistent";
    ReportError();
    return;
  }

  // Everything looks Okey so far. Now starting import operation.
  SyncToStorage();

  // Open dictionary
  scoped_ptr<UserDictionaryImporter::TextLineIteratorInterface> iter(
      CreateTextLineIterator(encoding_type, file_name, this));
  if (iter.get() == NULL) {
    LOG(ERROR) << "CreateTextLineIterator returns NULL";
    return;
  }

  const int old_size = dic->entries_size();
  const UserDictionaryImporter::ErrorType error =
      UserDictionaryImporter::ImportFromTextLineIterator(ime_type,
                                                         iter.get(),
                                                         dic);

  const int added_entries_size = dic->entries_size() - old_size;

  // Update window state.
  InitDictionaryList();
  for (int row = 0; row < dic_list_->count(); ++row) {
    if (dic_id == dic_list_->item(row)->data(Qt::UserRole).toULongLong()) {
      dic_list_->setCurrentRow(row);
    }
  }

  UpdateUIStatus();

  ReportImportError(error, dic_name, added_entries_size);
}

void DictionaryTool::ImportFromDefaultIME() {
#ifdef OS_WIN
  if (RunLevel::IsElevatedByUAC()) {
    // MS-IME's dictionary importer doesn't work if the current
    // process is already elevated by UAC. If user disables UAC<
    // it's not the case. Here we simply remove the MS-IME import function
    // if dictionary tool is UAC-elevated
    QMessageBox::warning(this, window_title_,
                         tr("Microsoft IME dictionary import function doesn't "
                            "work on UAC-elevated process."));
    return;
  }

  DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item == NULL) {
    LOG(WARNING) << "No dictionary to import is selected";
    QMessageBox::information(this, window_title_,
                             tr("No dictionary is selected."));
    return;
  }

  const QMessageBox::StandardButton answer =
      QMessageBox::information(this, window_title_,
                               tr("All user-registered words in %1 "
                                  "are migrated into the "
                                  "current dictionary.").arg("Microsoft IME"),
                               QMessageBox::Ok | QMessageBox::Cancel,
                               QMessageBox::Cancel);
  if (answer != QMessageBox::Ok) {
    LOG(WARNING) << "Cancele by the user.";
    return;
  }

  SyncToStorage();

  UserDictionary *dic =
      session_->mutable_storage()->GetUserDictionary(dic_info.id);
  DCHECK(dic);

  const int old_size = dic->entries_size();
  const string dic_name = dic_info.item->text().toStdString();

  const UserDictionaryImporter::ErrorType error =
      UserDictionaryImporter::ImportFromMSIME(dic);

  const int added_entries_size = dic->entries_size() - old_size;

  OnDictionarySelectionChanged();
  UpdateUIStatus();

  ReportImportError(error, dic_name, added_entries_size);
#endif
}

void DictionaryTool::ExportDictionary() {
  DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item == NULL) {
    LOG(WARNING) << "No dictionary to export is selected";
    QMessageBox::information(this, window_title_,
                             tr("No dictionary is selected."));
    return;
  }

  const string file_name = QFileDialog::getSaveFileName(
      this, tr("Export dictionary"),
      QDir::homePath(), tr("Text Files (*.txt);;All Files (*)")).toStdString();
  if (file_name.empty()) {
    return;
  }

  if (!IsWritableToExport(file_name)) {
    LOG(ERROR) << "File is not writable to export.";
    QMessageBox::critical(this, window_title_,
                          tr("Can't open %1.").arg(file_name.c_str()));
    return;
  }

  SyncToStorage();

  if (!session_->mutable_storage()->ExportDictionary(dic_info.id, file_name)) {
    LOG(ERROR) << "Failed to export the dictionary.";
    ReportError();
    return;
  }

  QMessageBox::information(this, window_title_,
                           tr("Dictionary export finished."));
}

void DictionaryTool::AddWord() {
  const int row = dic_content_->rowCount();
  if (row >= max_entry_size_) {
    QMessageBox::information(
        this, window_title_,
        tr("You can't have more than %1 words in one dictionary.").arg(
            max_entry_size_));
    return;
  }

  dic_content_->insertRow(row);
  dic_content_->setItem(row, 0, new QTableWidgetItem(""));
  dic_content_->setItem(row, 1, new QTableWidgetItem(""));
  dic_content_->setItem(row, 2, new QTableWidgetItem(default_pos_));
  dic_content_->setItem(row, 3, new QTableWidgetItem(""));

  if (row + 1 >= max_entry_size_) {
    new_word_button_->setEnabled(false);
  }

  QTableWidgetItem *item = dic_content_->item(row, 0);
  dic_content_->setCurrentItem(item);
  dic_content_->editItem(item);

  UpdateUIStatus();
}

void DictionaryTool::GetSortedSelectedRows(vector<int> *rows) const {
  DCHECK(rows);
  rows->clear();

  const QList<QTableWidgetItem *> items = dic_content_->selectedItems();
  if (items.empty()) {
    return;
  }
  rows->reserve(items.count());
  for (int i = 0; i < items.size(); ++i) {
    rows->push_back(items[i]->row());
  }

  sort(rows->begin(), rows->end(), greater<int>());
  vector<int>::const_iterator end = unique(rows->begin(), rows->end());

  rows->resize(end - rows->begin());
}

QListWidgetItem *DictionaryTool::GetFirstSelectedDictionary() const {
  QList<QListWidgetItem *> selected_dicts = dic_list_->selectedItems();
  if (selected_dicts.isEmpty()) {
    LOG(WARNING) << "No current dictionary.";
    return NULL;
  }
  if (selected_dicts.size() > 1) {
    LOG(WARNING) << "Multiple items are selected.";
  }
  return selected_dicts[0];
}

void DictionaryTool::DeleteWord() {
  vector<int> rows;
  GetSortedSelectedRows(&rows);
  if (rows.size() == 0) {
    return;
  }

  QString message;
  if (rows.size() == 1) {
    message = tr("Do you want to delete this word?");
  } else {
    message = tr("Do you want to delete the selected words?");
  }
  if (QMessageBox::question(
          this, window_title_, message,
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
      != QMessageBox::Yes) {
    return;
  }

  setUpdatesEnabled(false);

  {
    scoped_ptr<QProgressDialog> progress(
        CreateProgressDialog(
            tr("Deleting the selected words..."),
            this,
            rows.size()));

    for (size_t i = 0; i < rows.size(); ++i) {
      dic_content_->removeRow(rows[i]);
      progress->setValue(i);
    }
  }

  setUpdatesEnabled(true);
  dic_content_->setEnabled(true);

  if (dic_content_->rowCount() < max_entry_size_) {
    new_word_button_->setEnabled(true);
  }

  UpdateUIStatus();
  dic_content_->repaint();

  modified_ = true;
}

void DictionaryTool::EditPOS(const string &pos) {
  const QList<QTableWidgetItem *> items =
      dic_content_->selectedItems();
  if (items.empty()) {
    return;
  }

  const QString new_pos = QString::fromUtf8(pos.c_str());
  setUpdatesEnabled(false);

  for (int i = 0; i < items.size(); ++i) {
    const int row = items[i]->row();
    dic_content_->setItem(row, 2, new QTableWidgetItem(new_pos));
  }

  setUpdatesEnabled(true);
  dic_content_->setEnabled(true);
}

void DictionaryTool::MoveTo(int dictionary_row) {
  UserDictionary *target_dict = NULL;
  {
    const QListWidgetItem * selected_dict = GetFirstSelectedDictionary();
    if (selected_dict == NULL) {
      return;
    }
    QListWidgetItem *target_dict_item = dic_list_->item(dictionary_row);
    DCHECK(target_dict_item);
    if (target_dict_item == selected_dict) {
      LOG(WARNING) << "Target dictionary is the current dictionary.";
      return;
    }

    target_dict = session_->mutable_storage()->GetUserDictionary(
        target_dict_item->data(Qt::UserRole).toULongLong());
  }

  vector<int> rows;
  GetSortedSelectedRows(&rows);
  if (rows.size() == 0) {
    return;
  }

  const size_t target_max_entry_size =
      mozc::UserDictionaryStorage::max_entry_size();

  if (target_dict->entries_size() + rows.size() > target_max_entry_size) {
    QMessageBox::critical(this, window_title_,
                          tr("Cannot move all the selected items.\n"
                             "The target dictionary can have maximum "
                             "%1 entries.").arg(target_max_entry_size));
    return;
  }

  setUpdatesEnabled(false);

  {
    // add |rows.size()| items and remove |rows.size()| items
    const int progress_max = rows.size() * 2;
    scoped_ptr<QProgressDialog> progress(
        CreateProgressDialog(
            tr("Moving the selected words..."),
            this,
            progress_max));

    int progress_index = 0;
    if (target_dict) {
      for (size_t i = 0; i < rows.size(); ++i) {
        UserDictionary::Entry *entry = target_dict->add_entries();
        const int row = rows[i];
        entry->set_key(dic_content_->item(row, 0)->text().toStdString());
        entry->set_value(dic_content_->item(row, 1)->text().toStdString());
        entry->set_pos(UserDictionaryUtil::ToPosType(
            dic_content_->item(row, 2)->text().toStdString().c_str()));
        entry->set_comment(dic_content_->item(row, 3)->text().toStdString());
        UserDictionaryUtil::SanitizeEntry(entry);
        progress->setValue(progress_index);
        ++progress_index;
      }
    }
    for (size_t i = 0; i < rows.size(); ++i) {
      dic_content_->removeRow(rows[i]);
      progress->setValue(progress_index);
      ++progress_index;
    }
  }

  setUpdatesEnabled(true);
  dic_content_->setEnabled(true);

  UpdateUIStatus();
  dic_content_->repaint();

  modified_ = true;
}

void DictionaryTool::EditComment() {
  const QList<QTableWidgetItem *> items =
      dic_content_->selectedItems();
  if (items.empty()) {
    return;
  }

  bool ok = false;
  const QString new_comment = QInputDialog::getText(
      this, window_title_, tr("New comment"),
      QLineEdit::Normal, "", &ok,
      Qt::WindowTitleHint | Qt::WindowSystemMenuHint);

  if (!ok) {
    return;
  }

  setUpdatesEnabled(false);
  for (int i = 0; i < items.size(); ++i) {
    const int row = items[i]->row();
    dic_content_->setItem(row, 3, new QTableWidgetItem(new_comment));
  }

  setUpdatesEnabled(true);
  dic_content_->setEnabled(true);
}

void DictionaryTool::CloseWindow() {
  // move the focus to the close button to submit all incomplete inputs in
  // the word cells. http://b/211766
  // This is reuqired for MacOSX
  new_word_button_->setFocus();
  this->close();
}

void DictionaryTool::OnItemChanged(QTableWidgetItem *item) {
  if (item->column() == 0 &&
      !item->text().isEmpty() &&
      !UserDictionaryUtil::IsValidReading(item->text().toStdString())) {
    QMessageBox::critical(
        this, window_title_,
        tr("An invalid character is included in the reading."));
  }

  UpdateUIStatus();

  sort_state_.sorted = false;
  modified_ = true;
}

void DictionaryTool::OnHeaderClicked(int logicalIndex) {
  if (sort_state_.sorted &&
      sort_state_.column == logicalIndex &&
      sort_state_.order == Qt::AscendingOrder) {
    dic_content_->sortItems(logicalIndex, Qt::DescendingOrder);
    sort_state_.order = Qt::DescendingOrder;
  } else {
    dic_content_->sortItems(logicalIndex, Qt::AscendingOrder);
    sort_state_.sorted = true;
    sort_state_.column = logicalIndex;
    sort_state_.order = Qt::AscendingOrder;
  }
  modified_ = true;
}

void DictionaryTool::OnContextMenuRequestedForContent(const QPoint &pos) {
  QTableWidgetItem *item = dic_content_->itemAt(pos);
  // When the mouse pointer is not on an item of the table widget, we
  // don't show context menu.
  if (item == NULL) {
    return;
  }

  QMenu *menu = new QMenu(this);
  QAction *add_action = menu->addAction(tr("Add a word"));

  // Count the number of selected words and create delete menu with an
  // appropriate text.
  const QList<QTableWidgetItem *> items = dic_content_->selectedItems();
  QString delete_menu_text = tr("Delete this word");
  QString move_to_menu_text = tr("Move this word to");
  if (items.size() > 0) {
    const int first_row = items[0]->row();
    for (int i = 1; i < items.size(); ++i) {
      if (items[i]->row() != first_row) {
        // More than one words are selected.
        delete_menu_text = tr("Delete the selected words");
        move_to_menu_text = tr("Move the selected words to");
        break;
      }
    }
  }
  vector<pair<int, QAction *> > change_dictionary_actions;
  // "Move to" is available only when we have 2 or more dictionaries.
  if (dic_list_->count() > 1) {
    QMenu *move_to = menu->addMenu(move_to_menu_text);
    change_dictionary_actions.reserve(dic_list_->count() - 1);
    {
      const QListWidgetItem *selected_dict = GetFirstSelectedDictionary();
      if (selected_dict != NULL) {
        for (size_t i = 0; i < dic_list_->count(); ++i) {
          QListWidgetItem *item = dic_list_->item(i);
          DCHECK(item);
          if (item == selected_dict) {
            // Do not add the current dictionary into the "Move to" list.
            continue;
          }
          change_dictionary_actions.push_back(
              make_pair(i, move_to->addAction(item->text())));
        }
      }
    }
  }
  QAction *delete_action = menu->addAction(delete_menu_text);

  menu->addSeparator();
  QMenu *change_category_to = menu->addMenu(tr("Change category to"));
  vector<string> pos_list;
  user_pos_->GetPOSList(&pos_list);
  vector<QAction *> change_pos_actions(pos_list.size());
  for (size_t i = 0; i < pos_list.size(); ++i) {
    change_pos_actions[i] = change_category_to->addAction(
        QString::fromUtf8(pos_list[i].c_str()));
  }
  QAction *edit_comment_action = menu->addAction(tr("Edit comment"));

  QAction *selected_action = menu->exec(QCursor::pos());

  if (selected_action == add_action) {
    AddWord();
  } else if (selected_action == delete_action) {
    DeleteWord();
  } else if (selected_action == edit_comment_action) {
    EditComment();
  } else {
    bool found = false;
    for (int i = 0; i < change_pos_actions.size(); ++i) {
      if (selected_action == change_pos_actions[i]) {
        EditPOS(pos_list[i]);
        found = true;
        break;
      }
    }
    if (!found) {
      for (int i = 0; i < change_dictionary_actions.size(); ++i) {
        if (selected_action == change_dictionary_actions[i].second) {
          MoveTo(change_dictionary_actions[i].first);
          found = true;
          break;
        }
      }
    }
  }
}

void DictionaryTool::OnContextMenuRequestedForList(const QPoint &pos) {
  QListWidgetItem *item = dic_list_->itemAt(pos);
  if (item == NULL) {
    return;
  }

  QMenu *menu = new QMenu(this);

  QAction *rename_action = menu->addAction(tr("Rename..."));
  QAction *delete_action = menu->addAction(tr("Delete"));
  QAction *import_action = menu->addAction(tr("Import to this dictionary..."));
  QAction *export_action = menu->addAction(tr("Export this dictionary..."));
  QAction *selected_action = menu->exec(QCursor::pos());

  if ((rename_action != NULL) && (selected_action == rename_action)) {
    RenameDictionary();
  } else if ((delete_action != NULL) && (selected_action == delete_action)) {
    DeleteDictionary();
  } else if ((import_action != NULL) && (selected_action == import_action)) {
    ImportAndAppendDictionary();
  } else if ((export_action != NULL) && (selected_action == export_action)) {
    ExportDictionary();
  }
}

DictionaryTool::DictionaryInfo DictionaryTool::current_dictionary() const {
  DictionaryInfo retval = { -1, 0, NULL };

  QListWidgetItem *selected_dict = GetFirstSelectedDictionary();
  if (selected_dict == NULL) {
    return retval;
  }

  retval.row  = dic_list_->row(selected_dict);
  retval.id   = selected_dict->data(Qt::UserRole).toULongLong();
  retval.item = selected_dict;
  return retval;
}

void DictionaryTool::SyncToStorage() {
  if (current_dic_id_ == 0 || !modified_) {
    return;
  }

  UserDictionary *dic =
      session_->mutable_storage()->GetUserDictionary(current_dic_id_);

  if (dic == NULL) {
    LOG(ERROR) << "No save dictionary: " << current_dic_id_;
    return;
  }

  dic->clear_entries();

  for (int i = 0; i < dic_content_->rowCount(); ++i) {
    UserDictionary::Entry *entry =
        dic->add_entries();
    entry->set_key(dic_content_->item(i, 0)->text().toStdString());
    entry->set_value(dic_content_->item(i, 1)->text().toStdString());
    entry->set_pos(UserDictionaryUtil::ToPosType(
        dic_content_->item(i, 2)->text().toStdString().c_str()));
    entry->set_comment(dic_content_->item(i, 3)->text().toStdString());
    UserDictionaryUtil::SanitizeEntry(entry);
  }

  modified_ = false;
}

void DictionaryTool::CreateDictionaryHelper(const QString &dic_name) {
  uint64 new_dic_id = 0;
  if (!session_->mutable_storage()->CreateDictionary(
          dic_name.toStdString(),
          &new_dic_id)) {
    LOG(ERROR) << "Failed to create a new dictionary.";
    ReportError();
    return;
  }

  QListWidgetItem *item = new QListWidgetItem(dic_list_);
  DCHECK(item);
  item->setText(dic_name);
  item->setData(Qt::UserRole,
                QVariant(static_cast<qulonglong>(new_dic_id)));

  dic_list_->setCurrentRow(dic_list_->row(item));

  AddWord();
}

bool DictionaryTool::InitDictionaryList() {
  dic_list_->clear();
  const UserDictionaryStorage &storage = session_->storage();

  for (size_t i = 0; i < storage.dictionaries_size(); ++i) {
    QListWidgetItem *item = new QListWidgetItem(dic_list_);
    DCHECK(item);
    item->setText(storage.dictionaries(i).name().c_str());
    item->setData(Qt::UserRole,
                  QVariant(
                      static_cast<qulonglong>(storage.dictionaries(i).id())));
  }

  UpdateUIStatus();

  return true;
}

QString DictionaryTool::PromptForDictionaryName(const QString &text,
                                                const QString &label) {
  bool ok = false;
  QString dic_name;
  do {
    dic_name = QInputDialog::getText(
        this, window_title_, label, QLineEdit::Normal, text, &ok,
        // To disable context help on Windows.
        Qt::WindowTitleHint | Qt::WindowSystemMenuHint);
  } while (ok && dic_name.isEmpty());

  if (!ok) {
    LOG(WARNING) << "Canceled by the user.";
    return "";
  }

  return dic_name;
}

void DictionaryTool::ReportError() {
  switch (session_->mutable_storage()->GetLastError()) {
    case mozc::UserDictionaryStorage::INVALID_CHARACTERS_IN_DICTIONARY_NAME:
      LOG(ERROR) << "Dictionary name contains an invalid character.";
      QMessageBox::critical(
          this, window_title_,
          tr("An invalid character is included in the dictionary name."));
      break;
    case mozc::UserDictionaryStorage::EMPTY_DICTIONARY_NAME:
      LOG(ERROR) << "Dictionary name is empty.";
      QMessageBox::critical(this, window_title_,
                            tr("Dictionary name is empty."));
      break;
    case mozc::UserDictionaryStorage::TOO_LONG_DICTIONARY_NAME:
      LOG(ERROR) << "Dictionary name is too long.";
      QMessageBox::critical(this, window_title_,
                            tr("Dictionary name is too long."));
      break;
    case mozc::UserDictionaryStorage::DUPLICATED_DICTIONARY_NAME:
      LOG(ERROR) << "duplicated dictionary name";
      QMessageBox::critical(this, window_title_,
                            tr("Dictionary already exists."));
      break;
    default:
      LOG(ERROR) << "A fatal error occurred";
      QMessageBox::critical(this, window_title_,
                            tr("A fatal error occurred."));
      break;
  }
}

void DictionaryTool::StartMonitoringUserEdit() {
  if (monitoring_user_edit_) {
    return;
  }
  connect(dic_content_, SIGNAL(itemChanged(QTableWidgetItem *)),
          this, SLOT(OnItemChanged(QTableWidgetItem *)));
  monitoring_user_edit_ = true;
}

void DictionaryTool::StopMonitoringUserEdit() {
  if (!monitoring_user_edit_) {
    return;
  }
  disconnect(dic_content_, SIGNAL(itemChanged(QTableWidgetItem *)),
             this, SLOT(OnItemChanged(QTableWidgetItem *)));
  monitoring_user_edit_ = false;
}

void DictionaryTool::SaveAndReloadServer() {
  if (!session_->mutable_storage()->Save() &&
      session_->mutable_storage()->GetLastError() ==
      mozc::UserDictionaryStorage::SYNC_FAILURE) {
    LOG(ERROR) << "Cannot save dictionary";
    return;
  }

  // If server is not running, we don't need to
  // execute Reload command.
  if (!client_->PingServer()) {
    LOG(WARNING) << "Server is not running. Do nothing";
    return;
  }

  // Update server version if need be.
  if (!client_->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return;
  }

  // We don't show any dialog even when an error happens, since
  // dictionary serialization is finished correctly.
  if (!client_->Reload()) {
    LOG(ERROR) << "Reload command failed";
  }
}

bool DictionaryTool::IsReadableToImport(const string &file_name) {
  QFileInfo file_info(file_name.c_str());
  return file_info.isFile() && file_info.isReadable();
}

bool DictionaryTool::IsWritableToExport(const string &file_name) {
  QFileInfo file_info(file_name.c_str());
  if (file_info.exists()) {
    return file_info.isFile() && file_info.isWritable();
  } else {
    QFileInfo dir_info(file_info.dir().absolutePath());
    // This preprocessor conditional is a workaround for a problem
    // that export fails on Windows.
#ifdef OS_WIN
    return dir_info.isExecutable();
#else
    return dir_info.isExecutable() && dir_info.isWritable();
#endif
  }
}

void DictionaryTool::paintEvent(QPaintEvent *event) {
#ifdef OS_WIN
  if (!gui::WinUtil::IsCompositionEnabled()) {
    return;
  }

  const QRect message_rect = WinUtil::GetTextRect(this, statusbar_message_);
  const int kMargin = 2;
  const int kGlowSize = 10;
  const QRect rect(QPoint(kMargin,
                          this->height() - statusbar_->height()),
                   QPoint(message_rect.width() + kMargin + kGlowSize * 2,
                          this->height()));

  statusbar_->clearMessage();
  QPainter painter(this);
  WinUtil::DrawThemeText(statusbar_message_, rect, kGlowSize, &painter);
#endif
}

void DictionaryTool::UpdateUIStatus() {
  const bool is_enable_new_dic =
      dic_list_->count() < session_->mutable_storage()->max_dictionary_size();
  new_action_->setEnabled(is_enable_new_dic);
  import_create_action_->setEnabled(is_enable_new_dic);

  delete_action_->setEnabled(dic_list_->count() > 0);
  import_append_action_->setEnabled(dic_list_->count() > 0);
#ifdef OS_WIN
  import_default_ime_action_->setEnabled(dic_list_->count() > 0);
#endif

  const bool is_enable_new_word =
      dic_list_->count() > 0 &&
      dic_content_->rowCount() < max_entry_size_;

  new_word_button_->setEnabled(is_enable_new_word);
  delete_word_button_->setEnabled(dic_content_->rowCount() > 0);

  const DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item != NULL) {
    statusbar_message_ =  QString(tr("%1: %2 entries")).arg(
        dic_info.item->text()).arg(
            dic_content_->rowCount());
  } else {
    statusbar_message_.clear();
  }

#ifdef OS_WIN
  if (!gui::WinUtil::IsCompositionEnabled()) {
    statusbar_->showMessage(statusbar_message_);
  } else {
    update();
  }
#else
  statusbar_->showMessage(statusbar_message_);
#endif
}

#ifdef OS_WIN
bool DictionaryTool::winEvent(MSG *message, long *result) {
  if (message != NULL &&
      message->message == WM_LBUTTONDOWN &&
      toolbar_->cursor().shape() == Qt::ArrowCursor &&
      WinUtil::IsCompositionEnabled()) {
    const QWidget *widget = qApp->widgetAt(
        mapToGlobal(QPoint(message->lParam & 0xFFFF,
                           (message->lParam >> 16) & 0xFFFF)));
    if (widget == statusbar_ || widget == toolbar_) {
      ::PostMessage(message->hwnd, WM_NCLBUTTONDOWN,
                    static_cast<WPARAM>(HTCAPTION), message->lParam);
      return true;
    }
  }

  return QWidget::winEvent(message, result);
}
#endif
}  // namespace gui
}  // namespace mozc

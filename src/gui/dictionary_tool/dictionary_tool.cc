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

#include "gui/dictionary_tool/dictionary_tool.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QShortcut>
#include <QTimer>
#include <QtGui>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ios>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "base/vlog.h"
#include "client/client.h"
#include "dictionary/user_dictionary_importer.h"
#include "dictionary/user_dictionary_session.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "gui/base/encoding_util.h"
#include "gui/base/util.h"
#include "gui/config_dialog/combobox_delegate.h"
#include "gui/dictionary_tool/find_dialog.h"
#include "gui/dictionary_tool/import_dialog.h"
#include "protocol/user_dictionary_storage.pb.h"

#ifdef _WIN32
#include <windows.h>

#include "base/run_level.h"
#include "gui/base/msime_user_dictionary_importer.h"
#include "gui/base/win_util.h"
#endif  // _WIN32

#if defined(__ANDROID__) || defined(__wasm__)
#error "This platform is not supported."
#endif  // __ANDROID__ || __wasm__

namespace mozc {
namespace gui {
namespace {

using ::mozc::user_dictionary::UserDictionary;
using ::mozc::user_dictionary::UserDictionaryCommandStatus;
using ::mozc::user_dictionary::UserDictionarySession;
using ::mozc::user_dictionary::UserDictionaryStorage;

inline QString QUtf8(absl::string_view str) {
  return QString::fromUtf8(str.data(), static_cast<int>(str.size()));
}

// set longer timeout because it takes longer time
// to reload all user dictionary.
constexpr absl::Duration kSessionTimeout = absl::Milliseconds(100000);

int GetTableHeight(QTableWidget *widget) {
  // Dragon Hack:
  // Here we use "龍" to calc font size, as it looks almsot square
  const QRect rect =
      QFontMetrics(widget->font()).boundingRect(QStringLiteral("龍"));
  return static_cast<int>(rect.height() * 1.4);
}

std::unique_ptr<QProgressDialog> CreateProgressDialog(const QString &message,
                                                      QWidget *parent,
                                                      int size) {
  auto progress =
      std::make_unique<QProgressDialog>(message, QString(), 0, size, parent);
  CHECK(progress);
  progress->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint |
                           Qt::WindowCloseButtonHint);
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

// Use QTextStream to read UTF16 text -- we can't use ifstream,
// since ifstream cannot handle Wide character.
class UTF16TextLineIterator
    : public UserDictionaryImporter::TextLineIteratorInterface {
 public:
  UTF16TextLineIterator(UserDictionaryImporter::EncodingType encoding_type,
                        const std::string &filename, const QString &message,
                        QWidget *parent)
      : stream_(std::make_unique<QTextStream>()) {
    CHECK_EQ(UserDictionaryImporter::UTF16, encoding_type);
    file_.setFileName(QUtf8(filename));
    if (!file_.open(QIODevice::ReadOnly)) {
      LOG(ERROR) << "Cannot open: " << filename;
    }
    stream_->setDevice(&file_);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream_->setEncoding(QStringConverter::Utf16);
#else   // Qt5 or lower
    stream_->setCodec("UTF-16");
#endif  // QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    progress_ = CreateProgressDialog(message, parent, file_.size());
  }

  bool IsAvailable() const override { return file_.error() == QFile::NoError; }

  bool Next(std::string *line) override {
    if (stream_->atEnd()) {
      return false;
    }

    // Can't use ReadLine as ReadLine doesn't support CR only text.
    QChar ch;
    QString output_line;
    while (!stream_->atEnd()) {
      *stream_ >> ch;
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

    *line = output_line.toStdString();
    return true;
  }

  void Reset() override {
    file_.seek(0);
    stream_ = std::make_unique<QTextStream>();
    stream_->setDevice(&file_);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream_->setEncoding(QStringConverter::Utf16);
#else   // Qt5 or lower
    stream_->setCodec("UTF-16");
#endif  // QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  }

 private:
  QFile file_;
  std::unique_ptr<QTextStream> stream_;
  std::unique_ptr<QProgressDialog> progress_;
};

class MultiByteTextLineIterator
    : public UserDictionaryImporter::TextLineIteratorInterface {
 public:
  MultiByteTextLineIterator(UserDictionaryImporter::EncodingType encoding_type,
                            const std::string &filename, const QString &message,
                            QWidget *parent)
      : encoding_type_(encoding_type),
        ifs_(std::make_unique<InputFileStream>(filename)),
        first_line_(true) {
    const std::streampos begin = ifs_->tellg();
    ifs_->seekg(0, std::ios::end);
    const size_t size = static_cast<size_t>(ifs_->tellg() - begin);
    ifs_->seekg(0, std::ios::beg);
    progress_ = CreateProgressDialog(message, parent, size);
  }

  bool IsAvailable() const override {
    // This means that neither failbit nor badbit is set.
    // TODO(yukawa): Consider to remove |ifs_->eof()|. Furthermore, we should
    // replace IsAvailable() with something easier to understand, e.g.,
    // Done() or HasNext().
    return ifs_->good() || ifs_->eof();
  }

  bool Next(std::string *line) override {
    if (!ifs_->good()) {
      return false;
    }

    // Can't use getline as getline doesn't support CR only text.
    char ch;
    std::string output_line;
    while (ifs_->good()) {
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
      *line = EncodingUtil::SjisToUtf8(*line);
    }

    // strip UTF8 BOM
    if (first_line_ && encoding_type_ == UserDictionaryImporter::UTF8) {
      *line = Util::StripUtf8Bom(*line);
    }

    Util::ChopReturns(line);

    first_line_ = false;
    return true;
  }

  void Reset() override {
    // Clear state bit (eofbit, failbit, badbit).
    ifs_->clear();
    ifs_->seekg(0, std::ios_base::beg);
    first_line_ = true;
  }

 private:
  UserDictionaryImporter::EncodingType encoding_type_;
  std::unique_ptr<InputFileStream> ifs_;
  std::unique_ptr<QProgressDialog> progress_;
  bool first_line_;
};

std::unique_ptr<UserDictionaryImporter::TextLineIteratorInterface>
CreateTextLineIterator(UserDictionaryImporter::EncodingType encoding_type,
                       const std::string &filename, QWidget *parent) {
  if (encoding_type == UserDictionaryImporter::ENCODING_AUTO_DETECT) {
    encoding_type = UserDictionaryImporter::GuessFileEncodingType(filename);
  }

  if (encoding_type == UserDictionaryImporter::NUM_ENCODINGS) {
    LOG(ERROR) << "GuessFileEncodingType() returns UNKNOWN encoding.";
    // set default encoding
#ifdef _WIN32
    encoding_type = UserDictionaryImporter::SHIFT_JIS;
#else   // _WIN32
    encoding_type = UserDictionaryImporter::UTF16;
#endif  // _WIN32
  }

  MOZC_VLOG(1) << "Setting Encoding to: " << static_cast<int>(encoding_type);

  const QString message = QObject::tr("Importing new words...");

  switch (encoding_type) {
    case UserDictionaryImporter::UTF8:
    case UserDictionaryImporter::SHIFT_JIS:
      return std::make_unique<MultiByteTextLineIterator>(
          encoding_type, filename, message, parent);
      break;
    case UserDictionaryImporter::UTF16:
      return std::make_unique<UTF16TextLineIterator>(encoding_type, filename,
                                                     message, parent);
      break;
    default:
      return nullptr;
  }

  return nullptr;
}
}  // namespace

DictionaryTool::DictionaryTool(QWidget *parent)
    : QMainWindow(parent),
      import_dialog_(nullptr),
      find_dialog_(nullptr),
      session_(std::make_unique<UserDictionarySession>(
          UserDictionaryUtil::GetUserDictionaryFileName())),
      current_dic_id_(0),
      window_title_(GuiUtil::ProductName()),
      dic_menu_(new QMenu),
      new_action_(nullptr),
      rename_action_(nullptr),
      delete_action_(nullptr),
      find_action_(nullptr),
      import_create_action_(nullptr),
      import_append_action_(nullptr),
      export_action_(nullptr),
      import_default_ime_action_(nullptr),
      client_(client::ClientFactory::NewClient()),
      max_entry_size_(mozc::UserDictionaryStorage::max_entry_size()),
      modified_(false),
      monitoring_user_edit_(false),
      is_available_(true) {
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

  // main window
#ifndef __linux__
  // For some reason setCentralWidget crashes the dictionary_tool on Linux
  // TODO(taku): investigate the cause of the crashes
  setCentralWidget(splitter_);
#endif  // __linux__

  setContextMenuPolicy(Qt::NoContextMenu);

  // toolbar
  toolbar_->setFloatable(false);
  toolbar_->setMovable(false);
  dic_menu_button_ = new QPushButton(tr("Tools"), this);
  dic_menu_button_->setFlat(true);
  new_word_button_ = new QPushButton(tr("Add"), this);
  new_word_button_->setFlat(true);
  delete_word_button_ = new QPushButton(tr("Remove"), this);
  delete_word_button_->setFlat(true);
  toolbar_->addWidget(dic_menu_button_);
  toolbar_->addWidget(new_word_button_);
  toolbar_->addWidget(delete_word_button_);

  // Cosmetic tweaks for Mac.
#ifdef __APPLE__
  dic_list_->setAttribute(Qt::WA_MacShowFocusRect, false);
  dic_list_->setAutoFillBackground(true);
  dic_list_->setFrameStyle(QFrame::NoFrame);

  dic_content_->setFrameStyle(QFrame::NoFrame);
  dic_content_->setShowGrid(false);
#endif  // __APPLE__

  dic_content_->setWordWrap(false);
  dic_content_->verticalHeader()->hide();
  dic_content_->horizontalHeader()->setStretchLastSection(true);
  dic_content_->horizontalHeader()->setSortIndicatorShown(true);
  dic_content_->horizontalHeader()->setHighlightSections(false);
  dic_content_->setAlternatingRowColors(true);

  // We fix the row height so that paint() is executed faster.
  // This changes allows DictionaryTool handle about 1M words.
  dic_content_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  dic_content_->verticalHeader()->setDefaultSectionSize(
      GetTableHeight(dic_content_));

  // Get a list of POS and set a custom delagate that holds the list.
  const std::vector<std::string> tmp_pos_vec = pos_list_provider_.GetPosList();
  QStringList pos_list;
  for (size_t i = 0; i < tmp_pos_vec.size(); ++i) {
    pos_list.append(QUtf8(tmp_pos_vec[i]));
  }
  ComboBoxDelegate *delegate = new ComboBoxDelegate;
  delegate->SetItemList(pos_list);
  dic_content_->setItemDelegateForColumn(2, delegate);

  // Set the default POS. It should be "名詞".
  default_pos_ = pos_list[pos_list_provider_.GetPosListDefaultIndex()];
  DCHECK(default_pos_ == "名詞") << "The default POS is not 名詞";

  // Set up the main table widget for dictionary contents.
  dic_content_->setColumnCount(4);
  QStringList header_labels;
  header_labels << tr("Reading") << tr("Word") << tr("Category")
                << tr("Comment");
  dic_content_->setHorizontalHeaderLabels(header_labels);

#ifdef __APPLE__
  // This is a workaround for MacOSX.
  // QKeysequence::Delete doesn't work on Mac,
  // so we set the key sequence directly.
  QShortcut *shortcut1 =
      new QShortcut(QKeySequence(Qt::Key_Backspace), dic_content_);
  // Qt::CTRL = Command key
  // Command+Backspace = delete
  QShortcut *shortcut2 =
      new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Backspace), dic_content_);
  connect(shortcut1, SIGNAL(activated()), this, SLOT(DeleteWord()));
  connect(shortcut2, SIGNAL(activated()), this, SLOT(DeleteWord()));
#else   // __APPLE__
  QShortcut *shortcut =
      new QShortcut(QKeySequence(QKeySequence::Delete), dic_content_);
  connect(shortcut, SIGNAL(activated()), this, SLOT(DeleteWord()));
#endif  // __APPLE__

  dic_content_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(dic_content_, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(OnContextMenuRequestedForContent(const QPoint &)));
  connect(dic_content_->horizontalHeader(), SIGNAL(sectionClicked(int)), this,
          SLOT(OnHeaderClicked(int)));

  dic_list_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(dic_list_, SIGNAL(customContextMenuRequested(const QPoint &)), this,
          SLOT(OnContextMenuRequestedForList(const QPoint &)));

  // Set up the menu for dictionary related operations.
  new_action_ = dic_menu_->addAction(tr("New dictionary..."));
  rename_action_ = dic_menu_->addAction(tr("Rename dictionary..."));
  delete_action_ = dic_menu_->addAction(tr("Delete dictionary"));
  find_action_ = dic_menu_->addAction(tr("Find..."));
  dic_menu_->addSeparator();
  import_create_action_ =
      dic_menu_->addAction(tr("Import as new dictionary..."));
  import_append_action_ =
      dic_menu_->addAction(tr("Import to current dictionary..."));
  export_action_ = dic_menu_->addAction(tr("Export current dictionary..."));

  // add Import from MS-IME's dictionary
#ifdef _WIN32
  dic_menu_->addSeparator();
  import_default_ime_action_ = dic_menu_->addAction(
      tr("Import from %1's user dictionary...").arg("Microsoft IME"));
#endif  // _WIN32

  dic_menu_button_->setMenu(dic_menu_);
  connect(new_action_, SIGNAL(triggered()), this, SLOT(CreateDictionary()));
  connect(rename_action_, SIGNAL(triggered()), this, SLOT(RenameDictionary()));
  connect(delete_action_, SIGNAL(triggered()), this, SLOT(DeleteDictionary()));
  connect(find_action_, SIGNAL(triggered()), find_dialog_, SLOT(show()));
  connect(import_create_action_, SIGNAL(triggered()), this,
          SLOT(ImportAndCreateDictionary()));
  connect(import_append_action_, SIGNAL(triggered()), this,
          SLOT(ImportAndAppendDictionary()));
  connect(export_action_, SIGNAL(triggered()), this, SLOT(ExportDictionary()));

#ifdef _WIN32
  connect(import_default_ime_action_, SIGNAL(triggered()), this,
          SLOT(ImportFromDefaultIME()));
#endif  // _WIN32

  // Signals and slots to connect buttons to actions.
  connect(new_word_button_, SIGNAL(clicked()), this, SLOT(AddWord()));
  connect(delete_word_button_, SIGNAL(clicked()), this, SLOT(DeleteWord()));

  // When empty area of Dictionary Table is clicked,
  // insert a new word. This is only enabled on Mac
  connect(dic_content_, SIGNAL(emptyAreaClicked()), this, SLOT(AddWord()));

  // Signals and slots to update state of objects on the window in
  // response to change of selection/data of items.
  connect(dic_list_, SIGNAL(itemSelectionChanged()), this,
          SLOT(OnDictionarySelectionChanged()));

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
  splitter_->setSizes(QList<int>() << static_cast<int>(width() * 0.25)
                                   << static_cast<int>(width() * 0.75));

  // adjust column width
  dic_content_->setColumnWidth(0, static_cast<int>(width() * 0.18));
  dic_content_->setColumnWidth(1, static_cast<int>(width() * 0.18));
  dic_content_->setColumnWidth(2, static_cast<int>(width() * 0.18));

  sort_state_.sorted = false;

  // If this is the first time for the user dictionary is used, create
  // a default dictionary.
  if (absl::Status s = session_->mutable_storage()->Exists(); !s.ok()) {
    LOG_IF(ERROR, !absl::IsNotFound(s))
        << "Cannot check if the storage file exists: " << s;
    CreateDictionaryHelper(tr("User Dictionary 1"));
  }

  // for Mac-like style
#ifdef __APPLE__
  setUnifiedTitleAndToolBarOnMac(true);
#endif  // __APPLE__

  StartMonitoringUserEdit();
  UpdateUIStatus();

  GuiUtil::ReplaceWidgetLabels(this);
  qApp->installEventFilter(this);
}

void DictionaryTool::closeEvent(QCloseEvent *event) {
  // QEvent::ApplicationDeactivate is not emitted here.

  // Change the focus so that the last incomplete itmes on
  // TableView is submitted to the model
  dic_menu_button_->setFocus(Qt::MouseFocusReason);

  SyncToStorage();
  SaveAndReloadServer();

  if (session_->mutable_storage()->GetLastError() ==
      mozc::UserDictionaryStorage::TOO_BIG_FILE_BYTES) {
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
  // This is a workaround for http://b/2190275.
  // TODO(taku): Find out a better way.
  if (event->type() == QEvent::ApplicationDeactivate) {
    constexpr int kDelayOnDeactivateTime = 200;
    QTimer::singleShot(kDelayOnDeactivateTime, this, SLOT(OnDeactivate()));
  }
  return QWidget::eventFilter(obj, event);
}

void DictionaryTool::OnDictionarySelectionChanged() {
  SyncToStorage();

  DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item == nullptr) {
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

void DictionaryTool::SetupDicContentEditor(const DictionaryInfo &dic_info) {
  UserDictionary *dic =
      session_->mutable_storage()->GetUserDictionary(dic_info.id);

  if (dic == nullptr) {
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
    std::unique_ptr<QProgressDialog> progress(CreateProgressDialog(
        tr("Updating the current view data..."), this, dic->entries_size()));

    for (size_t i = 0; i < dic->entries_size(); ++i) {
      const UserDictionary::Entry &entry = dic->entries(i);
      dic_content_->setItem(i, 0, new QTableWidgetItem(QUtf8(entry.key())));
      dic_content_->setItem(i, 1, new QTableWidgetItem(QUtf8(entry.value())));
      dic_content_->setItem(
          i, 2,
          new QTableWidgetItem(
              QUtf8(UserDictionaryUtil::GetStringPosType(entry.pos()))));
      dic_content_->setItem(i, 3, new QTableWidgetItem(QUtf8(entry.comment())));
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
      PromptForDictionaryName(QString(), tr("Name of the new dictionary"));
  if (dic_name.isEmpty()) {
    return;  // canceld by user
  }

  SyncToStorage();

  CreateDictionaryHelper(dic_name);
}

void DictionaryTool::DeleteDictionary() {
  DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item == nullptr) {
    QMessageBox::information(this, window_title_,
                             tr("No dictionary is selected."));
    return;
  }

  if (QMessageBox::question(
          this, window_title_,
          tr("Do you want to delete %1?").arg(dic_info.item->text()),
          QMessageBox::Yes | QMessageBox::No,
          QMessageBox::No) != QMessageBox::Yes) {
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
  if (dic_info.item == nullptr) {
    QMessageBox::information(this, window_title_,
                             tr("No dictionary is selected."));
    return;
  }

  const QString dic_name = PromptForDictionaryName(
      dic_info.item->text(), tr("New name of the dictionary"));
  if (dic_name.isEmpty()) {
    return;
  }

  if (!session_->mutable_storage()->RenameDictionary(dic_info.id,
                                                     dic_name.toStdString())) {
    LOG(ERROR) << "Failed to rename the dictionary.";
    ReportError();
    return;
  }

  dic_info.item->setText(dic_name);

  UpdateUIStatus();
}

void DictionaryTool::ImportAndCreateDictionary() {
  const int max_size =
      static_cast<int>(mozc::UserDictionaryStorage::max_dictionary_size());
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

  ImportHelper(0,  // dic_id == 0 means that "CreateNewDictionary" mode
               import_dialog_->dic_name(), import_dialog_->file_name(),
               import_dialog_->ime_type(), import_dialog_->encoding_type());
}

void DictionaryTool::ImportAndAppendDictionary() {
  DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item == nullptr) {
    LOG(WARNING) << "No dictionary to import is selected";
    QMessageBox::information(this, window_title_,
                             tr("No dictionary is selected."));
    return;
  }

  if (dic_content_->rowCount() >= max_entry_size_) {
    QMessageBox::critical(this, window_title_,
                          tr("You can't have more than %1 "
                             "words in one dictionary.")
                              .arg(max_entry_size_));
    return;
  }

  if (import_dialog_->ExecInAppendMode() != QDialog::Accepted) {
    LOG(WARNING) << "Cancele by the user.";
    return;
  }

  ImportHelper(dic_info.id, dic_info.item->text(), import_dialog_->file_name(),
               import_dialog_->ime_type(), import_dialog_->encoding_type());
}

void DictionaryTool::ReportImportError(UserDictionaryImporter::ErrorType error,
                                       const QString &dic_name,
                                       int added_entries_size) {
  switch (error) {
    case UserDictionaryImporter::IMPORT_NO_ERROR:
      QMessageBox::information(this, window_title_,
                               tr("%1 entries are imported to %2.")
                                   .arg(added_entries_size)
                                   .arg(dic_name));
      break;
    case UserDictionaryImporter::IMPORT_NOT_SUPPORTED:
      QMessageBox::information(this, window_title_,
                               tr("You have imported a file in an invalid or "
                                  "unsupported file format.\n\n"
                                  "Please check the file format. "
                                  "ATOK11 or older format is not supported by "
                                  "%1.")
                                   .arg(GuiUtil::ProductName()));
      break;
    case UserDictionaryImporter::IMPORT_TOO_MANY_WORDS:
      QMessageBox::information(
          this, window_title_,
          tr("%1 doesn't have enough space to import all words in "
             "the file. First %2 entries "
             "are imported.")
              .arg(dic_name)
              .arg(added_entries_size));
      break;
    case UserDictionaryImporter::IMPORT_INVALID_ENTRIES:
      QMessageBox::information(
          this, window_title_,
          tr("%1 entries are imported to %2.\n\nSome imported "
             "words were not recognized by %3. "
             "Please check the original import file.")
              .arg(added_entries_size)
              .arg(dic_name)
              .arg(window_title_));
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
    uint64_t dic_id, const QString &dic_name, const QString &file_name,
    UserDictionaryImporter::IMEType ime_type,
    UserDictionaryImporter::EncodingType encoding_type) {
  if (!IsReadableToImport(file_name)) {
    LOG(ERROR) << "File is not readable to import.";
    QMessageBox::critical(this, window_title_,
                          tr("Can't open %1.").arg(file_name));
    return;
  }

  const std::string dic_name_str = dic_name.toStdString();
  if (dic_id == 0 &&
      !session_->mutable_storage()->CreateDictionary(dic_name_str, &dic_id)) {
    LOG(ERROR) << "Failed to create the dictionary.";
    ReportError();
    return;
  }

  UserDictionary *dic = session_->mutable_storage()->GetUserDictionary(dic_id);

  if (dic == nullptr) {
    LOG(ERROR) << "Cannot find dictionary id: " << dic_id;
    ReportError();
    return;
  }

  if (dic->name() != dic_name_str) {
    LOG(ERROR) << "Dictionary name is inconsistent";
    ReportError();
    return;
  }

  // Everything looks Okey so far. Now starting import operation.
  SyncToStorage();

  // Open dictionary
  std::unique_ptr<UserDictionaryImporter::TextLineIteratorInterface> iter(
      CreateTextLineIterator(encoding_type, file_name.toStdString(), this));
  if (iter == nullptr) {
    LOG(ERROR) << "CreateTextLineIterator returns nullptr";
    return;
  }

  const int old_size = dic->entries_size();
  const UserDictionaryImporter::ErrorType error =
      UserDictionaryImporter::ImportFromTextLineIterator(ime_type, iter.get(),
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
#ifdef _WIN32
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
  if (dic_info.item == nullptr) {
    LOG(WARNING) << "No dictionary to import is selected";
    QMessageBox::information(this, window_title_,
                             tr("No dictionary is selected."));
    return;
  }

  const QMessageBox::StandardButton answer = QMessageBox::information(
      this, window_title_,
      tr("All user-registered words in %1 "
         "are migrated into the "
         "current dictionary.")
          .arg("Microsoft IME"),
      QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
  if (answer != QMessageBox::Ok) {
    LOG(WARNING) << "Cancele by the user.";
    return;
  }

  SyncToStorage();

  UserDictionary *dic =
      session_->mutable_storage()->GetUserDictionary(dic_info.id);
  DCHECK(dic);

  const int old_size = dic->entries_size();

  UserDictionaryImporter::ErrorType error =
      UserDictionaryImporter::IMPORT_NOT_SUPPORTED;
  {
    std::unique_ptr<UserDictionaryImporter::InputIteratorInterface> iter(
        MSIMEUserDictionarImporter::Create());
    if (iter) {
      error = UserDictionaryImporter::ImportFromIterator(iter.get(), dic);
    }
  }

  const int added_entries_size = dic->entries_size() - old_size;

  OnDictionarySelectionChanged();
  UpdateUIStatus();

  ReportImportError(error, dic_info.item->text(), added_entries_size);
#endif  // _WIN32
}

void DictionaryTool::ExportDictionary() {
  DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item == nullptr) {
    LOG(WARNING) << "No dictionary to export is selected";
    QMessageBox::information(this, window_title_,
                             tr("No dictionary is selected."));
    return;
  }

  const QString file_name = QFileDialog::getSaveFileName(
      this, tr("Export dictionary"), QDir::homePath(),
      tr("Text Files (*.txt);;All Files (*)"));
  if (file_name.isEmpty()) {
    return;
  }

  if (!IsWritableToExport(file_name)) {
    LOG(ERROR) << "File is not writable to export.";
    QMessageBox::critical(this, window_title_,
                          tr("Can't open %1.").arg(file_name));
    return;
  }

  SyncToStorage();

  if (!session_->mutable_storage()->ExportDictionary(dic_info.id,
                                                     file_name.toStdString())) {
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
        tr("You can't have more than %1 words in one dictionary.")
            .arg(max_entry_size_));
    return;
  }

  dic_content_->insertRow(row);
  dic_content_->setItem(row, 0, new QTableWidgetItem(QString()));
  dic_content_->setItem(row, 1, new QTableWidgetItem(QString()));
  dic_content_->setItem(row, 2, new QTableWidgetItem(default_pos_));
  dic_content_->setItem(row, 3, new QTableWidgetItem(QString()));

  if (row + 1 >= max_entry_size_) {
    new_word_button_->setEnabled(false);
  }

  QTableWidgetItem *item = dic_content_->item(row, 0);
  dic_content_->setCurrentItem(item);
  dic_content_->editItem(item);

  UpdateUIStatus();
}

void DictionaryTool::GetSortedSelectedRows(std::vector<int> *rows) const {
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

  std::sort(rows->begin(), rows->end(), std::greater<int>());
  std::vector<int>::const_iterator end =
      std::unique(rows->begin(), rows->end());

  rows->resize(end - rows->begin());
}

QListWidgetItem *DictionaryTool::GetFirstSelectedDictionary() const {
  QList<QListWidgetItem *> selected_dicts = dic_list_->selectedItems();
  if (selected_dicts.isEmpty()) {
    LOG(WARNING) << "No current dictionary.";
    return nullptr;
  }
  if (selected_dicts.size() > 1) {
    LOG(WARNING) << "Multiple items are selected.";
  }
  return selected_dicts[0];
}

void DictionaryTool::DeleteWord() {
  std::vector<int> rows;
  GetSortedSelectedRows(&rows);
  if (rows.empty()) {
    return;
  }

  QString message;
  if (rows.size() == 1) {
    message = tr("Do you want to delete this word?");
  } else {
    message = tr("Do you want to delete the selected words?");
  }
  if (QMessageBox::question(this, window_title_, message,
                            QMessageBox::Yes | QMessageBox::No,
                            QMessageBox::No) != QMessageBox::Yes) {
    return;
  }

  setUpdatesEnabled(false);

  {
    std::unique_ptr<QProgressDialog> progress(CreateProgressDialog(
        tr("Deleting the selected words..."), this, rows.size()));

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

void DictionaryTool::EditPos(const absl::string_view pos) {
  const QList<QTableWidgetItem *> items = dic_content_->selectedItems();
  if (items.empty()) {
    return;
  }

  const QString new_pos = QUtf8(pos);
  setUpdatesEnabled(false);

  for (int i = 0; i < items.size(); ++i) {
    const int row = items[i]->row();
    dic_content_->setItem(row, 2, new QTableWidgetItem(new_pos));
  }

  setUpdatesEnabled(true);
  dic_content_->setEnabled(true);
}

void DictionaryTool::MoveTo(int dictionary_row) {
  UserDictionary *target_dict = nullptr;
  {
    const QListWidgetItem *selected_dict = GetFirstSelectedDictionary();
    if (selected_dict == nullptr) {
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

  std::vector<int> rows;
  GetSortedSelectedRows(&rows);
  if (rows.empty()) {
    return;
  }

  const size_t target_max_entry_size =
      mozc::UserDictionaryStorage::max_entry_size();

  if (target_dict->entries_size() + rows.size() > target_max_entry_size) {
    QMessageBox::critical(this, window_title_,
                          tr("Cannot move all the selected items.\n"
                             "The target dictionary can have maximum "
                             "%1 entries.")
                              .arg(target_max_entry_size));
    return;
  }

  setUpdatesEnabled(false);

  {
    // add |rows.size()| items and remove |rows.size()| items
    const int progress_max = rows.size() * 2;
    std::unique_ptr<QProgressDialog> progress(CreateProgressDialog(
        tr("Moving the selected words..."), this, progress_max));

    int progress_index = 0;
    if (target_dict) {
      for (size_t i = 0; i < rows.size(); ++i) {
        UserDictionary::Entry *entry = target_dict->add_entries();
        const int row = rows[i];
        entry->set_key(dic_content_->item(row, 0)->text().toStdString());
        entry->set_value(dic_content_->item(row, 1)->text().toStdString());
        // TODO(yuryu): remove c_str() after changing ToPosType to take
        // absl::string_view.
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
  const QList<QTableWidgetItem *> items = dic_content_->selectedItems();
  if (items.empty()) {
    return;
  }

  bool ok = false;
  const QString new_comment = QInputDialog::getText(
      this, window_title_, tr("New comment"), QLineEdit::Normal, QString(), &ok,
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
  if (item->column() == 0 && !item->text().isEmpty() &&
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
  if (sort_state_.sorted && sort_state_.column == logicalIndex &&
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
  if (item == nullptr) {
    return;
  }

  QMenu *menu = new QMenu(this);
  QAction *add_action = menu->addAction(tr("Add a word"));

  // Count the number of selected words and create delete menu with an
  // appropriate text.
  const QList<QTableWidgetItem *> items = dic_content_->selectedItems();
  QString delete_menu_text = tr("Delete this word");
  QString move_to_menu_text = tr("Move this word to");
  if (!items.empty()) {
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
  std::vector<std::pair<int, QAction *> > change_dictionary_actions;
  // "Move to" is available only when we have 2 or more dictionaries.
  if (dic_list_->count() > 1) {
    QMenu *move_to = menu->addMenu(move_to_menu_text);
    change_dictionary_actions.reserve(dic_list_->count() - 1);
    {
      const QListWidgetItem *selected_dict = GetFirstSelectedDictionary();
      if (selected_dict != nullptr) {
        for (size_t i = 0; i < dic_list_->count(); ++i) {
          QListWidgetItem *item = dic_list_->item(i);
          DCHECK(item);
          if (item == selected_dict) {
            // Do not add the current dictionary into the "Move to" list.
            continue;
          }
          change_dictionary_actions.push_back(
              std::make_pair(i, move_to->addAction(item->text())));
        }
      }
    }
  }
  QAction *delete_action = menu->addAction(delete_menu_text);

  menu->addSeparator();
  QMenu *change_category_to = menu->addMenu(tr("Change category to"));
  const std::vector<std::string> pos_list = pos_list_provider_.GetPosList();
  std::vector<QAction *> change_pos_actions(pos_list.size());
  for (size_t i = 0; i < pos_list.size(); ++i) {
    change_pos_actions[i] = change_category_to->addAction(QUtf8(pos_list[i]));
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
        EditPos(pos_list[i]);
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
  if (item == nullptr) {
    return;
  }

  QMenu *menu = new QMenu(this);

  QAction *rename_action = menu->addAction(tr("Rename..."));
  QAction *delete_action = menu->addAction(tr("Delete"));
  QAction *import_action = menu->addAction(tr("Import to this dictionary..."));
  QAction *export_action = menu->addAction(tr("Export this dictionary..."));
  QAction *selected_action = menu->exec(QCursor::pos());

  if ((rename_action != nullptr) && (selected_action == rename_action)) {
    RenameDictionary();
  } else if ((delete_action != nullptr) && (selected_action == delete_action)) {
    DeleteDictionary();
  } else if ((import_action != nullptr) && (selected_action == import_action)) {
    ImportAndAppendDictionary();
  } else if ((export_action != nullptr) && (selected_action == export_action)) {
    ExportDictionary();
  }
}

DictionaryTool::DictionaryInfo DictionaryTool::current_dictionary() const {
  DictionaryInfo retval = {-1, 0, nullptr};

  QListWidgetItem *selected_dict = GetFirstSelectedDictionary();
  if (selected_dict == nullptr) {
    return retval;
  }

  retval.row = dic_list_->row(selected_dict);
  retval.id = selected_dict->data(Qt::UserRole).toULongLong();
  retval.item = selected_dict;
  return retval;
}

void DictionaryTool::SyncToStorage() {
  if (current_dic_id_ == 0 || !modified_) {
    return;
  }

  UserDictionary *dic =
      session_->mutable_storage()->GetUserDictionary(current_dic_id_);

  if (dic == nullptr) {
    LOG(ERROR) << "No save dictionary: " << current_dic_id_;
    return;
  }

  dic->clear_entries();

  for (int i = 0; i < dic_content_->rowCount(); ++i) {
    UserDictionary::Entry *entry = dic->add_entries();
    entry->set_key(dic_content_->item(i, 0)->text().toStdString());
    entry->set_value(dic_content_->item(i, 1)->text().toStdString());
    // TODO(yuryu): remove c_str() after changing ToPosType to take
    // absl::string_view.
    entry->set_pos(UserDictionaryUtil::ToPosType(
        dic_content_->item(i, 2)->text().toStdString().c_str()));
    entry->set_comment(dic_content_->item(i, 3)->text().toStdString());
    UserDictionaryUtil::SanitizeEntry(entry);
  }

  modified_ = false;
}

void DictionaryTool::CreateDictionaryHelper(const QString &dic_name) {
  uint64_t new_dic_id = 0;
  if (!session_->mutable_storage()->CreateDictionary(dic_name.toStdString(),
                                                     &new_dic_id)) {
    LOG(ERROR) << "Failed to create a new dictionary.";
    ReportError();
    return;
  }

  QListWidgetItem *item = new QListWidgetItem(dic_list_);
  DCHECK(item);
  item->setText(dic_name);
  item->setData(Qt::UserRole, QVariant(static_cast<qulonglong>(new_dic_id)));

  dic_list_->setCurrentRow(dic_list_->row(item));

  AddWord();
}

bool DictionaryTool::InitDictionaryList() {
  dic_list_->clear();
  const UserDictionaryStorage &storage = session_->storage();

  for (size_t i = 0; i < storage.dictionaries_size(); ++i) {
    QListWidgetItem *item = new QListWidgetItem(dic_list_);
    DCHECK(item);
    item->setText(QUtf8(storage.dictionaries(i).name()));
    item->setData(
        Qt::UserRole,
        QVariant(static_cast<qulonglong>(storage.dictionaries(i).id())));
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
      QMessageBox::critical(this, window_title_, tr("A fatal error occurred."));
      break;
  }
}

void DictionaryTool::StartMonitoringUserEdit() {
  if (monitoring_user_edit_) {
    return;
  }
  connect(dic_content_, SIGNAL(itemChanged(QTableWidgetItem *)), this,
          SLOT(OnItemChanged(QTableWidgetItem *)));
  monitoring_user_edit_ = true;
}

void DictionaryTool::StopMonitoringUserEdit() {
  if (!monitoring_user_edit_) {
    return;
  }
  disconnect(dic_content_, SIGNAL(itemChanged(QTableWidgetItem *)), this,
             SLOT(OnItemChanged(QTableWidgetItem *)));
  monitoring_user_edit_ = false;
}

void DictionaryTool::SaveAndReloadServer() {
  if (absl::Status s = session_->mutable_storage()->Save();
      !s.ok() && session_->mutable_storage()->GetLastError() ==
                     mozc::UserDictionaryStorage::SYNC_FAILURE) {
    LOG(ERROR) << "Cannot save dictionary: " << s;
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

bool DictionaryTool::IsReadableToImport(const QString &file_name) {
  QFileInfo file_info(file_name);
  return file_info.isFile() && file_info.isReadable();
}

bool DictionaryTool::IsWritableToExport(const QString &file_name) {
  QFileInfo file_info(file_name);
  if (file_info.exists()) {
    return file_info.isFile() && file_info.isWritable();
  } else {
    QFileInfo dir_info(file_info.dir().absolutePath());
    // This preprocessor conditional is a workaround for a problem
    // that export fails on Windows.
#ifdef _WIN32
    return dir_info.isExecutable();
#else   // _WIN32
    return dir_info.isExecutable() && dir_info.isWritable();
#endif  // _WIN32
  }
}

void DictionaryTool::UpdateUIStatus() {
  const bool is_enable_new_dic =
      dic_list_->count() < session_->mutable_storage()->max_dictionary_size();
  new_action_->setEnabled(is_enable_new_dic);
  import_create_action_->setEnabled(is_enable_new_dic);

  delete_action_->setEnabled(dic_list_->count() > 0);
  import_append_action_->setEnabled(dic_list_->count() > 0);
#ifdef _WIN32
  import_default_ime_action_->setEnabled(dic_list_->count() > 0);
#endif  // _WIN32

  const bool is_enable_new_word =
      dic_list_->count() > 0 && dic_content_->rowCount() < max_entry_size_;

  new_word_button_->setEnabled(is_enable_new_word);
  delete_word_button_->setEnabled(dic_content_->rowCount() > 0);

  const DictionaryInfo dic_info = current_dictionary();
  if (dic_info.item != nullptr) {
    statusbar_message_ = tr("%1: %2 entries")
                             .arg(dic_info.item->text())
                             .arg(dic_content_->rowCount());
  } else {
    statusbar_message_.clear();
  }

  statusbar_->showMessage(statusbar_message_);
}
}  // namespace gui
}  // namespace mozc

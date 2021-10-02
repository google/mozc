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

#include "gui/word_register_dialog/word_register_dialog.h"

#include <cstdint>

#if defined(OS_ANDROID) || defined(OS_WASM)
#error "This platform is not supported."
#endif  // OS_ANDROID || OS_WASM

#ifdef OS_WIN
// clang-format off
#include <windows.h>
#include <imm.h>
// clang-format on
#endif  // OS_WIN

#include <QtGui/QtGui>
#include <QtWidgets/QMessageBox>
#include <cstdlib>
#ifdef OS_WIN
#include <memory>  // for std::unique_ptr
#endif             // OS_WIN
#include <string>
#include <vector>

#include "base/const.h"
#include "base/logging.h"
#include "base/util.h"
#include "client/client.h"
#include "data_manager/pos_list_provider.h"
#include "dictionary/user_dictionary_session.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "gui/base/util.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace gui {

using mozc::user_dictionary::UserDictionary;
using mozc::user_dictionary::UserDictionaryCommandStatus;
using mozc::user_dictionary::UserDictionarySession;
using mozc::user_dictionary::UserDictionaryStorage;

namespace {
constexpr int kSessionTimeout = 100000;
constexpr int kMaxEditLength = 100;
constexpr int kMaxReverseConversionLength = 30;

QString GetEnv(const char *envname) {
#if defined(OS_WIN)
  std::wstring wenvname;
  mozc::Util::Utf8ToWide(envname, &wenvname);
  const DWORD buffer_size =
      ::GetEnvironmentVariable(wenvname.c_str(), nullptr, 0);
  if (buffer_size == 0) {
    return QLatin1String("");
  }
  std::unique_ptr<wchar_t[]> buffer(new wchar_t[buffer_size]);
  const DWORD num_copied =
      ::GetEnvironmentVariable(wenvname.c_str(), buffer.get(), buffer_size);
  if (num_copied > 0) {
    // The size of wchar_t can be 2 or 4.
    if (sizeof(wchar_t) == sizeof(ushort)) {
      // On Windows the size of wchar_t is 2.
      return QString::fromUtf16(reinterpret_cast<const ushort *>(buffer.get()));
    } else {
      // This is a fallback just in case.
      return QString::fromUcs4(reinterpret_cast<const uint *>(buffer.get()));
    }
  }
  return QLatin1String("");
#endif  // OS_WIN
#if defined(__APPLE__) || defined(OS_LINUX)
  return QString::fromUtf8(::getenv(envname));
#endif  // __APPLE__ or OS_LINUX
  // TODO(team): Support other platforms.
  return QLatin1String("");
}
}  // namespace

WordRegisterDialog::WordRegisterDialog()
    : is_available_(true),
      session_(new UserDictionarySession(
          UserDictionaryUtil::GetUserDictionaryFileName())),
      client_(client::ClientFactory::NewClient()),
      window_title_(GuiUtil::ProductName()),
      pos_list_provider_(new PosListProvider()) {
  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint |
                 Qt::WindowStaysOnTopHint);
  setWindowModality(Qt::NonModal);

  ReadinglineEdit->setMaxLength(kMaxEditLength);
  WordlineEdit->setMaxLength(kMaxEditLength);

  if (!SetDefaultEntryFromEnvironmentVariable()) {
#ifdef OS_WIN
    // On Windows, try to use clipboard as a fallback.
    SetDefaultEntryFromClipboard();
#endif  // OS_WIN
  }

  client_->set_timeout(kSessionTimeout);

  if (session_->Load() !=
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    LOG(WARNING) << "UserDictionarySession::Load() failed";
  }

  if (!session_->mutable_storage()->Lock()) {
    QMessageBox::information(
        this, window_title_,
        tr("Close dictionary tool before using word register dialog."));
    is_available_ = false;
    return;
  }

#ifndef ENABLE_CLOUD_SYNC
  if (session_->mutable_storage()
          ->ConvertSyncDictionariesToNormalDictionaries()) {
    LOG(INFO) << "Syncable dictionaries are converted to normal dictionaries";
    if (absl::Status s = session_->mutable_storage()->Save(); !s.ok()) {
      LOG(ERROR) << "Failed to save the storage: " << s;
    }
  }
#endif  // !ENABLE_CLOUD_SYNC

  // Initialize ComboBox
  std::vector<std::string> pos_set;
  pos_list_provider_->GetPosList(&pos_set);
  CHECK(!pos_set.empty());

  for (const std::string &pos : pos_set) {
    CHECK(!pos.empty());
    PartOfSpeechcomboBox->addItem(QString::fromUtf8(pos.c_str()));
  }

  // Create new dictionary if empty
  if (!session_->mutable_storage()->Exists() ||
      session_->storage().dictionaries_size() == 0) {
    const QString name = tr("User Dictionary 1");
    uint64_t dic_id = 0;
    if (!session_->mutable_storage()->CreateDictionary(name.toStdString(),
                                                       &dic_id)) {
      LOG(ERROR) << "Failed to create a new dictionary.";
      is_available_ = false;
      return;
    }
  }

  // Load Dictionary List
  {
    const UserDictionaryStorage &storage = session_->storage();
    CHECK_GT(storage.dictionaries_size(), 0);
    for (const auto &dictionary : storage.dictionaries()) {
      DictionarycomboBox->addItem(QString::fromUtf8(dictionary.name().c_str()));
    }
  }

  connect(WordlineEdit, SIGNAL(textChanged(const QString &)), this,
          SLOT(LineEditChanged(const QString &)));
  connect(ReadinglineEdit, SIGNAL(textChanged(const QString &)), this,
          SLOT(LineEditChanged(const QString &)));
  connect(WordlineEdit, SIGNAL(editingFinished()), this,
          SLOT(CompleteReading()));
  connect(WordRegisterDialogbuttonBox, SIGNAL(clicked(QAbstractButton *)), this,
          SLOT(Clicked(QAbstractButton *)));
  connect(LaunchDictionaryToolpushButton, SIGNAL(clicked()), this,
          SLOT(LaunchDictionaryTool()));

  if (!WordlineEdit->text().isEmpty()) {
    ReadinglineEdit->setFocus(Qt::OtherFocusReason);
    if (!ReadinglineEdit->text().isEmpty()) {
      ReadinglineEdit->selectAll();
    }
  }

  UpdateUIStatus();
  GuiUtil::ReplaceWidgetLabels(this);

  // Turn on IME
  EnableIME();
}

WordRegisterDialog::~WordRegisterDialog() {}

bool WordRegisterDialog::IsAvailable() const { return is_available_; }

void WordRegisterDialog::LineEditChanged(const QString &str) {
  UpdateUIStatus();
}

void WordRegisterDialog::CompleteReading() {
  if (ReadinglineEdit->text().isEmpty()) {
    ReadinglineEdit->setText(GetReading(WordlineEdit->text()));
    ReadinglineEdit->selectAll();
  }
  UpdateUIStatus();
}

void WordRegisterDialog::UpdateUIStatus() {
  const bool enabled =
      !ReadinglineEdit->text().isEmpty() && !WordlineEdit->text().isEmpty();

  QAbstractButton *button =
      WordRegisterDialogbuttonBox->button(QDialogButtonBox::Ok);
  if (button != nullptr) {
    button->setEnabled(enabled);
  }
}

void WordRegisterDialog::Clicked(QAbstractButton *button) {
  switch (WordRegisterDialogbuttonBox->buttonRole(button)) {
    case QDialogButtonBox::AcceptRole:
      switch (SaveEntry()) {
        case EMPTY_KEY:
        case EMPTY_VALUE:
          LOG(FATAL) << "key/value is empty. This case will never occur.";
          return;
        case INVALID_KEY:
          QMessageBox::warning(this, window_title_,
                               tr("Reading part contains invalid characters."));
          return;
        case INVALID_VALUE:
          QMessageBox::warning(this, window_title_,
                               tr("Word part contains invalid characters."));
          return;
        case FATAL_ERROR:
          QMessageBox::warning(this, window_title_,
                               tr("Unexpected error occurs."));
          break;
        case SAVE_FAILURE:
          QMessageBox::warning(this, window_title_,
                               tr("Failed to update user dictionary."));
          break;
        case SAVE_SUCCESS:
          break;
        default:
          return;
      }
      QDialog::accept();
      break;
    default:
      QDialog::reject();
      break;
  }
}

WordRegisterDialog::ErrorCode WordRegisterDialog::SaveEntry() {
  const std::string key = ReadinglineEdit->text().toStdString();
  const std::string value = WordlineEdit->text().toStdString();
  UserDictionary::PosType pos = UserDictionaryUtil::ToPosType(
      PartOfSpeechcomboBox->currentText().toStdString().c_str());

  if (key.empty()) {
    return EMPTY_KEY;
  }

  if (value.empty()) {
    return EMPTY_VALUE;
  }

  if (!UserDictionaryUtil::IsValidReading(key)) {
    return INVALID_KEY;
  }

  if (!UserDictionary::PosType_IsValid(pos)) {
    LOG(ERROR) << "POS is invalid";
    return FATAL_ERROR;
  }

  const int index = DictionarycomboBox->currentIndex();
  if (index < 0 || index >= session_->storage().dictionaries_size()) {
    LOG(ERROR) << "index is out of range";
    return FATAL_ERROR;
  }

  UserDictionary *dic =
      session_->mutable_storage()->GetProto().mutable_dictionaries(index);
  CHECK(dic);

  if (dic->name() != DictionarycomboBox->currentText().toStdString().c_str()) {
    LOG(ERROR) << "Inconsistent dictionary name";
    return FATAL_ERROR;
  }

  UserDictionary::Entry *entry = dic->add_entries();
  CHECK(entry);
  entry->set_key(key);
  entry->set_value(value);
  entry->set_pos(pos);

  if (absl::Status s = session_->mutable_storage()->Save();
      !s.ok() && session_->mutable_storage()->GetLastError() ==
                     mozc::UserDictionaryStorage::SYNC_FAILURE) {
    LOG(ERROR) << "Cannot save dictionary: " << s;
    return SAVE_FAILURE;
  }

  if (!client_->PingServer()) {
    LOG(WARNING) << "Server is not running. Do nothing";
    return SAVE_SUCCESS;
  }

#ifndef __APPLE__
  // Update server version if need be.
  if (!client_->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return SAVE_SUCCESS;
  }
#endif  // __APPLE__

  if (!client_->Reload()) {
    LOG(ERROR) << "Reload command failed";
    return SAVE_SUCCESS;
  }

  return SAVE_SUCCESS;
}

void WordRegisterDialog::LaunchDictionaryTool() {
  session_->mutable_storage()->UnLock();
  client_->LaunchTool("dictionary_tool", "");
  QWidget::close();
}

const QString WordRegisterDialog::GetReading(const QString &str) {
  if (str.isEmpty()) {
    LOG(ERROR) << "given string is empty";
    return QLatin1String("");
  }

  if (str.count() >= kMaxReverseConversionLength) {
    LOG(ERROR) << "too long input";
    return QLatin1String("");
  }

  commands::Output output;
  {
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ON);
    if (!client_->SendKey(key, &output)) {
      LOG(ERROR) << "SendKey failed";
      return QLatin1String("");
    }

    commands::SessionCommand command;
    command.set_type(commands::SessionCommand::CONVERT_REVERSE);
    command.set_text(str.toStdString());

    if (!client_->SendCommand(command, &output)) {
      LOG(ERROR) << "SendCommand failed";
      return QLatin1String("");
    }

    commands::Output dummy_output;
    command.set_type(commands::SessionCommand::REVERT);
    client_->SendCommand(command, &dummy_output);
  }

  if (!output.has_preedit()) {
    LOG(ERROR) << "No preedit";
    return QLatin1String("");
  }

  std::string key;
  for (size_t segment_index = 0;
       segment_index < output.preedit().segment_size(); ++segment_index) {
    const commands::Preedit::Segment &segment =
        output.preedit().segment(segment_index);
    if (!segment.has_key()) {
      LOG(ERROR) << "No segment";
      return QLatin1String("");
    }
    key.append(segment.key());
  }

  if (key.empty() || !UserDictionaryUtil::IsValidReading(key)) {
    LOG(WARNING) << "containing invalid characters";
    return QLatin1String("");
  }

  return QString::fromUtf8(key.c_str());
}

// Get default value from Clipboard
void WordRegisterDialog::SetDefaultEntryFromClipboard() {
  if (QApplication::clipboard() == nullptr) {
    return;
  }
  CopyCurrentSelectionToClipboard();
  const QString value = TrimValue(QApplication::clipboard()->text());
  WordlineEdit->setText(value);
  ReadinglineEdit->setText(GetReading(value));
}

void WordRegisterDialog::CopyCurrentSelectionToClipboard() {
#ifdef OS_WIN
  const HWND foreground_window = ::GetForegroundWindow();
  if (foreground_window == nullptr) {
    LOG(ERROR) << "GetForegroundWindow() failed: " << ::GetLastError();
    return;
  }

  const DWORD thread_id =
      ::GetWindowThreadProcessId(foreground_window, nullptr);

  if (!::AttachThreadInput(::GetCurrentThreadId(), thread_id, TRUE)) {
    LOG(ERROR) << "AttachThreadInput failed: " << ::GetLastError();
    return;
  }

  const HWND focus_window = ::GetFocus();

  ::AttachThreadInput(::GetCurrentThreadId(), thread_id, FALSE);

  if (focus_window == nullptr || !::IsWindow(focus_window)) {
    LOG(WARNING) << "No focus window";
    return;
  }

  DWORD message_response = 0;
  constexpr DWORD kSendMessageTimeout = 10 * 1000;  // 10sec.
  const LRESULT send_result =
      ::SendMessageTimeout(focus_window, WM_COPY, 0, 0, SMTO_NORMAL,
                           kSendMessageTimeout, &message_response);
  if (send_result == 0) {
    LOG(ERROR) << "SendMessageTimeout() failed: " << ::GetLastError();
  }
#endif  // OS_WIN
}

bool WordRegisterDialog::SetDefaultEntryFromEnvironmentVariable() {
  const QString entry = TrimValue(GetEnv(mozc::kWordRegisterEnvironmentName));
  if (entry.isEmpty()) {
    return false;
  }
  WordlineEdit->setText(entry);

  QString reading_string =
      TrimValue(GetEnv(mozc::kWordRegisterEnvironmentReadingName));
  if (reading_string.isEmpty()) {
    reading_string = GetReading(entry);
  }
  ReadinglineEdit->setText(reading_string);

  return true;
}

const QString WordRegisterDialog::TrimValue(const QString &str) const {
  return str.trimmed()
      .replace(QLatin1Char('\r'), QLatin1String(""))
      .replace(QLatin1Char('\n'), QLatin1String(""));
}

void WordRegisterDialog::EnableIME() {
#ifdef OS_WIN
  // TODO(taku): implement it for other platform.
  HIMC himc = ::ImmGetContext(reinterpret_cast<HWND>(winId()));
  if (himc != nullptr) {
    ::ImmSetOpenStatus(himc, TRUE);
  }
#endif  // OS_WIN
}
}  // namespace gui
}  // namespace mozc

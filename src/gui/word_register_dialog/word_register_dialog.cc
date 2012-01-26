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

#include "gui/word_register_dialog/word_register_dialog.h"

#ifdef OS_WINDOWS
# include <windows.h>
# include <imm.h>
#endif  // OS_WINDOWS

#include <QtGui/QtGui>
#include <string>
#include <vector>
#include <stdlib.h>

#include "base/base.h"
#include "base/const.h"
#include "base/scoped_ptr.h"
#include "base/util.h"
#include "client/client.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_pos.h"

namespace mozc {
namespace gui {

namespace {
const int kSessionTimeout = 100000;
const int kMaxEditLength = 100;
const int kMaxReverseConversionLength = 30;

QString GetEnv(const char *envname) {
#if defined(OS_WINDOWS)
  wstring wenvname;
  mozc::Util::UTF8ToWide(envname, &wenvname);
  const DWORD buffer_size =
      ::GetEnvironmentVariable(wenvname.c_str(), NULL, 0);
  if (buffer_size == 0) {
    return "";
  }
  scoped_array<wchar_t> buffer(new wchar_t[buffer_size]);
  const DWORD num_copied =
      ::GetEnvironmentVariable(wenvname.c_str(), buffer.get(), buffer_size);
  if (num_copied > 0) {
    return QString::fromWCharArray(buffer.get());
  }
  return "";
#endif  // OS_WINDOWS
#if defined(OS_MACOSX) || defined(OS_LINUX)
  return ::getenv(envname);
#endif  // OS_MACOSX or OS_LINUX
  // TODO(team): Support other platforms.
  return "";
}
}  // anonymous namespace

WordRegisterDialog::WordRegisterDialog()
    : is_available_(true),
      storage_(
          new UserDictionaryStorage(
              UserDictionaryUtil::GetUserDictionaryFileName())),
      client_(client::ClientFactory::NewClient()),
  window_title_(tr("Mozc")) {
  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);
  setWindowModality(Qt::NonModal);

  ReadinglineEdit->setMaxLength(kMaxEditLength);
  WordlineEdit->setMaxLength(kMaxEditLength);

  if (!SetDefaultEntryFromEnvironmentVariable()) {
#ifdef OS_WINDOWS
    // On Windows, try to use clipboard as a fallback.
    SetDefaultEntryFromClipboard();
#endif  // OS_WINDOWS
  }

  client_->set_timeout(kSessionTimeout);

  if (!storage_->Load()) {
    LOG(WARNING) << "UserDictionaryStorage::Load() failed";
  }

  if (!storage_->Lock()) {
    QMessageBox::information(
        this, window_title_,
        tr("Close dictionary tool before using word register dialog."));
    is_available_ = false;
    return;
  }

  // Initialize ComboBox
  vector<string> pos_set;
  UserPOS::GetPOSList(&pos_set);
  CHECK(!pos_set.empty());

  for (size_t i = 0; i < pos_set.size(); ++i) {
    CHECK(!pos_set[i].empty());
    PartOfSpeechcomboBox->addItem(pos_set[i].c_str());
  }

  // Create new dictionary if empty
  if (!storage_->Exists() || storage_->dictionaries_size() == 0) {
    const QString name = tr("User Dictionary 1");
    uint64 dic_id = 0;
    if (!storage_->CreateDictionary(name.toStdString(),
                                    &dic_id)) {
      LOG(ERROR) << "Failed to create a new dictionary.";
      is_available_ = false;
      return;
    }
  }

  // Load Dictionary List
  CHECK_GT(storage_->dictionaries_size(), 0);
  for (size_t i = 0; i < storage_->dictionaries_size(); ++i) {
    DictionarycomboBox->addItem(storage_->dictionaries(i).name().c_str());
  }

  connect(WordlineEdit, SIGNAL(textChanged(const QString &)),
          this, SLOT(LineEditChanged(const QString &)));
  connect(ReadinglineEdit, SIGNAL(textChanged(const QString &)),
          this, SLOT(LineEditChanged(const QString &)));
  connect(WordlineEdit, SIGNAL(editingFinished()),
          this, SLOT(CompleteReading()));
  connect(WordRegisterDialogbuttonBox,
          SIGNAL(clicked(QAbstractButton *)),
          this,
          SLOT(Clicked(QAbstractButton *)));
  connect(LaunchDictionaryToolpushButton, SIGNAL(clicked()),
          this, SLOT(LaunchDictionaryTool()));

  if (!WordlineEdit->text().isEmpty()) {
    ReadinglineEdit->setFocus(Qt::OtherFocusReason);
    if (!ReadinglineEdit->text().isEmpty()) {
      ReadinglineEdit->selectAll();
    }
  }

  UpdateUIStatus();

  // Turn on IME
  EnableIME();
}

WordRegisterDialog::~WordRegisterDialog() {}

bool WordRegisterDialog::IsAvailable() const {
  return is_available_;
}

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
      !ReadinglineEdit->text().isEmpty() &&
      !WordlineEdit->text().isEmpty();

  QAbstractButton *button =
      WordRegisterDialogbuttonBox->button(QDialogButtonBox::Ok);
  if (button != NULL) {
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
          QMessageBox::warning(
              this, window_title_,
              tr("Reading part contains invalid characters."));
          return;
        case INVALID_VALUE:
          QMessageBox::warning(
              this, window_title_,
              tr("Word part contains invalid characters."));
          return;
        case FATAL_ERROR:
          QMessageBox::warning(
              this, window_title_, tr("Unexpected error occurs."));
          break;
        case SAVE_FAILURE:
          QMessageBox::warning(
              this, window_title_, tr("Failed to update user dictionary."));
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
  const string key = ReadinglineEdit->text().toStdString();
  const string value = WordlineEdit->text().toStdString();
  const string pos = PartOfSpeechcomboBox->currentText().toStdString();

  if (key.empty()) {
    return EMPTY_KEY;
  }

  if (value.empty()) {
    return EMPTY_VALUE;
  }

  if (!UserDictionaryUtil::IsValidReading(key)) {
    return INVALID_KEY;
  }

  if (pos.empty()) {
    LOG(ERROR) << "POS is empty";
    return FATAL_ERROR;
  }

  const int index = DictionarycomboBox->currentIndex();
  if (index < 0 || index >= storage_->dictionaries_size()) {
    LOG(ERROR) << "index is out of range";
    return FATAL_ERROR;
  }

  UserDictionaryStorage::UserDictionary *dic =
      storage_->mutable_dictionaries(index);
  CHECK(dic);

  if (dic->name() != DictionarycomboBox->currentText().toStdString()) {
    LOG(ERROR) << "Inconsitent dictionary name";
    return FATAL_ERROR;
  }

  UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
  CHECK(entry);
  entry->set_key(key);
  entry->set_value(value);
  entry->set_pos(pos);

  if (!storage_->Save() &&
      storage_->GetLastError() == UserDictionaryStorage::SYNC_FAILURE) {
    LOG(ERROR) << "Cannot save dictionary";
    return SAVE_FAILURE;
  }

  if (!client_->PingServer()) {
    LOG(WARNING) << "Server is not running. Do nothing";
    return SAVE_SUCCESS;
  }

#ifndef OS_MACOSX
  // Update server version if need be.
  if (!client_->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return SAVE_SUCCESS;
  }
#endif  // OS_MACOSX

  if (!client_->Reload()) {
    LOG(ERROR) << "Reload command failed";
    return SAVE_SUCCESS;
  }

  return SAVE_SUCCESS;
}

void WordRegisterDialog::LaunchDictionaryTool() {
  storage_->UnLock();
  client_->LaunchTool("dictionary_tool", "");
  QWidget::close();
}

const QString WordRegisterDialog::GetReading(const QString &str) {
  if (str.isEmpty()) {
    LOG(ERROR) << "given string is empty";
    return "";
  }

  if (str.count() >= kMaxReverseConversionLength) {
    LOG(ERROR) << "too long input";
    return "";
  }

  commands::Output output;
  {
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ON);
    if (!client_->SendKey(key, &output)) {
      LOG(ERROR) << "SendKey failed";
      return "";
    }

    commands::SessionCommand command;
    command.set_type(commands::SessionCommand::CONVERT_REVERSE);
    command.set_text(str.toStdString());

    if (!client_->SendCommand(command, &output)) {
      LOG(ERROR) << "SendCommand failed";
      return "";
    }

    commands::Output dummy_output;
    command.set_type(commands::SessionCommand::REVERT);
    client_->SendCommand(command, &dummy_output);
  }

  if (!output.has_preedit()) {
    LOG(ERROR) << "No preedit";
    return "";
  }

  string key;
  for (size_t segment_index = 0;
       segment_index < output.preedit().segment_size();
       ++segment_index) {
    const commands::Preedit::Segment &segment =
        output.preedit().segment(segment_index);
    if (!segment.has_key()) {
      LOG(ERROR) << "No segment";
      return "";
    }
    key.append(segment.key());
  }

  if (key.empty() ||
      !UserDictionaryUtil::IsValidReading(key)) {
    LOG(WARNING) << "containing invalid characters";
    return "";
  }

  return QString(key.c_str());
}

// Get default value from Clipboard
void WordRegisterDialog::SetDefaultEntryFromClipboard() {
  if (QApplication::clipboard() == NULL) {
    return;
  }
  CopyCurrentSelectionToClipboard();
  const QString value = TrimValue(QApplication::clipboard()->text());
  WordlineEdit->setText(value);
  ReadinglineEdit->setText(GetReading(value));
}

void WordRegisterDialog::CopyCurrentSelectionToClipboard() {
#ifdef OS_WINDOWS
  const HWND foreground_window = ::GetForegroundWindow();
  if (foreground_window == NULL) {
    LOG(ERROR) << "GetForegroundWindow() failed: " << ::GetLastError();
    return;
  }

  const DWORD thread_id =
      ::GetWindowThreadProcessId(foreground_window, NULL);

  if (!::AttachThreadInput(::GetCurrentThreadId(), thread_id, TRUE)) {
    LOG(ERROR) << "AttachThreadInput failed: " << ::GetLastError();
    return;
  }

  const HWND focus_window = ::GetFocus();

  ::AttachThreadInput(::GetCurrentThreadId(), thread_id, FALSE);

  if (focus_window == NULL || !::IsWindow(focus_window)) {
    LOG(WARNING) << "No focus window";
    return;
  }

  DWORD message_response = 0;
  const DWORD kSendMessageTimeout = 10 * 1000;  // 10sec.
  const LRESULT send_result =
      ::SendMessageTimeout(focus_window, WM_COPY, 0, 0, SMTO_NORMAL,
                           kSendMessageTimeout, &message_response);
  if (send_result == 0) {
    LOG(ERROR) << "SendMessageTimeout() failed: " << ::GetLastError();
  }
#endif  // OS_WINDOWS

  return;
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
  return str.trimmed().replace('\r', "").replace('\n', "");
}

void WordRegisterDialog::EnableIME() {
#ifdef OS_WINDOWS
  // TODO(taku): implement it for other platform.
  HIMC himc = ::ImmGetContext(winId());
  if (himc != NULL) {
    ::ImmSetOpenStatus(himc, TRUE);
  }
#endif  // OS_WINDOWS
}
}  // namespace gui
}  // namespace mozc

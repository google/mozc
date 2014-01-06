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

#include "gui/base/setup_util.h"

#include "base/logging.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#ifdef OS_WIN
#include "dictionary/user_dictionary_importer.h"
#include "gui/base/win_util.h"
#include "usage_stats/usage_stats.h"
#include "win32/base/imm_util.h"
#endif  // OS_WIN

namespace mozc {
namespace gui {

SetupUtil::SetupUtil()
    : storage_(new UserDictionaryStorage(
        UserDictionaryUtil::GetUserDictionaryFileName())),
      is_userdictionary_locked_(false) {}

SetupUtil::~SetupUtil() {}

bool SetupUtil::LockUserDictionary() {
  is_userdictionary_locked_ = storage_->Lock();
  return is_userdictionary_locked_;
}

bool SetupUtil::IsUserDictionaryLocked() const {
  return is_userdictionary_locked_;
}

void SetupUtil::SetDefaultProperty(uint32 flags) {
#ifdef OS_WIN
  if (flags & IME_DEFAULT) {
    mozc::usage_stats::UsageStats::IncrementCount("PostInstallSetDefault");
    win32::ImeUtil::SetDefault();
  } else {
    mozc::usage_stats::UsageStats::IncrementCount("PostInstallNotSetDefault");
  }

  if (flags & DISABLE_HOTKEY) {
    if (!WinUtil::SetIMEHotKeyDisabled(true)) {
      LOG(ERROR) << "Failed to set IMEHotKey";
    }
  }

  if (flags & IMPORT_MSIME_DICTIONARY) {
    if (!MigrateDictionaryFromMSIME()) {
      LOG(ERROR) << "Failed to migrate dictionary";
    }
  }
#else
  // not supported on Mac and Linux
#endif  // OS_WIN
}

// - Imports MS-IME's user dictionary to Mozc' dictionary
bool SetupUtil::MigrateDictionaryFromMSIME() {
#ifdef OS_WIN
  if (!is_userdictionary_locked_ && !storage_->Lock()) {
    return false;
  }
  if (!storage_->Load()) {
    return false;
  }

  // create UserDictionary if the current user dictionary is empty
  if (!storage_->Exists()) {
    const string kUserdictionaryName = "User Dictionary 1";
    uint64 dic_id = 0;
    if (!storage_->CreateDictionary(kUserdictionaryName, &dic_id)) {
      LOG(ERROR) << "Failed to create a new dictionary.";
      return false;
    }
  }

  // Import MS-IME's dictionary to a unique dictionary labeled
  // as "MS-IME"
  uint64 dic_id = 0;
  const string kMsimeUserdictionaryName = "MS-IME User Dictionary";
  for (size_t i = 0; i < storage_->dictionaries_size(); ++i) {
    if (storage_->dictionaries(i).name() ==
        kMsimeUserdictionaryName) {
      dic_id = storage_->dictionaries(i).id();
      break;
    }
  }

  if (dic_id == 0) {
    if (!storage_->CreateDictionary(kMsimeUserdictionaryName, &dic_id)) {
      LOG(ERROR) << "Failed to create a new dictionary.";
      return false;
    }
  }

  UserDictionaryStorage::UserDictionary *dic =
      storage_->GetUserDictionary(dic_id);
  if (dic == NULL) {
    LOG(ERROR) << "GetUserDictionary returned NULL";
    return false;
  }

  if (UserDictionaryImporter::ImportFromMSIME(dic) !=
    UserDictionaryImporter::IMPORT_NO_ERROR) {
    LOG(ERROR) << "ImportFromMSIME failed";
    return false;
  }

  storage_->Save();
  return true;
#else
  return false;
#endif  // OS_WIN
}
}  // namespace gui
}  // namespace mozc

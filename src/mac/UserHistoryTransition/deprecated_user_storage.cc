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

#include "mac/UserHistoryTransition/deprecated_user_storage.h"

#include <Carbon/Carbon.h>

#include "base/base.h"
#include "base/const.h"
#include "base/encryptor.h"
#include "base/mmap.h"
#include "base/password_manager.h"
#include "base/singleton.h"

namespace mozc {
//////////////////////////////////////////////////////////////////
// DeprecatedMacPasswordManager copied from password_manager.cc
namespace {
static const char kMacPasswordManagerName[] = kProductPrefix;

class DeprecatedMacPasswordManager : public PasswordManagerInterface {
 public:
  DeprecatedMacPasswordManager(const string &key = kProductPrefix)
      : key_(key) {}

  bool FindKeychainItem(string *password, SecKeychainItemRef *item_ref) const {
    UInt32 password_length = 0;
    void *password_data = NULL;
    OSStatus status = SecKeychainFindGenericPassword(
        NULL, strlen(kMacPasswordManagerName), kMacPasswordManagerName,
        key_.size(), key_.data(),
        &password_length, &password_data,
        item_ref);
    if (status == noErr) {
      if (password != NULL) {
        password->assign(
            reinterpret_cast<char*>(password_data), password_length);
      }
      SecKeychainItemFreeContent(NULL, password_data);
      return true;
    }
    return false;
  }

  bool SetPassword(const string &password) const {
    SecKeychainItemRef item_ref = NULL;
    string old_password;
    OSStatus status;
    if (FindKeychainItem(NULL, &item_ref)) {
      status = SecKeychainItemModifyAttributesAndData(
          item_ref, NULL, password.size(), password.data());
    } else {
      status = SecKeychainAddGenericPassword(
          NULL, strlen(kMacPasswordManagerName), kMacPasswordManagerName,
          key_.size(), key_.data(),
          password.size(), password.data(),
          NULL);
    }
    if (item_ref) {
      CFRelease(item_ref);
    }
    if (status != noErr) {
      LOG(ERROR) << "SetPassword failed.";
      return false;
    }
    return true;
  }

  virtual bool GetPassword(string *password) const {
    if (!FindKeychainItem(password, NULL)) {
      LOG(ERROR) << "Password item not found.";
      return false;
    }
    return true;
  }

  virtual bool RemovePassword() const {
    SecKeychainItemRef item_ref = NULL;
    if (FindKeychainItem(NULL, &item_ref)) {
      OSStatus status = SecKeychainItemDelete(item_ref);
      CFRelease(item_ref);
      if (status != noErr) {
        LOG(ERROR) << "RemovePassword failed.";
        return false;
      }
      return true;
    }
    LOG(ERROR) << "Password item not found.";
    return false;
  }

 private:
  string key_;
};

// salt size for encryption
const size_t kSaltSize = 32;

// 64Mbyte
// Maximum file size for history
const size_t kMaxFileSize = 64 * 1024 * 1024;
}  // namespace

namespace mac {
DeprecatedUserHistoryStorage::DeprecatedUserHistoryStorage(const string &filename)
    : filename_(filename) {}

DeprecatedUserHistoryStorage::~DeprecatedUserHistoryStorage() {}

bool DeprecatedUserHistoryStorage::Load() {
  string input, salt;

  // read encrypted message and salt from local file
  {
    Mmap mmap;
    if (!mmap.Open(filename_.c_str(), "r")) {
      LOG(ERROR) << "cannot open user history file";
      return false;
    }

    if (mmap.size() < kSaltSize) {
      LOG(ERROR) << "file size is too small";
      return false;
    }

    if (mmap.size() > kMaxFileSize) {
      LOG(ERROR) << "file size is too big.";
      return false;
    }

    // copy salt
    salt.assign(mmap.begin(), kSaltSize);

    // copy body
    input.assign(mmap.begin() + kSaltSize, mmap.size() - kSaltSize);
  }

  string password;
  PasswordManagerInterface *deprecated_password_manager =
      Singleton<DeprecatedMacPasswordManager>::get();
  if (!deprecated_password_manager->GetPassword(&password)) {
    LOG(ERROR) << "PasswordManager::GetPassword() failed";
    return false;
  }

  if (password.empty()) {
    LOG(ERROR) << "password is empty";
    return false;
  }

  // Decrypt message
  Encryptor::Key key;
  if (!key.DeriveFromPassword(password, salt)) {
    LOG(ERROR) << "Encryptor::Key::DeriveFromPassword failed";
    return false;
  }

  if (!Encryptor::DecryptString(key, &input)) {
    LOG(ERROR) << "Encryptor::DecryptString() failed";
    return false;
  }

  if (!ParseFromString(input)) {
    LOG(ERROR) << "ParseFromString failed. message looks broken";
    return false;
  }

  VLOG(1) << "Loaded user histroy, size=" << entries_size();

  return true;
}

}  // namespace mozc::mac
}  // namespace mozc

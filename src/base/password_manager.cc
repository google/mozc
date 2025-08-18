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

#include "base/password_manager.h"

#ifdef _WIN32
#include <windows.h>
#else  // _WIN32
#include <sys/stat.h>
#endif  // _WIN32

#include <cstddef>
#include <cstdlib>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "base/file_util.h"
#include "base/mmap.h"
#include "base/random.h"
#include "base/singleton.h"
#include "base/system_util.h"

#ifdef _WIN32
#include "base/win32/wide_char.h"
#endif  // _WIN32

#if (defined(_WIN32) || defined(__APPLE__))
#include "base/encryptor.h"
#endif  // _WIN32 | __APPLE__

namespace mozc {
namespace {

#ifdef _WIN32
constexpr char kPasswordFile[] = "encrypt_key.db";
#else   // _WIN32
constexpr char kPasswordFile[] = ".encrypt_key.db";  // dot-file (hidden file)
#endif  // _WIN32

constexpr size_t kPasswordSize = 32;

std::string CreateRandomPassword() {
  mozc::Random random;
  return random.ByteString(kPasswordSize);
}

// RAII class to make a given file writable/read-only
class ScopedReadWriteFile {
 public:
  explicit ScopedReadWriteFile(const std::string& filename)
      : filename_(filename) {
    if (absl::Status s = FileUtil::FileExists(filename_); !s.ok()) {
      LOG(WARNING) << "file not found: " << filename << ": " << s;
      return;
    }
#ifdef _WIN32
    if (!::SetFileAttributesW(win32::Utf8ToWide(filename).c_str(),
                              FILE_ATTRIBUTE_NORMAL)) {
      LOG(ERROR) << "Cannot make writable: " << filename_;
    }
#else   // _WIN32
    chmod(filename_.c_str(), 0600);  // write temporary
#endif  // _WIN32
  }

  ~ScopedReadWriteFile() {
    if (absl::Status s = FileUtil::FileExists(filename_); !s.ok()) {
      LOG(WARNING) << "file not found: " << filename_;
      return;
    }
#ifdef _WIN32
    if (!FileUtil::HideFileWithExtraAttributes(filename_,
                                               FILE_ATTRIBUTE_READONLY)) {
      LOG(ERROR) << "Cannot make readonly: " << filename_;
    }
#else   // _WIN32
    chmod(filename_.c_str(), 0400);  // read only
#endif  // _WIN32
  }

 private:
  std::string filename_;
};

std::string GetFileName() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kPasswordFile);
}

bool SavePassword(const std::string& password) {
  const std::string filename = GetFileName();
  ScopedReadWriteFile l(filename);
  if (absl::Status s = FileUtil::SetContents(filename, password); !s.ok()) {
    LOG(ERROR) << "Cannot save password: " << s;
    return false;
  }
  return true;
}

bool LoadPassword(std::string* password) {
  const std::string filename = GetFileName();
  const absl::StatusOr<Mmap> mmap = Mmap::Map(filename, Mmap::READ_ONLY);
  if (!mmap.ok()) {
    LOG(ERROR) << "cannot open: " << filename << ": " << mmap.status();
    return false;
  }

  // Seems that the size of DPAPI-encrypted-message
  // becomes bigger than the original message.
  // Typical file size is 32 * 5 = 160byte.
  // We just set the maximum file size to be 4096byte just in case.
  if (mmap->empty() || mmap->size() > 4096) {
    LOG(ERROR) << "Invalid password is saved";
    return false;
  }

  password->assign(mmap->begin(), mmap->size());

  return true;
}

bool RemovePasswordFile() {
  const std::string filename = GetFileName();
  ScopedReadWriteFile l(filename);
  if (absl::Status s = FileUtil::Unlink(filename); !s.ok()) {
    LOG(ERROR) << "Cannot unlink " << filename << ": " << s;
    return false;
  }
  return true;
}

}  // namespace

//////////////////////////////////////////////////////////////////
// PlainPasswordManager
class PlainPasswordManager : public PasswordManagerInterface {
 public:
  bool SetPassword(const std::string& password) const override;
  bool GetPassword(std::string* password) const override;
  bool RemovePassword() const override;
};

bool PlainPasswordManager::SetPassword(const std::string& password) const {
  if (password.size() != kPasswordSize) {
    LOG(ERROR) << "Invalid password given";
    return false;
  }

  if (!SavePassword(password)) {
    LOG(ERROR) << "SavePassword failed";
    return false;
  }

  return true;
}

bool PlainPasswordManager::GetPassword(std::string* password) const {
  if (password == nullptr) {
    LOG(ERROR) << "password is nullptr";
    return false;
  }

  password->clear();

  if (!LoadPassword(password)) {
    LOG(ERROR) << "LoadPassword failed";
    return false;
  }

  if (password->size() != kPasswordSize) {
    LOG(ERROR) << "Password size is invalid";
    return false;
  }

  return true;
}

bool PlainPasswordManager::RemovePassword() const {
  return RemovePasswordFile();
}

//////////////////////////////////////////////////////////////////
// WinPasswordManager
// We use this manager with both Windows and Mac
#if (defined(_WIN32) || defined(__APPLE__))
class WinMacPasswordManager : public PasswordManagerInterface {
 public:
  virtual bool SetPassword(const std::string& password) const;
  virtual bool GetPassword(std::string* password) const;
  virtual bool RemovePassword() const;
};

bool WinMacPasswordManager::SetPassword(const std::string& password) const {
  if (password.size() != kPasswordSize) {
    LOG(ERROR) << "password size is invalid";
    return false;
  }

  std::string enc_password;
  if (!Encryptor::ProtectData(password, &enc_password)) {
    LOG(ERROR) << "ProtectData failed";
    return false;
  }

  return SavePassword(enc_password);
}

bool WinMacPasswordManager::GetPassword(std::string* password) const {
  if (password == nullptr) {
    LOG(ERROR) << "password is nullptr";
    return false;
  }

  std::string enc_password;
  if (!LoadPassword(&enc_password)) {
    LOG(ERROR) << "LoadPassword failed";
    return false;
  }

  password->clear();
  if (!Encryptor::UnprotectData(enc_password, password)) {
    LOG(ERROR) << "UnprotectData failed";
    return false;
  }

  if (password->size() != kPasswordSize) {
    LOG(ERROR) << "password size is invalid";
    return false;
  }

  return true;
}

bool WinMacPasswordManager::RemovePassword() const {
  return RemovePasswordFile();
}
#endif  // _WIN32 | __APPLE__

// We use plain text file for password storage on Linux. If you port this module
// to other Linux distro, you might want to implement a new password manager
// which adopts some secure mechanism such like gnome-keyring.
#if defined(__linux__) || defined(__wasm__)
typedef PlainPasswordManager DefaultPasswordManager;
#endif  // __linux__ || __wasm__

// Windows or Mac
#if (defined(_WIN32) || defined(__APPLE__))
typedef WinMacPasswordManager DefaultPasswordManager;
#endif  // (defined(_WIN32) || defined(__APPLE__))

namespace {
class PasswordManagerImpl {
 public:
  PasswordManagerImpl() {
    password_manager_ = Singleton<DefaultPasswordManager>::get();
    DCHECK(password_manager_ != nullptr);
  }

  bool InitPassword() {
    absl::MutexLock l(&mutex_);
    return InitPasswordUnlocked();
  }

  bool GetPassword(std::string* password) {
    absl::MutexLock l(&mutex_);
    if (password_manager_->GetPassword(password)) {
      return true;
    }

    LOG(WARNING) << "Cannot get password. call InitPassword";

    if (!InitPasswordUnlocked()) {
      LOG(ERROR) << "InitPassword failed";
      return false;
    }

    if (!password_manager_->GetPassword(password)) {
      LOG(ERROR) << "Cannot get password.";
      return false;
    }

    return true;
  }

  bool RemovePassword() {
    absl::MutexLock l(&mutex_);
    return password_manager_->RemovePassword();
  }

  void SetPasswordManagerHandler(PasswordManagerInterface* handler) {
    absl::MutexLock l(&mutex_);
    password_manager_ = handler;
  }

 private:
  bool InitPasswordUnlocked() {
    std::string password;
    if (password_manager_->GetPassword(&password)) {
      return true;
    }
    password = CreateRandomPassword();
    return password_manager_->SetPassword(password);
  }

  PasswordManagerInterface* password_manager_;
  absl::Mutex mutex_;
};
}  // namespace

bool PasswordManager::InitPassword() {
  return Singleton<PasswordManagerImpl>::get()->InitPassword();
}

bool PasswordManager::GetPassword(std::string* password) {
  return Singleton<PasswordManagerImpl>::get()->GetPassword(password);
}

// remove current password
bool PasswordManager::RemovePassword() {
  return Singleton<PasswordManagerImpl>::get()->RemovePassword();
}

// set internal interface for unittesting
void PasswordManager::SetPasswordManagerHandler(
    PasswordManagerInterface* handler) {
  Singleton<PasswordManagerImpl>::get()->SetPasswordManagerHandler(handler);
}
}  // namespace mozc

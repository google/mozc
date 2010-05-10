// Copyright 2010, Google Inc.
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

#ifdef OS_WINDOWS
#include <windows.h>
#endif

#ifdef OS_MACOSX
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif

#include <string>
#include "base/base.h"
#include "base/encryptor.h"
#include "base/file_stream.h"
#include "base/mmap.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "base/util.h"

namespace mozc {
namespace {

#ifdef OS_WINDOWS
const char  kPasswordFile[] = "encrypt_key.db";
#else
const char  kPasswordFile[] = ".encrypt_key.db";   // dot-file (hidden file)
#endif

const size_t kPasswordSize  = 32;

string CreateRandomPassword() {
  char buf[kPasswordSize];
  if (!Util::GetSecureRandomSequence(buf, sizeof(buf))) {
    LOG(ERROR) << "GetSecureRandomSequence failed. "
               << "make random key with rand()";
    for (size_t i = 0; i < sizeof(buf); ++i) {
      buf[i] = static_cast<char>(rand() % 256);
    }
  }
  return string(buf, sizeof(buf));
}

// RAII class to make a given file writable/read-only
class ScopedReadWriteFile {
 public:
  explicit ScopedReadWriteFile(const string &filename)
      : filename_(filename) {
    if (!Util::FileExists(filename_)) {
      LOG(WARNING) << "file not found: " << filename;
      return;
    }
#ifdef OS_WINDOWS
    wstring wfilename;
    Util::UTF8ToWide(filename_.c_str(), &wfilename);
    if (!::SetFileAttributesW(wfilename.c_str(), FILE_ATTRIBUTE_NORMAL)) {
      LOG(ERROR) << "Cannot make writable: " << filename_;
    }
#else
    chmod(filename_.c_str(), 0600);  // write temporary
#endif
  }

  ~ScopedReadWriteFile() {
    if (!Util::FileExists(filename_)) {
      LOG(WARNING) << "file not found: " << filename_;
      return;
    }
#ifdef OS_WINDOWS
    wstring wfilename;
    Util::UTF8ToWide(filename_.c_str(), &wfilename);
    if (!::SetFileAttributesW(wfilename.c_str(),
                              FILE_ATTRIBUTE_HIDDEN |
                              FILE_ATTRIBUTE_SYSTEM |
                              FILE_ATTRIBUTE_NOT_CONTENT_INDEXED |
                              FILE_ATTRIBUTE_READONLY)) {
      LOG(ERROR) << "Cannot make readonly: " << filename_;
    }
#else
    chmod(filename_.c_str(), 0400);  // read only
#endif
  }

 private:
  string filename_;
};

string GetFileName() {
  return Util::JoinPath(Util::GetUserProfileDirectory(),
                        kPasswordFile);
}

bool SavePassword(const string &password) {
  const string filename = GetFileName();
  ScopedReadWriteFile l(filename);

  {
    OutputFileStream ofs(filename.c_str(), ios::out | ios::binary);
    if (!ofs) {
      LOG(ERROR) << "cannot open: " << filename;
      return false;
    }
    ofs.write(password.data(), password.size());
  }

  return true;
}

bool LoadPassword(string *password) {
  const string filename = GetFileName();
  Mmap<char> mmap;
  if (!mmap.Open(filename.c_str(), "r")) {
    LOG(ERROR) << "cannot open: " << filename;
    return false;
  }

  // Seems that the size of DPAPI-encrypted-mesage
  // becomes bigger than the original message.
  // Typical file size is 32 * 5 = 160byte.
  // We just set the maximum file size to be 4096byte just in case.
  if (mmap.GetFileSize() == 0 || mmap.GetFileSize() > 4096) {
    LOG(ERROR) << "Invalid password is saved";
    return false;
  }

  password->assign(mmap.begin(), mmap.GetFileSize());

  return true;
}

bool RemovePasswordFile() {
  const string filename = GetFileName();
  ScopedReadWriteFile l(filename);
  return Util::Unlink(filename);
}
}  // namespace

//////////////////////////////////////////////////////////////////
// PlainPasswordManager
class PlainPasswordManager : public PasswordManagerInterface {
 public:
  virtual bool SetPassword(const string &password) const;
  virtual bool GetPassword(string *password) const;
  virtual bool RemovePassword() const;
};

bool PlainPasswordManager::SetPassword(const string &password) const {
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

bool PlainPasswordManager::GetPassword(string *password) const {
  if (password == NULL) {
    LOG(ERROR) << "password is NULL";
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
#ifdef OS_WINDOWS
class WinPasswordManager : public PasswordManagerInterface {
 public:
  virtual bool SetPassword(const string &password) const;
  virtual bool GetPassword(string *password) const;
  virtual bool RemovePassword() const;
};

bool WinPasswordManager::SetPassword(const string &password) const {
  if (password.size() != kPasswordSize) {
    LOG(ERROR) << "password size is invalid";
    return false;
  }

  string enc_password;
  if (!Encryptor::ProtectData(password, &enc_password)) {
    LOG(ERROR) << "ProtectData failed";
    return false;
  }

  return SavePassword(enc_password);
}

bool WinPasswordManager::GetPassword(string *password) const {
  if (password == NULL) {
    LOG(ERROR) << "password is NULL";
    return false;
  }

  string enc_password;
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

bool WinPasswordManager::RemovePassword() const {
  return RemovePasswordFile();
}
#endif  // OS_WINDOWS

#ifdef OS_MACOSX
//////////////////////////////////////////////////////////////////
// MacPasswordManager
namespace {
static const char kMacPasswordManagerName[] = "GoogleJapaneseInput";
}  // anonymous namespace

class MacPasswordManager : public PasswordManagerInterface {
 public:
  MacPasswordManager(const string &key = "GoogleJapaneseInput")
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
#endif

// Chrome OS(Linux)
// We use plain text file for password storage but this is ok
// because local files are encrypted by ChromeOS.  If you port
// this module to other Linux distro, you might want to implement
// a new password manager which adopts some secure mechanism such
// like gnome-keyring.
#if defined OS_LINUX
typedef PlainPasswordManager DefaultPasswordManager;
#endif

// Windows
#if defined OS_WINDOWS
typedef WinPasswordManager DefaultPasswordManager;
#endif
#ifdef OS_MACOSX
// If a binary is not codesigned, MacPasswordManager in the binary
// opens a confirmation dialog which grabs key focuses.
typedef MacPasswordManager DefaultPasswordManager;
#endif

namespace {
static const char kPassword[] = "DummyPasswordForUnittest";
}

class MockPasswordManager : public PasswordManagerInterface {
 public:
  bool SetPassword(const string &password) const {
    password_ = password;
    return true;
  }
  bool GetPassword(string *password) const {
    if (password_.empty()) {
      return false;
    }

    *password = password_;
    return true;
  }
  bool RemovePassword() const {
    password_.clear();
    return true;
  }

  MockPasswordManager()
      : password_(kPassword) {}

  virtual ~MockPasswordManager() {}

 private:
  mutable string password_;
};

namespace {
class PasswordManagerImpl {
 public:
  PasswordManagerImpl() : password_manager_(NULL) {
    if (kUseMockPasswordManager) {
      password_manager_impl_.reset(new MockPasswordManager);
    } else {
      password_manager_impl_.reset(new DefaultPasswordManager);
    }

    password_manager_ = password_manager_impl_.get();
    DCHECK(password_manager_ != NULL);
  }

  bool InitPassword() {
    string password;
    if (password_manager_->GetPassword(&password)) {
      return true;
    }
    password = CreateRandomPassword();
    scoped_lock l(&mutex_);
    return password_manager_->SetPassword(password);
  }

  bool GetPassword(string *password) {
    scoped_lock l(&mutex_);
    if (password_manager_->GetPassword(password)) {
      return true;
    }

    LOG(WARNING) << "Cannot get password. call InitPassword";

    if (!InitPassword()) {
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
    scoped_lock l(&mutex_);
    return password_manager_->RemovePassword();
  }

  void SetPasswordManagerHandler(PasswordManagerInterface *handler) {
    scoped_lock l(&mutex_);
    password_manager_impl_.reset(NULL);
    password_manager_ = handler;
  }

 public:
  scoped_ptr<PasswordManagerInterface> password_manager_impl_;
  PasswordManagerInterface *password_manager_;
  Mutex mutex_;
};
}  // anonymous namespace

bool PasswordManager::InitPassword() {
  return Singleton<PasswordManagerImpl>::get()->InitPassword();
}

bool PasswordManager::GetPassword(string *password) {
  return Singleton<PasswordManagerImpl>::get()->GetPassword(password);
}

// remove current password
bool PasswordManager::RemovePassword() {
  return Singleton<PasswordManagerImpl>::get()->RemovePassword();
}

// set internal interface for unittesting
void PasswordManager::SetPasswordManagerHandler(
    PasswordManagerInterface *handler) {
  Singleton<PasswordManagerImpl>::get()->SetPasswordManagerHandler(handler);
}
}  // namespace mozc

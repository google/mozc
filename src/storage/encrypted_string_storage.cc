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

#include "storage/encrypted_string_storage.h"

#ifdef OS_WIN
#include <Windows.h>
#endif  // OS_WIN

#include <cstring>
#include <ios>
#include <string>

#include "base/encryptor.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/password_manager.h"
#include "base/util.h"

namespace mozc {
namespace storage {

namespace {
// Salt size for encryption
constexpr size_t kSaltSize = 32;

// Maximum file size (64Mbyte)
constexpr size_t kMaxFileSize = 64 * 1024 * 1024;
}  // namespace

EncryptedStringStorage::EncryptedStringStorage(const std::string &filename)
    : filename_(filename) {}

EncryptedStringStorage::~EncryptedStringStorage() {}

bool EncryptedStringStorage::Load(std::string *output) const {
  DCHECK(output);

  std::string salt;

  // Reads encrypted message and salt from local file
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
    output->assign(mmap.begin() + kSaltSize, mmap.size() - kSaltSize);
  }

  return Decrypt(salt, output);
}

bool EncryptedStringStorage::Decrypt(const std::string &salt,
                                     std::string *data) const {
  DCHECK(data);

  std::string password;
  if (!PasswordManager::GetPassword(&password)) {
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

  if (!Encryptor::DecryptString(key, data)) {
    LOG(ERROR) << "Encryptor::DecryptString() failed";
    return false;
  }

  return true;
}

bool EncryptedStringStorage::Save(const std::string &input) const {
  std::string output, salt;
  // Generate salt.
  salt.resize(kSaltSize);
  Util::GetRandomSequence(&salt[0], kSaltSize);

  output.assign(input);
  if (!Encrypt(salt, &output)) {
    return false;
  }

  // Even if histoy is empty, save to them into a file to
  // make the file empty
  const std::string tmp_filename = filename_ + ".tmp";
  {
    OutputFileStream ofs(tmp_filename, std::ios::out | std::ios::binary);
    if (!ofs) {
      LOG(ERROR) << "failed to write: " << tmp_filename;
      return false;
    }

    VLOG(1) << "Syncing user history to: " << filename_;
    ofs.write(salt.data(), salt.size());
    ofs.write(output.data(), output.size());
  }

  if (absl::Status s = FileUtil::AtomicRename(tmp_filename, filename_);
      !s.ok()) {
    LOG(ERROR) << "AtomicRename failed: " << s << "; from: " << tmp_filename
               << ", to: " << filename_;
    return false;
  }

#ifdef OS_WIN
  if (!FileUtil::HideFile(filename_)) {
    LOG(ERROR) << "Cannot make hidden: " << filename_ << " "
               << ::GetLastError();
  }
#endif  // OS_WIN

  return true;
}

bool EncryptedStringStorage::Encrypt(const std::string &salt,
                                     std::string *data) const {
  DCHECK(data);

  std::string password;
  if (!PasswordManager::GetPassword(&password)) {
    LOG(ERROR) << "PasswordManager::GetPassword() failed";
    return false;
  }

  if (password.empty()) {
    LOG(ERROR) << "password is empty";
    return false;
  }

  Encryptor::Key key;
  if (!key.DeriveFromPassword(password, salt)) {
    LOG(ERROR) << "Encryptor::Key::DeriveFromPassword() failed";
    return false;
  }

  if (!Encryptor::EncryptString(key, data)) {
    LOG(ERROR) << "Encryptor::EncryptString() failed";
    return false;
  }

  return true;
}

}  // namespace storage
}  // namespace mozc

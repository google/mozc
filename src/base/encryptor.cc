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

#include "base/encryptor.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/password_manager.h"
#include "base/random.h"
#include "base/unverified_aes256.h"
#include "base/unverified_sha1.h"

#if defined(_WIN32)
// clang-format off
#include <windows.h>
#include <wincrypt.h>
// clang-format on
#elif defined(__APPLE__)
#include <sys/types.h>
#include <unistd.h>

#include "absl/strings/str_format.h"
#include "base/mac/mac_util.h"
#else  // Other platforms
#include <string.h>
#endif  // platforms (_WIN32, __APPLE__, ...)

namespace mozc {
namespace {

using ::mozc::internal::UnverifiedSHA1;

// By using SHA1, emulate Microsoft's CryptDerivePassword API
// http://msdn.microsoft.com/en-us/library/aa379916.aspx
// says:
// Let n be the required derived key length, in bytes.
// The derived key is the first n bytes of the hash value after
// the hash computation has been completed by CryptDeriveKey.
// If the hash is not a member of the SHA-2 family and the required key
// is for either 3DES or AES, the key is derived as follows:
//
// 1. Form a 64-byte buffer by repeating the constant 0x36 64 times.
//    Let k be the length of the hash value that is represented by
//    the input parameter hBaseData. Set the first k bytes of the
//    buffer to the result of an XOR operation of the first k bytes
//    of the buffer with the hash value that is represented by the
//    input parameter hBaseData.
// 2. Form a 64-byte buffer by repeating the constant 0x5C 64 times.
//    Set the first k bytes of the buffer to the result of an XOR
//    operation of the first k bytes of the buffer with the hash
//    value that is represented by the input parameter hBaseData.
// 3. Hash the result of step 1 by using the same hash algorithm
//    as that used to compute the hash value that is represented
//     by the hBaseData parameter.
// 4. Hash the result of step 2 by using the same hash algorithm as
//    that used to compute the hash value that is represented by the
//    hBaseData parameter.
// 5. Concatenate the result of step 3 with the result of step 4.
// 6. Use the first n bytes of the result of step 5 as the derived key.
std::string GetMSCryptDeriveKeyWithSHA1(const absl::string_view password,
                                        const absl::string_view salt) {
  // Step1.
  std::string buf1(64, 0x36);

  // Step2.
  std::string buf2(64, 0x5c);

  // Step 3 & 4
  const std::string hash =
      UnverifiedSHA1::MakeDigest(absl::StrCat(password, salt));
  for (size_t i = 0; i < hash.size(); ++i) {
    buf1[i] ^= hash[i];
    buf2[i] ^= hash[i];
  }

  // Step 5 & 6
  return absl::StrCat(UnverifiedSHA1::MakeDigest(buf1),
                      UnverifiedSHA1::MakeDigest(buf2));
}

}  // namespace

size_t Encryptor::Key::GetEncryptedSize(size_t size) const {
  // Even when given size is already multples of 16, we add
  // an extra block_size as a padding.
  // The extra padding can allow us to know that the message decrypted
  // is invalid or broken.
  return (size / block_size() + 1) * block_size();
}

bool Encryptor::Key::DeriveFromPassword(const absl::string_view password,
                                        const absl::string_view salt,
                                        const uint8_t* iv) {
  if (IsAvailable()) {
    LOG(WARNING) << "key is already set";
    return false;
  }

  if (password.empty()) {
    LOG(WARNING) << "password is empty";
    return false;
  }

  if (iv != nullptr) {
    std::copy_n(iv, iv_size(), iv_);
  } else {
    std::fill_n(iv_, iv_size(), 0);
  }

  const std::string key = GetMSCryptDeriveKeyWithSHA1(password, salt);
  DCHECK_EQ(40, key.size());  // SHA1 is 160bit hash, so 160*2/8 = 40byte

  // Store the session key.
  // NOTE: key_size() returns size in bit for historical reasons.
  std::copy_n(key.begin(), key_size() / 8, key_);

  is_available_ = true;

  return true;
}

bool Encryptor::EncryptString(const Encryptor::Key& key, std::string* data) {
  if (data == nullptr || data->empty()) {
    LOG(ERROR) << "data is nullptr or empty";
    return false;
  }
  size_t size = data->size();
  data->resize(key.GetEncryptedSize(size));
  if (!Encryptor::EncryptArray(key, data->data(), &size)) {
    LOG(ERROR) << "EncryptArray() failed";
    return false;
  }
  return true;
}

bool Encryptor::DecryptString(const Encryptor::Key& key, std::string* data) {
  if (data == nullptr || data->empty()) {
    LOG(ERROR) << "data is nullptr or empty";
    return false;
  }
  size_t size = data->size();
  if (!Encryptor::DecryptArray(key, data->data(), &size)) {
    LOG(ERROR) << "DecryptArray() failed";
    return false;
  }
  data->resize(size);
  return true;
}

bool Encryptor::EncryptArray(const Encryptor::Key& key, char* buf,
                             size_t* buf_size) {
  if (!key.IsAvailable()) {
    LOG(ERROR) << "key is not available";
    return false;
  }

  if (buf == nullptr || buf_size == nullptr || *buf_size == 0) {
    LOG(ERROR) << "invalid buffer given";
    return false;
  }

  const size_t enc_size = key.GetEncryptedSize(*buf_size);

  // perform PKCS#5 padding
  const size_t padding_size = enc_size - *buf_size;
  const uint8_t padding_value = static_cast<uint8_t>(padding_size);
  for (size_t i = *buf_size; i < enc_size; ++i) {
    buf[i] = static_cast<char>(padding_value);
  }

  // For historical reasons, we are using AES256/CBC for obfuscation.
  internal::UnverifiedAES256::TransformCBC(key.key_, key.iv_,
                                           reinterpret_cast<uint8_t*>(buf),
                                           enc_size / kBlockSize);
  *buf_size = enc_size;
  return true;
}

bool Encryptor::DecryptArray(const Encryptor::Key& key, char* buf,
                             size_t* buf_size) {
  if (!key.IsAvailable()) {
    LOG(ERROR) << "key is not available";
    return false;
  }

  if (buf == nullptr || buf_size == nullptr || *buf_size == 0) {
    LOG(ERROR) << "invalid buffer given";
    return false;
  }

  if (*buf_size < key.block_size() || *buf_size % key.block_size() != 0) {
    LOG(ERROR) << "buf size is not multiples of " << key.block_size();
    return false;
  }

  size_t size = *buf_size;

  // For historical reasons, we are using AES256/CBC for obfuscation.
  internal::UnverifiedAES256::InverseTransformCBC(
      key.key_, key.iv_, reinterpret_cast<uint8_t*>(buf), size / kBlockSize);

  // perform PKCS#5 un-padding
  // see. http://www.chilkatsoft.com/faq/PKCS5_Padding.html
  const uint8_t padding_value = static_cast<uint8_t>(buf[size - 1]);
  const size_t padding_size = static_cast<size_t>(padding_value);
  if (padding_value == 0x00 || padding_value > kBlockSize) {
    LOG(ERROR) << "Cannot find PKCS#5 padding values: ";
    return false;
  }

  if (size <= padding_size) {
    LOG(ERROR) << "padding size is no smaller than original message";
    return false;
  }

  for (size_t i = size - padding_size; i < size; ++i) {
    if (static_cast<uint8_t>(buf[i]) != padding_value) {
      LOG(ERROR) << "invalid padding value. message is broken";
      return false;
    }
  }
  *buf_size -= padding_size;  // remove padding part
  return true;
}

// Protect|Unprotect Data
#ifdef _WIN32
// See. http://msdn.microsoft.com/en-us/library/aa380261.aspx
bool Encryptor::ProtectData(const absl::string_view plain_text,
                            std::string* cipher_text) {
  DCHECK(cipher_text);
  DATA_BLOB input;
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plain_text.data()));
  input.cbData = static_cast<DWORD>(plain_text.size());

  DATA_BLOB output;
  const BOOL result = ::CryptProtectData(&input, L"", nullptr, nullptr, nullptr,
                                         CRYPTPROTECT_UI_FORBIDDEN, &output);
  if (!result) {
    LOG(ERROR) << "CryptProtectData failed: " << ::GetLastError();
    return false;
  }

  cipher_text->assign(reinterpret_cast<char*>(output.pbData), output.cbData);

  ::LocalFree(output.pbData);

  return true;
}

// See. http://msdn.microsoft.com/en-us/library/aa380882(VS.85).aspx
bool Encryptor::UnprotectData(const absl::string_view cipher_text,
                              std::string* plain_text) {
  DCHECK(plain_text);
  DATA_BLOB input;
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(cipher_text.data()));
  input.cbData = static_cast<DWORD>(cipher_text.length());

  DATA_BLOB output;
  const BOOL result =
      ::CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                           CRYPTPROTECT_UI_FORBIDDEN, &output);
  if (!result) {
    LOG(ERROR) << "CryptUnprotectData failed: " << ::GetLastError();
    return false;
  }

  plain_text->assign(reinterpret_cast<char*>(output.pbData), output.cbData);

  ::LocalFree(output.pbData);

  return true;
}

#elif defined(__APPLE__)

// ProtectData for Mac uses the serial number and the current pid as the key.
bool Encryptor::ProtectData(const absl::string_view plain_text,
                            std::string* cipher_text) {
  DCHECK(cipher_text);
  Encryptor::Key key;
  const std::string serial_number = MacUtil::GetSerialNumber();
  const std::string salt = absl::StrFormat("%x", ::getuid());
  if (serial_number.empty()) {
    LOG(ERROR) << "Cannot get the serial number";
    return false;
  }
  if (!key.DeriveFromPassword(serial_number, salt)) {
    LOG(ERROR) << "Cannot prepare the internal key";
    return false;
  }

  cipher_text->assign(plain_text.begin(), plain_text.end());
  if (!EncryptString(key, cipher_text)) {
    cipher_text->clear();
    LOG(ERROR) << "Cannot encrypt the text";
    return false;
  }
  return true;
}

// Same as ProtectData.
bool Encryptor::UnprotectData(const absl::string_view cipher_text,
                              std::string* plain_text) {
  DCHECK(plain_text);
  Encryptor::Key key;
  const std::string serial_number = MacUtil::GetSerialNumber();
  const std::string salt = absl::StrFormat("%x", ::getuid());
  if (serial_number.empty()) {
    LOG(ERROR) << "Cannot get the serial number";
    return false;
  }
  if (!key.DeriveFromPassword(serial_number, salt)) {
    LOG(ERROR) << "Cannot prepare the internal key";
    return false;
  }

  plain_text->assign(cipher_text.begin(), cipher_text.end());
  if (!DecryptString(key, plain_text)) {
    plain_text->clear();
    LOG(ERROR) << "Cannot encrypt the text";
    return false;
  }
  return true;
}

#else   // _WIN32 | __APPLE__

namespace {

constexpr size_t kSaltSize = 32;

}  // namespace

// Use AES to emulate ProtectData
bool Encryptor::ProtectData(const absl::string_view plain_text,
                            std::string* cipher_text) {
  std::string password;
  if (!PasswordManager::GetPassword(&password)) {
    LOG(ERROR) << "Cannot get password";
    return false;
  }

  Random random;
  const std::string salt = random.ByteString(kSaltSize);

  Encryptor::Key key;
  if (!key.DeriveFromPassword(password, salt)) {
    LOG(ERROR) << "DeriveFromPassword failed";
    return false;
  }

  std::string buf(plain_text.begin(), plain_text.end());
  if (!Encryptor::EncryptString(key, &buf)) {
    LOG(ERROR) << "Encryptor::EncryptString failed";
    return false;
  }

  *cipher_text = absl::StrCat(salt, buf);

  return true;
}

// Use AES to emulate UnprotectData
bool Encryptor::UnprotectData(const absl::string_view cipher_text,
                              std::string* plain_text) {
  if (cipher_text.size() < kSaltSize) {
    LOG(ERROR) << "encrypted message is too short";
    return false;
  }

  std::string password;
  if (!PasswordManager::GetPassword(&password)) {
    LOG(ERROR) << "Cannot get password";
    return false;
  }

  const std::string salt(cipher_text.data(), kSaltSize);

  Encryptor::Key key;
  if (!key.DeriveFromPassword(password, salt)) {
    LOG(ERROR) << "DeriveFromPassword failed";
    return false;
  }

  std::string buf(cipher_text.data() + kSaltSize,
                  cipher_text.size() - kSaltSize);

  if (!Encryptor::DecryptString(key, &buf)) {
    LOG(ERROR) << "Encryptor::DecryptString failed";
    return false;
  }

  *plain_text = std::move(buf);

  return true;
}
#endif  // _WIN32

}  // namespace mozc

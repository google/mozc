// Copyright 2010-2013, Google Inc.
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


#if defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#elif defined(OS_MACOSX)
#include <unistd.h>
#include <sys/types.h>
#else
#include <string.h>
#endif  // platforms (OS_WIN, OS_MACOSX, ...)

#ifdef HAVE_OPENSSL
#include <openssl/sha.h>   // Use default openssl
#include <openssl/aes.h>
#endif  // HAVE_OPENSSL

#ifdef OS_ANDROID
#include "base/android_jni_proxy.h"
#endif  // OS_ANDROID

#include "base/logging.h"
#include "base/password_manager.h"
#include "base/util.h"

#ifdef OS_MACOSX
#include "base/mac_util.h"
#endif  // OS_MACOSX

#include <string>

namespace mozc {
namespace {

#if defined(OS_ANDROID)
using jni::JavaEncryptorProxy;
const size_t kBlockSize = JavaEncryptorProxy::kBlockSizeInBytes;
const size_t kKeySize = JavaEncryptorProxy::kKeySizeInBits;
#elif defined(OS_WIN)
const size_t kBlockSize = 16;  // 128bit
const size_t kKeySize = 256;   // key length in bit
// Use CBC mode:
// CBC has been the most commonly used mode of operation.
// See http://en.wikipedia.org/wiki/Block_cipher_modes_of_operation
const DWORD kCryptMode = CRYPT_MODE_CBC;   // crypt mode (CBC)
// Use PKCS5 padding:
// See http://www.chilkatsoft.com/faq/PKCS5_Padding.html
const DWORD kPaddingMode = PKCS5_PADDING;  // padding mode
#elif defined(HAVE_OPENSSL)
const size_t kBlockSize = AES_BLOCK_SIZE;
const size_t kKeySize = 256;   // key length in bit

// Return SHA1 digest
string HashSHA1(const string &data) {
  uint8 buf[SHA_DIGEST_LENGTH];   // 160bit
  SHA1(reinterpret_cast<const uint8 *>(data.data()), data.size(), buf);
  string result(reinterpret_cast<char *>(buf), SHA_DIGEST_LENGTH);
  return result;
}

// By using OpenSSL's SHA1, emulate Microsoft's CryptDerivePassword API
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
string GetMSCryptDeriveKeyWithSHA1(const string &password,
                                   const string &salt) {
  uint8 buf1[64];
  uint8 buf2[64];

  // Step1.
  memset(buf1, 0x36, sizeof(buf1));

  // Step2.
  memset(buf2, 0x5c, sizeof(buf2));

  // Step 3 & 4
  const string hash = HashSHA1(password + salt);
  for (size_t i = 0; i < hash.size(); ++i) {
    buf1[i] ^= static_cast<uint8>(hash[i]);
    buf2[i] ^= static_cast<uint8>(hash[i]);
  }

  // Step 5 & 6
  const string result1(reinterpret_cast<const char *>(buf1), sizeof(buf1));
  const string result2(reinterpret_cast<const char *>(buf2), sizeof(buf2));

  return HashSHA1(result1) + HashSHA1(result2);
}
#else
// None of OS_ANDROID/OS_WIN/HAVE_OPENSSL is defined.
#error "Encryptor does not support your platform."
#endif

}  // namespace

struct KeyData {
#if defined(OS_ANDROID)
  uint8 key[kKeySize / 8];
#elif defined(OS_WIN)
  HCRYPTPROV prov;
  HCRYPTHASH hash;
  HCRYPTKEY  key;
#elif defined(HAVE_OPENSSL)
  AES_KEY encrypt_key;
  AES_KEY decrypt_key;
#else
// None of OS_ANDROID/OS_WIN/HAVE_OPENSSL is defined.
#error "Encryptor does not support your platform."
#endif
};

size_t Encryptor::Key::block_size() const {
  return kBlockSize;
}

const uint8 *Encryptor::Key::iv() const {
  return iv_.get();
}

size_t Encryptor::Key::iv_size() const {
  return block_size();   // the same as block size
}

size_t Encryptor::Key::key_size() const {
  return kKeySize;
}

bool Encryptor::Key::IsAvailable() const {
  return is_available_;
}

size_t Encryptor::Key::GetEncryptedSize(size_t size) const {
  // Even when given size is already multples of 16, we add
  // an extra block_size as a padding.
  // The extra padding can allow us to know that the message decrypted
  // is invalid or broken.
  return (size / block_size() + 1) * block_size();
}

KeyData *Encryptor::Key::GetKeyData() const {
  return data_.get();
}

#ifdef OS_WIN
bool Encryptor::Key::DeriveFromPassword(const string &password,
                                        const string &salt,
                                        const uint8 *iv) {
  if (is_available_) {
    LOG(WARNING) << "key is already set";
    return false;
  }

  if (password.empty()) {
    LOG(WARNING) << "password is empty";
    return false;
  }

  // http://support.microsoft.com/kb/238187/en-us/
  // First, AcquireContext. if it failed, make a
  // key with CRYPT_NEWKEYSET flag
  if (!::CryptAcquireContext(&(GetKeyData()->prov),
                             NULL,
                             NULL,  // use default
                             PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
    if (NTE_BAD_KEYSET == ::GetLastError()) {
      if (!::CryptAcquireContext(&(GetKeyData()->prov),
                                 NULL,
                                 NULL,  // use default
                                 PROV_RSA_AES,
                                 CRYPT_NEWKEYSET)) {
        LOG(ERROR) << "CryptAcquireContext failed: " << ::GetLastError();
        return false;
      }
    } else {
      LOG(ERROR) << "CryptAcquireContext failed: " << ::GetLastError();
      return false;
    }
  }

  // Create Hash
  if (!::CryptCreateHash(GetKeyData()->prov, CALG_SHA1, 0, 0,
                         &(GetKeyData()->hash))) {
    LOG(ERROR) << "CryptCreateHash failed: " << ::GetLastError();
    ::CryptReleaseContext(GetKeyData()->prov, 0);
    return false;
  }

  // Set password + salt
  const string final_password = password + salt;
  if (!::CryptHashData(GetKeyData()->hash,
                       const_cast<BYTE *>(
                           reinterpret_cast<const BYTE *>(
                               final_password.data())),
                       final_password.size(), 0)) {
    LOG(ERROR) << "CryptHashData failed: " << ::GetLastError();
    ::CryptReleaseContext(GetKeyData()->prov, 0);
    ::CryptDestroyHash(GetKeyData()->hash);
    return false;
  }

  // Set algorithm
  if (!::CryptDeriveKey(GetKeyData()->prov,
                        CALG_AES_256,
                        GetKeyData()->hash,
                        (key_size() << 16),
                        &(GetKeyData()->key))) {
    LOG(ERROR) << "CryptDeriveKey failed: "<< ::GetLastError();
    ::CryptReleaseContext(GetKeyData()->prov, 0);
    ::CryptDestroyHash(GetKeyData()->hash);
    return false;
  }

  // Set IV
  if (iv != NULL) {
    memcpy(iv_.get(), iv, iv_size());
  } else {
    memset(iv_.get(), '\0', iv_size());
  }

  // Set padding mode, IV, CBC
  if (!::CryptSetKeyParam(GetKeyData()->key,
                          KP_PADDING,
                          const_cast<BYTE *>(
                              reinterpret_cast<const BYTE *>(
                                  &kPaddingMode)), 0) ||
      !::CryptSetKeyParam(GetKeyData()->key,
                          KP_MODE,
                          const_cast<BYTE *>(
                              reinterpret_cast<const BYTE *>(
                                  &kCryptMode)), 0) ||
      !::CryptSetKeyParam(GetKeyData()->key,
                          KP_IV,
                          iv_.get(), 0)) {
    LOG(ERROR) << "CryptSetKeyParam failed" << ::GetLastError();
    ::CryptReleaseContext(GetKeyData()->prov, 0);
    ::CryptDestroyKey(GetKeyData()->key);
    ::CryptDestroyHash(GetKeyData()->hash);
    return false;
  }

  is_available_ = true;

  return true;
}
#else  // OS_WIN

bool Encryptor::Key::DeriveFromPassword(const string &password,
                                        const string &salt,
                                        const uint8 *iv) {
  if (is_available_) {
    LOG(WARNING) << "key is already set";
    return false;
  }

  if (password.empty()) {
    LOG(WARNING) << "password is empty";
    return false;
  }

  if (iv != NULL) {
    memcpy(iv_.get(), iv, iv_size());
  } else {
    memset(iv_.get(), '\0', iv_size());
  }

#ifdef OS_ANDROID
  size_t size = sizeof(GetKeyData()->key);
  if (!JavaEncryptorProxy::DeriveFromPassword(
          password, salt, GetKeyData()->key, &size)) {
    LOG(WARNING) << "Failed to generate key.";
    return false;
  }
  if (size != sizeof(GetKeyData()->key)) {
    LOG(WARNING) << "Key size is changed: " << size;
    return false;
  }
#else
  const string key = GetMSCryptDeriveKeyWithSHA1(password, salt);
  DCHECK_EQ(40, key.size());   // SHA1 is 160bit hash, so 160*2/8 = 40byte

  AES_set_encrypt_key(reinterpret_cast<const uint8 *>(key.data()),
                      key_size(),
                      &(GetKeyData()->encrypt_key));
  AES_set_decrypt_key(reinterpret_cast<const uint8 *>(key.data()),
                      key_size(),
                      &(GetKeyData()->decrypt_key));
#endif  // OS_ANDROID

  is_available_ = true;

  return true;
}
#endif   // OS_WIN

Encryptor::Key::Key()
    : data_(new KeyData),
      iv_(new uint8[kBlockSize]),
      is_available_(false) {
  memset(iv_.get(), '\0', iv_size());
}

Encryptor::Key::~Key() {
#ifdef OS_WIN
  if (is_available_) {
    ::CryptDestroyKey(GetKeyData()->key);
    ::CryptDestroyHash(GetKeyData()->hash);
    ::CryptReleaseContext(GetKeyData()->prov, 0);
  }
#endif
}

bool Encryptor::EncryptString(const Encryptor::Key &key, string *data) {
  if (data == NULL || data->empty()) {
    LOG(ERROR) << "data is NULL or empty";
    return false;
  }
  size_t size = data->size();
  scoped_array<char> buf(new char[key.GetEncryptedSize(data->size())]);
  memcpy(buf.get(), data->data(), data->size());
  if (!Encryptor::EncryptArray(key, buf.get(), &size)) {
    LOG(ERROR) << "EncryptArray() failed";
    return false;
  }
  data->assign(buf.get(), size);
  return true;
}

bool Encryptor::DecryptString(const Encryptor::Key &key, string *data) {
  if (data == NULL || data->empty()) {
    LOG(ERROR) << "data is NULL or empty";
    return false;
  }
  size_t size = data->size();
  scoped_array<char> buf(new char[data->size()]);
  memcpy(buf.get(), data->data(), data->size());
  if (!Encryptor::DecryptArray(key, buf.get(), &size)) {
    LOG(ERROR) << "DecryptArray() failed";
    return false;
  }
  data->assign(buf.get(), size);
  return true;
}

bool Encryptor::EncryptArray(const Encryptor::Key &key,
                             char *buf, size_t *buf_size) {
  if (!key.IsAvailable()) {
    LOG(ERROR) << "key is not available";
    return false;
  }

  if (buf == NULL || buf_size == NULL || *buf_size == 0) {
    LOG(ERROR) << "invalid buffer given";
    return false;
  }

  const size_t enc_size = key.GetEncryptedSize(*buf_size);

#if defined(OS_ANDROID)
  if (!JavaEncryptorProxy::Encrypt(
          key.GetKeyData()->key, key.iv(), enc_size, buf, buf_size)) {
    LOG(ERROR) << "CryptEncrypt failed";
    return false;
  }
  return true;
#elif defined(OS_WIN)
  uint32 size = *buf_size;
  if (!::CryptEncrypt(key.GetKeyData()->key,
                      0, TRUE, 0,
                      reinterpret_cast<BYTE *>(buf),
                      reinterpret_cast<DWORD *>(&size),
                      static_cast<DWORD>(enc_size))) {
    LOG(ERROR) << "CryptEncrypt failed: " << ::GetLastError();
    return false;
  }
  *buf_size = enc_size;
  return true;
#elif defined(HAVE_OPENSSL)
  // perform PKCS#5 padding
  const size_t padding_size = enc_size - *buf_size;
  const uint8 padding_value = static_cast<uint8>(padding_size);
  for (size_t i = *buf_size; i < enc_size; ++i) {
    buf[i] = static_cast<char>(padding_value);
  }

  // iv is used inside AES_cbc_encrypt, so must copy it.
  scoped_array<uint8> iv(new uint8[key.iv_size()]);
  memcpy(iv.get(), key.iv(), key.iv_size());  // copy iv

  AES_cbc_encrypt(reinterpret_cast<const uint8 *>(buf),
                  reinterpret_cast<uint8 *>(buf),
                  enc_size, &(key.GetKeyData()->encrypt_key),
                  iv.get(), AES_ENCRYPT);
  *buf_size = enc_size;
  return true;
#else
// None of OS_ANDROID/OS_WIN/HAVE_OPENSSL is defined.
#error "Encryptor does not support your platform."
  return false;
#endif
}

bool Encryptor::DecryptArray(const Encryptor::Key &key,
                             char *buf, size_t *buf_size) {
  if (!key.IsAvailable()) {
    LOG(ERROR) << "key is not available";
    return false;
  }

  if (buf == NULL || buf_size == NULL || *buf_size == 0) {
    LOG(ERROR) << "invalid buffer given";
    return false;
  }

  if (*buf_size < key.block_size() || *buf_size % key.block_size() != 0) {
    LOG(ERROR) << "buf size is not multiples of " << key.block_size();
    return false;
  }

#if defined(OS_ANDROID)
  if (!JavaEncryptorProxy::Decrypt(
          key.GetKeyData()->key, key.iv(), *buf_size, buf, buf_size)) {
    LOG(ERROR) << "CryptDecrypt failed";
    return false;
  }
  return true;
#elif defined(OS_WIN)
  DWORD size = static_cast<DWORD>(*buf_size);
  if (!::CryptDecrypt(key.GetKeyData()->key,
                      0, TRUE, 0,
                      reinterpret_cast<uint8 *>(buf),
                      &size)) {
    LOG(ERROR) << "CryptDecrypt failed: " << ::GetLastError();
    return false;
  }

  *buf_size = size;
  return true;
#elif defined(HAVE_OPENSSL)
  size_t size = *buf_size;
  // iv is used inside AES_cbc_encrypt, so must copy it.
  scoped_array<uint8> iv(new uint8[key.iv_size()]);
  memcpy(iv.get(), key.iv(), key.iv_size());  // copy iv

  AES_cbc_encrypt(reinterpret_cast<const uint8 *>(buf),
                  reinterpret_cast<uint8 *>(buf), size,
                  &(key.GetKeyData()->decrypt_key), iv.get(), AES_DECRYPT);

  // perform PKCS#5 un-padding
  // see. http://www.chilkatsoft.com/faq/PKCS5_Padding.html
  const uint8 padding_value = static_cast<uint8>(buf[size - 1]);
  const size_t padding_size = static_cast<size_t>(padding_value);
  if (padding_value == 0x00 || padding_value > AES_BLOCK_SIZE) {
    LOG(ERROR) << "Cannot find PKCS#5 padding values: ";
    return false;
  }

  if (size <= padding_size) {
    LOG(ERROR) << "padding size is no smaller than original message";
    return false;
  }

  for (size_t i = size - padding_size; i < size; ++i) {
    if (static_cast<uint8>(buf[i]) != padding_value) {
      LOG(ERROR) << "invalid padding value. message is broken";
      return false;
    }
  }
  *buf_size -= padding_size;   // remove padding part
  return true;
#else
// None of OS_ANDROID/OS_WIN/HAVE_OPENSSL is defined.
#error "Encryptor does not support your platform."
  return false;
#endif
}

// Protect|Unprotect Data
#ifdef OS_WIN
// See. http://msdn.microsoft.com/en-us/library/aa380261.aspx
bool Encryptor::ProtectData(const string &plain_text,
                            string *cipher_text) {
  DCHECK(cipher_text);
  DATA_BLOB input;
  input.pbData = const_cast<BYTE *>(
      reinterpret_cast<const BYTE *>(plain_text.data()));
  input.cbData = static_cast<DWORD>(plain_text.size());

  DATA_BLOB output;
  const BOOL result = ::CryptProtectData(&input, L"",
                                         NULL, NULL, NULL,
                                         CRYPTPROTECT_UI_FORBIDDEN,
                                         &output);
  if (!result) {
    LOG(ERROR) << "CryptProtectData failed: " << ::GetLastError();
    return false;
  }

  cipher_text->assign(reinterpret_cast<char *>(output.pbData),
                      output.cbData);

  ::LocalFree(output.pbData);

  return true;
}

// See. http://msdn.microsoft.com/en-us/library/aa380882(VS.85).aspx
bool Encryptor::UnprotectData(const string &cipher_text,
                              string *plain_text) {
  DCHECK(plain_text);
  DATA_BLOB input;
  input.pbData = const_cast<BYTE *>(
      reinterpret_cast<const BYTE*>(cipher_text.data()));
  input.cbData = static_cast<DWORD>(cipher_text.length());

  DATA_BLOB output;
  const BOOL result = ::CryptUnprotectData(&input,
                                           NULL, NULL, NULL, NULL,
                                           CRYPTPROTECT_UI_FORBIDDEN,
                                           &output);
  if (!result) {
    LOG(ERROR) << "CryptUnprotectData failed: " << ::GetLastError();
    return false;
  }

  plain_text->assign(reinterpret_cast<char *>(output.pbData), output.cbData);

  ::LocalFree(output.pbData);

  return true;
}

#elif defined(OS_MACOSX)

// ProtectData for Mac uses the serial number and the current pid as the key.
bool Encryptor::ProtectData(const string &plain_text,
                            string *cipher_text) {
  DCHECK(cipher_text);
  Encryptor::Key key;
  const string serial_number = MacUtil::GetSerialNumber();
  const string salt = Util::StringPrintf("%x", ::getuid());
  if (serial_number.empty()) {
    LOG(ERROR) << "Cannot get the serial number";
    return false;
  }
  if (!key.DeriveFromPassword(serial_number, salt)) {
    LOG(ERROR) << "Cannot prepare the internal key";
    return false;
  }

  cipher_text->assign(plain_text);
  if (!EncryptString(key, cipher_text)) {
    cipher_text->clear();
    LOG(ERROR) << "Cannot encrypt the text";
    return false;
  }
  return true;
}

// Same as ProtectData.
bool Encryptor::UnprotectData(const string &cipher_text,
                              string *plain_text) {
  DCHECK(plain_text);
  Encryptor::Key key;
  const string serial_number = MacUtil::GetSerialNumber();
  const string salt = Util::StringPrintf("%x", ::getuid());
  if (serial_number.empty()) {
    LOG(ERROR) << "Cannot get the serial number";
    return false;
  }
  if (!key.DeriveFromPassword(serial_number, salt)) {
    LOG(ERROR) << "Cannot prepare the internal key";
    return false;
  }

  plain_text->assign(cipher_text);
  if (!DecryptString(key, plain_text)) {
    plain_text->clear();
    LOG(ERROR) << "Cannot encrypt the text";
    return false;
  }
  return true;
}

#else  // OS_WIN | OS_MACOSX

namespace {

const size_t kSaltSize = 32;

}  // namespace

// Use AES to emulate ProtectData
bool Encryptor::ProtectData(const string &plain_text, string *cipher_text) {
  string password;
  if (!PasswordManager::GetPassword(&password)) {
    LOG(ERROR) << "Cannot get password";
    return false;
  }

  char salt_buf[kSaltSize];
  Util::GetRandomSequence(salt_buf, sizeof(salt_buf));

  string salt(salt_buf, kSaltSize);

  Encryptor::Key key;
  if (!key.DeriveFromPassword(password, salt)) {
    LOG(ERROR) << "DeriveFromPassword failed";
    return false;
  }

  string buf = plain_text;
  if (!Encryptor::EncryptString(key, &buf)) {
    LOG(ERROR) << "Encryptor::EncryptString failed";
    return false;
  }

  cipher_text->clear();
  *cipher_text += salt;
  *cipher_text += buf;

  return true;
}

// Use AES to emulate UnprotectData
bool Encryptor::UnprotectData(const string &cipher_text, string *plain_text) {
  if (cipher_text.size() < kSaltSize) {
    LOG(ERROR) << "encrypted message is too short";
    return false;
  }

  string password;
  if (!PasswordManager::GetPassword(&password)) {
    LOG(ERROR) << "Cannot get password";
    return false;
  }

  const string salt(cipher_text.data(), kSaltSize);

  Encryptor::Key key;
  if (!key.DeriveFromPassword(password, salt)) {
    LOG(ERROR) << "DeriveFromPassword failed";
    return false;
  }

  string buf(cipher_text.data() + kSaltSize,
             cipher_text.size() - kSaltSize);

  if (!Encryptor::DecryptString(key, &buf)) {
    LOG(ERROR) << "Encryptor::DecryptString failed";
    return false;
  }

  plain_text->clear();
  *plain_text += buf;

  return true;
}
#endif  // OS_WIN

}  // namespace mozc

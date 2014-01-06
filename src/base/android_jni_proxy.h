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

#ifndef IME_MOZC_BASE_ANDROID_JNI_PROXY_H_
#define IME_MOZC_BASE_ANDROID_JNI_PROXY_H_

#include <jni.h>
#include <iostream>
#include <string>

#include "base/port.h"
#include "net/http_client.h"
#include "net/http_client_common.h"

namespace mozc {
namespace jni {

// Proxy to invoke
// org.mozc.android.inputmethod.japanese.nativecallback.Encryptor from
// mozc server.
// To use this class, it is necessary to set JavaVM instance via SetJavaVM.
class JavaEncryptorProxy {
 public:
  static const size_t kBlockSizeInBytes = 16;
  // (kKeySizeInBits % 8) must be 0.
  static const size_t kKeySizeInBits = 256;

  // Creates encryption key from the given password and salt.
  // buf_size is the capacity of buf.
  // Returns true if the method is finished successfully, and stores the result
  // into buf. Otherwise returns false.
  // The size of buf needs to be 256 (or greater) [bytes].
  static bool DeriveFromPassword(const string &password, const string &salt,
                                 uint8 *buf, size_t *buf_size);

  // Encrypts the buf's contents and stores back the result into buf.
  // The size of key must be 256, and the size of iv must be 16 [bytes].
  // max_buf_size is the capacity of the buf, and buf_size is a pointer
  // to the current contents size of the buf.
  // Returns true if the method is completed successfully, and stores the
  // result to buf, and its size to buf_size.
  static bool Encrypt(const uint8 *key, const uint8 *iv, size_t max_buf_size,
                      char *buf, size_t *buf_size);

  // Decrypts the buf's contents and stores back the result into buf.
  // The size of key must be 256, and the size of iv must be 16 [bytes].
  // max_buf_size is the capacity of the buf, and buf_size is a pointer
  // to the current contents size of the buf.
  // Returns true if the method is completed successfully, and stores the
  // result to buf, and its size to buf_size.
  static bool Decrypt(const uint8 *key, const uint8 *iv, size_t max_buf_size,
                      char *buf, size_t *buf_size);

  static void SetJavaVM(JavaVM *jvm);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JavaEncryptorProxy);
};

class JavaHttpClientProxy {
 public:
  static bool Request(HTTPMethodType type,
                      const string &url,
                      const char *post_data,
                      size_t post_size,
                      const HTTPClient::Option &option,
                      string *output_string);
  static void SetJavaVM(JavaVM *jvm);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JavaHttpClientProxy);
};

}  // namespace jni
}  // namespace mozc

#endif  // IME_MOZC_BASE_ANDROID_JNI_PROXY_H_

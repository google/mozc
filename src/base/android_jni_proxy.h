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

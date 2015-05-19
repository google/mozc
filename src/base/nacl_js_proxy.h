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

#ifndef MOZC_BASE_NACL_JS_PROXY_H_
#define MOZC_BASE_NACL_JS_PROXY_H_

#include <string>

#include "base/scoped_ptr.h"

namespace pp {
class Instance;
}

namespace Json {
class Value;
}

namespace mozc {

class NaclJsProxyImplInterface {
 public:
  virtual ~NaclJsProxyImplInterface() {}
  virtual bool GetAuthToken(bool interactive, string *access_token) = 0;
  virtual void OnProxyCallResult(Json::Value *result) = 0;
};

class NaclJsProxy {
 public:
  // Initializes NaclJsProxy. You must call it before using NaclJsProxy.
  static void Initialize(pp::Instance *instance);

  // Calls chrome.identity.getAuthToken JavaScript API in Chrome.
  // This method is blocking and doesn't support recursive call.
  static bool GetAuthToken(bool interactive, string *access_token);

  // OnProxyCallResult is called when the result of "jscall" is received in
  // pp::Instance::HandleMessage().
  // This method takes the ownership of the result object.
  static void OnProxyCallResult(Json::Value *result);

  // Registers NaclJsProxyImplInterface for testing. This method takes the
  // ownership of NaclJsProxyImplInterface instance.
  static void RegisterNaclJsProxyImplForTest(NaclJsProxyImplInterface *impl);

 private:
  static scoped_ptr<NaclJsProxyImplInterface> impl_;
};

}  // namespace mozc

#endif  // MOZC_BASE_NACL_JS_PROXY_H_

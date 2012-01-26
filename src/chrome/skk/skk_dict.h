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

#ifndef MOZC_CHROME_SKK_SKK_DICT_H_
#define MOZC_CHROME_SKK_SKK_DICT_H_

#include "dictionary/system/system_dictionary.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "third_party/jsoncpp/json.h"

namespace mozc {

class SkkDictInstance : public pp::Instance {
 public:
  explicit SkkDictInstance(PP_Instance instance);
  virtual ~SkkDictInstance();
  virtual void HandleMessage(const pp::Var &message);

 private:
  void PostDebugMessage(const string &message);
  void PostErrorMessage(int id, const string &message);
  void LookupEntry(const Json::Value &request);

  mozc::SystemDictionary *dictionary_;

  DISALLOW_COPY_AND_ASSIGN(SkkDictInstance);
};

class SkkDictModule : public pp::Module {
 public:
  SkkDictModule();
  virtual ~SkkDictModule();
  virtual pp::Instance* CreateInstance(PP_Instance instance);

 private:
  DISALLOW_COPY_AND_ASSIGN(SkkDictModule);
};

}  // namespace mozc

#endif  // MOZC_CHROME_SKK_SKK_DICT_H_

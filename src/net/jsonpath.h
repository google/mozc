// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_NET_JSONPATH_H_
#define MOZC_NET_JSONPATH_H_

#include <string>
#include <vector>

#include "base/port.h"
#include "net/jsoncpp.h"

namespace mozc {
namespace net {

// C++ implementation of JsonPath.
// http://goessner.net/articles/JsonPath/
//
// "@", "?()" and "()" are not supported as they require an
// "eval()" method on the underlying script language.
class JsonPath {
 public:
  // Perform JsonPath query |jsonpath| to the Json node |root|.
  // Results are saved in |output|. |root| node has the
  // ownership of the pointer (const Json::Value *) in |output|.
  //
  // |output| contains both Object/Array nodes. If you want
  // to obtain a string expression, use Json::Value::asString()
  // or Json::Value::toStyledString().
  //
  // Usage:
  //
  // net::JsonPath::Parse(root, "$.foo.bar[1:2].*.buz", &output);
  //
  // for (int i = 0; i < outputs.size(); ++i) {
  //   const Json::Value *value = output[i];
  //   if (value->isObjet() || value->isArray()) {
  //     // output in Json format again.
  //     std::cout << value->toStyledString();
  //   } else {
  //     std::cout << value->asString();
  //   }
  // }
  static bool Parse(const Json::Value &root, const std::string &jsonpath,
                    std::vector<const Json::Value *> *output);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JsonPath);
};

}  // namespace net
}  // namespace mozc

#endif  // MOZC_NET_JSONPATH_H_

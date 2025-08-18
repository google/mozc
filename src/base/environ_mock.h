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

#ifndef MOZC_BASE_ENVIRON_MOCK_H_
#define MOZC_BASE_ENVIRON_MOCK_H_

#include <map>
#include <string>

#include "base/environ.h"
#include "base/strings/zstring_view.h"

namespace mozc {
class EnvironMock : public EnvironInterface {
 public:
  EnvironMock() {
    env_vars_["HOME"] = "/home/mozcuser";
    Environ::SetMockForUnitTest(this);
  }
  ~EnvironMock() override { Environ::SetMockForUnitTest(nullptr); }

  std::string GetEnv(zstring_view env_var) override {
    auto it = env_vars_.find({env_var.data(), env_var.size()});
    return it != env_vars_.end() ? it->second.c_str() : "";
  }

  // This is not an override function.
  void SetEnv(const std::string& env_var, const std::string& value) {
    env_vars_[env_var] = value;
  }

  std::map<std::string, std::string> env_vars_;
};
}  // namespace mozc

#endif  // MOZC_BASE_ENVIRON_MOCK_H_

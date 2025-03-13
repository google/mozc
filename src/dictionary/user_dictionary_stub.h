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

#ifndef MOZC_DICTIONARY_USER_DICTIONARY_STUB_H_
#define MOZC_DICTIONARY_USER_DICTIONARY_STUB_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "dictionary/dictionary_interface.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "request/conversion_request.h"

namespace mozc {
namespace dictionary {

class UserDictionaryStub : public UserDictionaryInterface {
 public:
  bool HasKey(absl::string_view key) const override { return false; }
  bool HasValue(absl::string_view value) const override { return false; }

  void LookupPredictive(absl::string_view key,
                        const ConversionRequest &conversion_request,
                        Callback *callback) const override {}

  void LookupPrefix(absl::string_view key,
                    const ConversionRequest &conversion_request,
                    Callback *callback) const override {}

  void LookupExact(absl::string_view key,
                   const ConversionRequest &conversion_request,
                   Callback *callback) const override {}

  void LookupReverse(absl::string_view str,
                     const ConversionRequest &conversion_request,
                     Callback *callback) const override {}

  bool LookupComment(absl::string_view key, absl::string_view value,
                     const ConversionRequest &conversion_request,
                     std::string *comment) const override {
    if (key == "comment" || value == "comment") {
      comment->assign("UserDictionaryStub");
      return true;
    }
    return false;
  }

  std::vector<std::string> GetPosList() const override { return {}; }

  void WaitForReloader() override {}

  bool Load(const user_dictionary::UserDictionaryStorage &storage) override {
    return true;
  }

  bool IsSuppressedEntry(absl::string_view key,
                         absl::string_view value) const override {
    return false;
  }

  bool HasSuppressedEntries() const override { return false; }
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_DICTIONARY_STUB_H_

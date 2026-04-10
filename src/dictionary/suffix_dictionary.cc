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

#include "dictionary/suffix_dictionary.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/bits.h"
#include "base/container/serialized_string_array.h"
#include "dictionary/dictionary_token.h"
#include "request/conversion_request.h"

namespace mozc {
namespace dictionary {

SuffixDictionary::SuffixDictionary(absl::string_view key_array_data,
                                   absl::string_view value_array_data,
                                   absl::string_view token_array_data)
    : token_array_(MakeAlignedConstSpan<TokenArrayData>(token_array_data)) {
  DCHECK(SerializedStringArray::VerifyData(key_array_data));
  DCHECK(SerializedStringArray::VerifyData(value_array_data));
  key_array_.Set(key_array_data);
  value_array_.Set(value_array_data);

  DCHECK_EQ(key_array_.size(), token_array_.size());
  DCHECK_EQ(value_array_.size(), token_array_.size());
}

bool SuffixDictionary::HasKey(absl::string_view key) const {
  // SuffixDictionary::HasKey() is never called and unnecessary to
  // implement. To avoid accidental calls of this method, the method simply dies
  // so that we can immediately notice this unimplemented method during
  // development.
  LOG(FATAL) << "bool SuffixDictionary::HasKey() is not implemented";
  return false;
}

bool SuffixDictionary::HasValue(absl::string_view value) const {
  // SuffixDictionary::HasValue() is never called and unnecessary to
  // implement. To avoid accidental calls of this method, the method simply dies
  // so that we can immediately notice this unimplemented method during
  // development.
  LOG(FATAL) << "bool SuffixDictionary::HasValue() is not implemented";
  return false;
}

void SuffixDictionary::LookupPredictive(
    absl::string_view key, const ConversionRequest& conversion_request,
    Callback* callback) const {
  const auto [begin, end] = std::equal_range(
      key_array_.begin(), key_array_.end(), key,
      // compare by the prefix.
      [&](absl::string_view x, absl::string_view y) {
        return x.substr(0, key.size()) < y.substr(0, key.size());
      });

  Token token;
  token.attributes = Token::SUFFIX_DICTIONARY;
  for (auto it = begin; it != end; ++it) {
    token.key.assign((*it).data(), (*it).size());
    switch (callback->OnKey(token.key)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
        continue;
      case Callback::TRAVERSE_CULL:
        LOG(FATAL) << "Culling is not supported.";
      default:
        break;
    }
    if (callback->OnActualKey(token.key, token.key, /* num_expanded= */ 0) ==
        Callback::TRAVERSE_DONE) {
      return;
    }
    const size_t index = it - key_array_.begin();
    if (value_array_[index].empty()) {
      token.value = token.key;
    } else {
      token.value.assign(value_array_[index].data(),
                         value_array_[index].size());
    }

    // Invalid index.
    if (index >= token_array_.size()) break;

    const TokenArrayData& data = token_array_[index];
    token.lid = data.lid;
    token.rid = data.rid;
    token.cost = data.cost;
    if (callback->OnToken(token.key, token.key, token) !=
        Callback::TRAVERSE_CONTINUE) {
      break;
    }
  }
}

}  // namespace dictionary
}  // namespace mozc

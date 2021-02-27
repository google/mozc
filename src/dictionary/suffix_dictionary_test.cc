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

#include <cstdint>
#include <memory>

#include "base/util.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_test_util.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace dictionary {
namespace {

TEST(SuffixDictionaryTest, LookupPredictive) {
  // Test SuffixDictionary with mock data.
  std::unique_ptr<const SuffixDictionary> dic;
  ConversionRequest convreq;
  {
    const testing::MockDataManager manager;
    absl::string_view key_array_data, value_arra_data;
    const uint32_t *token_array = nullptr;
    manager.GetSuffixDictionaryData(&key_array_data, &value_arra_data,
                                    &token_array);
    dic = absl::make_unique<SuffixDictionary>(key_array_data, value_arra_data,
                                              token_array);
    ASSERT_NE(nullptr, dic.get());
  }

  {
    // Lookup with empty key.  All tokens are looked up.  Here, just verify the
    // result is nonempty and each token has valid data.
    CollectTokenCallback callback;
    dic->LookupPredictive("", convreq, &callback);
    EXPECT_FALSE(callback.tokens().empty());
    for (size_t i = 0; i < callback.tokens().size(); ++i) {
      const Token &token = callback.tokens()[i];
      EXPECT_FALSE(token.key.empty());
      EXPECT_FALSE(token.value.empty());
      EXPECT_LT(0, token.lid);
      EXPECT_LT(0, token.rid);
      EXPECT_EQ(Token::SUFFIX_DICTIONARY, token.attributes);
    }
  }
  {
    // Non-empty prefix.
    const std::string kPrefix = "た";
    CollectTokenCallback callback;
    dic->LookupPredictive(kPrefix, convreq, &callback);
    EXPECT_FALSE(callback.tokens().empty());
    for (size_t i = 0; i < callback.tokens().size(); ++i) {
      const Token &token = callback.tokens()[i];
      EXPECT_TRUE(Util::StartsWith(token.key, kPrefix));
      EXPECT_FALSE(token.value.empty());
      EXPECT_LT(0, token.lid);
      EXPECT_LT(0, token.rid);
      EXPECT_EQ(Token::SUFFIX_DICTIONARY, token.attributes);
    }
  }
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc

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

#ifndef MOZC_DICTIONARY_DICTIONARY_TEST_UTIL_H_
#define MOZC_DICTIONARY_DICTIONARY_TEST_UTIL_H_

#include <cstddef>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "testing/gunit.h"

namespace mozc {
namespace dictionary {

// Used to collect all the tokens looked up.
class CollectTokenCallback : public DictionaryInterface::Callback {
 public:
  absl::Span<const Token> tokens() const { return tokens_; }
  void Clear() { tokens_.clear(); }

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token& token) override;

 private:
  std::vector<Token> tokens_;
};

// Used to test if a given token is looked up.
class CheckTokenExistenceCallback : public DictionaryInterface::Callback {
 public:
  explicit CheckTokenExistenceCallback(const Token* target_token);
  bool found() const { return found_; }

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token& token) override;

 private:
  const Token* target_token_;
  bool found_;
};

class CheckMultiTokensExistenceCallback : public DictionaryInterface::Callback {
 public:
  explicit CheckMultiTokensExistenceCallback(absl::Span<Token* const> tokens);
  bool IsFound(const Token* token) const;
  bool AreAllFound() const;

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token& token) override;

 private:
  size_t found_count_;
  absl::flat_hash_map<const Token*, bool> result_;
};

// Generates a human redable string of token(s).
std::string PrintToken(const Token& token);
std::string PrintTokens(absl::Span<const Token> tokens);
std::string PrintTokens(absl::Span<Token* const> token_ptrs);

// Tests if two tokens are equal to each other.
#define EXPECT_TOKEN_EQ(expected, actual)                                   \
  EXPECT_PRED_FORMAT2(::mozc::dictionary::internal::IsTokenEqual, expected, \
                      actual)

// Tests if two token vectors are equal to each other as unordered set.
#define EXPECT_TOKENS_EQ_UNORDERED(expected, actual)                         \
  EXPECT_PRED_FORMAT2(::mozc::dictionary::internal::AreTokensEqualUnordered, \
                      expected, actual)

namespace internal {

::testing::AssertionResult IsTokenEqual(const char* expected_expr,
                                        const char* actual_expr,
                                        const Token& expected,
                                        const Token& actual);

::testing::AssertionResult AreTokensEqualUnordered(
    const char* expected_expr, const char* actual_expr,
    absl::Span<Token* const> expected, absl::Span<const Token> actual);

}  // namespace internal
}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_DICTIONARY_TEST_UTIL_H_

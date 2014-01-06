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

#include "dictionary/dictionary_test_util.h"

#include <set>
#include <string>

#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace dictionary {
namespace {

bool IsTokenEqaulImpl(const Token &expected, const Token &actual) {
  return expected.key == actual.key &&
         expected.value == actual.value &&
         expected.lid == actual.lid &&
         expected.rid == actual.rid &&
         expected.attributes == actual.attributes;
}

}  // namespace

DictionaryInterface::Callback::ResultType
CollectTokenCallback::OnToken(StringPiece,  // key
                              StringPiece,  // actual_key
                              const Token &token) {
  tokens_.push_back(token);
  return TRAVERSE_CONTINUE;
}

CheckTokenExistenceCallback::CheckTokenExistenceCallback(
    const Token *target_token)
    : target_token_(target_token), found_(false) {}

DictionaryInterface::Callback::ResultType
CheckTokenExistenceCallback::OnToken(StringPiece,  // key
                                     StringPiece,  // actual_key
                                     const Token &token) {
  if (IsTokenEqaulImpl(*target_token_, token)) {
    found_ = true;
    return TRAVERSE_DONE;
  }
  return TRAVERSE_CONTINUE;
}

string PrintToken(const Token &token) {
  return Util::StringPrintf("{%s, %s, %d, %d, %d}",
                            token.key.c_str(), token.value.c_str(),
                            token.lid, token.rid, token.attributes);
}

string PrintTokens(const vector<Token> &tokens) {
  string s = "[";
  for (size_t i = 0; i < tokens.size(); ++i) {
    s.append(PrintToken(tokens[i])).append(", ");
  }
  return s.append(1, ']');
}

string PrintTokens(const vector<Token *> &token_ptrs) {
  string s = "[";
  for (size_t i = 0; i < token_ptrs.size(); ++i) {
    s.append(PrintToken(*token_ptrs[i])).append(", ");
  }
  return s.append(1, ']');
}

namespace internal {

::testing::AssertionResult IsTokenEqaul(
     const char *,  // expected_expr
     const char *,  // actual_expr
     const Token &expected, const Token &actual) {
  if (IsTokenEqaulImpl(expected, actual)) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
          << "Tokens are not equal\n"
          << "Expected: " << PrintToken(expected) << "\n"
          << "Actual: " << PrintToken(actual);
}

::testing::AssertionResult AreTokensEqaulUnordered(
     const char *,  // expected_expr
     const char *,  // actual_expr
     const vector<Token *> &expected,
     const vector<Token> &actual) {
  if (expected.size() != actual.size()) {
    return ::testing::AssertionFailure()
            << "Size are different\n"
            << "Expected: " << PrintTokens(expected) << "\n"
            << "Actual: " << PrintTokens(actual);
  }
  set<string> encoded_actual;
  for (size_t i = 0; i < actual.size(); ++i) {
    encoded_actual.insert(PrintToken(actual[i]));
  }
  for (size_t i = 0; i < expected.size(); ++i) {
    const string encoded = PrintToken(*expected[i]);
    if (encoded_actual.find(encoded) == encoded_actual.end()) {
      return ::testing::AssertionFailure()
             << "Expected token " << i << " not found\n"
             << "Expected: " << PrintTokens(expected) << "\n"
             << "Actual: " << PrintTokens(actual);
    }
  }
  return ::testing::AssertionSuccess();
}

}  // namespace internal
}  // namespace dictionary
}  // namespace mozc

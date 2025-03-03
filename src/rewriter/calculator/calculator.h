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

#ifndef MOZC_REWRITER_CALCULATOR_CALCULATOR_H_
#define MOZC_REWRITER_CALCULATOR_CALCULATOR_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"

namespace mozc {

class Calculator {
 public:
  Calculator();

  bool CalculateString(absl::string_view key, std::string *result) const;

 private:
  using TokenSequence = std::vector<std::pair<int, double>>;

  // Max byte length of operator character
  static constexpr size_t kMaxLengthOfOperator = 3;

  // Tokenizes |expression_body| and sets the tokens into |tokens|.
  // It returns false if |expression_body| includes an invalid token or
  // does not include both of a number token and an operator token.
  // Parenthesis is not considered as an operator.
  bool Tokenize(absl::string_view expression_body, TokenSequence *tokens) const;

  // Perform calculation with a given sequence of token.
  bool CalculateTokens(const TokenSequence &tokens, double *result_value) const;

  // Mapping from operator character such as '+' to the corresponding
  // token type such as PLUS.
  absl::flat_hash_map<absl::string_view, int> operator_map_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_CALCULATOR_CALCULATOR_H_

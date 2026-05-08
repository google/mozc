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

#include "rewriter/calculator/calculator.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "base/japanese_util.h"
#include "base/number_util.h"

namespace mozc {
namespace {

using TokenType = Calculator::TokenType;
using Token = Calculator::Token;

// Security and performance limits for the calculator.
// Set to 2048 bytes and 256 operations to pass the existing stress test suite
// which contains extremely long and complex automatically generated
// expressions.
constexpr size_t kMaxInputLength = 2048;
constexpr int kMaxOperations = 256;

// Context holding the current state of parsing.
// It groups the tokens array and the processing index to simplify arguments.
struct ParserContext {
  const std::vector<Token>& tokens;
  size_t pos = 0;
  int operation_count = 0;
};

// Detailed Grammar & Association Rules:
//
// -----------------------------------------------------------------------------
// 1. Precedence (from lowest to highest):
//    Expression : '+' | '-'               (Left-associative)
//    Term       : '*' | '/' | '%'         (Left-associative)
//    Factor     : '^'                     (Right-associative)
//    Unary      : '+' | '-'               (Prefix, right-associative)
//    Primary    : Integer | Function | '(' Expression ')'
//
// -----------------------------------------------------------------------------
// 2. Behavior of right-associativity vs left-associativity:
//    - '+' and '-' (binary) are parsed using loop evaluation:
//        A + B + C is processed as (A + B) + C.
//    - '^' is right-associative:
//        A ^ B ^ C is processed as A ^ (B ^ C) via direct recursive calls.
//
// -----------------------------------------------------------------------------
// 3. Mutual Recursion:
//    - ParsePrimary handles parentheses '( Expression )' and functions.
//    - Minimum forward-declaration is required below to allow this cycle.
//    - Recursive calls are guarded by the operation count limit
//      (kMaxOperations) to prevent stack overflow.
// -----------------------------------------------------------------------------

std::optional<double> ParseExpression(ParserContext& ctx);

std::optional<double> ParsePrimary(ParserContext& ctx) {
  if (++ctx.operation_count > kMaxOperations) {
    return std::nullopt;
  }
  if (ctx.pos >= ctx.tokens.size()) {
    return std::nullopt;
  }
  if (ctx.tokens[ctx.pos].type == TokenType::INTEGER) {
    double val = ctx.tokens[ctx.pos].value;
    ++ctx.pos;
    return val;
  }
  if (ctx.tokens[ctx.pos].type == TokenType::LP) {
    ++ctx.pos;
    std::optional<double> val = ParseExpression(ctx);
    if (!val.has_value()) {
      return std::nullopt;
    }
    if (ctx.pos >= ctx.tokens.size() ||
        ctx.tokens[ctx.pos].type != TokenType::RP) {
      return std::nullopt;
    }
    ++ctx.pos;
    return val;
  }

  // Handle mathematical functions (e.g. log, sin, cos)
  TokenType type = ctx.tokens[ctx.pos].type;
  if (type == TokenType::FUNC_LOG || type == TokenType::FUNC_LN ||
      type == TokenType::FUNC_EXP || type == TokenType::FUNC_SQRT ||
      type == TokenType::FUNC_SIN || type == TokenType::FUNC_COS ||
      type == TokenType::FUNC_TAN || type == TokenType::FUNC_ABS) {
    ++ctx.pos;
    // Require left parenthesis '('
    if (ctx.pos >= ctx.tokens.size() ||
        ctx.tokens[ctx.pos].type != TokenType::LP) {
      return std::nullopt;
    }
    ++ctx.pos;
    std::optional<double> arg = ParseExpression(ctx);
    if (!arg.has_value()) {
      return std::nullopt;
    }
    // Require right parenthesis ')'
    if (ctx.pos >= ctx.tokens.size() ||
        ctx.tokens[ctx.pos].type != TokenType::RP) {
      return std::nullopt;
    }
    ++ctx.pos;

    if (type == TokenType::FUNC_LOG) {
      return std::log10(*arg);
    } else if (type == TokenType::FUNC_LN) {
      return std::log(*arg);
    } else if (type == TokenType::FUNC_EXP) {
      return std::exp(*arg);
    } else if (type == TokenType::FUNC_SQRT) {
      if (*arg < 0.0) {
        return std::nullopt;
      }
      return std::sqrt(*arg);
    } else if (type == TokenType::FUNC_SIN) {
      return std::sin(*arg);
    } else if (type == TokenType::FUNC_COS) {
      return std::cos(*arg);
    } else if (type == TokenType::FUNC_TAN) {
      return std::tan(*arg);
    } else if (type == TokenType::FUNC_ABS) {
      return std::abs(*arg);
    } else {
      DCHECK(false) << "Unreachable function type in ParsePrimary";
      return std::nullopt;
    }
  }

  return std::nullopt;
}

std::optional<double> ParseUnary(ParserContext& ctx) {
  if (++ctx.operation_count > kMaxOperations) {
    return std::nullopt;
  }
  if (ctx.pos >= ctx.tokens.size()) {
    return std::nullopt;
  }
  TokenType op = ctx.tokens[ctx.pos].type;
  if (op == TokenType::PLUS) {
    ++ctx.pos;
    return ParseUnary(ctx);
  }
  if (op == TokenType::MINUS) {
    ++ctx.pos;
    std::optional<double> rhs = ParseUnary(ctx);
    if (!rhs.has_value()) {
      return std::nullopt;
    }
    return -(*rhs);
  }
  return ParsePrimary(ctx);
}

std::optional<double> ParseFactor(ParserContext& ctx) {
  if (++ctx.operation_count > kMaxOperations) {
    return std::nullopt;
  }
  std::optional<double> val = ParseUnary(ctx);
  if (!val.has_value()) {
    return std::nullopt;
  }
  // POW is right-associative: call ParseFactor recursively for the right side.
  if (ctx.pos < ctx.tokens.size() &&
      ctx.tokens[ctx.pos].type == TokenType::POW) {
    ++ctx.pos;
    std::optional<double> rhs = ParseFactor(ctx);
    if (!rhs.has_value()) {
      return std::nullopt;
    }
    *val = std::pow(*val, *rhs);
    if (!std::isfinite(*val)) {
      return std::nullopt;
    }
  }
  return val;
}

std::optional<double> ParseTerm(ParserContext& ctx) {
  if (++ctx.operation_count > kMaxOperations) {
    return std::nullopt;
  }
  std::optional<double> val = ParseFactor(ctx);
  if (!val.has_value()) {
    return std::nullopt;
  }
  while (ctx.pos < ctx.tokens.size()) {
    TokenType op = ctx.tokens[ctx.pos].type;
    if (op != TokenType::TIMES && op != TokenType::DIVIDE &&
        op != TokenType::MOD) {
      break;
    }
    ++ctx.pos;
    std::optional<double> rhs = ParseFactor(ctx);
    if (!rhs.has_value()) {
      return std::nullopt;
    }
    if (op == TokenType::TIMES) {
      *val *= *rhs;
    } else if (op == TokenType::DIVIDE) {
      if (*rhs == 0.0) {
        return std::nullopt;
      }
      *val /= *rhs;
    } else if (op == TokenType::MOD) {
      if (*rhs == 0.0) {
        return std::nullopt;
      }
      *val = std::fmod(*val, *rhs);
    } else {
      DCHECK(false) << "Unreachable operator in ParseTerm";
      return std::nullopt;
    }
    if (!std::isfinite(*val)) {
      return std::nullopt;
    }
  }
  return val;
}

std::optional<double> ParseExpression(ParserContext& ctx) {
  if (++ctx.operation_count > kMaxOperations) {
    return std::nullopt;
  }
  std::optional<double> val = ParseTerm(ctx);
  if (!val.has_value()) {
    return std::nullopt;
  }
  while (ctx.pos < ctx.tokens.size()) {
    TokenType op = ctx.tokens[ctx.pos].type;
    if (op != TokenType::PLUS && op != TokenType::MINUS) {
      break;
    }
    ++ctx.pos;
    std::optional<double> rhs = ParseTerm(ctx);
    if (!rhs.has_value()) {
      return std::nullopt;
    }
    if (op == TokenType::PLUS) {
      *val += *rhs;
    } else if (op == TokenType::MINUS) {
      *val -= *rhs;
    } else {
      DCHECK(false) << "Unreachable operator in ParseExpression";
      return std::nullopt;
    }
    if (!std::isfinite(*val)) {
      return std::nullopt;
    }
  }
  return val;
}

}  // namespace

Calculator::Calculator() {
  operator_map_["+"] = TokenType::PLUS;
  operator_map_["-"] = TokenType::MINUS;
  // "ー". It is called cho-ompu, onbiki, bobiki, or "nobashi-bou" casually.
  // It is not a full-width hyphen, and may appear in conversion segments by
  // typing '-' more than one time continuously.
  operator_map_["ー"] = TokenType::MINUS;
  operator_map_["*"] = TokenType::TIMES;
  operator_map_["/"] = TokenType::DIVIDE;
  operator_map_["・"] = TokenType::DIVIDE;  // "･". Consider it as "/".
  operator_map_["%"] = TokenType::MOD;
  operator_map_["^"] = TokenType::POW;
  operator_map_["("] = TokenType::LP;
  operator_map_[")"] = TokenType::RP;
  operator_map_["log"] = TokenType::FUNC_LOG;
  operator_map_["ln"] = TokenType::FUNC_LN;
  operator_map_["exp"] = TokenType::FUNC_EXP;
  operator_map_["sqrt"] = TokenType::FUNC_SQRT;
  operator_map_["sin"] = TokenType::FUNC_SIN;
  operator_map_["cos"] = TokenType::FUNC_COS;
  operator_map_["tan"] = TokenType::FUNC_TAN;
  operator_map_["abs"] = TokenType::FUNC_ABS;
}

std::optional<std::string> Calculator::CalculateString(
    const absl::string_view key) const {
  if (key.empty() || key.size() > kMaxInputLength) {
    LOG(ERROR) << "Key is empty or too long.";
    return std::nullopt;
  }
  std::string normalized_key =
      japanese_util::FullWidthAsciiToHalfWidthAscii(key);

  if (normalized_key.starts_with('=') && normalized_key.ends_with('=')) {
    return std::nullopt;
  }
  absl::string_view expression_body = normalized_key;
  if (!absl::ConsumePrefix(&expression_body, "=") &&
      !absl::ConsumeSuffix(&expression_body, "=")) {
    return std::nullopt;
  }

  std::optional<TokenSequence> tokens = Tokenize(expression_body);
  if (!tokens.has_value()) {
    // normalized_key is not valid sequence of tokens
    return std::nullopt;
  }

  std::optional<double> result_value = CalculateTokens(*tokens);
  if (!result_value.has_value()) {
    // Calculation is failed. Syntax error or arithmetic error such as
    // overflow, divide-by-zero, etc.
    return std::nullopt;
  }
  return absl::StrFormat("%.8g", *result_value);
}

std::optional<Calculator::TokenSequence> Calculator::Tokenize(
    absl::string_view expression_body) const {
  int num_operator = 0;  // Number of operators or functions appeared
  int num_value = 0;     // Number of values appeared

  TokenSequence tokens;
  absl::string_view rest = expression_body;

  while (!rest.empty()) {
    // Skip spaces
    while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
      rest.remove_prefix(1);
    }
    if (rest.empty()) {
      break;
    }

    // Read value token
    size_t value_len = 0;
    while (value_len < rest.size() &&
           (absl::ascii_isdigit(rest[value_len]) ||
            rest[value_len] == '.')) {
      ++value_len;
    }
    if (value_len > 0) {
      const absl::string_view number_token = rest.substr(0, value_len);
      double value = 0.0;
      if (!NumberUtil::SafeStrToDouble(number_token, &value)) {
        return std::nullopt;
      }
      tokens.push_back({TokenType::INTEGER, value});
      ++num_value;
      rest.remove_prefix(value_len);
      continue;
    }

    // Read alphabetical function name token
    if (absl::ascii_isalpha(rest.front())) {
      size_t func_len = 0;
      while (func_len < rest.size() && absl::ascii_isalpha(rest[func_len])) {
        ++func_len;
      }
      std::string name(rest.data(), func_len);
      absl::AsciiStrToLower(&name);
      const auto func_it = operator_map_.find(name);
      if (func_it != operator_map_.end()) {
        tokens.push_back({func_it->second, 0.0});
        rest.remove_prefix(func_len);
        ++num_operator;
        continue;
      }
      return std::nullopt;
    }

    // Read operator token
    bool matched_operator = false;
    for (size_t length = 1; length <= kMaxLengthOfOperator; ++length) {
      if (length > rest.size()) {
        break;
      }
      const absl::string_view window = rest.substr(0, length);
      const auto op_it = operator_map_.find(window);
      if (op_it == operator_map_.end()) {
        continue;
      }
      tokens.push_back({op_it->second, 0.0});
      rest.remove_prefix(length);
      matched_operator = true;
      // Does not count parenthesis as an operator.
      if ((op_it->second != TokenType::LP) &&
          (op_it->second != TokenType::RP)) {
        ++num_operator;
      }
      break;
    }
    if (matched_operator) {
      continue;
    }

    // Invalid token
    return std::nullopt;
  }

  if (num_operator == 0 || num_value == 0) {
    // Must contain at least one operator and one value.
    return std::nullopt;
  }
  return tokens;
}

std::optional<double> Calculator::CalculateTokens(
    const TokenSequence& tokens) const {
  ParserContext ctx{tokens, 0};
  std::optional<double> result = ParseExpression(ctx);
  if (!result.has_value()) {
    return std::nullopt;
  }
  if (ctx.pos != ctx.tokens.size()) {
    return std::nullopt;
  }
  return result;
}

}  // namespace mozc

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

// NOTE(tabata): This code is used mainly by louds trie builder to build
// dictionary. Please check error handling, if you want to include
// this to run within client.

#include "dictionary/text_dictionary_loader.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/japanese_util.h"
#include "base/multifile.h"
#include "base/util.h"
#include "base/vlog.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"

ABSL_FLAG(int32_t, tokens_reserve_size, 1400000,
          "Reserve the specified size of token buffer in advance.");

namespace mozc {
namespace dictionary {
namespace {

using ValueAndKey = std::pair<absl::string_view, absl::string_view>;

ValueAndKey ToValueAndKey(const std::unique_ptr<Token>& token) {
  return ValueAndKey(token->value, token->key);
}

// Functor to sort a sequence of Tokens first by value and then by key.
struct OrderByValueThenByKey {
  bool operator()(const std::unique_ptr<Token>& l,
                  const std::unique_ptr<Token>& r) const {
    return ToValueAndKey(l) < ToValueAndKey(r);
  }

  bool operator()(const std::unique_ptr<Token>& token,
                  const ValueAndKey& value_key) const {
    return ToValueAndKey(token) < value_key;
  }

  bool operator()(const ValueAndKey& value_key,
                  const std::unique_ptr<Token>& token) const {
    return value_key < ToValueAndKey(token);
  }
};

// Functor to sort a sequence of Tokens by value.
struct OrderByValue {
  bool operator()(const std::unique_ptr<Token>& token,
                  absl::string_view value) const {
    return token->value < value;
  }

  bool operator()(absl::string_view value,
                  const std::unique_ptr<Token>& token) const {
    return value < token->value;
  }
};

// Parses one line of reading correction file.  Since the result is returned as
// string views, |line| needs to outlive |value_key|.
ValueAndKey ParseReadingCorrectionTSV(
    const absl::string_view line ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  // Format: value\terror\tcorrect
  const std::vector<absl::string_view> fields = absl::StrSplit(line, '\t');
  CHECK_GE(fields.size(), 2);
  return {fields[0], fields[1]};
}

}  // namespace

TextDictionaryLoader::TextDictionaryLoader(const PosMatcher& pos_matcher)
    : zipcode_id_(pos_matcher.GetZipcodeId()),
      isolated_word_id_(pos_matcher.GetIsolatedWordId()) {}

TextDictionaryLoader::TextDictionaryLoader(uint16_t zipcode_id,
                                           uint16_t isolated_word_id)
    : zipcode_id_(zipcode_id), isolated_word_id_(isolated_word_id) {}

bool TextDictionaryLoader::RewriteSpecialToken(Token* token,
                                               absl::string_view label) const {
  CHECK(token);
  if (label.empty()) {
    return true;
  }
  if (label.starts_with("SPELLING_CORRECTION")) {
    token->attributes = Token::SPELLING_CORRECTION;
    return true;
  }
  if (label.starts_with("ZIP_CODE")) {
    token->lid = zipcode_id_;
    token->rid = zipcode_id_;
    return true;
  }
  if (label.starts_with("ENGLISH")) {
    // TODO(noriyukit): Might be better to use special POS for english words.
    token->lid = isolated_word_id_;
    token->rid = isolated_word_id_;
    return true;
  }
  LOG(ERROR) << "Unknown special label: " << label;
  return false;
}

void TextDictionaryLoader::Load(
    const absl::string_view dictionary_filename,
    const absl::string_view reading_correction_filename) {
  LoadWithLineLimit(dictionary_filename, reading_correction_filename, -1);
}

void TextDictionaryLoader::LoadWithLineLimit(
    const absl::string_view dictionary_filename,
    const absl::string_view reading_correction_filename, int limit) {
  tokens_.clear();

  // Roughly allocate buffers for Token pointers.
  if (limit < 0) {
    tokens_.reserve(absl::GetFlag(FLAGS_tokens_reserve_size));
    limit = std::numeric_limits<int>::max();
  } else {
    tokens_.reserve(limit);
  }

  // Read system dictionary.
  {
    InputMultiFile file(dictionary_filename);
    std::string line;
    while (limit > 0 && file.ReadLine(&line)) {
      Util::ChopReturns(&line);
      std::unique_ptr<Token> token = ParseTSVLine(line);
      if (token) {
        tokens_.push_back(std::move(token));
        --limit;
      }
    }
    LOG(INFO) << tokens_.size() << " tokens from " << dictionary_filename;
  }

  if (reading_correction_filename.empty() || limit <= 0) {
    return;
  }

  // Prepare for loading reading corrections. We sort |tokens_| first by value
  // and then by key so that we can perform the following operations both in
  // O(log(N)), where N is the size of tokens.
  //   1. Checking the existence of any key-value pairs: This can be done by
  //      binary-searching for a pair of value and key.
  //   2. Accessing all the tokens that have the same value: Since tokens are
  //      also sorted in order of value, this can be done by finding a range of
  //      tokens that have the same value.
  std::stable_sort(tokens_.begin(), tokens_.end(), OrderByValueThenByKey());

  std::vector<std::unique_ptr<Token>> reading_correction_tokens =
      LoadReadingCorrectionTokens(reading_correction_filename, tokens_, &limit);
  tokens_.insert(tokens_.end(),
                 std::make_move_iterator(reading_correction_tokens.begin()),
                 std::make_move_iterator(reading_correction_tokens.end()));
}

// Loads reading correction data into |tokens|.  The second argument is used to
// determine costs of reading correction tokens and must be sorted by
// OrderByValueThenByKey().  The output tokens are newly allocated and the
// caller is responsible to delete them.
std::vector<std::unique_ptr<Token>>
TextDictionaryLoader::LoadReadingCorrectionTokens(
    const absl::string_view reading_correction_filename,
    absl::Span<const std::unique_ptr<Token>> ref_sorted_tokens, int* limit) {
  // Load reading correction entries.
  std::vector<std::unique_ptr<Token>> tokens;
  int reading_correction_size = 0;
  InputMultiFile file(reading_correction_filename);
  std::string line;
  while (file.ReadLine(&line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // Parse TSV line in a pair of value and key (Note: first element is value
    // and the second key).
    Util::ChopReturns(&line);
    const ValueAndKey value_key = ParseReadingCorrectionTSV(line);

    // Filter the entry if this key value pair already exists in the system
    // dictionary.
    if (std::binary_search(ref_sorted_tokens.begin(), ref_sorted_tokens.end(),
                           value_key, OrderByValueThenByKey())) {
      MOZC_VLOG(1) << "System dictionary has the same key-value: " << line;
      continue;
    }

    // Since reading correction entries lack POS and cost, we recover those
    // fields from a token in the system dictionary that has the same value.
    // Since multiple tokens may have the same value, from such tokens, we
    // select the one that has the maximum cost.
    auto [begin, end] =
        std::equal_range(ref_sorted_tokens.begin(), ref_sorted_tokens.end(),
                         value_key.first, OrderByValue());
    if (begin == end) {
      MOZC_VLOG(1) << "Cannot find the value in system dicitonary - ignored:"
                   << line;
      continue;
    }
    // Now [begin, end) contains all the tokens that have the same value as
    // this reading correction entry.  Next, find the token that has the
    // maximum cost in [begin, end).  Note that linear search is sufficiently
    // fast here because the size of the range is small.
    const Token* max_cost_token = begin->get();
    for (++begin; begin != end; ++begin) {
      if ((*begin)->cost > max_cost_token->cost) {
        max_cost_token = begin->get();
      }
    }

    // The cost is calculated as -log(prob) * 500.
    // We here assume that the wrong reading appear with 1/100 probability
    // of the original (correct) reading.
    constexpr int kCostPenalty = 2302;  // -log(1/100) * 500;
    auto token = std::make_unique<Token>();
    token->key.assign(value_key.second.data(), value_key.second.size());
    token->value = max_cost_token->value;
    token->lid = max_cost_token->lid;
    token->rid = max_cost_token->rid;
    token->cost = max_cost_token->cost + kCostPenalty;
    // We don't set SPELLING_CORRECTION. The entries in reading_correction
    // data are also stored in rewriter/correction_rewriter.cc.
    // reading_correction_rewriter annotates the spelling correction
    // notations.
    token->attributes = Token::NONE;
    tokens.push_back(std::move(token));
    ++reading_correction_size;
    if (--*limit <= 0) {
      break;
    }
  }
  LOG(INFO) << reading_correction_size << " tokens from "
            << reading_correction_filename;
  return tokens;
}

void TextDictionaryLoader::CollectTokens(std::vector<Token*>* res) const {
  DCHECK(res);
  res->reserve(res->size() + tokens_.size());
  for (const std::unique_ptr<Token>& token : tokens_) {
    res->push_back(token.get());
  }
}

std::unique_ptr<Token> TextDictionaryLoader::ParseTSVLine(
    absl::string_view line) const {
  const std::vector<absl::string_view> columns =
      absl::StrSplit(line, '\t', absl::SkipEmpty());
  return ParseTSV(columns);
}

std::unique_ptr<Token> TextDictionaryLoader::ParseTSV(
    absl::Span<const absl::string_view> columns) const {
  CHECK_LE(5, columns.size()) << "Lack of columns: " << columns.size();

  auto token = std::make_unique<Token>();

  // Parse key, lid, rid, cost, value.
  token->key = japanese_util::NormalizeVoicedSoundMark(columns[0]);
  CHECK(absl::SimpleAtoi(columns[1], &token->lid))
      << "Wrong lid: " << columns[1];
  CHECK(absl::SimpleAtoi(columns[2], &token->rid))
      << "Wrong rid: " << columns[2];
  CHECK(absl::SimpleAtoi(columns[3], &token->cost))
      << "Wrong cost: " << columns[3];
  token->value = japanese_util::NormalizeVoicedSoundMark(columns[4]);

  // Optionally, label (SPELLING_CORRECTION, ZIP_CODE, etc.) may be provided in
  // column 6.
  if (columns.size() > 5) {
    CHECK(RewriteSpecialToken(token.get(), columns[5]))
        << "Invalid label: " << columns[5];
  }
  return token;
}

}  // namespace dictionary
}  // namespace mozc

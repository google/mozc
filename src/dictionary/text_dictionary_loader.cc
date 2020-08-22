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

// NOTE(tabata): This code is used mainly by louds trie builder to build
// dictionary. Please check error handling, if you want to include
// this to run within client.

#include "dictionary/text_dictionary_loader.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/flags.h"
#include "base/iterator_adapter.h"
#include "base/logging.h"
#include "base/multifile.h"
#include "base/number_util.h"
#include "base/stl_util.h"
#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "absl/strings/string_view.h"

DEFINE_int32(tokens_reserve_size, 1400000,
             "Reserve the specified size of token buffer in advance.");

namespace mozc {
namespace dictionary {
namespace {

// Functor to sort a sequence of Tokens first by value and then by key.
struct OrderByValueThenByKey {
  bool operator()(const Token *l, const Token *r) const {
    const int comp = l->value.compare(r->value);
    return comp == 0 ? (l->key < r->key) : (comp < 0);
  }
};

// Provides a view of Token iterator as a pair of value and key.  Used to look
// up tokens from a sorted range of Token pointers using value and key.
struct AsValueAndKey
    : public AdapterBase<std::pair<absl::string_view, absl::string_view> > {
  value_type operator()(std::vector<Token *>::const_iterator iter) const {
    const Token *token = *iter;
    return std::pair<absl::string_view, absl::string_view>(token->value,
                                                           token->key);
  }
};

// Provides a view of Token iterator as a value string.  Used to look up tokens
// from a sorted range of Tokens using value.
struct AsValue : public AdapterBase<absl::string_view> {
  value_type operator()(std::vector<Token *>::const_iterator iter) const {
    return absl::string_view((*iter)->value);
  }
};

// Parses one line of reading correction file.  Since the result is returned as
// string views, |line| needs to outlive |value_key|.
void ParseReadingCorrectionTSV(
    const std::string &line,
    std::pair<absl::string_view, absl::string_view> *value_key) {
  // Format: value\terror\tcorrect
  SplitIterator<SingleDelimiter> iter(line, "\t");
  CHECK(!iter.Done());
  value_key->first = iter.Get();
  iter.Next();
  CHECK(!iter.Done());
  value_key->second = iter.Get();
}

// Helper function to parse an integer from a string.
inline bool SafeStrToInt(absl::string_view s, int *n) {
  uint32 u32 = 0;
  const bool ret = NumberUtil::SafeStrToUInt32(s, &u32);
  *n = u32;
  return ret;
}

// Helper functions to get const iterators.
inline std::vector<Token *>::const_iterator CBegin(
    const std::vector<Token *> &tokens) {
  return tokens.begin();
}

inline std::vector<Token *>::const_iterator CEnd(
    const std::vector<Token *> &tokens) {
  return tokens.end();
}

}  // namespace

TextDictionaryLoader::TextDictionaryLoader(const POSMatcher &pos_matcher)
    : zipcode_id_(pos_matcher.GetZipcodeId()),
      isolated_word_id_(pos_matcher.GetIsolatedWordId()) {}

TextDictionaryLoader::TextDictionaryLoader(uint16 zipcode_id,
                                           uint16 isolated_word_id)
    : zipcode_id_(zipcode_id), isolated_word_id_(isolated_word_id) {}

TextDictionaryLoader::~TextDictionaryLoader() { Clear(); }

bool TextDictionaryLoader::RewriteSpecialToken(Token *token,
                                               absl::string_view label) const {
  CHECK(token);
  if (label.empty()) {
    return true;
  }
  if (Util::StartsWith(label, "SPELLING_CORRECTION")) {
    token->attributes = Token::SPELLING_CORRECTION;
    return true;
  }
  if (Util::StartsWith(label, "ZIP_CODE")) {
    token->lid = zipcode_id_;
    token->rid = zipcode_id_;
    return true;
  }
  if (Util::StartsWith(label, "ENGLISH")) {
    // TODO(noriyukit): Might be better to use special POS for english words.
    token->lid = isolated_word_id_;
    token->rid = isolated_word_id_;
    return true;
  }
  LOG(ERROR) << "Unknown special label: " << label;
  return false;
}

void TextDictionaryLoader::Load(
    const std::string &dictionary_filename,
    const std::string &reading_correction_filename) {
  LoadWithLineLimit(dictionary_filename, reading_correction_filename, -1);
}

void TextDictionaryLoader::LoadWithLineLimit(
    const std::string &dictionary_filename,
    const std::string &reading_correction_filename, int limit) {
  Clear();

  // Roughly allocate buffers for Token pointers.
  if (limit < 0) {
    tokens_.reserve(FLAGS_tokens_reserve_size);
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
      Token *token = ParseTSVLine(line);
      if (token) {
        tokens_.push_back(token);
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
  std::sort(tokens_.begin(), tokens_.end(), OrderByValueThenByKey());

  std::vector<Token *> reading_correction_tokens;
  LoadReadingCorrectionTokens(reading_correction_filename, tokens_, &limit,
                              &reading_correction_tokens);
  const size_t tokens_size = tokens_.size();
  tokens_.resize(tokens_size + reading_correction_tokens.size());
  for (size_t i = 0; i < reading_correction_tokens.size(); ++i) {
    // |tokens_| takes the ownership of each allocated token.
    tokens_[tokens_size + i] = reading_correction_tokens[i];
  }
}

// Loads reading correction data into |tokens|.  The second argument is used to
// determine costs of reading correction tokens and must be sorted by
// OrderByValueThenByKey().  The output tokens are newly allocated and the
// caller is responsible to delete them.
void TextDictionaryLoader::LoadReadingCorrectionTokens(
    const std::string &reading_correction_filename,
    const std::vector<Token *> &ref_sorted_tokens, int *limit,
    std::vector<Token *> *tokens) {
  // Load reading correction entries.
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
    std::pair<absl::string_view, absl::string_view> value_key;
    ParseReadingCorrectionTSV(line, &value_key);

    // Filter the entry if this key value pair already exists in the system
    // dictionary.
    if (std::binary_search(
            MakeIteratorAdapter(ref_sorted_tokens.begin(), AsValueAndKey()),
            MakeIteratorAdapter(ref_sorted_tokens.end(), AsValueAndKey()),
            value_key)) {
      VLOG(1) << "System dictionary has the same key-value: " << line;
      continue;
    }

    // Since reading correction entries lack POS and cost, we recover those
    // fields from a token in the system dictionary that has the same value.
    // Since multple tokens may have the same value, from such tokens, we
    // select the one that has the maximum cost.
    typedef std::vector<Token *>::const_iterator TokenIterator;
    typedef IteratorAdapter<TokenIterator, AsValue> AsValueIterator;
    typedef std::pair<AsValueIterator, AsValueIterator> Range;
    Range range = std::equal_range(
        MakeIteratorAdapter(CBegin(ref_sorted_tokens), AsValue()),
        MakeIteratorAdapter(CEnd(ref_sorted_tokens), AsValue()),
        value_key.first);
    TokenIterator begin = range.first.base(), end = range.second.base();
    if (begin == end) {
      VLOG(1) << "Cannot find the value in system dicitonary - ignored:"
              << line;
      continue;
    }
    // Now [begin, end) contains all the tokens that have the same value as
    // this reading correction entry.  Next, find the token that has the
    // maximum cost in [begin, end).  Note that linear search is sufficiently
    // fast here because the size of the range is small.
    const Token *max_cost_token = *begin;
    for (++begin; begin != end; ++begin) {
      if ((*begin)->cost > max_cost_token->cost) {
        max_cost_token = *begin;
      }
    }

    // The cost is calculated as -log(prob) * 500.
    // We here assume that the wrong reading appear with 1/100 probability
    // of the original (correct) reading.
    const int kCostPenalty = 2302;  // -log(1/100) * 500;
    std::unique_ptr<Token> token(new Token);
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
    tokens->push_back(token.release());
    ++reading_correction_size;
    if (--*limit <= 0) {
      break;
    }
  }
  LOG(INFO) << reading_correction_size << " tokens from "
            << reading_correction_filename;
}

void TextDictionaryLoader::Clear() { STLDeleteElements(&tokens_); }

void TextDictionaryLoader::CollectTokens(std::vector<Token *> *res) const {
  DCHECK(res);
  res->reserve(res->size() + tokens_.size());
  res->insert(res->end(), tokens_.begin(), tokens_.end());
}

Token *TextDictionaryLoader::ParseTSVLine(absl::string_view line) const {
  std::vector<absl::string_view> columns;
  Util::SplitStringUsing(line, "\t", &columns);
  return ParseTSV(columns);
}

Token *TextDictionaryLoader::ParseTSV(
    const std::vector<absl::string_view> &columns) const {
  CHECK_LE(5, columns.size()) << "Lack of columns: " << columns.size();

  std::unique_ptr<Token> token(new Token);

  // Parse key, lid, rid, cost, value.
  Util::NormalizeVoicedSoundMark(columns[0], &token->key);
  CHECK(SafeStrToInt(columns[1], &token->lid)) << "Wrong lid: " << columns[1];
  CHECK(SafeStrToInt(columns[2], &token->rid)) << "Wrong rid: " << columns[2];
  CHECK(SafeStrToInt(columns[3], &token->cost)) << "Wrong cost: " << columns[3];
  Util::NormalizeVoicedSoundMark(columns[4], &token->value);

  // Optionally, label (SPELLING_CORRECTION, ZIP_CODE, etc.) may be provided in
  // column 6.
  if (columns.size() > 5) {
    CHECK(RewriteSpecialToken(token.get(), columns[5]))
        << "Invalid label: " << columns[5];
  }
  return token.release();
}

}  // namespace dictionary
}  // namespace mozc

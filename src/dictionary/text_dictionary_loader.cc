// Copyright 2010-2013, Google Inc.
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
#include <string>
#include <vector>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/flags.h"
#include "base/iterator_adapter.h"
#include "base/logging.h"
#include "base/multifile.h"
#include "base/number_util.h"
#include "base/stl_util.h"
#include "base/string_piece.h"
#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"

DEFINE_int32(tokens_reserve_size, 1400000,
             "Reserve the specified size of token buffer in advance.");

namespace mozc {
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
struct AsValueAndKey : public AdapterBase<pair<StringPiece, StringPiece> > {
  value_type operator()(vector<Token *>::const_iterator iter) const {
    const Token *token = *iter;
    return pair<StringPiece, StringPiece>(token->value, token->key);
  }
};

// Provides a view of Token iterator as a value string.  Used to look up tokens
// from a sorted range of Tokens using value.
struct AsValue : public AdapterBase<StringPiece> {
  value_type operator()(vector<Token *>::const_iterator iter) const {
    return StringPiece((*iter)->value);
  }
};

// Parses one line of reading correction file.  Since the result is returned as
// StringPieces, |line| needs to outlive |value_key|.
void ParseReadingCorrectionTSV(const string &line,
                               pair<StringPiece, StringPiece> *value_key) {
  // Format: value\terror\tcorrect
  SplitIterator<SingleDelimiter> iter(line, "\t");
  CHECK(!iter.Done());
  value_key->first = iter.Get();
  iter.Next();
  CHECK(!iter.Done());
  value_key->second = iter.Get();
}

// Helper function to parse an integer from a string.
inline bool SafeStrToInt(StringPiece s, int *n) {
  uint32 u32 = 0;
  const bool ret = NumberUtil::SafeStrToUInt32(s, &u32);
  *n = u32;
  return ret;
}

// Helper functions to get const iterators.
inline vector<Token *>::const_iterator CBegin(const vector<Token *> &tokens) {
  return tokens.begin();
}

inline vector<Token *>::const_iterator CEnd(const vector<Token *> &tokens) {
  return tokens.end();
}

}  // namespace

TextDictionaryLoader::TextDictionaryLoader(const POSMatcher &pos_matcher)
    : pos_matcher_(&pos_matcher) {
}

TextDictionaryLoader::~TextDictionaryLoader() {
  Clear();
}

bool TextDictionaryLoader::RewriteSpecialToken(Token *token,
                                               StringPiece label) {
  CHECK(token);
  if (label.empty()) {
    return true;
  }
  if (Util::StartsWith(label, "SPELLING_CORRECTION")) {
    token->attributes = Token::SPELLING_CORRECTION;
    return true;
  }
  if (Util::StartsWith(label, "ZIP_CODE")) {
    token->lid = pos_matcher_->GetZipcodeId();
    token->rid = pos_matcher_->GetZipcodeId();
    return true;
  }
  if (Util::StartsWith(label, "ENGLISH")) {
    // TODO(noriyukit): Might be better to use special POS for english words.
    token->lid = pos_matcher_->GetIsolatedWordId();
    token->rid = pos_matcher_->GetIsolatedWordId();
    return true;
  }
  LOG(ERROR) << "Unknown special label: " << label;
  return false;
}

void TextDictionaryLoader::Load(const string &dictionary_filename,
                                const string &reading_correction_filename) {
  LoadWithLineLimit(dictionary_filename, reading_correction_filename, -1);
}

void TextDictionaryLoader::LoadWithLineLimit(
    const string &dictionary_filename,
    const string &reading_correction_filename,
    int limit) {
  Clear();

  // Roughly allocate buffers for Token pointers.
  if (limit < 0) {
    tokens_.reserve(FLAGS_tokens_reserve_size);
    limit = numeric_limits<int>::max();
  } else {
    tokens_.reserve(limit);
  }

  // Read system dictionary.
  {
    InputMultiFile file(dictionary_filename);
    string line;
    while (limit > 0 && file.ReadLine(&line)) {
      Util::ChopReturns(&line);
      tokens_.push_back(ParseTSV(line));
      --limit;
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
  sort(tokens_.begin(), tokens_.end(), OrderByValueThenByKey());

  // Load reading correction entries.
  int reading_correction_size = 0;
  {
    InputMultiFile file(reading_correction_filename);
    string line;
    while (file.ReadLine(&line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }

      // Parse TSV line in a pair of value and key (Note: first element is value
      // and the second key).
      Util::ChopReturns(&line);
      pair<StringPiece, StringPiece> value_key;
      ParseReadingCorrectionTSV(line, &value_key);

      // Filter the entry if this key value pair already exists in the system
      // dictionary.
      if (binary_search(MakeIteratorAdapter(tokens_.begin(), AsValueAndKey()),
                        MakeIteratorAdapter(tokens_.end(), AsValueAndKey()),
                        value_key)) {
        VLOG(1) << "System dictionary has the same key-value: " << line;
        continue;
      }

      // Since reading correction entries lack POS and cost, we recover those
      // fields from a token in the system dictionary that has the same value.
      // Since multple tokens may have the same value, from such tokens, we
      // select the one that has the maximum cost.
      typedef vector<Token *>::const_iterator TokenIterator;
      typedef IteratorAdapter<TokenIterator, AsValue> AsValueIterator;
      typedef pair<AsValueIterator, AsValueIterator> Range;
      Range range = equal_range(MakeIteratorAdapter(CBegin(tokens_), AsValue()),
                                MakeIteratorAdapter(CEnd(tokens_), AsValue()),
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
      const int kCostPenalty = 2302;      // -log(1/100) * 500;
      scoped_ptr<Token> token(new Token);
      value_key.second.CopyToString(&token->key);
      token->value = max_cost_token->value;
      token->lid = max_cost_token->lid;
      token->rid = max_cost_token->rid;
      token->cost = max_cost_token->cost + kCostPenalty;
      // We don't set SPELLING_CORRECTION. The entries in reading_correction
      // data are also stored in rewriter/correction_rewriter.cc.
      // reading_correction_rewriter annotates the spelling correction
      // notations.
      token->attributes = Token::NONE;
      tokens_.push_back(token.release());
      ++reading_correction_size;
      if (--limit <= 0) {
        break;
      }
    }
    LOG(INFO) << reading_correction_size << " tokens from "
              << reading_correction_filename;
  }
}

void TextDictionaryLoader::Clear() {
  STLDeleteElements(&tokens_);
}

void TextDictionaryLoader::CollectTokens(vector<Token *> *res) {
  DCHECK(res);
  res->reserve(res->size() + tokens_.size());
  res->insert(res->end(), tokens_.begin(), tokens_.end());
}

Token *TextDictionaryLoader::ParseTSV(const string &line) {
  scoped_ptr<Token> token(new Token);

  SplitIterator<SingleDelimiter> iter(line, "\t");
  CHECK(!iter.Done()) << "Malformed line: " << line;
  Util::NormalizeVoicedSoundMark(iter.Get(), &token->key);

  iter.Next();
  CHECK(!iter.Done()) << "Malformed line: " << line;
  CHECK(SafeStrToInt(iter.Get(), &token->lid))
      << "Wrong lid: " << iter.Get();

  iter.Next();
  CHECK(!iter.Done()) << "Malformed line: " << line;
  CHECK(SafeStrToInt(iter.Get(), &token->rid))
      << "Wrong rid: " << iter.Get();

  iter.Next();
  CHECK(!iter.Done()) << "Malformed line: " << line;
  CHECK(SafeStrToInt(iter.Get(), &token->cost))
      << "Wrong cost: " << iter.Get();

  iter.Next();
  CHECK(!iter.Done()) << "Malformed line: " << line;
  Util::NormalizeVoicedSoundMark(iter.Get(), &token->value);

  iter.Next();
  const StringPiece label = iter.Done() ? StringPiece() : iter.Get();
  CHECK(RewriteSpecialToken(token.get(), label))
      << "Invalid label: " << line;

  return token.release();
}

}  // namespace mozc

// Copyright 2010-2012, Google Inc.
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

#include "rewriter/single_kanji_rewriter.h"

#include <algorithm>
#include <string>
#include <vector>
#include <set>

#include "base/base.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/embedded_dictionary.h"
#include "rewriter/rewriter_interface.h"
#include "session/commands.pb.h"
#include "session/request_handler.h"

namespace mozc {

namespace {

struct SingleKanjiList {
  // reading, single_kanji_s
  // { "あ", "亜阿有..." }
  const char *key;
  const char *values;
};

struct KanjiVariantItem {
  // target, original, type_id
  // { "亞", "亜", 0 } ,
  const char *target;
  const char *original;
  int type_id;
};

#include "rewriter/single_kanji_rewriter_data.h"

// Since NounPrefixDictionary is just a tentative workaround,
// we copy the SingleKanji structure so that we can remove this workaround
// easily. Also, the logic of NounPrefix insertion is put independently from
// the single kanji dictionary. Ideally, we want to regenerate our
// language model for fixing noun-prefix issue.
class NounPrefixDictionary {
 public:
  NounPrefixDictionary()
      : dic_(new EmbeddedDictionary(kNounPrefixData_token_data,
                                    kNounPrefixData_token_size)) {}

  ~NounPrefixDictionary() {}

  EmbeddedDictionary *GetDictionary() const {
    return dic_.get();
  }

 private:
  scoped_ptr<EmbeddedDictionary> dic_;
};

struct SingleKanjiListCompare {
  bool operator() (const SingleKanjiList &lhs, const SingleKanjiList &rhs) {
    return (strcmp(lhs.key, rhs.key) < 0);
  }
};

// Lookup SingleKanjiList from key (reading).
// Returns false if not found.
bool LookupKanjiList(const string &key, vector<string> *kanji_list) {
  DCHECK(kanji_list);
  SingleKanjiList key_item;
  key_item.key = key.c_str();
  const SingleKanjiList *result =
      lower_bound(kSingleKanjis, kSingleKanjis + arraysize(kSingleKanjis),
                  key_item, SingleKanjiListCompare());
  if (result == (kSingleKanjis + arraysize(kSingleKanjis)) ||
      key.compare(result->key) != 0) {
    return false;
  }
  Util::SplitStringToUtf8Chars(result->values, kanji_list);
  return true;
}

struct KanjiVariantItemCompare {
  bool operator() (const KanjiVariantItem &lhs, const KanjiVariantItem &rhs) {
    return (strcmp(lhs.target, rhs.target) < 0);
  }
};

// Generates kanji variant description from key.
// Does nothing if not found.
void GenerateDescription(const string &key, string *desc) {
  DCHECK(desc);
  KanjiVariantItem key_item;
  key_item.target = key.c_str();
  const KanjiVariantItem *result =
      lower_bound(kKanjiVariants, kKanjiVariants + arraysize(kKanjiVariants),
                  key_item, KanjiVariantItemCompare());
  if (result == (kKanjiVariants + arraysize(kKanjiVariants)) ||
      key.compare(result->target) != 0) {
    return;
  }
  DCHECK_LT(result->type_id, arraysize(kKanjiVariantTypes));
  desc->assign(Util::StringPrintf(
      // "%sの%s"
      "%s\xe3\x81\xae%s",
      result->original, kKanjiVariantTypes[result->type_id]));
}

// Add single kanji variants description to existing candidates,
// because if we have candidates with same value, the lower ranked candidate
// will be removed.
void AddDescriptionForExsistingCandidates(Segment *segment) {
  DCHECK(segment);
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    Segment::Candidate *cand = segment->mutable_candidate(i);
    if (!cand->description.empty()) {
      continue;
    }
    GenerateDescription(cand->value, &cand->description);
  }
}

void FillCandidate(const string &key, const string &value,
                   int cost, uint16 single_kanji_id,
                   Segment::Candidate *cand) {
  cand->lid = single_kanji_id;
  cand->rid = single_kanji_id;
  cand->cost = cost;
  cand->content_key = key;
  cand->content_value = value;
  cand->key = key;
  cand->value = value;
  cand->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
  cand->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  GenerateDescription(value, &cand->description);
}

// Insert SingleKanji into segment.
void InsertCandidate(bool is_single_segment,
                     uint16 single_kanji_id,
                     const vector<string> &kanji_list,
                     Segment *segment) {
  DCHECK(segment);
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return;
  }

  const string &candidate_key = ((!segment->key().empty()) ?
                                 segment->key() :
                                 segment->candidate(0).key);

  // Adding 8000 to the single kanji cost
  // Note that this cost does not make no effect.
  // Here we set the cost just in case.
  const int kOffsetCost = 8000;

  // Append single-kanji
  for (size_t i = 0; i < kanji_list.size(); ++i) {
    Segment::Candidate *c = segment->push_back_candidate();
    FillCandidate(candidate_key, kanji_list[i],
                  kOffsetCost + i, single_kanji_id, c);
  }
}

// Insert Noun prefix into segment.
void InsertNounPrefix(const POSMatcher &pos_matcher,
                      Segment *segment,
                      const EmbeddedDictionary::Value *dict_values,
                      size_t dict_values_size) {
  DCHECK(dict_values);
  DCHECK_GT(dict_values_size, 0);

  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return;
  }

  if (segment->segment_type() == Segment::FIXED_VALUE) {
    return;
  }

  const string &candidate_key = ((!segment->key().empty()) ?
                                 segment->key() :
                                 segment->candidate(0).key);
  for (int i = 0; i < dict_values_size; ++i) {
    const int insert_pos = min(
        static_cast<int>(segment->candidates_size()),
        static_cast<int>(dict_values[i].cost +
                         (segment->candidate(0).attributes &
                          Segment::Candidate::CONTEXT_SENSITIVE) ? 1 : 0));
    Segment::Candidate *c = segment->insert_candidate(insert_pos);
    c->lid = pos_matcher.GetNounPrefixId();
    c->rid = pos_matcher.GetNounPrefixId();
    c->cost = 5000;
    c->content_value = dict_values[i].value;
    c->key = candidate_key;
    c->content_key = candidate_key;
    c->value = dict_values[i].value;
    c->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  }
}
}  // namespace

SingleKanjiRewriter::SingleKanjiRewriter(const POSMatcher &pos_matcher)
    : pos_matcher_(&pos_matcher) {}

SingleKanjiRewriter::~SingleKanjiRewriter() {}

int SingleKanjiRewriter::capability() const {
  if (GET_REQUEST(mixed_conversion)) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool SingleKanjiRewriter::Rewrite(const ConversionRequest &request,
                                  Segments *segments) const {
  if (!GET_CONFIG(use_single_kanji_conversion)) {
    VLOG(2) << "no use_single_kanji_conversion";
    return false;
  }

  bool modified = false;
  const size_t segments_size = segments->conversion_segments_size();
  const bool is_single_segment = (segments_size == 1);
  for (size_t i = 0; i < segments_size; ++i) {
    AddDescriptionForExsistingCandidates(
        segments->mutable_conversion_segment(i));

    const string &key = segments->conversion_segment(i).key();
    vector<string> kanji_list;
    if (!LookupKanjiList(key, &kanji_list)) {
      continue;
    }
    InsertCandidate(is_single_segment,
                    pos_matcher_->GetGeneralSymbolId(),
                    kanji_list,
                    segments->mutable_conversion_segment(i));

    modified = true;
  }

  // Tweak for noun prefix.
  // TODO(team): Ideally, this issue can be fixed via the language model
  // and dictionary generation.
  for (size_t i = 0; i < segments_size; ++i) {
    if (segments->conversion_segment(i).candidates_size() == 0) {
      continue;
    }

    if (i + 1 < segments_size) {
      const Segment::Candidate &right_candidate =
          segments->conversion_segment(i + 1).candidate(0);
      // right segment must be a noun.
      if (!pos_matcher_->IsContentNoun(right_candidate.lid)) {
        continue;
      }
    } else if (segments_size != 1) {  // also apply if segments_size == 1.
      continue;
    }

    const string &key = segments->conversion_segment(i).key();
    const EmbeddedDictionary::Token *token =
        Singleton<NounPrefixDictionary>::get()->GetDictionary()->Lookup(key);
    if (token == NULL) {
      continue;
    }
    InsertNounPrefix(*pos_matcher_,
                     segments->mutable_conversion_segment(i),
                     token->value, token->value_size);
    // Ignore the next noun content word.
    ++i;
    modified = true;
  }

  return modified;
}
}  // namespace mozc

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

#include "rewriter/user_segment_history_rewriter.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/compiler_specific.h"
#include "base/config_file_stream.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "config/character_form_manager.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/variants_rewriter.h"
#include "storage/lru_storage.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"

namespace mozc {

using config::CharacterFormManager;
using config::Config;
using storage::LRUStorage;

namespace {
const uint32 kValueSize = 4;
const uint32 kLRUSize   = 20000;
const uint32 kSeedValue = 0xf28defe3;
const uint32 kMaxCandidatesSize = 255;

const char kFileName[] = "user://segment.db";

LRUStorage *g_lru_storage = NULL;

// Temporarily disable unused private field warning against
// FeatureValue::reserved_ from Clang.
MOZC_CLANG_PUSH_WARNING();
MOZC_CLANG_DISABLE_WARNING(unused-private-field);
class FeatureValue {
 public:
  FeatureValue() : feature_type_(1), reserved_(0) {}
  bool IsValid() const {
    return (feature_type_ == 1);
  }

 private:
  uint32 feature_type_ : 1;   // always 1
  uint32 reserved_     : 31;  // this area is reserved for future
};
MOZC_CLANG_POP_WARNING();

bool IsPunctuationInternal(const string &str) {
  // return (str == "。" || str == "｡" ||
  // str == "、" || str == "､" ||
  // str == "，" || str == "," ||
  // str == "．"  || str == ".");
  return (str == "\xE3\x80\x82" || str == "\xEF\xBD\xA1" ||
          str == "\xE3\x80\x81" || str == "\xEF\xBD\xA4" ||
          str == "\xEF\xBC\x8C" || str == "," ||
          str == "\xEF\xBC\x8E"  || str == ".");
}

// Temporarily disable unused private field warning against
// KeyTriggerValue::reserved_ from Clang.
MOZC_CLANG_PUSH_WARNING();
MOZC_CLANG_DISABLE_WARNING(unused-private-field);
class KeyTriggerValue {
 public:
  KeyTriggerValue()
      : feature_type_(0), reserved_(0), candidates_size_(0) {}

  bool IsValid() const {
    return (feature_type_ == 0);
  }

  uint32 candidates_size() const {
    return candidates_size_;
  }

  void set_candidates_size(uint32 size) {
    candidates_size_ = min(size, kMaxCandidatesSize);
  }

 private:
  uint32 feature_type_    : 1;   // always 0
  uint32 reserved_        : 23;  // this area is reserved for future
  // want to encode POS, freq etc.
  uint32 candidates_size_ : 8;   // candidate size
};
MOZC_CLANG_POP_WARNING();

class ScoreTypeCompare {
 public:
  bool operator() (const UserSegmentHistoryRewriter::ScoreType &a,
                   const UserSegmentHistoryRewriter::ScoreType &b) const {
    if (a.score != b.score) {
      return (a.score > b.score);
    }
    return (a.last_access_time > b.last_access_time);
  }
};

// return the first candiadte which has "BEST_CANDIDATE" attribute
inline int GetDefaultCandidateIndex(const Segment &segment) {
  const int size = min(static_cast<int>(segment.candidates_size()), 5);
  for (int i = 0; i < size; ++i) {
    if (segment.candidate(i).attributes &
        Segment::Candidate::BEST_CANDIDATE) {
      return i;
    }
  }

  LOG(WARNING) << "Cannot find default candidate. "
               << "key: " << segment.key() << ", "
               << "candidates_size: " << segment.candidates_size();
  return 0;
}

// Feature "Left Right"
inline bool GetFeatureLR(const Segments &segments, size_t i,
                         const string &base_key,
                         const string &base_value, string *value) {
  DCHECK(value);
  if (i + 1 >= segments.segments_size() || i <= 0) {
    return false;
  }
  const int j1 = GetDefaultCandidateIndex(segments.segment(i - 1));
  const int j2 = GetDefaultCandidateIndex(segments.segment(i + 1));
  *value = string("LR") + '\t' + base_key + '\t' +
      segments.segment(i - 1).candidate(j1).value + '\t' +
      base_value + '\t' +
      segments.segment(i + 1).candidate(j2).value;
  return true;
}

// Feature "Left Left"
inline bool GetFeatureLL(const Segments &segments, size_t i,
                         const string &base_key,
                         const string &base_value, string *value) {
  DCHECK(value);
  if (i < 2) {
    return false;
  }
  const int j1 = GetDefaultCandidateIndex(segments.segment(i - 2));
  const int j2 = GetDefaultCandidateIndex(segments.segment(i - 1));
  *value = string("LL") + '\t' + base_key + '\t' +
      segments.segment(i - 2).candidate(j1).value + '\t' +
      segments.segment(i - 1).candidate(j2).value + '\t' +
      base_value;
  return true;
}

// Feature "Right Right"
inline bool GetFeatureRR(const Segments &segments, size_t i,
                         const string &base_key,
                         const string &base_value, string *value) {
  DCHECK(value);
  if (i + 2 >= segments.segments_size()) {
    return false;
  }
  const int j1 = GetDefaultCandidateIndex(segments.segment(i + 1));
  const int j2 = GetDefaultCandidateIndex(segments.segment(i + 2));
  *value = string("RR") + '\t' + base_key + '\t' +
      base_value + '\t' +
      segments.segment(i + 1).candidate(j1).value + '\t' +
      segments.segment(i + 2).candidate(j2).value;
  return true;
}

// Feature "Left"
inline bool GetFeatureL(const Segments &segments, size_t i,
                        const string &base_key,
                        const string &base_value, string *value) {
  DCHECK(value);
  if (i < 1) {
    return false;
  }
  const int j = GetDefaultCandidateIndex(segments.segment(i - 1));
  *value = string("L") + '\t' + base_key + '\t' +
      segments.segment(i - 1).candidate(j).value + '\t' +
      base_value;
  return true;
}

// Feature "Right"
inline bool GetFeatureR(const Segments &segments, size_t i,
                        const string &base_key,
                        const string &base_value, string *value) {
  DCHECK(value);
  if (i + 1 >= segments.segments_size()) {
    return false;
  }
  const int j = GetDefaultCandidateIndex(segments.segment(i + 1));
  *value = string("R") + '\t' + base_key + '\t' +
      base_value + '\t' +
      segments.segment(i + 1).candidate(j).value;
  return true;
}

// Feature "Current"
inline bool GetFeatureC(const Segments &segments, size_t i,
                        const string &base_key,
                        const string &base_value, string *value) {
  DCHECK(value);
  *value = string("C") + '\t' + base_key + '\t' + base_value;
  return true;
}

// Feature "Single"
inline bool GetFeatureS(const Segments &segments, size_t i,
                        const string &base_key,
                        const string &base_value, string *value) {
  DCHECK(value);
  if (segments.segments_size() - segments.history_segments_size() != 1) {
    return false;
  }
  *value = string("S") + '\t' + base_key + '\t' + base_value;
  return true;
}

// Feature "Number"
// used for number rewrite
inline bool GetFeatureN(uint16 type, string *value) {
  DCHECK(value);
  *value = string("N") + '\t' + NumberUtil::SimpleItoa(type);
  return true;
}

bool IsNumberSegment(const Segment &seg) {
  if (seg.key().empty()) {
    return false;
  }
  bool is_number = true;
  for (size_t i = 0; i < seg.key().size(); ++i) {
    if (!isdigit(static_cast<unsigned char>(seg.key()[i]))) {
      is_number = false;
      break;
    }
  }
  return is_number;
}

void GetValueByType(const Segment *segment,
                    NumberUtil::NumberString::Style style,
                    string *output) {
  DCHECK(output);
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    if (segment->candidate(i).style == style) {
      *output = segment->candidate(i).value;
      return;
    }
  }
  return;
}

// NormalizeCandidate using config
void NormalizeCandidate(const Segment *segment, int n,
                        string *normalized_value) {
  const Segment::Candidate &candidate = segment->candidate(n);

  // use "AS IS"
  if (candidate.attributes & Segment::Candidate::NO_VARIANTS_EXPANSION) {
    *normalized_value = candidate.value;
    return;
  }

  string result = candidate.value;
  switch (candidate.style) {
    case NumberUtil::NumberString::DEFAULT_STYLE:
      CharacterFormManager::GetCharacterFormManager()->
          ConvertConversionString(candidate.value, &result);
      break;
    case NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH:
    case NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH:
      // Convert separated arabic here and don't use character form manager
      // so that suppressing mixed form of candidates
      // ("1，234" etc.)
      // and the forms of separated arabics are learned in converter using
      // style.
      {
        const Config::CharacterForm form =
            CharacterFormManager::GetCharacterFormManager()->
            GetConversionCharacterForm("0");
        if (form == Config::FULL_WIDTH) {
          GetValueByType(
              segment,
              NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH,
              &result);
        } else if (form == Config::HALF_WIDTH) {
          GetValueByType(
              segment,
              NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH,
              &result);
        }
      }
      break;
    default:
      break;
  }
  *normalized_value = result;
}

// Gets the candidate index which has same value as given candidate.
// This function returns false if not found.
// When candidate is in meta candidate,
// set meta candidate index, (-index-1) to position.
bool GetSameValueCandidatePosition(const Segment *segment,
                                   const Segment::Candidate *candidate,
                                   int *position) {
  DCHECK(position);
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    if (segment->candidate(i).value == candidate->value) {
      *position = i;
      return true;
    }
  }
  for (size_t i = 0; i < segment->meta_candidates_size(); ++i) {
    if (segment->meta_candidate(i).value == candidate->value) {
      *position = (-static_cast<int>(i)-1);  // meta candidate index
      return true;
    }
  }
  return false;
}

bool IsT13NCandidate(const Segment::Candidate &cand) {
  // Regard the cand with 0-id as the transliterated candidate.
  return (cand.lid == 0 && cand.rid == 0);
}
}  // namespace

bool UserSegmentHistoryRewriter::SortCandidates(
    const vector<ScoreType> &sorted_scores, Segment *segment) const {
  const uint32 top_score = sorted_scores[0].score;
  const size_t kMaxRerankSize = min(sorted_scores.size(),
                                    static_cast<size_t>(5));
  const uint32 kScoreGap = 20;   // TODO(taku): no justification
  set<string> seen;

  size_t next_pos = 0;
  for (size_t n = 0; n < kMaxRerankSize; ++n) {
    // Move candidates when the score is close to the top score.
    if (kScoreGap < (top_score - sorted_scores[n].score)) {
      break;
    }
    const Segment::Candidate *candidate = sorted_scores[n].candidate;
    DCHECK(candidate);
    int old_position = 0;

    if (!GetSameValueCandidatePosition(segment, candidate, &old_position)) {
      LOG(ERROR) << "cannot find the candidate: " << sorted_scores[0].candidate;
      return false;
    }

    // We check character form here. If user prefers "half-width",
    // Mozc always provides half-width even when user input
    // full-width before.
    string normalized_value;
    NormalizeCandidate(segment, old_position, &normalized_value);

    if (normalized_value != candidate->value) {
      const Segment::Candidate *normalized_cand = NULL;
      for (size_t l = 0; l < segment->candidates_size(); ++l) {
        if (segment->candidate(l).value == normalized_value) {
          normalized_cand = &segment->candidate(l);
          break;
        }
      }

      if (normalized_cand != NULL) {
        if (seen.find(normalized_value) == seen.end()) {
          const int pos = segment->indexOf(normalized_cand);
          DCHECK(pos != segment->candidates_size());
          segment->move_candidate(pos, next_pos);
          ++next_pos;
          seen.insert(normalized_value);
        }
      } else {
        // If default character form is different and
        // is not found in the candidates, make a new
        // candidate and push it to the top.
        Segment::Candidate *new_candidate =
            segment->insert_candidate(next_pos);
        DCHECK(new_candidate);

        *new_candidate = *candidate;   // copy candidate
        new_candidate->value = normalized_value;
        CharacterFormManager::GetCharacterFormManager()->
            ConvertConversionString(candidate->content_value,
                                    &(new_candidate->content_value));
        // Update description so it matches candidate's current value.
        // This fix addresses Bug #3493644.
        // (Wrong character width annotation after learning alphabet)
        new_candidate->description.clear();
        VariantsRewriter::SetDescriptionForCandidate(*pos_matcher_,
                                                     new_candidate);
        ++next_pos;
        seen.insert(normalized_value);
      }
    } else {
      if (seen.find(candidate->value) == seen.end()) {
        segment->move_candidate(old_position, next_pos);
        ++next_pos;
        seen.insert(candidate->value);
      }
    }
  }
  return true;
}

UserSegmentHistoryRewriter::UserSegmentHistoryRewriter(
    const POSMatcher *pos_matcher,
    const PosGroup *pos_group)
    : storage_(new LRUStorage),
      pos_matcher_(pos_matcher),
      pos_group_(pos_group) {
  Reload();
  g_lru_storage = storage_.get();

  CHECK_EQ(sizeof(uint32), sizeof(FeatureValue));
  CHECK_EQ(sizeof(uint32), sizeof(KeyTriggerValue));
}

UserSegmentHistoryRewriter::~UserSegmentHistoryRewriter() {}

#define INSERT_FEATURE(func, base_key, base_value, force_insert) \
do { \
  if (func((segments), segment_index, base_key, base_value, &feature_key)) { \
    FeatureValue v; \
    DCHECK(v.IsValid()); \
    if (force_insert) { \
      storage_->Insert(feature_key, reinterpret_cast<const char *>(&v)); \
    } else { \
      storage_->TryInsert(feature_key, reinterpret_cast<const char *>(&v)); \
    } \
  } \
} while (0)

#define FETCH_FEATURE(func, base_key, base_value, weight)        \
do { \
  if (func(segments, segment_index, base_key, base_value, &feature_key)) { \
    const FeatureValue *v = \
      reinterpret_cast<const FeatureValue *> \
       (storage_->Lookup(feature_key, &last_access_time_result)); \
    if (v != NULL && v->IsValid()) { \
       *score = max(*score, weight);                                     \
       *last_access_time = max(*last_access_time, last_access_time_result); \
    } \
  } \
} while (0)

bool UserSegmentHistoryRewriter::GetScore(const Segments &segments,
                                          size_t segment_index,
                                          int candidate_index,
                                          uint32 *score,
                                          uint32 *last_access_time) const {
  const size_t segments_size = segments.conversion_segments_size();
  const Segment::Candidate &top_candidate =
      segments.segment(segment_index).candidate(0);
  const Segment::Candidate &candidate =
      segments.segment(segment_index).candidate(candidate_index);
  const string &all_value = candidate.value;
  const string &content_value = candidate.content_value;
  const string &all_key = segments.segment(segment_index).key();
  const string &content_key = candidate.content_key;
  // if the segments are resized by user OR
  // either top/target candidate has CONTEXT_SENSITIVE flags,
  // don't apply UNIGRAM model
  const bool context_sensitive =
      segments.resized() ||
      (candidate.attributes & Segment::Candidate::CONTEXT_SENSITIVE) ||
      (segments.segment(segment_index).candidate(0).attributes &
       Segment::Candidate::CONTEXT_SENSITIVE);
  DCHECK(score);
  DCHECK(last_access_time);

  *score = 0;
  *last_access_time = 0;

  // They are used inside FETCH_FEATURE
  uint32 last_access_time_result = 0;
  string feature_key;

  const uint32 trigram_score       = (segments_size == 3) ? 180 : 30;
  const uint32 bigram_score        = (segments_size == 2) ? 60  : 10;
  const uint32 bigram_number_score = (segments_size == 2) ? 50  : 8;
  const uint32 unigram_score       = (segments_size == 1) ? 36  : 6;
  const uint32 single_score        = (segments_size == 1) ? 90  : 15;

  FETCH_FEATURE(GetFeatureLR, all_key, all_value, trigram_score);
  FETCH_FEATURE(GetFeatureLL, all_key, all_value, trigram_score);
  FETCH_FEATURE(GetFeatureRR, all_key, all_value, trigram_score);
  FETCH_FEATURE(GetFeatureL,  all_key, all_value, bigram_score);
  FETCH_FEATURE(GetFeatureR,  all_key, all_value, bigram_score);
  FETCH_FEATURE(GetFeatureS,  all_key, all_value, single_score);
  FETCH_FEATURE(GetFeatureLN, content_key, content_value, bigram_number_score);
  FETCH_FEATURE(GetFeatureRN, content_key, content_value, bigram_number_score);

  const bool is_replaceable = Replaceable(top_candidate, candidate);

  if (!context_sensitive && is_replaceable) {
    FETCH_FEATURE(GetFeatureC,  all_key, all_value, unigram_score);
  }

  if (!is_replaceable) {
    return (*score > 0);
  }

  FETCH_FEATURE(GetFeatureLR, content_key, content_value, trigram_score / 2);
  FETCH_FEATURE(GetFeatureLL, content_key, content_value, trigram_score / 2);
  FETCH_FEATURE(GetFeatureRR, content_key, content_value, trigram_score / 2);
  FETCH_FEATURE(GetFeatureL,  content_key, content_value, bigram_score / 2);
  FETCH_FEATURE(GetFeatureR,  content_key, content_value, bigram_score / 2);
  FETCH_FEATURE(GetFeatureS,  content_key, content_value, single_score / 2);
  FETCH_FEATURE(GetFeatureLN, content_key,
                content_value, bigram_number_score / 2);
  FETCH_FEATURE(GetFeatureRN, content_key,
                content_value, bigram_number_score / 2);

  if (!context_sensitive) {
    FETCH_FEATURE(GetFeatureC,  content_key, content_value, unigram_score / 2);
  }

  return (*score > 0);
}

// Returns true if |lhs| candidate can be replaceable with |rhs|.
bool UserSegmentHistoryRewriter::Replaceable(
    const Segment::Candidate &lhs, const Segment::Candidate &rhs) const {
  const bool same_functional_value =
      (lhs.functional_value() == rhs.functional_value());
  const bool same_pos_group =
      (pos_group_->GetPosGroup(lhs.lid) == pos_group_->GetPosGroup(rhs.lid));
  return (same_functional_value &&
          (same_pos_group || IsT13NCandidate(lhs) || IsT13NCandidate(rhs)));
}


void UserSegmentHistoryRewriter::RememberNumberPreference(
    const Segment &segment) {
  const Segment::Candidate &candidate = segment.candidate(0);

  if ((candidate.style ==
       NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH) ||
      (candidate.style ==
       NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH)) {
    // in the case of:
    // 1. submit "123"
    // 2. submit "一二三"
    // 3. submit "１、２３４"
    // 4. type "123"
    // We want "１２３", not "一二三"
    // So learn default before learning separated
    // However, access time is count by second, so
    // separated and default is learned at same time
    // This problem is solved by workaround on lookup.
    string default_feature_key;
    GetFeatureN(NumberUtil::NumberString::DEFAULT_STYLE, &default_feature_key);
    FeatureValue v;
    DCHECK(v.IsValid());
    storage_->Insert(default_feature_key, reinterpret_cast<const char *>(&v));
  }

  string feature_key;
  GetFeatureN(candidate.style, &feature_key);
  FeatureValue v;
  DCHECK(v.IsValid());
  // Always insert for numbers
  storage_->Insert(feature_key, reinterpret_cast<const char *>(&v));
}

void UserSegmentHistoryRewriter::RememberFirstCandidate(
    const Segments &segments,
    size_t segment_index) {
  const Segment &seg = segments.segment(segment_index);
  if (seg.candidates_size() <= 1) {
    return;
  }

  const Segment::Candidate &candidate = seg.candidate(0);

  // http://b/issue?id=3156109
  // Do not remember the preference of Punctuations
  if (IsPunctuation(seg, candidate)) {
    return;
  }

  const bool context_sensitive = segments.resized() ||
      (candidate.attributes & Segment::Candidate::CONTEXT_SENSITIVE);
  const string &all_value = candidate.value;
  const string &content_value = candidate.content_value;
  const string &all_key = seg.key();
  const string &content_key = candidate.content_key;

  // even if the candiate was the top (default) candidate,
  // ERANKED will be set when user changes the ranking
  const bool force_insert =
      (candidate.attributes & Segment::Candidate::RERANKED);

  // Compare the POS group and Functional value.
  // if "is_replaceable" is true, it means that  the target candidate can
  // "SAFELY" be replaceable with the top candidate.
  const int top_index = GetDefaultCandidateIndex(seg);
  const bool is_replaceable_with_top =
      ((top_index == 0) || Replaceable(seg.candidate(top_index), candidate));

  // |feature_key| is used inside INSERT_FEATURE
  string feature_key;
  INSERT_FEATURE(GetFeatureLR, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureLL, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureRR, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureL,  all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureR,  all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureLN, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureRN, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureS,  all_key, all_value, force_insert);

  if (!context_sensitive && is_replaceable_with_top) {
    INSERT_FEATURE(GetFeatureC, all_key, all_value, force_insert);
  }

  // save content value
  if (all_value != content_value &&
      all_key != content_key &&
      is_replaceable_with_top) {
    INSERT_FEATURE(GetFeatureLR, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureLL, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureRR, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureL,  content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureR,  content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureLN, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureRN, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureS,  content_key, content_value, force_insert);
    if (!context_sensitive) {
      INSERT_FEATURE(GetFeatureC, content_key, content_value, force_insert);
    }
  }

  // learn CloseBracket when OpenBracket is fixed.
  string close_bracket_key;
  string close_bracket_value;
  if (Util::IsOpenBracket(content_key, &close_bracket_key) &&
      Util::IsOpenBracket(content_value, &close_bracket_value)) {
    INSERT_FEATURE(GetFeatureS, close_bracket_key,
                   close_bracket_value, force_insert);
    if (!context_sensitive) {
      INSERT_FEATURE(GetFeatureC, close_bracket_key,
                     close_bracket_value, force_insert);
    }
  }
}

bool UserSegmentHistoryRewriter::IsAvailable(const Segments &segments) const {
  if (GET_CONFIG(incognito_mode)) {
    VLOG(2) << "incognito_mode";
    return false;
  }

  if (!segments.user_history_enabled()) {
    VLOG(2) << "!user_history_enabled";
    return false;
  }

  if (storage_.get() == NULL) {
    VLOG(2) << "storage is NULL";
    return false;
  }

  // check that all segments have candidate
  for (size_t i = 0; i < segments.segments_size(); ++i) {
    if (segments.segment(i).candidates_size() == 0) {
      LOG(ERROR) << "candidate size is 0";
      return false;
    }
  }

  return true;
}

void UserSegmentHistoryRewriter::Finish(Segments *segments) {
  if (!IsAvailable(*segments)) {
    return;
  }

  if (GET_CONFIG(history_learning_level) != Config::DEFAULT_HISTORY) {
    VLOG(2) << "history_learning_level is not DEFAULT_HISTORY";
    return;
  }

  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    const Segment &segment = segments->segment(i);
    if (segment.candidates_size() <= 0 ||
        segment.segment_type() != Segment::FIXED_VALUE ||
        segment.candidate(0).attributes &
        Segment::Candidate::NO_HISTORY_LEARNING) {
      continue;
    }
    if (IsNumberSegment(segment)) {
      RememberNumberPreference(segment);
      continue;
    }
    InsertTriggerKey(segment);
    RememberFirstCandidate(*segments, i);
  }
  // update usage stats here
  usage_stats::UsageStats::SetInteger("UserSegmentHistoryEntrySize",
                                      static_cast<int>(storage_->used_size()));
}

bool UserSegmentHistoryRewriter::Reload() {
  const string filename = ConfigFileStream::GetFileName(kFileName);
  if (!storage_->OpenOrCreate(filename.c_str(),
                              kValueSize, kLRUSize, kSeedValue)) {
    LOG(WARNING) << "cannot initialize UserSegmentHistoryRewriter";
    storage_.reset(NULL);
    g_lru_storage = NULL;
    return false;
  }

  const char kFileSuffix[] = ".merge_pending";
  const string merge_pending_file = filename + kFileSuffix;

  // merge pending file does not always exist.
  if (Util::FileExists(merge_pending_file)) {
    storage_->Merge(merge_pending_file.c_str());
    Util::Unlink(merge_pending_file);
  }

  return true;
}

bool UserSegmentHistoryRewriter::ShouldRewrite(
    const Segment &segment,
    size_t *max_candidates_size) const {
  if (segment.candidates_size() == 0) {
    LOG(ERROR) << "candidate size is 0";
    return false;
  }

  DCHECK(storage_.get());
  const KeyTriggerValue *v1 =
      reinterpret_cast<const KeyTriggerValue *>
      (storage_->Lookup(segment.key()));

  const KeyTriggerValue *v2 = NULL;
  if (segment.key() != segment.candidate(0).content_key) {
    v2 = reinterpret_cast<const KeyTriggerValue *>
        (storage_->Lookup(segment.candidate(0).content_key));
  }

  const size_t v1_size = (v1 == NULL || !v1->IsValid()) ?
      0 : v1->candidates_size();
  const size_t v2_size = (v2 == NULL || !v2->IsValid()) ?
      0 : v2->candidates_size();

  *max_candidates_size = max(v1_size, v2_size);

  return *max_candidates_size > 0;
}

void UserSegmentHistoryRewriter::InsertTriggerKey(const Segment &segment) {
  if (!(segment.candidate(0).attributes & Segment::Candidate::RERANKED)) {
    VLOG(2) << "InsertTriggerKey is skipped";
    return;
  }

  DCHECK(storage_.get());

  KeyTriggerValue v;
  COMPILE_ASSERT(sizeof(uint32) == sizeof(v), check_sizeof_KeyTriggerValue);

  // TODO(taku): saving segment.candidate_size() might be too heavy and
  // increases the chance of hash collisions.
  v.set_candidates_size(segment.candidates_size());

  storage_->Insert(segment.key(), reinterpret_cast<const char *>(&v));
  if (segment.key() != segment.candidate(0).content_key) {
    storage_->Insert(segment.candidate(0).content_key,
                     reinterpret_cast<const char *>(&v));
  }

  string close_bracket_key;
  if (Util::IsOpenBracket(segment.key(), &close_bracket_key)) {
    storage_->Insert(close_bracket_key,
                     reinterpret_cast<const char *>(&v));
  }
}

bool UserSegmentHistoryRewriter::RewriteNumber(Segment *segment) const {
  vector<ScoreType> scores;
  for (size_t l = 0;
       l < segment->candidates_size() + segment->meta_candidates_size(); ++l) {
    int j = static_cast<int>(l);
    if (j >= static_cast<int>(segment->candidates_size())) {
      j -= static_cast<int>(segment->candidates_size() +
                            segment->meta_candidates_size());
    }
    uint32 score = 0;
    uint32 last_access_time = 0;
    string feature_key;
    GetFeatureN(segment->candidate(j).style, &feature_key);
    const FeatureValue *v =
        reinterpret_cast<const FeatureValue *>
        (storage_->Lookup(feature_key, &last_access_time));
    if (v != NULL && v->IsValid()) {
      score = 10;
      // Workaround for separated arabic.
      // Because separated arabic and normal number is learned at the
      // same time, make the time gap here so that separated arabic
      // has higher rank by sorting of scores.
      if (last_access_time > 0 &&
          (segment->candidate(j).style
           != NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH) &&
          (segment->candidate(j).style
           != NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH)) {
        last_access_time--;
      }
      scores.resize(scores.size() + 1);
      scores.back().score = score;
      scores.back().last_access_time = last_access_time;
      scores.back().candidate = segment->mutable_candidate(j);
    }
  }

  if (scores.empty()) {
    return false;
  }

  stable_sort(scores.begin(), scores.end(), ScoreTypeCompare());
  return SortCandidates(scores, segment);
}

bool UserSegmentHistoryRewriter::Rewrite(const ConversionRequest &request,
                                         Segments *segments) const {
  if (!IsAvailable(*segments)) {
    return false;
  }

  if (GET_CONFIG(history_learning_level) == Config::NO_HISTORY) {
    VLOG(2) << "history_learning_level is NO_HISTORY";
    return false;
  }

  // set BEST_CANDIDATE marker in advance
  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *segment = segments->mutable_segment(i);
    DCHECK(segment);
    DCHECK_GT(segment->candidates_size(), 0);
    segment->mutable_candidate(0)->attributes |=
        Segment::Candidate::BEST_CANDIDATE;
  }

  bool modified = false;
  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *segment = segments->mutable_segment(i);
    DCHECK(segment);
    DCHECK_GT(segment->candidates_size(), 0);

    if (segment->segment_type() == Segment::FIXED_VALUE) {
      continue;
    }

    if (IsPunctuation(*segment, segment->candidate(0))) {
      continue;
    }

    if (IsNumberSegment(*segment)) {
      modified |= RewriteNumber(segment);
      continue;
    }

    size_t max_candidates_size = 0;
    if (!ShouldRewrite(*segment, &max_candidates_size)) {
      continue;
    }

    if (segment->candidates_size() < max_candidates_size) {
      LOG(WARNING) << "cannot expand candidates. ignored."
                   << "rewrite may be failed ";
    }

    // for each all candidates expanded
    vector<ScoreType> scores;
    for (size_t l = 0;
         l < segment->candidates_size() + segment->meta_candidates_size();
         ++l) {
      int j = static_cast<int>(l);
      if (j >= static_cast<int>(segment->candidates_size())) {
        j -= static_cast<int>(segment->candidates_size() +
                              transliteration::NUM_T13N_TYPES);
      }

      uint32 score = 0;
      uint32 last_access_time = 0;
      if (GetScore(*segments, i, j, &score, &last_access_time)) {
        scores.resize(scores.size() + 1);
        scores.back().score = score;
        scores.back().last_access_time = last_access_time;
        scores.back().candidate = segment->mutable_candidate(j);
      }
    }

    if (scores.empty()) {
      continue;
    }

    stable_sort(scores.begin(), scores.end(), ScoreTypeCompare());
    modified |= SortCandidates(scores, segment);
  }
  return modified;
}

void UserSegmentHistoryRewriter::Clear() {
  if (storage_.get() != NULL) {
    VLOG(1) << "Clearing user segment data";
    storage_->Clear();
  }
}

// static
LRUStorage *UserSegmentHistoryRewriter::GetStorage() {
  return g_lru_storage;
}

bool UserSegmentHistoryRewriter::IsPunctuation(
    const Segment &seg,
    const Segment::Candidate &candidate) const {
  return (pos_matcher_->IsJapanesePunctuations(candidate.lid) &&
          candidate.lid == candidate.rid &&
          IsPunctuationInternal(seg.key()) &&
          IsPunctuationInternal(candidate.value));
}

// Feature "Left Number"
bool UserSegmentHistoryRewriter::GetFeatureLN(const Segments &segments,
                                              size_t i,
                                              const string &base_key,
                                              const string &base_value,
                                              string *value) const {
  DCHECK(value);
  if (i < 1) {
    return false;
  }
  const int j = GetDefaultCandidateIndex(segments.segment(i - 1));
  const Segment::Candidate &candidate = segments.segment(i - 1).candidate(j);
  if (pos_matcher_->IsNumber(candidate.rid) ||
      pos_matcher_->IsKanjiNumber(candidate.rid) ||
      Util::GetScriptType(candidate.value) == Util::NUMBER) {
    *value = string("LN") + '\t' + base_key + '\t' + base_value;
    return true;
  }
  return false;
}

// Feature "Right Number"
bool UserSegmentHistoryRewriter::GetFeatureRN(const Segments &segments,
                                              size_t i,
                                              const string &base_key,
                                              const string &base_value,
                                              string *value) const {
  DCHECK(value);
  if (i + 1 >= segments.segments_size()) {
    return false;
  }
  const int j = GetDefaultCandidateIndex(segments.segment(i + 1));
  const Segment::Candidate &candidate = segments.segment(i + 1).candidate(j);
  if (pos_matcher_->IsNumber(candidate.lid) ||
      pos_matcher_->IsKanjiNumber(candidate.lid) ||
      Util::GetScriptType(candidate.value) == Util::NUMBER) {
    *value = string("RN") + '\t' + base_key + '\t' + base_value;
    return true;
  }
  return false;
}

}  // namespace mozc

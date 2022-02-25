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

#include "rewriter/user_segment_history_rewriter.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/variants_rewriter.h"
#include "storage/lru_storage.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"
#include "absl/container/btree_set.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"

using mozc::config::CharacterFormManager;
using mozc::config::Config;
using mozc::dictionary::PosGroup;
using mozc::dictionary::PosMatcher;
using mozc::storage::LruStorage;

namespace mozc {
namespace {

constexpr uint32_t kValueSize = 4;
constexpr uint32_t kLruSize = 20000;
constexpr uint32_t kSeedValue = 0xf28defe3;
constexpr uint32_t kMaxCandidatesSize = 255;
// Size of candidates to be reranked to the top at one sorting operation.
// Note, if sorting operation is called twice, up to 10 (= 5 * 2) candidates
// could be reranked in total.
constexpr size_t kMaxRerankSize = 5;

constexpr char kFileName[] = "user://segment.db";

// Temporarily disable unused private field warning against
// FeatureValue::reserved_ from Clang.
// We use MOZC_CLANG_HAS_WARNING to check whether "-Wunused-private-field" is
// available, because XCode 4.4 clang (based on LLVM 3.1svn) doesn't have it.
MOZC_CLANG_PUSH_WARNING();
// clang-format off
#if MOZC_CLANG_HAS_WARNING(unused-private-field)
MOZC_CLANG_DISABLE_WARNING(unused-private-field);
#endif  // MOZC_CLANG_HAS_WARNING(unused-private-field)
// clang-format on
class FeatureValue {
 public:
  FeatureValue() : feature_type_(1), reserved_(0) {}
  bool IsValid() const { return (feature_type_ == 1); }

 private:
  uint32_t feature_type_ : 1;  // always 1
  uint32_t reserved_ : 31;     // this area is reserved for future
};
MOZC_CLANG_POP_WARNING();

bool IsPunctuationInternal(const std::string &str) {
  return (str == "。" || str == "｡" || str == "、" || str == "､" ||
          str == "，" || str == "," || str == "．" || str == ".");
}

// Temporarily disable unused private field warning against
// KeyTriggerValue::reserved_ from Clang.
// We use MOZC_CLANG_HAS_WARNING to check whether "-Wunused-private-field" is
// available, because XCode 4.4 clang (based on LLVM 3.1svn) doesn't have it.
MOZC_CLANG_PUSH_WARNING();
// clang-format off
#if MOZC_CLANG_HAS_WARNING(unused-private-field)
MOZC_CLANG_DISABLE_WARNING(unused-private-field);
#endif  // MOZC_CLANG_HAS_WARNING(unused-private-field)
// clang-format on
class KeyTriggerValue {
 public:
  KeyTriggerValue() : feature_type_(0), reserved_(0), candidates_size_(0) {}

  bool IsValid() const { return (feature_type_ == 0); }

  uint32_t candidates_size() const { return candidates_size_; }

  void set_candidates_size(uint32_t size) {
    candidates_size_ = std::min(size, kMaxCandidatesSize);
  }

 private:
  uint32_t feature_type_ : 1;  // always 0
  uint32_t reserved_ : 23;     // this area is reserved for future
  // want to encode POS, freq etc.
  uint32_t candidates_size_ : 8;  // candidate size
};
MOZC_CLANG_POP_WARNING();

class ScoreTypeCompare {
 public:
  bool operator()(const UserSegmentHistoryRewriter::ScoreType &a,
                  const UserSegmentHistoryRewriter::ScoreType &b) const {
    if (a.score != b.score) {
      return (a.score > b.score);
    }
    return (a.last_access_time > b.last_access_time);
  }
};

// return the first candidate which has "BEST_CANDIDATE" attribute
inline int GetDefaultCandidateIndex(const Segment &segment) {
  // Check up to kMaxRerankSize + 1 candidates because candidate with
  // BEST_CANDIDATE is highly possibly in that range (http://b/9992330).
  const int size = std::min<int>(segment.candidates_size(), kMaxRerankSize + 1);
  for (int i = 0; i < size; ++i) {
    if (segment.candidate(i).attributes & Segment::Candidate::BEST_CANDIDATE) {
      return i;
    }
  }

  LOG(WARNING) << "Cannot find default candidate. "
               << "key: " << segment.key() << ", "
               << "candidates_size: " << segment.candidates_size();
  return 0;
}

// JoinStringWithTabN joins N strings with TAB delimiters ('\t') in a way
// similar to absl::StrJoin() and/or Util::AppendStringWithDelimiter() but
// in a more efficient way. Since this module is called every key stroke and
// performs many string concatenation, we use these functions instead of ones
// from Util.
inline void JoinStringsWithTab2(const absl::string_view s1,
                                const absl::string_view s2,
                                std::string *output) {
  // Pre-allocate the buffer, including 1 TAB delimiter.
  output->reserve(s1.size() + s2.size() + 1);
  output->assign(s1.data(), s1.size())
      .append("\t")
      .append(s2.data(), s2.size());
}

inline void JoinStringsWithTab3(const absl::string_view s1,
                                const absl::string_view s2,
                                const absl::string_view s3,
                                std::string *output) {
  // Pre-allocate the buffer, including 2 TAB delimiters.
  output->reserve(s1.size() + s2.size() + s3.size() + 2);
  output->assign(s1.data(), s1.size())
      .append("\t")
      .append(s2.data(), s2.size())
      .append("\t")
      .append(s3.data(), s3.size());
}

inline void JoinStringsWithTab4(const absl::string_view s1,
                                const absl::string_view s2,
                                const absl::string_view s3,
                                const absl::string_view s4,
                                std::string *output) {
  // Pre-allocate the buffer, including 3 TAB delimiters.
  output->reserve(s1.size() + s2.size() + s3.size() + s4.size() + 3);
  output->assign(s1.data(), s1.size())
      .append("\t")
      .append(s2.data(), s2.size())
      .append("\t")
      .append(s3.data(), s3.size())
      .append("\t")
      .append(s4.data(), s4.size());
}

inline void JoinStringsWithTab5(const absl::string_view s1,
                                const absl::string_view s2,
                                const absl::string_view s3,
                                const absl::string_view s4,
                                const absl::string_view s5,
                                std::string *output) {
  // Pre-allocate the buffer, including 4 TAB delimiters.
  output->reserve(s1.size() + s2.size() + s3.size() + s4.size() + s5.size() +
                  4);
  output->assign(s1.data(), s1.size())
      .append("\t")
      .append(s2.data(), s2.size())
      .append("\t")
      .append(s3.data(), s3.size())
      .append("\t")
      .append(s4.data(), s4.size())
      .append("\t")
      .append(s5.data(), s5.size());
}

// Feature "Left Right"
inline bool GetFeatureLR(const Segments &segments, size_t i,
                         const std::string &base_key,
                         const std::string &base_value, std::string *value) {
  DCHECK(value);
  if (i + 1 >= segments.segments_size() || i <= 0) {
    return false;
  }
  const int j1 = GetDefaultCandidateIndex(segments.segment(i - 1));
  const int j2 = GetDefaultCandidateIndex(segments.segment(i + 1));
  JoinStringsWithTab5(absl::string_view("LR", 2), base_key,
                      segments.segment(i - 1).candidate(j1).value, base_value,
                      segments.segment(i + 1).candidate(j2).value, value);
  return true;
}

// Feature "Left Left"
inline bool GetFeatureLL(const Segments &segments, size_t i,
                         const std::string &base_key,
                         const std::string &base_value, std::string *value) {
  DCHECK(value);
  if (i < 2) {
    return false;
  }
  const int j1 = GetDefaultCandidateIndex(segments.segment(i - 2));
  const int j2 = GetDefaultCandidateIndex(segments.segment(i - 1));
  JoinStringsWithTab5(absl::string_view("LL", 2), base_key,
                      segments.segment(i - 2).candidate(j1).value,
                      segments.segment(i - 1).candidate(j2).value, base_value,
                      value);
  return true;
}

// Feature "Right Right"
inline bool GetFeatureRR(const Segments &segments, size_t i,
                         const std::string &base_key,
                         const std::string &base_value, std::string *value) {
  DCHECK(value);
  if (i + 2 >= segments.segments_size()) {
    return false;
  }
  const int j1 = GetDefaultCandidateIndex(segments.segment(i + 1));
  const int j2 = GetDefaultCandidateIndex(segments.segment(i + 2));
  JoinStringsWithTab5(absl::string_view("RR", 2), base_key, base_value,
                      segments.segment(i + 1).candidate(j1).value,
                      segments.segment(i + 2).candidate(j2).value, value);
  return true;
}

// Feature "Left"
inline bool GetFeatureL(const Segments &segments, size_t i,
                        const std::string &base_key,
                        const std::string &base_value, std::string *value) {
  DCHECK(value);
  if (i < 1) {
    return false;
  }
  const int j = GetDefaultCandidateIndex(segments.segment(i - 1));
  JoinStringsWithTab4(absl::string_view("L", 1), base_key,
                      segments.segment(i - 1).candidate(j).value, base_value,
                      value);
  return true;
}

// Feature "Right"
inline bool GetFeatureR(const Segments &segments, size_t i,
                        const std::string &base_key,
                        const std::string &base_value, std::string *value) {
  DCHECK(value);
  if (i + 1 >= segments.segments_size()) {
    return false;
  }
  const int j = GetDefaultCandidateIndex(segments.segment(i + 1));
  JoinStringsWithTab4(absl::string_view("R", 1), base_key, base_value,
                      segments.segment(i + 1).candidate(j).value, value);
  return true;
}

// Feature "Current"
inline bool GetFeatureC(const Segments &segments, size_t i,
                        const std::string &base_key,
                        const std::string &base_value, std::string *value) {
  DCHECK(value);
  JoinStringsWithTab3(absl::string_view("C", 1), base_key, base_value, value);
  return true;
}

// Feature "Single"
inline bool GetFeatureS(const Segments &segments, size_t i,
                        const std::string &base_key,
                        const std::string &base_value, std::string *value) {
  DCHECK(value);
  if (segments.segments_size() - segments.history_segments_size() != 1) {
    return false;
  }
  JoinStringsWithTab3(absl::string_view("S", 1), base_key, base_value, value);
  return true;
}

// Feature "Number"
// used for number rewrite
inline bool GetFeatureN(uint16_t type, std::string *value) {
  DCHECK(value);
  JoinStringsWithTab2("N", std::to_string(type), value);
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
                    std::string *output) {
  DCHECK(output);
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    if (segment->candidate(i).style == style) {
      *output = segment->candidate(i).value;
      return;
    }
  }
}

// NormalizeCandidate using config
void NormalizeCandidate(const Segment *segment, int n,
                        std::string *normalized_value) {
  const Segment::Candidate &candidate = segment->candidate(n);

  // use "AS IS"
  if (candidate.attributes & Segment::Candidate::NO_VARIANTS_EXPANSION) {
    *normalized_value = candidate.value;
    return;
  }

  std::string result = candidate.value;
  switch (candidate.style) {
    case NumberUtil::NumberString::DEFAULT_STYLE:
      CharacterFormManager::GetCharacterFormManager()->ConvertConversionString(
          candidate.value, &result);
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
            CharacterFormManager::GetCharacterFormManager()
                ->GetConversionCharacterForm("0");
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
      *position = (-static_cast<int>(i) - 1);  // meta candidate index
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
    const std::vector<ScoreType> &sorted_scores, Segment *segment) const {
  const uint32_t top_score = sorted_scores[0].score;
  const size_t size = std::min(sorted_scores.size(), kMaxRerankSize);
  constexpr uint32_t kScoreGap = 20;  // TODO(taku): no justification
  absl::btree_set<std::string> seen;

  size_t next_pos = 0;
  for (size_t n = 0; n < size; ++n) {
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
    std::string normalized_value;
    NormalizeCandidate(segment, old_position, &normalized_value);

    if (normalized_value != candidate->value) {
      const Segment::Candidate *normalized_cand = nullptr;
      int pos = segment->candidates_size();
      for (size_t l = 0; l < segment->candidates_size(); ++l) {
        if (segment->candidate(l).value == normalized_value) {
          normalized_cand = &segment->candidate(l);
          pos = l;
          break;
        }
      }

      if (normalized_cand != nullptr) {
        if (seen.find(normalized_value) == seen.end()) {
          DCHECK(pos != segment->candidates_size());
          segment->move_candidate(pos, next_pos);
          ++next_pos;
          seen.insert(normalized_value);
        }
      } else {
        // If default character form is different and
        // is not found in the candidates, make a new
        // candidate and push it to the top.
        Segment::Candidate *new_candidate = segment->insert_candidate(next_pos);
        DCHECK(new_candidate);

        *new_candidate = *candidate;  // copy candidate
        new_candidate->value = normalized_value;
        CharacterFormManager::GetCharacterFormManager()
            ->ConvertConversionString(candidate->content_value,
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
    const PosMatcher *pos_matcher, const PosGroup *pos_group)
    : storage_(new LruStorage),
      pos_matcher_(pos_matcher),
      pos_group_(pos_group) {
  Reload();

  CHECK_EQ(sizeof(uint32_t), sizeof(FeatureValue));
  CHECK_EQ(sizeof(uint32_t), sizeof(KeyTriggerValue));
}

UserSegmentHistoryRewriter::~UserSegmentHistoryRewriter() {}

#define INSERT_FEATURE(func, base_key, base_value, force_insert)               \
  do {                                                                         \
    if (func((segments), segment_index, base_key, base_value, &feature_key)) { \
      FeatureValue v;                                                          \
      DCHECK(v.IsValid());                                                     \
      if (force_insert) {                                                      \
        storage_->Insert(feature_key, reinterpret_cast<const char *>(&v));     \
      } else {                                                                 \
        storage_->TryInsert(feature_key, reinterpret_cast<const char *>(&v));  \
      }                                                                        \
    }                                                                          \
  } while (0)

#define FETCH_FEATURE(func, base_key, base_value, weight)                    \
  do {                                                                       \
    if (func(segments, segment_index, base_key, base_value, &feature_key)) { \
      const FeatureValue *v = reinterpret_cast<const FeatureValue *>(        \
          storage_->Lookup(feature_key, &last_access_time_result));          \
      if (v != NULL && v->IsValid()) {                                       \
        *score = std::max(*score, weight);                                   \
        *last_access_time =                                                  \
            std::max(*last_access_time, last_access_time_result);            \
      }                                                                      \
    }                                                                        \
  } while (0)

bool UserSegmentHistoryRewriter::GetScore(const Segments &segments,
                                          size_t segment_index,
                                          int candidate_index, uint32_t *score,
                                          uint32_t *last_access_time) const {
  const size_t segments_size = segments.conversion_segments_size();
  const Segment::Candidate &top_candidate =
      segments.segment(segment_index).candidate(0);
  const Segment::Candidate &candidate =
      segments.segment(segment_index).candidate(candidate_index);
  const std::string &all_value = candidate.value;
  const std::string &content_value = candidate.content_value;
  const std::string &all_key = segments.segment(segment_index).key();
  const std::string &content_key = candidate.content_key;
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
  uint32_t last_access_time_result = 0;
  std::string feature_key;

  const uint32_t trigram_score = (segments_size == 3) ? 180 : 30;
  const uint32_t bigram_score = (segments_size == 2) ? 60 : 10;
  const uint32_t bigram_number_score = (segments_size == 2) ? 50 : 8;
  const uint32_t unigram_score = (segments_size == 1) ? 36 : 6;
  const uint32_t single_score = (segments_size == 1) ? 90 : 15;

  FETCH_FEATURE(GetFeatureLR, all_key, all_value, trigram_score);
  FETCH_FEATURE(GetFeatureLL, all_key, all_value, trigram_score);
  FETCH_FEATURE(GetFeatureRR, all_key, all_value, trigram_score);
  FETCH_FEATURE(GetFeatureL, all_key, all_value, bigram_score);
  FETCH_FEATURE(GetFeatureR, all_key, all_value, bigram_score);
  FETCH_FEATURE(GetFeatureS, all_key, all_value, single_score);
  FETCH_FEATURE(GetFeatureLN, content_key, content_value, bigram_number_score);
  FETCH_FEATURE(GetFeatureRN, content_key, content_value, bigram_number_score);

  const bool is_replaceable = Replaceable(top_candidate, candidate);

  if (!context_sensitive && is_replaceable) {
    FETCH_FEATURE(GetFeatureC, all_key, all_value, unigram_score);
  }

  if (!is_replaceable) {
    return (*score > 0);
  }

  FETCH_FEATURE(GetFeatureLR, content_key, content_value, trigram_score / 2);
  FETCH_FEATURE(GetFeatureLL, content_key, content_value, trigram_score / 2);
  FETCH_FEATURE(GetFeatureRR, content_key, content_value, trigram_score / 2);
  FETCH_FEATURE(GetFeatureL, content_key, content_value, bigram_score / 2);
  FETCH_FEATURE(GetFeatureR, content_key, content_value, bigram_score / 2);
  FETCH_FEATURE(GetFeatureS, content_key, content_value, single_score / 2);
  FETCH_FEATURE(GetFeatureLN, content_key, content_value,
                bigram_number_score / 2);
  FETCH_FEATURE(GetFeatureRN, content_key, content_value,
                bigram_number_score / 2);

  if (!context_sensitive) {
    FETCH_FEATURE(GetFeatureC, content_key, content_value, unigram_score / 2);
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
    std::string default_feature_key;
    GetFeatureN(NumberUtil::NumberString::DEFAULT_STYLE, &default_feature_key);
    FeatureValue v;
    DCHECK(v.IsValid());
    storage_->Insert(default_feature_key, reinterpret_cast<const char *>(&v));
  }

  std::string feature_key;
  GetFeatureN(candidate.style, &feature_key);
  FeatureValue v;
  DCHECK(v.IsValid());
  // Always insert for numbers
  storage_->Insert(feature_key, reinterpret_cast<const char *>(&v));
}

void UserSegmentHistoryRewriter::RememberFirstCandidate(
    const Segments &segments, size_t segment_index) {
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

  const bool context_sensitive =
      segments.resized() ||
      (candidate.attributes & Segment::Candidate::CONTEXT_SENSITIVE);
  const std::string &all_value = candidate.value;
  const std::string &content_value = candidate.content_value;
  const std::string &all_key = seg.key();
  const std::string &content_key = candidate.content_key;

  // even if the candidate was the top (default) candidate,
  // ERANKED will be set when user changes the ranking
  const bool force_insert =
      ((candidate.attributes & Segment::Candidate::RERANKED) != 0);

  // Compare the POS group and Functional value.
  // if "is_replaceable" is true, it means that  the target candidate can
  // "SAFELY" be replaceable with the top candidate.
  const int top_index = GetDefaultCandidateIndex(seg);
  const bool is_replaceable_with_top =
      ((top_index == 0) || Replaceable(seg.candidate(top_index), candidate));

  // |feature_key| is used inside INSERT_FEATURE
  std::string feature_key;
  INSERT_FEATURE(GetFeatureLR, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureLL, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureRR, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureL, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureR, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureLN, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureRN, all_key, all_value, force_insert);
  INSERT_FEATURE(GetFeatureS, all_key, all_value, force_insert);

  if (!context_sensitive && is_replaceable_with_top) {
    INSERT_FEATURE(GetFeatureC, all_key, all_value, force_insert);
  }

  // save content value
  if (all_value != content_value && all_key != content_key &&
      is_replaceable_with_top) {
    INSERT_FEATURE(GetFeatureLR, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureLL, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureRR, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureL, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureR, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureLN, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureRN, content_key, content_value, force_insert);
    INSERT_FEATURE(GetFeatureS, content_key, content_value, force_insert);
    if (!context_sensitive) {
      INSERT_FEATURE(GetFeatureC, content_key, content_value, force_insert);
    }
  }

  // learn CloseBracket when OpenBracket is fixed.
  std::string close_bracket_key;
  std::string close_bracket_value;
  if (Util::IsOpenBracket(content_key, &close_bracket_key) &&
      Util::IsOpenBracket(content_value, &close_bracket_value)) {
    INSERT_FEATURE(GetFeatureS, close_bracket_key, close_bracket_value,
                   force_insert);
    if (!context_sensitive) {
      INSERT_FEATURE(GetFeatureC, close_bracket_key, close_bracket_value,
                     force_insert);
    }
  }
}

bool UserSegmentHistoryRewriter::IsAvailable(const ConversionRequest &request,
                                             const Segments &segments) const {
  if (request.config().incognito_mode()) {
    VLOG(2) << "incognito_mode";
    return false;
  }

  if (!segments.user_history_enabled()) {
    VLOG(2) << "!user_history_enabled";
    return false;
  }

  if (storage_ == nullptr) {
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

void UserSegmentHistoryRewriter::Finish(const ConversionRequest &request,
                                        Segments *segments) {
  if (segments->request_type() != Segments::CONVERSION) {
    return;
  }

  if (!IsAvailable(request, *segments)) {
    return;
  }

  if (request.config().history_learning_level() != Config::DEFAULT_HISTORY) {
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

bool UserSegmentHistoryRewriter::Sync() {
  if (storage_) {
    storage_->DeleteElementsUntouchedFor62Days();
  }
  return true;
}

bool UserSegmentHistoryRewriter::Reload() {
  const std::string filename = ConfigFileStream::GetFileName(kFileName);
  if (!storage_->OpenOrCreate(filename.c_str(), kValueSize, kLruSize,
                              kSeedValue)) {
    LOG(WARNING) << "cannot initialize UserSegmentHistoryRewriter";
    storage_.reset();
    return false;
  }

  constexpr char kFileSuffix[] = ".merge_pending";
  const std::string merge_pending_file = filename + kFileSuffix;

  // merge pending file does not always exist.
  if (absl::Status s = FileUtil::FileExists(merge_pending_file); s.ok()) {
    storage_->Merge(merge_pending_file.c_str());
    FileUtil::UnlinkOrLogError(merge_pending_file);
  } else if (!absl::IsNotFound(s)) {
    LOG(ERROR) << "Cannot check if " << merge_pending_file << " exists: " << s;
  }

  return true;
}

bool UserSegmentHistoryRewriter::ShouldRewrite(
    const Segment &segment, size_t *max_candidates_size) const {
  if (segment.candidates_size() == 0) {
    LOG(ERROR) << "candidate size is 0";
    return false;
  }

  DCHECK(storage_.get());
  const KeyTriggerValue *v1 = reinterpret_cast<const KeyTriggerValue *>(
      storage_->Lookup(segment.key()));

  const KeyTriggerValue *v2 = nullptr;
  if (segment.key() != segment.candidate(0).content_key) {
    v2 = reinterpret_cast<const KeyTriggerValue *>(
        storage_->Lookup(segment.candidate(0).content_key));
  }

  const size_t v1_size =
      (v1 == nullptr || !v1->IsValid()) ? 0 : v1->candidates_size();
  const size_t v2_size =
      (v2 == nullptr || !v2->IsValid()) ? 0 : v2->candidates_size();

  *max_candidates_size = std::max(v1_size, v2_size);

  return *max_candidates_size > 0;
}

void UserSegmentHistoryRewriter::InsertTriggerKey(const Segment &segment) {
  if (!(segment.candidate(0).attributes & Segment::Candidate::RERANKED)) {
    VLOG(2) << "InsertTriggerKey is skipped";
    return;
  }

  DCHECK(storage_.get());

  KeyTriggerValue v;
  static_assert(sizeof(uint32_t) == sizeof(v),
                "KeyTriggerValue must be 32-bit int size.");

  // TODO(taku): saving segment.candidate_size() might be too heavy and
  // increases the chance of hash collisions.
  v.set_candidates_size(segment.candidates_size());

  storage_->Insert(segment.key(), reinterpret_cast<const char *>(&v));
  if (segment.key() != segment.candidate(0).content_key) {
    storage_->Insert(segment.candidate(0).content_key,
                     reinterpret_cast<const char *>(&v));
  }

  std::string close_bracket_key;
  if (Util::IsOpenBracket(segment.key(), &close_bracket_key)) {
    storage_->Insert(close_bracket_key, reinterpret_cast<const char *>(&v));
  }
}

bool UserSegmentHistoryRewriter::RewriteNumber(Segment *segment) const {
  std::vector<ScoreType> scores;
  for (size_t l = 0;
       l < segment->candidates_size() + segment->meta_candidates_size(); ++l) {
    int j = static_cast<int>(l);
    if (j >= static_cast<int>(segment->candidates_size())) {
      j -= static_cast<int>(segment->candidates_size() +
                            segment->meta_candidates_size());
    }
    uint32_t score = 0;
    uint32_t last_access_time = 0;
    std::string feature_key;
    GetFeatureN(segment->candidate(j).style, &feature_key);
    const FeatureValue *v = reinterpret_cast<const FeatureValue *>(
        storage_->Lookup(feature_key, &last_access_time));
    if (v != nullptr && v->IsValid()) {
      score = 10;
      // Workaround for separated arabic.
      // Because separated arabic and normal number is learned at the
      // same time, make the time gap here so that separated arabic
      // has higher rank by sorting of scores.
      if (last_access_time > 0 &&
          (segment->candidate(j).style !=
           NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH) &&
          (segment->candidate(j).style !=
           NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH)) {
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

  std::stable_sort(scores.begin(), scores.end(), ScoreTypeCompare());
  return SortCandidates(scores, segment);
}

bool UserSegmentHistoryRewriter::Rewrite(const ConversionRequest &request,
                                         Segments *segments) const {
  if (!IsAvailable(request, *segments)) {
    return false;
  }

  if (request.config().history_learning_level() == Config::NO_HISTORY) {
    VLOG(2) << "history_learning_level is NO_HISTORY";
    return false;
  }

  // set BEST_CANDIDATE marker in advance
  for (size_t i = 0; i < segments->segments_size(); ++i) {
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

    DVLOG_IF(2, (segment->candidates_size() < max_candidates_size))
        << "Cannot expand candidates. ignored. Rewrite may be failed";

    // for each all candidates expanded
    std::vector<ScoreType> scores;
    for (size_t l = 0;
         l < segment->candidates_size() + segment->meta_candidates_size();
         ++l) {
      int j = static_cast<int>(l);
      if (j >= static_cast<int>(segment->candidates_size())) {
        j -= static_cast<int>(segment->candidates_size() +
                              transliteration::NUM_T13N_TYPES);
      }

      uint32_t score = 0;
      uint32_t last_access_time = 0;
      if (GetScore(*segments, i, j, &score, &last_access_time)) {
        scores.push_back(ScoreType());
        scores.back().score = score;
        scores.back().last_access_time = last_access_time;
        scores.back().candidate = segment->mutable_candidate(j);
      }
    }

    if (scores.empty()) {
      continue;
    }

    std::stable_sort(scores.begin(), scores.end(), ScoreTypeCompare());
    modified |= SortCandidates(scores, segment);
  }
  return modified;
}

void UserSegmentHistoryRewriter::Clear() {
  if (storage_ != nullptr) {
    VLOG(1) << "Clearing user segment data";
    storage_->Clear();
  }
}

bool UserSegmentHistoryRewriter::IsPunctuation(
    const Segment &seg, const Segment::Candidate &candidate) const {
  return (pos_matcher_->IsJapanesePunctuations(candidate.lid) &&
          candidate.lid == candidate.rid && IsPunctuationInternal(seg.key()) &&
          IsPunctuationInternal(candidate.value));
}

// Feature "Left Number"
bool UserSegmentHistoryRewriter::GetFeatureLN(const Segments &segments,
                                              size_t i,
                                              const std::string &base_key,
                                              const std::string &base_value,
                                              std::string *value) const {
  DCHECK(value);
  if (i < 1) {
    return false;
  }
  const int j = GetDefaultCandidateIndex(segments.segment(i - 1));
  const Segment::Candidate &candidate = segments.segment(i - 1).candidate(j);
  if (pos_matcher_->IsNumber(candidate.rid) ||
      pos_matcher_->IsKanjiNumber(candidate.rid) ||
      Util::GetScriptType(candidate.value) == Util::NUMBER) {
    JoinStringsWithTab3(absl::string_view("LN", 2), base_key, base_value,
                        value);
    return true;
  }
  return false;
}

// Feature "Right Number"
bool UserSegmentHistoryRewriter::GetFeatureRN(const Segments &segments,
                                              size_t i,
                                              const std::string &base_key,
                                              const std::string &base_value,
                                              std::string *value) const {
  DCHECK(value);
  if (i + 1 >= segments.segments_size()) {
    return false;
  }
  const int j = GetDefaultCandidateIndex(segments.segment(i + 1));
  const Segment::Candidate &candidate = segments.segment(i + 1).candidate(j);
  if (pos_matcher_->IsNumber(candidate.lid) ||
      pos_matcher_->IsKanjiNumber(candidate.lid) ||
      Util::GetScriptType(candidate.value) == Util::NUMBER) {
    JoinStringsWithTab3(absl::string_view("RN", 2), base_key, base_value,
                        value);
    return true;
  }
  return false;
}

}  // namespace mozc

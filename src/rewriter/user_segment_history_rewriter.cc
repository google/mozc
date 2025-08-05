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
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "base/number_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "config/character_form_manager.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/variants_rewriter.h"
#include "storage/lru_storage.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace {

using ::mozc::config::CharacterFormManager;
using ::mozc::config::Config;
using ::mozc::dictionary::PosGroup;
using ::mozc::dictionary::PosMatcher;
using ::mozc::storage::LruStorage;

constexpr uint32_t kValueSize = 4;
constexpr uint32_t kLruSize = 20000;
constexpr uint32_t kSeedValue = 0xf28defe3;
constexpr uint32_t kMaxCandidatesSize = 255;
// Size of candidates to be reranked to the top at one sorting operation.
// Note, if sorting operation is called twice, up to 10 (= 5 * 2) candidates
// could be reranked in total.
constexpr size_t kMaxRerankSize = 5;

constexpr char kFileName[] = "user://segment.db";

constexpr size_t kRevertCacheSize = 16;

bool IsNumberStyleLearningEnabled(const ConversionRequest &request) {
  // Enabled in mobile (software keyboard & hardware keyboard)
  return request.request().kana_modifier_insensitive_conversion();
}

bool UseInnerSegments(const ConversionRequest &request) {
  return request.request().mixed_conversion();
}

class FeatureValue {
 public:
  FeatureValue() : feature_type_(1), reserved_(0) {}
  bool IsValid() const { return (feature_type_ == 1); }

 private:
  uint32_t feature_type_ : 1;                // always 1
  [[maybe_unused]] uint32_t reserved_ : 31;  // this area is reserved for future
};

bool IsPunctuationInternal(absl::string_view str) {
  return (str == "。" || str == "｡" || str == "、" || str == "､" ||
          str == "，" || str == "," || str == "．" || str == ".");
}

class KeyTriggerValue {
 public:
  KeyTriggerValue() : feature_type_(0), reserved_(0), candidates_size_(0) {}

  bool IsValid() const { return (feature_type_ == 0); }

  uint32_t candidates_size() const { return candidates_size_; }

  void set_candidates_size(uint32_t size) {
    candidates_size_ = std::min(size, kMaxCandidatesSize);
  }

 private:
  uint32_t feature_type_ : 1;                // always 0
  [[maybe_unused]] uint32_t reserved_ : 23;  // this area is reserved for future
  // want to encode POS, freq etc.
  uint32_t candidates_size_ : 8;  // candidate size
};

// return the first candidate which has "BEST_CANDIDATE" attribute
inline int GetDefaultCandidateIndex(const Segment &segment) {
  // Check up to kMaxRerankSize + 1 candidates because candidate with
  // BEST_CANDIDATE is highly possibly in that range (http://b/9992330).
  const int size = std::min<int>(segment.candidates_size(), kMaxRerankSize + 1);
  for (int i = 0; i < size; ++i) {
    if (segment.candidate(i).attributes &
        converter::Candidate::BEST_CANDIDATE) {
      return i;
    }
  }
  MOZC_VLOG(2) << "Cannot find default candidate. " << "key: " << segment.key()
               << ", " << "candidates_size: " << segment.candidates_size();
  return 0;
}

template <typename... Strings>
std::string StrJoinWithTabs(const Strings &...strings) {
  // Make sure we use the string_view overload so the buffer is preallocated.
  return absl::StrJoin({static_cast<absl::string_view>(strings)...}, "\t");
}

class FeatureKey {
 public:
  FeatureKey(const Segments &segments, const PosMatcher &pos_matcher,
             size_t index)
      : segments_(segments), pos_matcher_(pos_matcher), index_(index) {}

  std::string LeftRight(absl::string_view base_key,
                        absl::string_view base_value) const;
  std::string LeftLeft(absl::string_view base_key,
                       absl::string_view base_value) const;
  std::string RightRight(absl::string_view base_key,
                         absl::string_view base_value) const;
  std::string Left(absl::string_view base_key,
                   absl::string_view base_value) const;
  std::string Right(absl::string_view base_key,
                    absl::string_view base_value) const;
  std::string Current(absl::string_view base_key,
                      absl::string_view base_value) const;
  std::string Single(absl::string_view base_key,
                     absl::string_view base_value) const;
  std::string LeftNumber(absl::string_view base_key,
                         absl::string_view base_value) const;
  std::string RightNumber(absl::string_view base_key,
                          absl::string_view base_value) const;

  static std::string Number(uint16_t type);

 private:
  const Segments &segments_;
  const PosMatcher &pos_matcher_;
  const size_t index_;
};

// Feature "Left Right"
std::string FeatureKey::LeftRight(absl::string_view base_key,
                                  absl::string_view base_value) const {
  if (index_ + 1 >= segments_.segments_size() || index_ <= 0) {
    return "";
  }
  const int j1 = GetDefaultCandidateIndex(segments_.segment(index_ - 1));
  const int j2 = GetDefaultCandidateIndex(segments_.segment(index_ + 1));
  return StrJoinWithTabs(
      "LR", base_key, segments_.segment(index_ - 1).candidate(j1).value,
      base_value, segments_.segment(index_ + 1).candidate(j2).value);
}

// Feature "Left Left"
std::string FeatureKey::LeftLeft(absl::string_view base_key,
                                 absl::string_view base_value) const {
  if (index_ < 2) {
    return "";
  }
  const int j1 = GetDefaultCandidateIndex(segments_.segment(index_ - 2));
  const int j2 = GetDefaultCandidateIndex(segments_.segment(index_ - 1));
  return StrJoinWithTabs(
      "LL", base_key, segments_.segment(index_ - 2).candidate(j1).value,
      segments_.segment(index_ - 1).candidate(j2).value, base_value);
}

// Feature "Right Right"
std::string FeatureKey::RightRight(absl::string_view base_key,
                                   absl::string_view base_value) const {
  if (index_ + 2 >= segments_.segments_size()) {
    return "";
  }
  const int j1 = GetDefaultCandidateIndex(segments_.segment(index_ + 1));
  const int j2 = GetDefaultCandidateIndex(segments_.segment(index_ + 2));
  return StrJoinWithTabs("RR", base_key, base_value,
                         segments_.segment(index_ + 1).candidate(j1).value,
                         segments_.segment(index_ + 2).candidate(j2).value);
}

// Feature "Left"
std::string FeatureKey::Left(absl::string_view base_key,
                             absl::string_view base_value) const {
  if (index_ < 1) {
    return "";
  }
  const int j = GetDefaultCandidateIndex(segments_.segment(index_ - 1));
  return StrJoinWithTabs("L", base_key,
                         segments_.segment(index_ - 1).candidate(j).value,
                         base_value);
}

// Feature "Right"
std::string FeatureKey::Right(absl::string_view base_key,
                              absl::string_view base_value) const {
  if (index_ + 1 >= segments_.segments_size()) {
    return "";
  }
  const int j = GetDefaultCandidateIndex(segments_.segment(index_ + 1));
  return StrJoinWithTabs("R", base_key, base_value,
                         segments_.segment(index_ + 1).candidate(j).value);
}

// Feature "Current"
std::string FeatureKey::Current(absl::string_view base_key,
                                absl::string_view base_value) const {
  return StrJoinWithTabs("C", base_key, base_value);
}

// Feature "Single"
std::string FeatureKey::Single(absl::string_view base_key,
                               absl::string_view base_value) const {
  if (segments_.conversion_segments_size() != 1) {
    return "";
  }
  return StrJoinWithTabs("S", base_key, base_value);
}

// Feature "Left Number"
std::string FeatureKey::LeftNumber(absl::string_view base_key,
                                   absl::string_view base_value) const {
  if (index_ < 1) {
    return "";
  }
  const int j = GetDefaultCandidateIndex(segments_.segment(index_ - 1));
  const converter::Candidate &candidate =
      segments_.segment(index_ - 1).candidate(j);
  if (pos_matcher_.IsNumber(candidate.rid) ||
      pos_matcher_.IsKanjiNumber(candidate.rid) ||
      Util::GetScriptType(candidate.value) == Util::NUMBER) {
    return StrJoinWithTabs("LN", base_key, base_value);
  }
  return "";
}

// Feature "Right Number"
std::string FeatureKey::RightNumber(absl::string_view base_key,
                                    absl::string_view base_value) const {
  if (index_ + 1 >= segments_.segments_size()) {
    return "";
  }
  const int j = GetDefaultCandidateIndex(segments_.segment(index_ + 1));
  const converter::Candidate &candidate =
      segments_.segment(index_ + 1).candidate(j);
  if (pos_matcher_.IsNumber(candidate.lid) ||
      pos_matcher_.IsKanjiNumber(candidate.lid) ||
      Util::GetScriptType(candidate.value) == Util::NUMBER) {
    return StrJoinWithTabs("RN", base_key, base_value);
  }
  return "";
}

// Feature "Number"
// used for number rewrite
std::string FeatureKey::Number(uint16_t type) {
  return StrJoinWithTabs("N", absl::StrCat(type));
}

bool IsNumberSegment(const Segment &segment) {
  if (segment.key().empty()) {
    return false;
  }
  bool is_number = true;
  for (size_t i = 0; i < segment.key().size(); ++i) {
    if (!isdigit(static_cast<unsigned char>(segment.key()[i]))) {
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
  for (const converter::Candidate *candidate : segment->candidates()) {
    if (candidate->style == style) {
      *output = candidate->value;
      return;
    }
  }
}

// NormalizeCandidate using config
void NormalizeCandidate(const Segment *segment, int n,
                        std::string *normalized_value) {
  const converter::Candidate &candidate = segment->candidate(n);

  // use "AS IS"
  if (candidate.attributes & converter::Candidate::NO_VARIANTS_EXPANSION) {
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
                                   const converter::Candidate *candidate,
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

bool IsT13NCandidate(const converter::Candidate &cand) {
  // The cand with 0-id can be the transliterated candidate.
  return (cand.lid == 0 && cand.rid == 0);
}

}  // namespace

bool UserSegmentHistoryRewriter::SortCandidates(
    absl::Span<const ScoreCandidate> sorted_scores, Segment *segment) const {
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
    const converter::Candidate *candidate = sorted_scores[n].candidate;
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
      const converter::Candidate *normalized_cand = nullptr;
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
        converter::Candidate *new_candidate =
            segment->insert_candidate(next_pos);
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
    const PosMatcher &pos_matcher, const PosGroup &pos_group)
    : storage_(std::make_unique<LruStorage>()),
      pos_matcher_(&pos_matcher),
      pos_group_(&pos_group),
      revert_cache_(kRevertCacheSize) {
  Reload();

  CHECK_EQ(sizeof(uint32_t), sizeof(FeatureValue));
  CHECK_EQ(sizeof(uint32_t), sizeof(KeyTriggerValue));
}

UserSegmentHistoryRewriter::Score UserSegmentHistoryRewriter::GetScore(
    const ConversionRequest &request, const Segments &segments,
    size_t segment_index, int candidate_index) const {
  const size_t segments_size = segments.conversion_segments_size();
  const converter::Candidate &top_candidate =
      segments.segment(segment_index).candidate(0);
  const converter::Candidate &candidate =
      segments.segment(segment_index).candidate(candidate_index);
  absl::string_view all_value = candidate.value;
  absl::string_view content_value = candidate.content_value;
  absl::string_view all_key = segments.segment(segment_index).key();
  absl::string_view content_key = candidate.content_key;
  // if the segments are resized by user OR
  // either top/target candidate has CONTEXT_SENSITIVE flags,
  // don't apply UNIGRAM model
  const bool context_sensitive =
      segments.resized() ||
      (candidate.attributes & converter::Candidate::CONTEXT_SENSITIVE) ||
      (segments.segment(segment_index).candidate(0).attributes &
       converter::Candidate::CONTEXT_SENSITIVE);

  const uint32_t trigram_weight = (segments_size == 3) ? 180 : 30;
  const uint32_t bigram_weight = (segments_size == 2) ? 60 : 10;
  const uint32_t bigram_number_weight = (segments_size == 2) ? 50 : 8;
  const uint32_t unigram_weight = (segments_size == 1) ? 36 : 6;
  const uint32_t single_weight = (segments_size == 1) ? 90 : 15;

  Score score = {0, 0};
  FeatureKey fkey(segments, *pos_matcher_, segment_index);
  score.Update(Fetch(fkey.LeftRight(all_key, all_value), trigram_weight));
  score.Update(Fetch(fkey.LeftLeft(all_key, all_value), trigram_weight));
  score.Update(Fetch(fkey.RightRight(all_key, all_value), trigram_weight));
  score.Update(Fetch(fkey.Left(all_key, all_value), bigram_weight));
  score.Update(Fetch(fkey.Right(all_key, all_value), bigram_weight));
  score.Update(Fetch(fkey.Single(all_key, all_value), single_weight));
  score.Update(
      Fetch(fkey.LeftNumber(content_key, content_value), bigram_number_weight));
  score.Update(Fetch(fkey.RightNumber(content_key, content_value),
                     bigram_number_weight));

  const bool is_replaceable = Replaceable(request, top_candidate, candidate);
  if (!context_sensitive && is_replaceable) {
    score.Update(Fetch(fkey.Current(all_key, all_value), unigram_weight));
  }

  if (!is_replaceable) {
    return score;
  }

  score.Update(
      Fetch(fkey.LeftRight(content_key, content_value), trigram_weight / 2));
  score.Update(
      Fetch(fkey.LeftLeft(content_key, content_value), trigram_weight / 2));
  score.Update(
      Fetch(fkey.RightRight(content_key, content_value), trigram_weight / 2));
  score.Update(Fetch(fkey.Left(content_key, content_value), bigram_weight / 2));
  score.Update(
      Fetch(fkey.Right(content_key, content_value), bigram_weight / 2));
  score.Update(
      Fetch(fkey.Single(content_key, content_value), single_weight / 2));
  score.Update(Fetch(fkey.LeftNumber(content_key, content_value),
                     bigram_number_weight / 2));
  score.Update(Fetch(fkey.RightNumber(content_key, content_value),
                     bigram_number_weight / 2));

  if (!context_sensitive) {
    score.Update(
        Fetch(fkey.Current(content_key, content_value), unigram_weight / 2));
  }

  return score;
}

// Returns true if |best_candidate| can be replaceable with |target_candidate|.
// Here, "best candidate" means the candidate from converter before applying
// personalization.
bool UserSegmentHistoryRewriter::Replaceable(
    const ConversionRequest &request,
    const converter::Candidate &best_candidate,
    const converter::Candidate &target_candidate) const {
  const bool same_functional_value = (best_candidate.functional_value() ==
                                      target_candidate.functional_value());
  const bool same_pos_group = (pos_group_->GetPosGroup(best_candidate.lid) ==
                               pos_group_->GetPosGroup(target_candidate.lid));
  return (same_functional_value &&
          (same_pos_group || IsT13NCandidate(best_candidate) ||
           IsT13NCandidate(target_candidate)));
}

void UserSegmentHistoryRewriter::RememberNumberPreference(
    const Segment &segment, std::vector<std::string> &revert_entries) {
  const converter::Candidate &candidate = segment.candidate(0);

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
    Insert(FeatureKey::Number(NumberUtil::NumberString::DEFAULT_STYLE), true,
           revert_entries);
  }

  // Always insert for numbers
  Insert(FeatureKey::Number(candidate.style), true, revert_entries);
}

void UserSegmentHistoryRewriter::RememberFirstCandidate(
    const ConversionRequest &request, const Segments &segments,
    size_t segment_index, std::vector<std::string> &revert_entries) {
  const Segment &seg = segments.segment(segment_index);
  const converter::Candidate &candidate = seg.candidate(0);

  // http://b/issue?id=3156109
  // Do not remember the preference of Punctuations
  if (IsPunctuation(seg, candidate)) {
    return;
  }

  const bool context_sensitive =
      segments.resized() ||
      (candidate.attributes & converter::Candidate::CONTEXT_SENSITIVE);
  absl::string_view all_value = candidate.value;
  absl::string_view content_value = candidate.content_value;
  absl::string_view all_key = seg.key();
  absl::string_view content_key = candidate.content_key;

  // even if the candidate was the top (default) candidate,
  // ERANKED will be set when user changes the ranking
  const bool force_insert =
      ((candidate.attributes & converter::Candidate::RERANKED) != 0);

  // Compare the POS group and Functional value.
  // if "is_replaceable_with_top" is true, it means that  the target candidate
  // can "SAFELY" be replaceable with the top candidate.
  const int top_index = GetDefaultCandidateIndex(seg);
  const bool is_replaceable_with_top =
      ((top_index == 0) ||
       Replaceable(request, seg.candidate(top_index), candidate));
  FeatureKey fkey(segments, *pos_matcher_, segment_index);
  Insert(fkey.LeftRight(all_key, all_value), force_insert, revert_entries);
  Insert(fkey.LeftLeft(all_key, all_value), force_insert, revert_entries);
  Insert(fkey.RightRight(all_key, all_value), force_insert, revert_entries);
  Insert(fkey.Left(all_key, all_value), force_insert, revert_entries);
  Insert(fkey.Right(all_key, all_value), force_insert, revert_entries);
  Insert(fkey.LeftNumber(all_key, all_value), force_insert, revert_entries);
  Insert(fkey.RightNumber(all_key, all_value), force_insert, revert_entries);
  Insert(fkey.Single(all_key, all_value), force_insert, revert_entries);

  if (!context_sensitive && is_replaceable_with_top) {
    Insert(fkey.Current(all_key, all_value), force_insert, revert_entries);
  }

  // save content value
  if (all_value != content_value && all_key != content_key &&
      is_replaceable_with_top) {
    Insert(fkey.LeftRight(content_key, content_value), force_insert,
           revert_entries);
    Insert(fkey.LeftLeft(content_key, content_value), force_insert,
           revert_entries);
    Insert(fkey.RightRight(content_key, content_value), force_insert,
           revert_entries);
    Insert(fkey.Left(content_key, content_value), force_insert, revert_entries);
    Insert(fkey.Right(content_key, content_value), force_insert,
           revert_entries);
    Insert(fkey.LeftNumber(content_key, content_value), force_insert,
           revert_entries);
    Insert(fkey.RightNumber(content_key, content_value), force_insert,
           revert_entries);
    Insert(fkey.Single(content_key, content_value), force_insert,
           revert_entries);
    if (!context_sensitive) {
      Insert(fkey.Current(content_key, content_value), force_insert,
             revert_entries);
    }
  }

  // learn CloseBracket when OpenBracket is fixed.
  absl::string_view close_bracket_key;
  absl::string_view close_bracket_value;
  if (Util::IsOpenBracket(content_key, &close_bracket_key) &&
      Util::IsOpenBracket(content_value, &close_bracket_value)) {
    Insert(fkey.Single(close_bracket_key, close_bracket_value), force_insert,
           revert_entries);
    if (!context_sensitive) {
      Insert(fkey.Current(close_bracket_key, close_bracket_value), force_insert,
             revert_entries);
    }
  }
}

bool UserSegmentHistoryRewriter::IsAvailable(const ConversionRequest &request,
                                             const Segments &segments) const {
  if (request.incognito_mode()) {
    MOZC_VLOG(2) << "incognito_mode";
    return false;
  }

  if (!request.enable_user_history_for_conversion()) {
    MOZC_VLOG(2) << "user history for conversion is disabled";
    return false;
  }

  if (storage_ == nullptr) {
    MOZC_VLOG(2) << "storage is NULL";
    return false;
  }

  // check that all segments have candidate
  for (const Segment &segment : segments) {
    if (segment.candidates_size() == 0) {
      LOG(ERROR) << "candidate size is 0";
      return false;
    }
  }

  return true;
}

// Returns segments for learning.
// Inner segments boundary will be expanded.
Segments UserSegmentHistoryRewriter::MakeLearningSegmentsFromInnerSegments(
    const ConversionRequest &request, const Segments &segments) {
  Segments ret;
  for (const Segment &segment : segments) {
    const converter::Candidate &candidate = segment.candidate(0);
    if (candidate.inner_segment_boundary.empty()) {
      // No inner segment info
      Segment *seg = ret.add_segment();
      *seg = segment;
      continue;
    }
    int index = 0;
    for (const auto &iter : candidate.inner_segments()) {
      absl::string_view key = iter.GetKey();
      Segment *seg = ret.add_segment();
      seg->set_segment_type(segment.segment_type());
      seg->set_key(key);
      seg->clear_candidates();

      converter::Candidate *cand = seg->add_candidate();
      cand->attributes = candidate.attributes;
      cand->key = key;
      cand->content_key = iter.GetContentKey();
      cand->value = iter.GetValue();
      cand->content_value = iter.GetContentValue();
      // Fill IDs for the first and last inner segment.
      if (index == 0) {
        cand->lid = candidate.lid;
        cand->rid = candidate.lid;
      } else if (index == candidate.inner_segments().size() - 1) {
        cand->lid = candidate.rid;
        cand->rid = candidate.rid;
      }
      ++index;
    }
  }
  return ret;
}

void UserSegmentHistoryRewriter::Finish(const ConversionRequest &request,
                                        const Segments &segments) {
  if (request.request_type() != ConversionRequest::CONVERSION) {
    return;
  }

  if (!IsAvailable(request, segments)) {
    return;
  }

  if (request.config().history_learning_level() != Config::DEFAULT_HISTORY) {
    MOZC_VLOG(2) << "history_learning_level is not DEFAULT_HISTORY";
    return;
  }

  const Segments target_segments =
      UseInnerSegments(request)
          ? MakeLearningSegmentsFromInnerSegments(request, segments)
          : segments;
  std::vector<std::string> revert_entries;
  for (size_t i = target_segments.history_segments_size();
       i < target_segments.segments_size(); ++i) {
    const Segment &segment = target_segments.segment(i);
    if (segment.candidates_size() <= 0 ||
        segment.segment_type() != Segment::FIXED_VALUE ||
        segment.candidate(0).attributes &
            converter::Candidate::NO_HISTORY_LEARNING) {
      continue;
    }
    if (IsNumberSegment(segment) && !IsNumberStyleLearningEnabled(request)) {
      RememberNumberPreference(segment, revert_entries);
      continue;
    }
    InsertTriggerKey(segment);
    RememberFirstCandidate(request, target_segments, i, revert_entries);
  }

  revert_cache_.Insert(segments.revert_id(), revert_entries);
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
  if (!(segment.candidate(0).attributes & converter::Candidate::RERANKED)) {
    MOZC_VLOG(2) << "InsertTriggerKey is skipped";
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

  absl::string_view close_bracket_key;
  if (Util::IsOpenBracket(segment.key(), &close_bracket_key)) {
    const std::string key{close_bracket_key.data(), close_bracket_key.size()};
    storage_->Insert(key, reinterpret_cast<const char *>(&v));
  }
}

bool UserSegmentHistoryRewriter::RewriteNumber(Segment *segment) const {
  std::vector<ScoreCandidate> scores;
  for (size_t l = 0;
       l < segment->candidates_size() + segment->meta_candidates_size(); ++l) {
    int j = static_cast<int>(l);
    if (j >= static_cast<int>(segment->candidates_size())) {
      j -= static_cast<int>(segment->candidates_size() +
                            segment->meta_candidates_size());
    }
    Score score = Fetch(FeatureKey::Number(segment->candidate(j).style), 10);

    if (score.score) {
      // Workaround for separated arabic.
      // Because separated arabic and normal number is learned at the
      // same time, make the time gap here so that separated arabic
      // has higher rank by sorting of scores.
      if (score.last_access_time > 0 &&
          (segment->candidate(j).style !=
           NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH) &&
          (segment->candidate(j).style !=
           NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH)) {
        score.last_access_time--;
      }
      scores.emplace_back(score, &segment->candidate(j));
    }
  }

  if (scores.empty()) {
    return false;
  }

  std::stable_sort(scores.begin(), scores.end(),
                   std::greater<ScoreCandidate>());
  return SortCandidates(scores, segment);
}

bool UserSegmentHistoryRewriter::Rewrite(const ConversionRequest &request,
                                         Segments *segments) const {
  if (!IsAvailable(request, *segments)) {
    return false;
  }

  if (request.config().history_learning_level() == Config::NO_HISTORY) {
    MOZC_VLOG(2) << "history_learning_level is NO_HISTORY";
    return false;
  }

  // set BEST_CANDIDATE marker in advance
  for (Segment &segment : *segments) {
    DCHECK_GT(segment.candidates_size(), 0);
    segment.mutable_candidate(0)->attributes |=
        converter::Candidate::BEST_CANDIDATE;
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
      // Number candidates will be rewritten in number rewriter
      // when number style learning is on.
      if (!IsNumberStyleLearningEnabled(request)) {
        modified |= RewriteNumber(segment);
      }
      continue;
    }

    size_t max_candidates_size = 0;
    if (!ShouldRewrite(*segment, &max_candidates_size)) {
      continue;
    }

    if (segment->candidates_size() < max_candidates_size) {
      MOZC_DVLOG(2)
          << "Cannot expand candidates. ignored. Rewrite may be failed";
    }

    // for each all candidates expanded
    std::vector<ScoreCandidate> scores;
    for (size_t l = 0;
         l < segment->candidates_size() + segment->meta_candidates_size();
         ++l) {
      int j = static_cast<int>(l);
      if (j >= static_cast<int>(segment->candidates_size())) {
        j -= static_cast<int>(segment->candidates_size() +
                              transliteration::NUM_T13N_TYPES);
      }

      const Score score = GetScore(request, *segments, i, j);
      if (score.score > 0) {
        scores.emplace_back(score, &segment->candidate(j));
      }
    }

    if (scores.empty()) {
      continue;
    }

    std::stable_sort(scores.begin(), scores.end(),
                     std::greater<ScoreCandidate>());
    modified |= SortCandidates(scores, segment);
    if (!(segment->candidate(0).attributes &
          converter::Candidate::BEST_CANDIDATE)) {
      segment->mutable_candidate(0)->attributes |=
          converter::Candidate::USER_SEGMENT_HISTORY_REWRITER;
    }
  }
  return modified;
}

void UserSegmentHistoryRewriter::Clear() {
  if (storage_ != nullptr) {
    MOZC_VLOG(1) << "Clearing user segment data";
    storage_->Clear();
  }
}

void UserSegmentHistoryRewriter::Revert(const Segments &segments) {
  const std::vector<std::string> *revert_entries =
      revert_cache_.LookupWithoutInsert(segments.revert_id());
  if (!revert_entries) {
    return;
  }
  for (const auto &key : *revert_entries) {
    MOZC_VLOG(2) << "Erasing the key: " << key;
    storage_->Delete(key);
  }
}

bool UserSegmentHistoryRewriter::ClearHistoryEntry(const Segments &segments,
                                                   size_t segment_index,
                                                   int candidate_index) {
  DCHECK_LT(segment_index, segments.segments_size());
  const Segment &segment = segments.segment(segment_index);
  DCHECK(segment.is_valid_index(candidate_index));
  const converter::Candidate &candidate = segment.candidate(0);
  absl::string_view key = candidate.key;
  absl::string_view value = candidate.value;

  FeatureKey fkey(segments, *pos_matcher_, segment_index);
  bool result = false;
  result |= DeleteEntry(fkey.LeftRight(key, value));
  result |= DeleteEntry(fkey.LeftLeft(key, value));
  result |= DeleteEntry(fkey.RightRight(key, value));
  result |= DeleteEntry(fkey.Left(key, value));
  result |= DeleteEntry(fkey.Right(key, value));
  result |= DeleteEntry(fkey.LeftNumber(key, value));
  result |= DeleteEntry(fkey.RightNumber(key, value));
  result |= DeleteEntry(fkey.Single(key, value));
  result |= DeleteEntry(fkey.Current(key, value));
  return result;
}

bool UserSegmentHistoryRewriter::IsPunctuation(
    const Segment &seg, const converter::Candidate &candidate) const {
  return (pos_matcher_->IsJapanesePunctuations(candidate.lid) &&
          candidate.lid == candidate.rid && IsPunctuationInternal(seg.key()) &&
          IsPunctuationInternal(candidate.value));
}

UserSegmentHistoryRewriter::Score UserSegmentHistoryRewriter::Fetch(
    const absl::string_view key, const uint32_t weight) const {
  if (!key.empty()) {
    uint32_t atime;
    const FeatureValue *v = std::launder(
        reinterpret_cast<const FeatureValue *>(storage_->Lookup(key, &atime)));
    if (v && v->IsValid()) {
      return {weight, atime};
    }
  }
  return {0, 0};
}

void UserSegmentHistoryRewriter::Insert(
    absl::string_view key, bool force,
    std::vector<std::string> &revert_entries) {
  if (key.empty()) {
    return;
  }

  MaybeInsertRevertEntry(key, revert_entries);

  FeatureValue v;
  DCHECK(v.IsValid());
  if (force) {
    storage_->Insert(key, reinterpret_cast<const char *>(&v));
  } else {
    storage_->TryInsert(key, reinterpret_cast<const char *>(&v));
  }
}

void UserSegmentHistoryRewriter::MaybeInsertRevertEntry(
    absl::string_view key, std::vector<std::string> &revert_entries) {
  if (storage_->Lookup(key) != nullptr) {
    return;
  }

  revert_entries.emplace_back(key);
}

bool UserSegmentHistoryRewriter::DeleteEntry(absl::string_view key) {
  if (storage_->Lookup(key) == nullptr) {
    return false;
  }
  MOZC_VLOG(2) << "Erasing the key: " << key;
  storage_->Delete(key);
  return true;
}

}  // namespace mozc

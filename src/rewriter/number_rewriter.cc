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

#include "rewriter/number_rewriter.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/serialized_string_array.h"
#include "base/japanese_util.h"
#include "base/number_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "config/character_form_manager.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/number_compound_util.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

using ::mozc::config::CharacterFormManager;
using ::mozc::dictionary::PosMatcher;

// Rewrite type
enum RewriteType {
  NO_REWRITE = 0,
  ARABIC_FIRST,  // arabic candidates first ordering
  KANJI_FIRST,   // kanji candidates first ordering
};

struct RewriteCandidateInfo {
  RewriteType type;
  int position;                  // Base (arabic number) candidate position
  Segment::Candidate candidate;  // Base candidate
};

bool IsNumberStyleLearningEnabled(const ConversionRequest &request) {
  // Enabled in mobile (software keyboard & hardware keyboard)
  return request.request().kana_modifier_insensitive_conversion();
}

// Returns RewriteCandidateInformation if available.
// base_candidate_pos: the index of the base candidate.
// *arabic_candidate: arabic candidate using numeric style conversion.
// POS information, cost, etc will be copied from base candidate.
std::optional<RewriteCandidateInfo> GetRewriteCandidateInfo(
    const SerializedStringArray &suffix_array, const Segment &seg,
    int base_candidate_pos, const PosMatcher &pos_matcher) {
  const Segment::Candidate &c = seg.candidate(base_candidate_pos);
  if (!number_compound_util::IsNumber(suffix_array, pos_matcher, c)) {
    return std::nullopt;
  }
  if (c.attributes & Segment::Candidate::NO_MODIFICATION) {
    return std::nullopt;
  }
  // Do not rewrite hex/oct/bin number.
  if (c.style == NumberUtil::NumberString::NUMBER_HEX ||
      c.style == NumberUtil::NumberString::NUMBER_OCT ||
      c.style == NumberUtil::NumberString::NUMBER_BIN) {
    return std::nullopt;
  }

  RewriteCandidateInfo info;
  info.position = base_candidate_pos;

  if (Util::GetScriptType(c.content_value) == Util::NUMBER) {
    info.candidate = c;
    info.candidate.inner_segment_boundary.clear();
    DCHECK(info.candidate.IsValid());
    if (Util::GetScriptType(c.content_key) == Util::NUMBER ||
        (c.attributes & Segment::Candidate::USER_DICTIONARY)) {
      // ARABIC_FIRST when:
      // - a user types number key
      // - or, the entry came from the user dictionary
      info.type = ARABIC_FIRST;
    } else {
      info.type = KANJI_FIRST;
    }
    return info;
  }

  const std::string half_width_new_content_value =
      japanese_util::FullWidthToHalfWidth(c.content_key);
  // Try to get normalized kanji_number and arabic_number.
  // If it failed, do nothing.
  // Retain suffix for later use.
  std::string number_suffix, kanji_number, arabic_number;
  if (!NumberUtil::NormalizeNumbersWithSuffix(c.content_value,
                                              false,  // trim_reading_zeros
                                              &kanji_number, &arabic_number,
                                              &number_suffix) ||
      arabic_number == half_width_new_content_value) {
    return std::nullopt;
  }
  const std::string new_content_value = arabic_number + number_suffix;
  if (new_content_value == half_width_new_content_value) {
    return std::nullopt;
  }
  const std::string suffix(c.value, c.content_value.size(),
                           c.value.size() - c.content_value.size());
  info.candidate.Clear();
  info.candidate.value = new_content_value + suffix;
  info.candidate.content_value = new_content_value;
  info.candidate.key = c.key;
  info.candidate.content_key = c.content_key;
  info.candidate.consumed_key_size = c.consumed_key_size;
  info.candidate.cost = c.cost;
  info.candidate.structure_cost = c.structure_cost;
  info.candidate.lid = c.lid;
  info.candidate.rid = c.rid;
  info.candidate.attributes |=
      c.attributes & Segment::Candidate::PARTIALLY_KEY_CONSUMED;
  DCHECK(info.candidate.IsValid());

  info.type = KANJI_FIRST;
  return info;
}

std::vector<RewriteCandidateInfo> GetRewriteCandidateInfos(
    const SerializedStringArray &suffix_array, const Segment &seg,
    const PosMatcher &pos_matcher) {
  std::vector<RewriteCandidateInfo> rewrite_candidate_info;
  constexpr int kMaxLenForPhoneticNumber = 6;  // "100000" (じゅうまん)

  // Use the higher ranked candidate for deciding the insertion position.
  absl::flat_hash_set<std::string> seen;
  for (size_t i = 0; i < seg.candidates_size(); ++i) {
    std::optional<RewriteCandidateInfo> info = GetRewriteCandidateInfo(
        suffix_array, seg, i, pos_matcher);
    if (!info.has_value()) {
      continue;
    }

    // Skip expanding number variation for large number for phonetic number
    // candidates. Generating "100000000" for the key "いちおく" would be noisy.
    const bool is_base_phonetic =
        (Util::GetFirstScriptType(info->candidate.key) != Util::NUMBER);
    if (is_base_phonetic &&
        Util::CharsLen(info->candidate.value) > kMaxLenForPhoneticNumber) {
      continue;
    }

    if (seen.insert(info->candidate.value).second) {
      rewrite_candidate_info.push_back(std::move(*info));
    }
  }
  return rewrite_candidate_info;
}

// If top candidate is Kanji numeric, we want to expand at least
// 5 candidates apart from base candidate.
// http://b/issue?id=2872048
constexpr int kArabicNumericOffset = 5;

int GetInsertOffset(RewriteType type) {
  // +2 for arabic half_width full_width expansion
  return (type == ARABIC_FIRST) ? 2 : kArabicNumericOffset;
}

void PushBackCandidate(const absl::string_view value,
                       const absl::string_view desc,
                       NumberUtil::NumberString::Style style,
                       std::vector<Segment::Candidate> *results) {
  bool found = false;
  for (std::vector<Segment::Candidate>::const_iterator it = results->begin();
       it != results->end(); ++it) {
    if (it->value == value) {
      found = true;
      break;
    }
  }
  if (!found) {
    Segment::Candidate cand;
    cand.value = std::string(value);
    cand.description = std::string(desc);
    cand.style = style;
    results->push_back(cand);
  }
}

void SetCandidatesInfo(const Segment::Candidate &arabic_cand,
                       std::vector<Segment::Candidate> *candidates) {
  const std::string suffix(
      arabic_cand.value, arabic_cand.content_value.size(),
      arabic_cand.value.size() - arabic_cand.content_value.size());

  for (std::vector<Segment::Candidate>::iterator it = candidates->begin();
       it != candidates->end(); ++it) {
    it->content_value.assign(it->value);
    it->value.append(suffix);
  }
}

// Note:
// Some number characters such as superscript (¹) is out of target of
// number styles.
bool IsNumberCandidate(const Segment::Candidate &candidate,
                       const PosMatcher &pos_matcher) {
  if (candidate.lid != candidate.rid) {
    return false;
  }
  const bool has_number_style =
      candidate.style != NumberUtil::NumberString::DEFAULT_STYLE;
  // Support number candidate with the default POS.
  // For example, transliteration rewriter can generate number
  // candidate with the unknown_id.
  const bool is_unknown_number_candidate =
      (pos_matcher.IsUnknown(candidate.lid) &&
       Util::IsScriptType(candidate.value, Util::NUMBER));
  return pos_matcher.IsNumber(candidate.lid) ||
         pos_matcher.IsKanjiNumber(candidate.lid) || has_number_style ||
         is_unknown_number_candidate;
}

void SetNumberInfoToExistingCandidates(
    absl::Span<const NumberUtil::NumberString> numbers,
    const PosMatcher &pos_matcher, Segment *segment) {
  absl::flat_hash_map<std::string, NumberUtil::NumberString> number_map;
  // Different number style can have the same surface
  // ex. (123, NUMBER_SEPARATED_ARABIC_HALFWIDTH) and (123, DEFAULT_STYLE)
  for (const auto &entry : numbers) {
    number_map.emplace(entry.value, entry);
  }

  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    Segment::Candidate *candidate = segment->mutable_candidate(i);
    const auto &itr = number_map.find(candidate->value);
    if (itr == number_map.end()) {
      continue;
    }
    if (!IsNumberCandidate(*candidate, pos_matcher)) {
      continue;
    }

    candidate->style = itr->second.style;
    if (candidate->description.empty()) {
      candidate->description = itr->second.description;
    }
  }
}

class CheckValueOperator {
 public:
  explicit CheckValueOperator(const absl::string_view v) : find_value_(v) {}
  bool operator()(const Segment::Candidate &cand) const {
    return (cand.value == find_value_);
  }

 private:
  const absl::string_view find_value_;
};

// If we have the candidates to be inserted before the base candidate,
// delete them.
void FindEraseCandidates(absl::Span<const Segment::Candidate> results,
                         int base_candidate_pos, RewriteType type,
                         const Segment &seg, std::set<int> *erase_positions) {
  // Remember base candidate value
  const int start_pos =
      std::min<int>(base_candidate_pos + GetInsertOffset(type),
                    seg.candidates_size() - 1);
  for (int pos = start_pos; pos >= 0; --pos) {
    if (pos == base_candidate_pos) {
      continue;
    }
    if (seg.candidate(pos).attributes & Segment::Candidate::NO_MODIFICATION) {
      continue;
    }

    // Simple liner search. |results| size is small. (at most 10 or so)
    for (const Segment::Candidate &cand : results) {
      if (cand.value == seg.candidate(pos).value) {
        erase_positions->insert(pos);
        break;
      }
    }
  }
}

// This is a utility function for InsertCandidate and UpdateCandidate.
// Do not use this function directly.
void MergeCandidateInfoInternal(const Segment::Candidate &base_cand,
                                const Segment::Candidate &result_cand,
                                Segment::Candidate *cand) {
  DCHECK(cand);
  cand->key = base_cand.key;
  cand->value = result_cand.value;
  cand->content_key = base_cand.content_key;
  cand->content_value = result_cand.content_value;
  cand->consumed_key_size = base_cand.consumed_key_size;
  cand->cost = base_cand.cost;
  cand->lid = base_cand.lid;
  cand->rid = base_cand.rid;
  cand->style = result_cand.style;
  cand->description.assign(result_cand.description);

  // Don't want to have FULL_WIDTH form for Hex/Oct/BIN..etc.
  if (cand->style == NumberUtil::NumberString::NUMBER_HEX ||
      cand->style == NumberUtil::NumberString::NUMBER_OCT ||
      cand->style == NumberUtil::NumberString::NUMBER_BIN) {
    cand->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  }
  cand->attributes |=
      base_cand.attributes & (Segment::Candidate::PARTIALLY_KEY_CONSUMED |
                              Segment::Candidate::NO_LEARNING);
  cand->attributes |=
      result_cand.attributes & Segment::Candidate::NO_VARIANTS_EXPANSION;
}

void InsertCandidate(Segment *segment, int32_t insert_position,
                     const Segment::Candidate &base_cand,
                     const Segment::Candidate &result_cand) {
  DCHECK(segment);
  Segment::Candidate *c = segment->insert_candidate(insert_position);
  MergeCandidateInfoInternal(base_cand, result_cand, c);
}

void UpdateCandidate(Segment *segment, int32_t update_position,
                     const Segment::Candidate &base_cand,
                     const Segment::Candidate &result_cand) {
  DCHECK(segment);
  Segment::Candidate *c = segment->mutable_candidate(update_position);
  // Do not call |c->Init()| for an existing candidate.
  // There are two major reasons.
  // 1) Future design change may introduce another field into
  //    Segment::Candidate. In such situation, simply calling |c->Init()|
  //    for an existing candidate may result in unexpeced data loss.
  // 2) In order to preserve existing attribute information such as
  //    Segment::Candidate::USER_DICTIONARY bit in |c|, we cannot call
  //    |c->Init()|. Note that neither |base_cand| nor |result[0]| has
  //    valid value in its |attributes|.
  MergeCandidateInfoInternal(base_cand, result_cand, c);
}

void InsertConvertedCandidates(absl::Span<const Segment::Candidate> results,
                               const Segment::Candidate &base_cand,
                               int base_candidate_pos, int insert_pos,
                               Segment *seg) {
  if (results.empty()) {
    return;
  }
  if (base_candidate_pos >= seg->candidates_size()) {
    LOG(WARNING) << "Invalid base candidate pos";
    return;
  }
  // First, insert top candidate
  // If we find the base candidate is equal to the converted
  // special form candidates, we will rewrite it.
  // Otherwise, we will insert top candidate just below the base.
  // Sometimes original base candidate is different from converted candidate
  // For example, "千万" v.s. "一千万", or "一二三" v.s. "百二十三".
  // We don't want to rewrite "千万" to "一千万".
  {
    const absl::string_view base_value =
        seg->candidate(base_candidate_pos).value;
    const auto itr = std::find_if(results.begin(), results.end(),
                                  CheckValueOperator(base_value));
    if (itr != results.end() &&
        itr->style != NumberUtil::NumberString::NUMBER_KANJI &&
        itr->style != NumberUtil::NumberString::NUMBER_KANJI_ARABIC) {
      // Update existing base candidate
      UpdateCandidate(seg, base_candidate_pos, base_cand, results[0]);
    } else {
      // Insert candidate just below the base candidate
      InsertCandidate(seg, base_candidate_pos + 1, base_cand, results[0]);
      ++insert_pos;
    }
  }

  // Insert others
  for (size_t i = 1; i < results.size(); ++i) {
    InsertCandidate(seg, insert_pos++, base_cand, results[i]);
  }
}

int GetInsertPos(int base_pos, const Segment &segment, RewriteType type) {
  return std::min<int>(base_pos + GetInsertOffset(type),
                       segment.candidates_size());
}

void InsertHalfArabic(const absl::string_view half_arabic,
                      std::vector<NumberUtil::NumberString> *output) {
  output->emplace_back(std::string(half_arabic), "",
                       NumberUtil::NumberString::DEFAULT_STYLE);
}

std::vector<NumberUtil::NumberString> GetNumbersInDefaultOrder(
    RewriteType type, bool exec_radix_conversion,
    const absl::string_view arabic_content_value) {
  std::vector<NumberUtil::NumberString> output;
  if (type == ARABIC_FIRST) {
    InsertHalfArabic(arabic_content_value, &output);
    NumberUtil::ArabicToWideArabic(arabic_content_value, &output);
    NumberUtil::ArabicToSeparatedArabic(arabic_content_value, &output);
    NumberUtil::ArabicToKanji(arabic_content_value, &output);
    NumberUtil::ArabicToOtherForms(arabic_content_value, &output);
  } else if (type == KANJI_FIRST) {
    NumberUtil::ArabicToKanji(arabic_content_value, &output);
    InsertHalfArabic(arabic_content_value, &output);
    NumberUtil::ArabicToWideArabic(arabic_content_value, &output);
    NumberUtil::ArabicToSeparatedArabic(arabic_content_value, &output);
    NumberUtil::ArabicToOtherForms(arabic_content_value, &output);
  }

  if (exec_radix_conversion) {
    NumberUtil::ArabicToOtherRadixes(arabic_content_value, &output);
  }
  return output;
}

}  // namespace

NumberRewriter::NumberRewriter(const DataManager *data_manager)
    : pos_matcher_(data_manager->GetPosMatcherData()) {
  absl::string_view data = data_manager->GetCounterSuffixSortedArray();
  // Data manager is responsible for providing a valid data.  Just verify data
  // in debug build.
  DCHECK(SerializedStringArray::VerifyData(data));
  suffix_array_.Set(data);

  Reload();
}

NumberRewriter::~NumberRewriter() = default;

int NumberRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool NumberRewriter::Rewrite(const ConversionRequest &request,
                             Segments *segments) const {
  DCHECK(segments);
  if (!request.config().use_number_conversion()) {
    MOZC_VLOG(2) << "no use_number_conversion";
    return false;
  }

  bool modified = false;

  for (Segment &segment : segments->conversion_segments()) {
    modified |= RewriteOneSegment(request, &segment, segments);
  }

  return modified;
}

namespace {
bool IsAlreadyUpdated(absl::Span<const Segment::Candidate> number_candidates,
                      const Segment &seg) {
  absl::flat_hash_set<absl::string_view> values;
  for (int i = 0; i < seg.candidates_size(); ++i) {
    values.insert(seg.candidate(i).value);
  }

  for (const auto &candidate : number_candidates) {
    if (!values.contains(candidate.value)) {
      return false;
    }
  }
  return true;
}
}  // namespace

bool NumberRewriter::RewriteOneSegment(const ConversionRequest &request,
                                       Segment *seg, Segments *segments) const {
  DCHECK(segments);
  // Radix conversion is done only for conversion mode.
  // Showing radix candidates is annoying for a user.
  const bool exec_radix_conversion =
      (segments->conversion_segments_size() == 1 &&
       request.request_type() == ConversionRequest::CONVERSION);
  const bool should_rerank = ShouldRerankCandidates(request, *segments);

  const std::vector<RewriteCandidateInfo> infos =
      GetRewriteCandidateInfos(suffix_array_, *seg, pos_matcher_);

  bool modified = false;
  std::set<int> erase_positions;  // Use std::set for deterministic order.
  for (auto it = infos.rbegin(); it != infos.rend(); ++it) {
    const RewriteCandidateInfo &info = *it;
    if (info.candidate.content_value.size() > info.candidate.value.size()) {
      LOG(ERROR) << "Invalid content_value/value: ";
      break;
    }

    const std::string arabic_content_value =
        japanese_util::FullWidthToHalfWidth(info.candidate.content_value);
    if (Util::GetScriptType(arabic_content_value) != Util::NUMBER) {
      if (Util::GetFirstScriptType(arabic_content_value) == Util::NUMBER) {
        // Rewrite for number suffix
        const int insert_pos =
            std::min<int>(info.position + 1, seg->candidates_size());
        InsertCandidate(seg, insert_pos, info.candidate, info.candidate);
        modified = true;
        continue;
      }
      LOG(ERROR) << "arabic_content_value is not number: "
                 << arabic_content_value;
      break;
    }
    const std::vector<NumberUtil::NumberString> output =
        GetNumbersInDefaultOrder(info.type, exec_radix_conversion,
                                 arabic_content_value);
    SetNumberInfoToExistingCandidates(output, pos_matcher_, seg);

    const std::vector<Segment::Candidate> number_candidates =
        GenerateCandidatesToInsert(info.candidate, output, should_rerank);

    // If all the candidates are already in the segment, do nothing.
    if (IsAlreadyUpdated(number_candidates, *seg)) {
      continue;
    }

    FindEraseCandidates(number_candidates, info.position, info.type, *seg,
                        &erase_positions);
    const int insert_pos = GetInsertPos(info.position, *seg, info.type);
    DCHECK_LT(info.position, insert_pos);
    InsertConvertedCandidates(number_candidates, info.candidate, info.position,
                              insert_pos, seg);
    modified = true;
  }

  for (auto it = erase_positions.rbegin(); it != erase_positions.rend(); ++it) {
    seg->erase_candidate(*it);
  }

  return modified;
}

std::vector<Segment::Candidate> NumberRewriter::GenerateCandidatesToInsert(
    const Segment::Candidate &arabic_candidate,
    absl::Span<const NumberUtil::NumberString> numbers,
    bool should_rerank) const {
  std::vector<Segment::Candidate> converted_numbers;
  for (const auto &number_string : numbers) {
    PushBackCandidate(number_string.value, number_string.description,
                      number_string.style, &converted_numbers);
  }
  SetCandidatesInfo(arabic_candidate, &converted_numbers);
  if (should_rerank) {
    RerankCandidates(converted_numbers);
  }
  return converted_numbers;
}

bool NumberRewriter::ShouldRerankCandidates(const ConversionRequest &request,
                                            const Segments &segments) const {
  if (!IsNumberStyleLearningEnabled(request)) {
    MOZC_VLOG(2) << "number style learning is not enabled.";
    return false;
  }
  if (request.config().incognito_mode()) {
    MOZC_VLOG(2) << "incognito mode";
    return false;
  }
  if (request.config().history_learning_level() == config::Config::NO_HISTORY) {
    MOZC_VLOG(2) << "history learning level is NO_HISTORY";
    return false;
  }
  if (!request.config().use_history_suggest() &&
      request.request_type() == ConversionRequest::SUGGESTION) {
    MOZC_VLOG(2) << "no history suggest";
    return false;
  }
  if (segments.conversion_segments_size() != 1) {
    // Rewriting "2|階" to "弐|階" using the history would be noisy.
    MOZC_VLOG(2) << "do not apply to the multiple segments.";
    return false;
  }
  return true;
}

void NumberRewriter::RerankCandidates(
    std::vector<Segment::Candidate> &candidates) const {
  std::optional<const CharacterFormManager::NumberFormStyle> stored_entry =
      CharacterFormManager::GetCharacterFormManager()->GetLastNumberStyle();
  if (!stored_entry.has_value()) {
    return;
  }
  const NumberUtil::NumberString::Style style = stored_entry->style;
  const config::Config::CharacterForm form = stored_entry->form;

  auto top_number_entry = candidates.begin();
  for (auto itr = candidates.begin(); itr != candidates.end(); ++itr) {
    if (itr->style != style) {
      continue;
    }
    if (style == NumberUtil::NumberString::DEFAULT_STYLE &&
        ((Util::GetFormType(itr->value) == Util::HALF_WIDTH) !=
         (form == config::Config::HALF_WIDTH))) {
      continue;
    }
    top_number_entry = itr;
    break;
  }

  // Rerank `top_number_entry` to top.
  std::rotate(candidates.begin(), top_number_entry, top_number_entry + 1);
  candidates.begin()->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
}

void NumberRewriter::Finish(const ConversionRequest &request,
                            Segments *segments) {
  if (!IsNumberStyleLearningEnabled(request)) {
    MOZC_VLOG(2) << "number style learning is not enabled.";
    return;
  }

  if (request.config().incognito_mode()) {
    MOZC_VLOG(2) << "incognito_mode";
    return;
  }

  if (request.config().history_learning_level() !=
      config::Config::DEFAULT_HISTORY) {
    MOZC_VLOG(2) << "history_learning_level is not DEFAULT_HISTORY";
    return;
  }

  for (const Segment &segment : segments->conversion_segments()) {
    if (segment.candidates_size() <= 0 ||
        segment.segment_type() != Segment::FIXED_VALUE ||
        segment.candidate(0).attributes &
            Segment::Candidate::NO_HISTORY_LEARNING) {
      continue;
    }
    if (segment.candidates_size() == 0) {
      continue;
    }
    if (!IsNumberCandidate(segment.candidate(0), pos_matcher_)) {
      continue;
    }
    RememberNumberStyle(segment.candidate(0));
  }
}

void NumberRewriter::RememberNumberStyle(const Segment::Candidate &candidate) {
  const Util::FormType form = Util::GetFormType(candidate.value);
  CharacterFormManager::NumberFormStyle entry = {
      form == Util::HALF_WIDTH ? config::Config::HALF_WIDTH
                               : config::Config::FULL_WIDTH,
      candidate.style};
  CharacterFormManager::GetCharacterFormManager()->SetLastNumberStyle(entry);
}
}  // namespace mozc

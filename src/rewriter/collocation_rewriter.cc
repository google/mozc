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

#include "rewriter/collocation_rewriter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/hash.h"
#include "base/util.h"
#include "base/vlog.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"
#include "rewriter/collocation_util.h"
#include "storage/existence_filter.h"

namespace mozc {
namespace collocation_rewriter_internal {

using ::mozc::storage::ExistenceFilter;

absl::StatusOr<CollocationFilter> CollocationFilter::Create(
    const absl::Span<const uint32_t> data) {
  absl::StatusOr<ExistenceFilter> filter = ExistenceFilter::Read(data);
  if (!filter.ok()) {
    return filter.status();
  }
  return CollocationFilter(*std::move(filter));
}

bool CollocationFilter::Exists(const absl::string_view left,
                               const absl::string_view right) const {
  if (left.empty() || right.empty()) {
    return false;
  }
  const uint64_t id = Fingerprint(absl::StrCat(left, right));
  return filter_.Exists(id);
}

absl::StatusOr<SuppressionFilter> SuppressionFilter::Create(
    const absl::Span<const uint32_t> data) {
  absl::StatusOr<ExistenceFilter> filter = ExistenceFilter::Read(data);
  if (!filter.ok()) {
    return filter.status();
  }
  return SuppressionFilter(*std::move(filter));
}

bool SuppressionFilter::Exists(const converter::Candidate &cand) const {
  // TODO(noriyukit): We should share key generation rule with
  // gen_collocation_suppression_data_main.cc.
  const uint64_t id =
      Fingerprint(absl::StrCat(cand.content_value, "\t", cand.content_key));
  return filter_.Exists(id);
}

}  // namespace collocation_rewriter_internal

namespace {

using ::mozc::collocation_rewriter_internal::CollocationFilter;
using ::mozc::collocation_rewriter_internal::SuppressionFilter;

constexpr size_t kCandidateSize = 12;
constexpr int kMaxCostDiff = 3453;  // -500*log(1/1000)

// For collocation, we use two segments.
enum SegmentLookupType {
  LEFT,
  RIGHT,
};

// returns true if the given string contains number including Kanji.
bool ContainsNumber(const absl::string_view str) {
  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    if (CollocationUtil::IsNumber(iter.Get())) {
      return true;
    }
  }
  return false;
}

// Returns true if value matches the pattern XXXPPPYYY, where XXX is a Kanji
// sequence, PPP is the given pattern, and YYY is a sequence containing at least
// one Kanji character. In the value matches the pattern, XXX and YYY are
// substituted to |first_content| and |second|, respectively. Returns false if
// the value isn't of the form XXXPPPYYY.
bool ParseCompound(const absl::string_view value,
                   const absl::string_view pattern,
                   absl::string_view *first_content,
                   absl::string_view *second) {
  DCHECK(!value.empty());
  DCHECK(!pattern.empty());

  // Find the |first_content| candidate and check if it consists of Kanji only.
  absl::string_view::const_iterator pattern_begin =
      std::find(value.begin(), value.end(), pattern[0]);
  if (pattern_begin == value.end()) {
    return false;
  }
  *first_content = absl::string_view(
      value.data(), std::distance(value.begin(), pattern_begin));
  if (!Util::IsScriptType(*first_content, Util::KANJI)) {
    return false;
  }

  // Check if the middle part matches |pattern|.
  const absl::string_view remaining_value =
      absl::ClippedSubstr(value, first_content->size());
  if (!remaining_value.starts_with(pattern)) {
    return false;
  }

  // Check if the last substring is eligible for |second|.
  *second = absl::ClippedSubstr(remaining_value, pattern.size());
  if (second->empty() || !Util::ContainsScriptType(*second, Util::KANJI)) {
    return false;
  }

  // Just verify that |value| = |first_content| + |pattern| + |second|.
  DCHECK_EQ(value, std::string(*first_content) + std::string(pattern) +
                       std::string(*second));
  return true;
}

// Handles compound such as "本を読む"(one segment)
// we want to rewrite using it as if it was "<本|を><読む>"
// so that we can use collocation data like "厚い本"
void ResolveCompoundSegment(const absl::string_view top_value,
                            const absl::string_view value,
                            const SegmentLookupType type,
                            std::vector<std::string> *output) {
  // see "http://ja.wikipedia.org/wiki/助詞"
  // "の" is excluded because it is not good for collocation.
  static constexpr std::array<absl::string_view, 8> kParticles = {
      "が", "を", "に", "へ", "と", "から", "より", "で"};

  for (absl::string_view particle : kParticles) {
    absl::string_view first_content, second;
    if (!ParseCompound(top_value, particle, &first_content, &second)) {
      continue;
    }
    if (ParseCompound(value, particle, &first_content, &second)) {
      if (type == LEFT) {
        output->emplace_back(second.data(), second.size());
        output->emplace_back(absl::StrCat(first_content, particle));
      } else {
        output->emplace_back(first_content.data(), first_content.size());
      }
      return;
    }
  }
}

// Generates strings for looking up collocation target for |cand|.
// Returns true if |cand| is valid for collocation look up.
// strings in |output| will be normalized for look up method.
bool GenerateLookupTokens(const converter::Candidate &cand,
                          const converter::Candidate &top_cand,
                          SegmentLookupType type,
                          std::vector<std::string> *output) {
  absl::string_view content = cand.content_value;
  absl::string_view value = cand.value;
  absl::string_view top_content = top_cand.content_value;
  absl::string_view top_value = top_cand.value;

  const size_t top_content_len = Util::CharsLen(top_content);
  const size_t content_len = Util::CharsLen(content);
  const bool should_remove_number = (type == LEFT);

  auto push_back_normalized_string = [&](absl::string_view str) {
    output->resize(output->size() + 1);
    CollocationUtil::GetNormalizedScript(str, should_remove_number,
                                         &output->back());
  };

  if (type == RIGHT && value != top_value && top_content_len >= 2 &&
      content_len == 1) {
    return false;
  }

  if (type == LEFT) {
    push_back_normalized_string(value);
  } else {
    output->push_back(std::string(content));
    // "舞って" workaround
    // V+"て" is often treated as one compound.
    static constexpr absl::string_view pattern = "て";
    if (content.starts_with(pattern)) {
      push_back_normalized_string(
          Util::Utf8SubString(content, 0, content_len - 1));
    }
  }

  // we don't rewrite NUMBER to others and vice versa
  if (ContainsNumber(value) != ContainsNumber(top_value)) {
    return false;
  }

  const absl::string_view top_aux_value =
      Util::Utf8SubString(top_value, top_content_len, std::string::npos);
  const size_t top_aux_value_len = Util::CharsLen(top_aux_value);
  const Util::ScriptType top_value_script_type = Util::GetScriptType(top_value);

  // we don't rewrite KATAKANA segment
  // for example, we don't rewrite "コーヒー飲みます" to "珈琲飲みます"
  if (type == LEFT && top_aux_value_len == 0 && top_value != value &&
      top_value_script_type == Util::KATAKANA) {
    return false;
  }

  // special cases
  if (top_content_len == 1) {
    const char32_t codepoint = Util::Utf8ToCodepoint(top_content);
    switch (codepoint) {
      case 0x304a:  // "お"
      case 0x5fa1:  // "御"
      case 0x3054:  // "ご"
        return true;
      default:
        break;
    }
  }

  const absl::string_view aux_value =
      Util::Utf8SubString(value, content_len, std::string::npos);

  {
    // Remove number in normalization for the left segment.
    std::string aux_normalized, top_aux_normalized;
    CollocationUtil::GetNormalizedScript(aux_value, should_remove_number,
                                         &aux_normalized);
    CollocationUtil::GetNormalizedScript(top_aux_value, should_remove_number,
                                         &top_aux_normalized);
    if (!aux_normalized.empty() &&
        !Util::IsScriptType(aux_normalized, Util::HIRAGANA)) {
      if (type == RIGHT) {
        return false;
      }
      if (aux_normalized != top_aux_normalized) {
        return false;
      }
    }
  }

  ResolveCompoundSegment(top_value, value, type, output);

  const size_t aux_value_len = Util::CharsLen(aux_value);
  const size_t value_len = Util::CharsLen(value);

  // "<XXいる|>" can be rewrote to "<YY|いる>" and vice versa
  {
    static constexpr absl::string_view kSuffix = "いる";
    if (top_aux_value_len == 0 && aux_value_len == 2 &&
        top_value.ends_with(kSuffix) && aux_value.ends_with(kSuffix)) {
      if (type == RIGHT) {
        // "YYいる" in addition to "YY"
        push_back_normalized_string(value);
      }
      return true;
    }
    if (aux_value_len == 0 && top_aux_value_len == 2 &&
        value.ends_with(kSuffix) && top_aux_value.ends_with(kSuffix)) {
      if (type == RIGHT) {
        // "YY" in addition to "YYいる"
        push_back_normalized_string(
            Util::Utf8SubString(value, 0, value_len - 2));
      }
      return true;
    }
  }

  // "<XXせる|>" can be rewrote to "<YY|せる>" and vice versa
  {
    static constexpr absl::string_view kSuffix = "せる";
    if (top_aux_value_len == 0 && aux_value_len == 2 &&
        top_value.ends_with(kSuffix) && aux_value.ends_with(kSuffix)) {
      if (type == RIGHT) {
        // "YYせる" in addition to "YY"
        push_back_normalized_string(value);
      }
      return true;
    }
    if (aux_value_len == 0 && top_aux_value_len == 2 &&
        value.ends_with(kSuffix) && top_aux_value.ends_with(kSuffix)) {
      if (type == RIGHT) {
        // "YY" in addition to "YYせる"
        push_back_normalized_string(
            Util::Utf8SubString(value, 0, value_len - 2));
      }
      return true;
    }
  }

  const Util::ScriptType content_script_type = Util::GetScriptType(content);

  // "<XX|する>" can be rewrote using "<XXす|る>" and "<XX|する>"
  // in "<XX|する>", XX must be single script type
  {
    static constexpr absl::string_view kSuffix = "する";
    if (aux_value_len == 2 && aux_value.ends_with(kSuffix)) {
      if (content_script_type != Util::KATAKANA &&
          content_script_type != Util::HIRAGANA &&
          content_script_type != Util::KANJI &&
          content_script_type != Util::ALPHABET) {
        return false;
      }
      if (type == RIGHT) {
        // "YYす" in addition to "YY"
        push_back_normalized_string(
            Util::Utf8SubString(value, 0, value_len - 1));
      }
      return true;
    }
  }

  // "<XXる>" can be rewrote using "<XX|る>"
  // "まとめる", "衰える"
  {
    static constexpr absl::string_view kSuffix = "る";
    if (aux_value_len == 0 && value.ends_with(kSuffix)) {
      if (type == RIGHT) {
        // "YY" in addition to "YYる"
        push_back_normalized_string(
            Util::Utf8SubString(value, 0, value_len - 1));
      }
      return true;
    }
  }

  // "<XXす>" can be rewrote using "XXする"
  {
    static constexpr absl::string_view kSuffix = "す";
    if (value.ends_with(kSuffix) &&
        Util::IsScriptType(Util::Utf8SubString(value, 0, value_len - 1),
                           Util::KANJI)) {
      if (type == RIGHT) {
        static constexpr absl::string_view kRu = "る";
        // "YYする" in addition to "YY"
        push_back_normalized_string(absl::StrCat(value, kRu));
      }
      return true;
    }
  }

  // "<XXし|た>" can be rewrote using "<XX|した>"
  {
    static constexpr absl::string_view kShi = "し";
    static constexpr absl::string_view kTa = "た";
    if (content.ends_with(kShi) && aux_value == kTa &&
        top_content.ends_with(kShi) && top_aux_value == kTa) {
      if (type == RIGHT) {
        const absl::string_view val =
            Util::Utf8SubString(content, 0, content_len - 1);
        // XX must be KANJI
        if (Util::IsScriptType(val, Util::KANJI)) {
          push_back_normalized_string(val);
        }
      }
      return true;
    }
  }

  const int aux_len = value_len - content_len;
  const int top_aux_len = Util::CharsLen(top_value) - top_content_len;
  if (aux_len != top_aux_len) {
    return false;
  }

  const Util::ScriptType top_content_script_type =
      Util::GetScriptType(top_content);

  // we don't rewrite HIRAGANA to KATAKANA
  if (top_content_script_type == Util::HIRAGANA &&
      content_script_type == Util::KATAKANA) {
    return false;
  }

  // we don't rewrite second KATAKANA
  // for example, we don't rewrite "このコーヒー" to "この珈琲"
  if (type == RIGHT && top_content_script_type == Util::KATAKANA &&
      value != top_value) {
    return false;
  }

  if (top_content_len == 1 && top_content_script_type == Util::HIRAGANA) {
    return false;
  }

  // suppress "<身|ています>" etc.
  if (top_content_len == 1 && content_len == 1 && top_aux_value_len >= 2 &&
      aux_value_len >= 2 && top_content_script_type == Util::KANJI &&
      content_script_type == Util::KANJI && top_content != content) {
    return false;
  }

  return true;
}

// Just a wrapper of IsNaturalContent for debug.
bool VerifyNaturalContent(const converter::Candidate &cand,
                          const converter::Candidate &top_cand,
                          SegmentLookupType type) {
  std::vector<std::string> nexts;
  return GenerateLookupTokens(cand, top_cand, RIGHT, &nexts);
}

inline bool IsKeyUnknown(const Segment &seg) {
  return Util::IsScriptType(seg.key(), Util::UNKNOWN_SCRIPT);
}

}  // namespace

bool CollocationRewriter::RewriteCollocation(Segments *segments) const {
  // Return false if at least one segment is fixed or at least one segment
  // contains no candidates.
  for (const Segment &seg : segments->conversion_segments()) {
    if (seg.segment_type() == Segment::FIXED_VALUE ||
        seg.candidates_size() == 0) {
      return false;
    }
  }

  std::vector<bool> segs_changed(segments->segments_size(), false);
  bool changed = false;

  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    bool rewrote_next = false;

    if (IsKeyUnknown(segments->segment(i))) {
      continue;
    }

    if (i + 1 < segments->segments_size() &&
        RewriteUsingNextSegment(segments->mutable_segment(i + 1),
                                segments->mutable_segment(i))) {
      changed = true;
      rewrote_next = true;
      segs_changed[i] = true;
      segs_changed[i + 1] = true;
    }

    if (!segs_changed[i] && !rewrote_next && i > 0 &&
        RewriteFromPrevSegment(segments->segment(i - 1).candidate(0),
                               segments->mutable_segment(i))) {
      changed = true;
      segs_changed[i - 1] = true;
      segs_changed[i] = true;
    }

    const converter::Candidate &cand = segments->segment(i).candidate(0);
    if (i >= 2 &&
        // Cross over only adverbs
        // Segment is adverb if;
        //  1) lid and rid is adverb.
        //  2) or rid is adverb suffix.
        ((pos_matcher_.IsAdverb(segments->segment(i - 1).candidate(0).lid) &&
          pos_matcher_.IsAdverb(segments->segment(i - 1).candidate(0).rid)) ||
         pos_matcher_.IsAdverbSegmentSuffix(
             segments->segment(i - 1).candidate(0).rid)) &&
        (cand.content_value != cand.value ||
         cand.value != "・")) {  // "・" workaround
      if (!segs_changed[i - 2] && !segs_changed[i] &&
          RewriteUsingNextSegment(segments->mutable_segment(i),
                                  segments->mutable_segment(i - 2))) {
        changed = true;
        segs_changed[i] = true;
        segs_changed[i - 2] = true;
      } else if (!segs_changed[i] &&
                 RewriteFromPrevSegment(segments->segment(i - 2).candidate(0),
                                        segments->mutable_segment(i))) {
        changed = true;
        segs_changed[i] = true;
        segs_changed[i - 2] = true;
      }
    }
  }

  return changed;
}

std::unique_ptr<CollocationRewriter> CollocationRewriter::Create(
    const DataManager &data_manager) {
  absl::StatusOr<CollocationFilter> collocation_filter =
      CollocationFilter::Create(data_manager.GetCollocationData());
  if (!collocation_filter.ok()) {
    LOG(ERROR) << collocation_filter.status();
    return nullptr;
  }

  absl::StatusOr<SuppressionFilter> suppression_filter =
      SuppressionFilter::Create(data_manager.GetCollocationSuppressionData());
  if (!suppression_filter.ok()) {
    LOG(ERROR) << suppression_filter.status();
    return nullptr;
  }

  return std::make_unique<CollocationRewriter>(
      dictionary::PosMatcher(data_manager.GetPosMatcherData()),
      *std::move(collocation_filter), *std::move(suppression_filter));
}

bool CollocationRewriter::Rewrite(const ConversionRequest &request,
                                  Segments *segments) const {
  return RewriteCollocation(segments);
}

bool CollocationRewriter::IsName(const converter::Candidate &cand) const {
  const bool ret = (cand.lid == last_name_id_ || cand.lid == first_name_id_);
  if (ret) {
    MOZC_VLOG(3) << cand.value << " is name sagment";
  }
  return ret;
}

bool CollocationRewriter::RewriteFromPrevSegment(
    const converter::Candidate &prev_cand, Segment *seg) const {
  std::string prev;
  CollocationUtil::GetNormalizedScript(prev_cand.value, true, &prev);

  const size_t i_max = std::min(seg->candidates_size(), kCandidateSize);

  // Reuse |curs| in the loop as this method is performance critical.
  std::vector<std::string> curs;
  for (size_t i = 0; i < i_max; ++i) {
    if (seg->candidate(i).cost > seg->candidate(0).cost + kMaxCostDiff) {
      continue;
    }
    if (IsName(seg->candidate(i))) {
      continue;
    }
    if (suppression_filter_.Exists(seg->candidate(i))) {
      continue;
    }
    curs.clear();
    if (!GenerateLookupTokens(seg->candidate(i), seg->candidate(0), RIGHT,
                              &curs)) {
      continue;
    }

    for (absl::string_view cur : curs) {
      if (collocation_filter_.Exists(prev, cur)) {
        if (i != 0) {
          MOZC_VLOG(3) << prev << cur << " " << seg->candidate(0).value << "->"
                       << seg->candidate(i).value;
        }
        seg->move_candidate(i, 0);
        seg->mutable_candidate(0)->attributes |=
            converter::Attribute::CONTEXT_SENSITIVE;
        return true;
      }
    }
  }
  return false;
}

bool CollocationRewriter::RewriteUsingNextSegment(Segment *next_seg,
                                                  Segment *seg) const {
  const size_t i_max = std::min(seg->candidates_size(), kCandidateSize);
  const size_t j_max = std::min(next_seg->candidates_size(), kCandidateSize);

  // Cache the results for the next segment
  std::vector<int> next_seg_ok(j_max);  // Avoiding std::vector<bool>
  std::vector<std::vector<std::string>> nexts(j_max);
  for (size_t j = 0; j < j_max; ++j) {
    next_seg_ok[j] = 0;

    if (IsName(next_seg->candidate(j))) {
      continue;
    }
    if (suppression_filter_.Exists(next_seg->candidate(j))) {
      continue;
    }
    if (!GenerateLookupTokens(next_seg->candidate(j), next_seg->candidate(0),
                              RIGHT, &nexts[j])) {
      continue;
    }

    next_seg_ok[j] = 1;
  }

  // Reuse |curs| in the loop as this method is performance critical.
  std::vector<std::string> curs;
  for (size_t i = 0; i < i_max; ++i) {
    if (seg->candidate(i).cost > seg->candidate(0).cost + kMaxCostDiff) {
      continue;
    }
    if (IsName(seg->candidate(i))) {
      continue;
    }
    if (suppression_filter_.Exists(seg->candidate(i))) {
      continue;
    }
    curs.clear();
    if (!GenerateLookupTokens(seg->candidate(i), seg->candidate(0), LEFT,
                              &curs)) {
      continue;
    }

    for (absl::string_view cur : curs) {
      for (size_t j = 0; j < j_max; ++j) {
        if (next_seg->candidate(j).cost >
            next_seg->candidate(0).cost + kMaxCostDiff) {
          continue;
        }
        if (!next_seg_ok[j]) {
          continue;
        }

        for (absl::string_view next : nexts[j]) {
          if (collocation_filter_.Exists(cur, next)) {
            DCHECK(VerifyNaturalContent(next_seg->candidate(j),
                                        next_seg->candidate(0), RIGHT))
                << "IsNaturalContent() should not fail here.";
            seg->move_candidate(i, 0);
            seg->mutable_candidate(0)->attributes |=
                converter::Attribute::CONTEXT_SENSITIVE;
            next_seg->move_candidate(j, 0);
            next_seg->mutable_candidate(0)->attributes |=
                converter::Attribute::CONTEXT_SENSITIVE;
            return true;
          }
        }
      }
    }
  }
  return false;
}

}  // namespace mozc

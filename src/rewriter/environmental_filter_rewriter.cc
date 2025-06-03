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

#include "rewriter/environmental_filter_rewriter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/serialized_string_array.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "data_manager/emoji_data.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {
using AdditionalRenderableCharacterGroup =
    commands::Request::AdditionalRenderableCharacterGroup;

// Returns (base ** multiplier) % modulo.
constexpr int64_t Power(int64_t base, int multiplier, int64_t modulo) {
  if (multiplier <= 0) {
    return 1;
  }
  return (Power(base, multiplier - 1, modulo) * base) % modulo;
}

// Calculates Rolling Hash for given String.
// ref: https://en.wikipedia.org/wiki/Rolling_hash
class RollingHasher {
 public:
  // kBase and kModulo are choosed considering
  // 1. kModulo * kBase < 2**63
  // 2. kModulo is as large as possible
  // 3. kBase is coprime with kModulo and large.
  static constexpr int64_t kBase = 2147483634;
  static constexpr int64_t kModulo = 2147483647;
  static constexpr int kMaxLength = 15;
  static constexpr int64_t kPowers[kMaxLength + 1]{
      Power(kBase, 0, kModulo),  Power(kBase, 1, kModulo),
      Power(kBase, 2, kModulo),  Power(kBase, 3, kModulo),
      Power(kBase, 4, kModulo),  Power(kBase, 5, kModulo),
      Power(kBase, 6, kModulo),  Power(kBase, 7, kModulo),
      Power(kBase, 8, kModulo),  Power(kBase, 9, kModulo),
      Power(kBase, 10, kModulo), Power(kBase, 11, kModulo),
      Power(kBase, 12, kModulo), Power(kBase, 13, kModulo),
      Power(kBase, 14, kModulo), Power(kBase, 15, kModulo),
  };
  void append(const char32_t value) {
    hashes_.push_back((hashes_.back() * kBase + value) % kModulo);
  }
  void reserve(const size_t size) { hashes_.reserve(size); }
  // Calculates hash for [l, r) partial string for target.
  int64_t hash_between(int l, int r);

 private:
  std::vector<int64_t> hashes_ = {0};
};

inline int64_t RollingHasher::hash_between(int l, int r) {
  DCHECK_LT(l, r);
  // Because kPowers is only prepared for up to kMaxLength, check it required.
  if (r - l > kMaxLength) {
    DLOG(ERROR) << "The hash length is more than the max: " << kMaxLength;
    l = r - kMaxLength;
  }
  // Enforce modulo to be non-negative.
  // This function is optimized. Intended implementation is
  // return (hashes_[r] - kPowers[r - l] * hashes_[l]) % kModulo;
  const int64_t d = hashes_[r] - (kPowers[r - l] * hashes_[l]) % kModulo;
  if (d >= 0) {
    return d;
  } else {
    return d + kModulo;
  }
}

bool CheckCodepointsAcceptable(const std::u32string_view codepoints) {
  for (const char32_t c : codepoints) {
    if (!Util::IsAcceptableCharacterAsCandidate(c)) {
      return false;
    }
  }
  return true;
}

bool FindCodepointsInClosedRange(const std::u32string_view codepoints,
                                 const char32_t left, const char32_t right) {
  for (const char32_t c : codepoints) {
    if (left <= c && c <= right) {
      return true;
    }
  }
  return false;
}

std::vector<AdditionalRenderableCharacterGroup> GetNonrenderableGroups(
    const ::mozc::protobuf::RepeatedField<int> &additional_groups) {
  // WARNING: Though it is named k'All'Cases, 'Empty' is intentionally omitted
  // here. All other cases should be added.
  constexpr std::array<AdditionalRenderableCharacterGroup, 12> kAllCases = {
      commands::Request::KANA_SUPPLEMENT_6_0,
      commands::Request::KANA_SUPPLEMENT_AND_KANA_EXTENDED_A_10_0,
      commands::Request::KANA_EXTENDED_A_14_0,
      commands::Request::EMOJI_12_1,
      commands::Request::EMOJI_13_0,
      commands::Request::EMOJI_13_1,
      commands::Request::EMOJI_14_0,
      commands::Request::EMOJI_15_0,
      commands::Request::EMOJI_15_1,
      commands::Request::EMOJI_16_0,
      commands::Request::EGYPTIAN_HIEROGLYPH_5_2,
      commands::Request::IVS_CHARACTER,
  };

  std::vector<AdditionalRenderableCharacterGroup> result;
  for (const AdditionalRenderableCharacterGroup group : kAllCases) {
    if (std::find(additional_groups.begin(), additional_groups.end(), group) !=
        additional_groups.end()) {
      continue;
    }
    result.push_back(group);
  }
  return result;
}

// If the candidate should not by modified by this rewriter, returns true.
bool ShouldKeepCandidate(const converter::Candidate &candidate) {
  return candidate.attributes & (converter::Candidate::NO_MODIFICATION |
                                 converter::Candidate::USER_DICTIONARY);
}

bool NormalizeCandidate(converter::Candidate *candidate,
                        TextNormalizer::Flag flag) {
  DCHECK(candidate);
  // ShouldKeepCandidate should be called before.
  const std::string value =
      TextNormalizer::NormalizeTextWithFlag(candidate->value, flag);
  const std::string content_value =
      TextNormalizer::NormalizeTextWithFlag(candidate->content_value, flag);

  if (content_value == candidate->content_value && value == candidate->value) {
    // No update.
    return false;
  }

  candidate->value = std::move(value);
  candidate->content_value = std::move(content_value);
  // Clear the description which might be wrong.
  candidate->description.clear();

  return true;
}

EmojiDataIterator begin(const absl::string_view token_array_data) {
  return EmojiDataIterator(token_array_data.data());
}

EmojiDataIterator end(const absl::string_view token_array_data) {
  return EmojiDataIterator(token_array_data.data() + token_array_data.size());
}

absl::flat_hash_map<EmojiVersion, std::vector<std::u32string>>
ExtractTargetEmojis(
    absl::Span<const EmojiVersion> target_versions,
    const std::pair<EmojiDataIterator, EmojiDataIterator> &range,
    const SerializedStringArray &string_array) {
  absl::flat_hash_map<EmojiVersion, std::vector<std::u32string>> results;
  for (const EmojiVersion target_version : target_versions) {
    results[target_version] = {};
  }
  for (auto iter = range.first; iter != range.second; ++iter) {
    const uint32_t unicode_version_index = iter.unicode_version_index();
    // unicode_version_index will not be negative.
    if (unicode_version_index > EMOJI_MAX_VERSION) {
      continue;
    }
    const EmojiVersion version =
        static_cast<EmojiVersion>(unicode_version_index);
    if (results.find(version) == results.end()) {
      continue;
    }
    const absl::string_view utf8_emoji = string_array[iter.emoji_index()];
    results[version].push_back(Util::Utf8ToUtf32(utf8_emoji));
  }
  return results;
}

std::u32string SortAndUnique(std::u32string_view codepoints) {
  std::u32string result(codepoints);
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

}  // namespace

void CharacterGroupFinder::Initialize(
    absl::Span<const std::u32string> target_codepoints) {
  std::u32string single_codepoints;
  for (const auto &codepoints : target_codepoints) {
    const size_t size = codepoints.size();
    if (size == 1) {
      single_codepoints.push_back(codepoints[0]);
    } else {
      max_length_ = std::max(max_length_, size);
      RollingHasher hasher = RollingHasher();
      hasher.reserve(size);
      for (const char32_t codepoint : codepoints) {
        hasher.append(codepoint);
      }
      multiple_codepoints_.push_back(std::move(codepoints));
      multiple_codepoints_hashes_.push_back(hasher.hash_between(0, size));
    }
  }

  std::sort(multiple_codepoints_hashes_.begin(),
            multiple_codepoints_hashes_.end());

  // Create intersection of multiple_codepoints_ to use early return key in
  // search algorithm.
  if (!multiple_codepoints_.empty()) {
    std::u32string intersection = SortAndUnique(multiple_codepoints_[0]);
    for (const std::u32string &codepoints : multiple_codepoints_) {
      std::u32string new_intersection;
      const std::u32string cp_set = SortAndUnique(codepoints);
      std::set_intersection(
          cp_set.begin(), cp_set.end(), intersection.begin(),
          intersection.end(),
          std::inserter(new_intersection, new_intersection.end()));
      intersection = std::move(new_intersection);
    }
    sorted_multiple_codepoints_intersection_ = std::move(intersection);
  }

  // sort and summarize them into range;
  std::sort(single_codepoints.begin(), single_codepoints.end());
  if (!single_codepoints.empty()) {
    min_single_codepoint_ = single_codepoints[0];
  }
  char32_t last_left = 0;
  char32_t last_right = 0;
  for (const char32_t codepoint : single_codepoints) {
    if (last_right == 0) {
      last_left = codepoint;
      last_right = codepoint;
      continue;
    }
    if (last_right + 1 == codepoint) {
      last_right += 1;
      continue;
    }
    sorted_single_codepoint_lefts_.push_back(last_left);
    sorted_single_codepoint_rights_.push_back(last_right);
    last_left = codepoint;
    last_right = codepoint;
  }
  if (last_right != 0) {
    sorted_single_codepoint_lefts_.push_back(last_left);
    sorted_single_codepoint_rights_.push_back(last_right);
  }
}

bool CharacterGroupFinder::FindMatch(const std::u32string_view target) const {
  for (const char32_t codepoint : target) {
    // Single codepoint check
    // If codepoint is smaller than min value, continue before executing binary
    // search.
    if (codepoint < min_single_codepoint_) {
      continue;
    }
    const auto position =
        std::upper_bound(sorted_single_codepoint_lefts_.begin(),
                         sorted_single_codepoint_lefts_.end(), codepoint);
    const auto index_upper =
        std::distance(sorted_single_codepoint_lefts_.begin(), position);
    if (index_upper != 0 &&
        codepoint <= sorted_single_codepoint_rights_[index_upper - 1]) {
      return true;
    }
  }

  // If target does not contain any intersection of multiple_codepoints_, return
  // false here.
  for (const char32_t codepoint : sorted_multiple_codepoints_intersection_) {
    if (std::find(target.begin(), target.end(), codepoint) == target.end()) {
      return false;
    }
  }

  // Multiple Codepoint Check.
  RollingHasher hasher = RollingHasher();
  hasher.reserve(target.size());
  for (int right = 0; right < target.size(); ++right) {
    hasher.append(target[right]);
    for (int left = right - 1; left >= 0; --left) {
      if (right - left > max_length_) {
        break;
      }
      // Example:
      //  For codepoints {0x0, 0x1, 0x2, 0x3, 0x4} and left = 1 and right = 3,
      //  `hash` is hash for {0x1, 0x2}
      const int64_t hash = hasher.hash_between(left, right + 1);
      if (std::binary_search(multiple_codepoints_hashes_.begin(),
                             multiple_codepoints_hashes_.end(), hash)) {
        // As hash can collide in some unfortunate case, double-check here.
        const std::u32string_view hashed_target =
            target.substr(left, right - left + 1);
        for (const std::u32string &codepoints : multiple_codepoints_) {
          if (hashed_target == codepoints) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

int EnvironmentalFilterRewriter::capability(
    const ConversionRequest &request) const {
  return RewriterInterface::ALL;
}

EnvironmentalFilterRewriter::EnvironmentalFilterRewriter(
    const DataManager &data_manager) {
  absl::string_view token_array_data;
  absl::string_view string_array_data;

  // TODO(mozc-team):
  // Currently, this rewriter uses data from emoji_data.tsv, which is for Emoji
  // conversion, as a source of Emoji version information. However,
  // emoji_data.tsv lacks some Emoji, including Emoji with skin-tones and
  // family/couple Emojis. As a future work, the data source should be refined.
  data_manager.GetEmojiRewriterData(&token_array_data, &string_array_data);
  SerializedStringArray string_array;
  string_array.Set(string_array_data);
  std::pair<EmojiDataIterator, EmojiDataIterator> range =
      std::make_pair(begin(token_array_data), end(token_array_data));
  const absl::flat_hash_map<EmojiVersion, std::vector<std::u32string>>
      version_to_targets = ExtractTargetEmojis(
          {EmojiVersion::E12_1, EmojiVersion::E13_0, EmojiVersion::E13_1,
           EmojiVersion::E14_0, EmojiVersion::E15_0, EmojiVersion::E15_1,
           EmojiVersion::E16_0},
          range, string_array);
  finder_e12_1_.Initialize(version_to_targets.at(EmojiVersion::E12_1));
  finder_e13_0_.Initialize(version_to_targets.at(EmojiVersion::E13_0));
  finder_e13_1_.Initialize(version_to_targets.at(EmojiVersion::E13_1));
  finder_e14_0_.Initialize(version_to_targets.at(EmojiVersion::E14_0));
  finder_e15_0_.Initialize(version_to_targets.at(EmojiVersion::E15_0));
  finder_e15_1_.Initialize(version_to_targets.at(EmojiVersion::E15_1));
  finder_e16_0_.Initialize(version_to_targets.at(EmojiVersion::E16_0));
}

bool EnvironmentalFilterRewriter::Rewrite(const ConversionRequest &request,
                                          Segments *segments) const {
  DCHECK(segments);
  const std::vector<AdditionalRenderableCharacterGroup> nonrenderable_groups =
      GetNonrenderableGroups(
          request.request().additional_renderable_character_groups());

  bool modified = false;
  for (Segment &segment : segments->conversion_segments()) {
    // Meta candidate
    for (size_t j = 0; j < segment.meta_candidates_size(); ++j) {
      converter::Candidate *candidate = segment.mutable_meta_candidate(j);
      DCHECK(candidate);
      if (ShouldKeepCandidate(*candidate)) {
        continue;
      }
      modified |= NormalizeCandidate(candidate, flag_);
    }

    // Regular candidate.
    const size_t candidates_size = segment.candidates_size();

    for (size_t j = 0; j < candidates_size; ++j) {
      const size_t reversed_j = candidates_size - j - 1;
      converter::Candidate *candidate = segment.mutable_candidate(reversed_j);
      DCHECK(candidate);

      if (ShouldKeepCandidate(*candidate)) {
        continue;
      }

      // Character Normalization
      modified |= NormalizeCandidate(candidate, flag_);

      const std::u32string codepoints = Util::Utf8ToUtf32(candidate->value);

      // Check acceptability of code points as a candidate.
      if (!CheckCodepointsAcceptable(codepoints)) {
        segment.erase_candidate(reversed_j);
        modified = true;
        continue;
      }

      // WARNING: Current implementation assumes cases are mutually exclusive.
      // If that assumption becomes no longer correct, revise this
      // implementation.
      //
      // Performance Notes:
      // - Order for checking impacts performance. It is ideal to re-order
      // character groups into often-hit order.
      // - Some groups can be merged when they are both rejected, For example,
      // if KANA_SUPPLEMENT_6_0 and KANA_SUPPLEMENT_AND_KANA_EXTENDED_A_10_0 are
      // both rejected, range can be [0x1B000, 0x1B11E], and then the number of
      // check can be reduced.
      for (const AdditionalRenderableCharacterGroup group :
           nonrenderable_groups) {
        bool found_nonrenderable = false;
        // Come here when the group is un-adapted option.
        // For this switch statement, 'default' case should not be added. For
        // enum, compiler can check exhaustiveness, so that compiler will cause
        // compile error when enum case is added but not handled. On the other
        // hand, if 'default' statement is added, compiler will say nothing even
        // though there are unhandled enum case.
        switch (group) {
          case commands::Request::EMPTY:
            break;
          case commands::Request::KANA_SUPPLEMENT_6_0:
            found_nonrenderable =
                FindCodepointsInClosedRange(codepoints, 0x1B000, 0x1B001);
            break;
          case commands::Request::KANA_SUPPLEMENT_AND_KANA_EXTENDED_A_10_0:
            found_nonrenderable =
                FindCodepointsInClosedRange(codepoints, 0x1B002, 0x1B11E);
            break;
          case commands::Request::KANA_EXTENDED_A_14_0:
            found_nonrenderable =
                FindCodepointsInClosedRange(codepoints, 0x1B11F, 0x1B122);
            break;
          case commands::Request::EMOJI_12_1:
            found_nonrenderable = finder_e12_1_.FindMatch(codepoints);
            break;
          case commands::Request::EMOJI_13_0:
            found_nonrenderable = finder_e13_0_.FindMatch(codepoints);
            break;
          case commands::Request::EMOJI_13_1:
            found_nonrenderable = finder_e13_1_.FindMatch(codepoints);
            break;
          case commands::Request::EMOJI_14_0:
            found_nonrenderable = finder_e14_0_.FindMatch(codepoints);
            break;
          case commands::Request::EMOJI_15_0:
            found_nonrenderable = finder_e15_0_.FindMatch(codepoints);
            break;
          case commands::Request::EMOJI_15_1:
            found_nonrenderable = finder_e15_1_.FindMatch(codepoints);
            break;
          case commands::Request::EMOJI_16_0:
            found_nonrenderable = finder_e16_0_.FindMatch(codepoints);
            break;
          case commands::Request::EGYPTIAN_HIEROGLYPH_5_2:
            found_nonrenderable =
                FindCodepointsInClosedRange(codepoints, 0x13000, 0x1342E);
            break;
          case commands::Request::IVS_CHARACTER:
            found_nonrenderable =
                FindCodepointsInClosedRange(codepoints, 0xE0100, 0xE010E);
            break;
        }
        if (found_nonrenderable) {
          segment.erase_candidate(reversed_j);
          modified = true;
          break;
        }
      }
    }
  }

  return modified;
}
}  // namespace mozc

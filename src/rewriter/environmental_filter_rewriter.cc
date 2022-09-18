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
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/protobuf/protobuf.h"
#include "base/serialized_string_array.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "converter/segments.h"
#include "data_manager/emoji_data.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "absl/strings/string_view.h"

namespace mozc {
using AdditionalRenderableCharacterGroup =
    commands::Request::AdditionalRenderableCharacterGroup;

namespace {

bool CheckCodepointsAcceptable(const std::vector<char32_t> &codepoints) {
  for (const char32_t c : codepoints) {
    if (!Util::IsAcceptableCharacterAsCandidate(c)) {
      return false;
    }
  }
  return true;
}

bool FindCodepointsInClosedRange(const std::vector<char32_t> &codepoints,
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
  constexpr std::array<AdditionalRenderableCharacterGroup, 8> kAllCases = {
      commands::Request::KANA_SUPPLEMENT_6_0,
      commands::Request::KANA_SUPPLEMENT_AND_KANA_EXTENDED_A_10_0,
      commands::Request::KANA_EXTENDED_A_14_0,
      commands::Request::EMOJI_12_1,
      commands::Request::EMOJI_13_0,
      commands::Request::EMOJI_13_1,
      commands::Request::EMOJI_14_0,
      commands::Request::EMOJI_15_0,
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

bool NormalizeCandidate(Segment::Candidate *candidate,
                        TextNormalizer::Flag flag) {
  DCHECK(candidate);
  if (candidate->attributes & (Segment::Candidate::NO_MODIFICATION |
                               Segment::Candidate::USER_DICTIONARY)) {
    return false;
  }

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

std::map<EmojiVersion, std::vector<std::vector<char32_t>>> ExtractTargetEmojis(
    const std::vector<EmojiVersion> &target_versions,
    const std::pair<EmojiDataIterator, EmojiDataIterator> &range,
    const SerializedStringArray &string_array) {
  std::map<EmojiVersion, std::vector<std::vector<char32_t>>> results;
  for (const EmojiVersion target_version : target_versions) {
    results[target_version] = {};
  }
  for (auto iter = range.first; iter != range.second; ++iter) {
    const EmojiVersion version =
        static_cast<EmojiVersion>(iter.unicode_version_index());
    if (results.find(version) == results.end()) {
      continue;
    }
    const absl::string_view utf8_emoji = string_array[iter.emoji_index()];
    const std::vector<char32_t> codepoints = Util::Utf8ToCodepoints(utf8_emoji);
    results[version].push_back(std::move(codepoints));
  }
  return results;
}

}  // namespace

void CharacterGroupFinder::Initialize(
    const std::vector<std::vector<char32_t>> &target_codepoints) {
  std::vector<char32_t> single_codepoints;

  for (const auto &codepoints : target_codepoints) {
    if (codepoints.size() == 1) {
      single_codepoints.push_back(codepoints[0]);
    } else {
      multiple_codepoints_.push_back(codepoints);
    }
  }

  // sort and summarize them into range;
  std::sort(single_codepoints.begin(), single_codepoints.end());
  std::pair<char32_t, char32_t> last_item = {
      0, 0};  // initialize with null character
  for (const char32_t codepoint : single_codepoints) {
    if (last_item.second == 0) {
      last_item = std::make_pair(codepoint, codepoint);
      continue;
    }
    if (last_item.second + 1 == codepoint) {
      last_item.second += 1;
      continue;
    }
    single_codepoint_ranges_.push_back(last_item);
    last_item = std::make_pair(codepoint, codepoint);
  }
  if (last_item.second != 0) {
    single_codepoint_ranges_.push_back(last_item);
  }
}

// Current implementation is brute-force search.
// It is possible to use Rabin-Karp search or other possibly faster algorithm.
bool CharacterGroupFinder::FindMatch(
    const std::vector<char32_t> &target) const {
  for (const char32_t codepoint : target) {
    // Single codepoint check
    for (const auto &[left, right] : single_codepoint_ranges_) {
      if (left <= codepoint && codepoint <= right) {
        return true;
      }
    }
  }
  // Multiple codepoints check
  for (const std::vector<char32_t> &codepoints : multiple_codepoints_) {
    if (target.end() != std::search(target.begin(), target.end(),
                                    codepoints.begin(), codepoints.end())) {
      return true;
    }
  }
  return false;
}

int EnvironmentalFilterRewriter::capability(
    const ConversionRequest &request) const {
  return RewriterInterface::ALL;
}

EnvironmentalFilterRewriter::EnvironmentalFilterRewriter(
    const DataManagerInterface &data_manager) {
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
  const std::map<EmojiVersion, std::vector<std::vector<char32_t>>>
      version_to_targets = ExtractTargetEmojis(
          {EmojiVersion::E12_1, EmojiVersion::E13_0, EmojiVersion::E13_1,
           EmojiVersion::E14_0, EmojiVersion::E15_0},
          range, string_array);
  finder_e12_1_.Initialize(version_to_targets.at(EmojiVersion::E12_1));
  finder_e13_0_.Initialize(version_to_targets.at(EmojiVersion::E13_0));
  finder_e13_1_.Initialize(version_to_targets.at(EmojiVersion::E13_1));
  finder_e14_0_.Initialize(version_to_targets.at(EmojiVersion::E14_0));
  finder_e15_0_.Initialize(version_to_targets.at(EmojiVersion::E15_0));
}

bool EnvironmentalFilterRewriter::Rewrite(const ConversionRequest &request,
                                          Segments *segments) const {
  DCHECK(segments);
  const std::vector<AdditionalRenderableCharacterGroup> nonrenderable_groups =
      GetNonrenderableGroups(
          request.request().additional_renderable_character_groups());

  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    DCHECK(segment);

    // Meta candidate
    for (size_t j = 0; j < segment->meta_candidates_size(); ++j) {
      Segment::Candidate *candidate =
          segment->mutable_candidate(-static_cast<int>(j) - 1);
      DCHECK(candidate);
      modified |= NormalizeCandidate(candidate, flag_);
    }

    // Regular candidate.
    const size_t candidates_size = segment->candidates_size();

    for (size_t j = 0; j < candidates_size; ++j) {
      const size_t reversed_j = candidates_size - j - 1;
      Segment::Candidate *candidate = segment->mutable_candidate(reversed_j);
      DCHECK(candidate);

      // Character Normalization
      modified |= NormalizeCandidate(candidate, flag_);

      const std::vector<char32_t> codepoints =
          Util::Utf8ToCodepoints(candidate->value);

      // Check acceptability of code points as a candidate.
      if (!CheckCodepointsAcceptable(codepoints)) {
        segment->erase_candidate(reversed_j);
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
          case commands::Request::EGYPTIAN_HIEROGLYPH_5_2:
            found_nonrenderable =
                FindCodepointsInClosedRange(codepoints, 0x13000, 0x1342E);
            break;
        }
        if (found_nonrenderable) {
          segment->erase_candidate(reversed_j);
          modified = true;
          break;
        }
      }
    }
  }

  return modified;
}
}  // namespace mozc

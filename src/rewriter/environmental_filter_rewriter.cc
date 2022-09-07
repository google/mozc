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
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/protobuf/protobuf.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"

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
  constexpr std::array<AdditionalRenderableCharacterGroup, 3> kAllCases = {
      commands::Request::KANA_SUPPLEMENT_6_0,
      commands::Request::KANA_SUPPLEMENT_AND_KANA_EXTENDED_A_10_0,
      commands::Request::KANA_EXTENDED_A_14_0,
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
}  // namespace

int EnvironmentalFilterRewriter::capability(
    const ConversionRequest &request) const {
  return RewriterInterface::ALL;
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

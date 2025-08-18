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

#include "rewriter/emoji_rewriter.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "base/japanese_util.h"
#include "base/strings/assign.h"
#include "base/vlog.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "data_manager/emoji_data.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_util.h"

// EmojiRewriter:
// Converts HIRAGANA strings to emoji characters, if they are names of emojis.

namespace mozc {
namespace {

constexpr absl::string_view kEmoji = "絵文字";
constexpr absl::string_view kEmojiKey = "えもじ";
// Where to insert emoji candidate by default.
constexpr size_t kDefaultInsertPos = 6;

// List of <emoji value, emoji description>.
using EmojiEntryList =
    std::vector<std::pair<absl::string_view, absl::string_view>>;

std::unique_ptr<converter::Candidate> CreateCandidate(
    absl::string_view key, absl::string_view value,
    absl::string_view description, int cost) {
  auto candidate = std::make_unique<converter::Candidate>();
  // Fill 0 (BOS/EOS) pos code intentionally.
  candidate->lid = 0;
  candidate->rid = 0;
  candidate->cost = cost;
  strings::Assign(candidate->value, value);
  strings::Assign(candidate->content_value, value);
  strings::Assign(candidate->key, key);
  strings::Assign(candidate->content_key, key);
  if (description.empty()) {
    strings::Assign(candidate->description, kEmoji);
  } else {
    candidate->description = absl::StrCat(kEmoji, " ", description);
  }
  candidate->attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;
  candidate->attributes |= converter::Attribute::CONTEXT_SENSITIVE;
  candidate->category = converter::Candidate::SYMBOL;

  return candidate;
}

int GetEmojiCost(const Segment& segment) {
  // Use the first candidate's cost (or 0 if not available).
  return segment.candidates_size() == 0 ? 0 : segment.candidate(0).cost;
}

void GatherAllEmojiData(EmojiDataIterator begin, EmojiDataIterator end,
                        const SerializedStringArray& string_array,
                        EmojiEntryList* utf8_emoji_list) {
  for (; begin != end; ++begin) {
    absl::string_view utf8_emoji = string_array[begin.emoji_index()];
    if (utf8_emoji.empty()) {
      continue;
    }
    utf8_emoji_list->emplace_back(utf8_emoji,
                                  string_array[begin.description_utf8_index()]);
  }

  std::sort(utf8_emoji_list->begin(), utf8_emoji_list->end());
}

std::vector<std::unique_ptr<converter::Candidate>> CreateAllEmojiData(
    absl::string_view key, const int cost, EmojiEntryList utf8_emoji_list) {
  std::vector<std::unique_ptr<converter::Candidate>> candidates;
  candidates.reserve(utf8_emoji_list.size());
  for (const auto& emoji_entry : utf8_emoji_list) {
    candidates.push_back(
        CreateCandidate(key, emoji_entry.first, emoji_entry.second, cost));
  }
  return candidates;
}

std::vector<std::unique_ptr<converter::Candidate>> CreateEmojiData(
    absl::string_view key, const int cost, EmojiRewriter::IteratorRange range,
    const SerializedStringArray& string_array) {
  std::vector<std::unique_ptr<converter::Candidate>> candidates;
  candidates.reserve(range.second - range.first);
  for (auto iter = range.first; iter != range.second; ++iter) {
    absl::string_view utf8_emoji = string_array[iter.emoji_index()];
    if (utf8_emoji.empty()) {
      continue;
    }
    candidates.push_back(CreateCandidate(
        key, utf8_emoji, string_array[iter.description_utf8_index()], cost));
  }
  return candidates;
}
}  // namespace

EmojiRewriter::EmojiRewriter(const DataManager& data_manager) {
  absl::string_view string_array_data;
  data_manager.GetEmojiRewriterData(&token_array_data_, &string_array_data);
  DCHECK(SerializedStringArray::VerifyData(string_array_data));
  string_array_.Set(string_array_data);
}

int EmojiRewriter::capability(const ConversionRequest& request) const {
  // The capability of the EmojiRewriter is up to the client's request.
  // Note that the bit representation of RewriterInterface::CapabilityType
  // and Request::RewriterCapability should exactly same, so it is ok
  // to just return the value as is.
  return request.request().emoji_rewriter_capability();
}

bool EmojiRewriter::Rewrite(const ConversionRequest& request,
                            Segments* segments) const {
  if (!request.config().use_emoji_conversion()) {
    MOZC_VLOG(2) << "no use_emoji_conversion";
    return false;
  }

  CHECK(segments != nullptr);
  return RewriteCandidates(segments);
}

bool EmojiRewriter::IsEmojiCandidate(const converter::Candidate& candidate) {
  return absl::StrContains(candidate.description, kEmoji);
}

std::pair<EmojiDataIterator, EmojiDataIterator> EmojiRewriter::LookUpToken(
    absl::string_view key) const {
  // Search string array for key.
  auto iter = std::lower_bound(string_array_.begin(), string_array_.end(), key);
  if (iter == string_array_.end() || *iter != key) {
    return std::pair<EmojiDataIterator, EmojiDataIterator>(end(), end());
  }
  // Search token array for the string index.
  return std::equal_range(begin(), end(), iter.index());
}

bool EmojiRewriter::RewriteCandidates(Segments* segments) const {
  bool modified = false;
  std::string reading;

  auto insert_candidates =
      [](std::vector<std::unique_ptr<converter::Candidate>>&& cands,
         Segment* segment) -> bool {
    if (cands.empty()) {
      return false;
    }
    const size_t insert_position =
        RewriterUtil::CalculateInsertPosition(*segment, kDefaultInsertPos);
    segment->insert_candidates(insert_position, std::move(cands));
    return true;
  };

  for (Segment& segment : segments->conversion_segments()) {
    reading = japanese_util::FullWidthAsciiToHalfWidthAscii(segment.key());
    if (reading.empty()) {
      continue;
    }

    if (reading == kEmojiKey) {
      // When key is "えもじ", we expect to expand all Emoji characters.
      EmojiEntryList utf8_emoji_list;
      GatherAllEmojiData(begin(), end(), string_array_, &utf8_emoji_list);
      if (utf8_emoji_list.empty()) {
        continue;
      }

      const int cost = GetEmojiCost(segment);
      std::vector<std::unique_ptr<converter::Candidate>> candidates =
          CreateAllEmojiData(reading, cost, utf8_emoji_list);
      modified |= insert_candidates(std::move(candidates), &segment);
      continue;
    }

    const auto range = LookUpToken(reading);
    if (range.first == range.second) {
      MOZC_VLOG(2) << "Token not found: " << reading;
      continue;
    }

    const int cost = GetEmojiCost(segment);
    std::vector<std::unique_ptr<converter::Candidate>> candidates =
        CreateEmojiData(reading, cost, range, string_array_);
    modified |= insert_candidates(std::move(candidates), &segment);
  }
  return modified;
}

}  // namespace mozc

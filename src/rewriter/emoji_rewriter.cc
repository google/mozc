// Copyright 2010-2018, Google Inc.
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
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "usage_stats/usage_stats.h"

// EmojiRewriter:
// Converts HIRAGANA strings to emoji characters, if they are names of emojis.

namespace mozc {

using commands::Request;

namespace {

const char kEmoji[] = "絵文字";
const char kEmojiKey[] = "えもじ";
// Where to insert emoji candidate by default.
const size_t kDefaultInsertPos = 6;

// Inserts a candidate to the segment at insert_position.
// Returns true if succeeded, otherwise false. Also, if succeeded, increments
// the insert_position to represent the next insert position.
bool InsertCandidate(StringPiece key,
                     StringPiece value,
                     StringPiece description,
                     int cost,
                     Segment *segment,
                     size_t *insert_position) {
  Segment::Candidate *candidate = segment->insert_candidate(*insert_position);
  if (candidate == NULL) {
    LOG(ERROR) << "cannot insert candidate at " << insert_position
               << "th position nor tail of candidates.";
    return false;
  }
  ++*insert_position;

  candidate->Init();
  // Fill 0 (BOS/EOS) pos code intentionally.
  candidate->lid = 0;
  candidate->rid = 0;
  candidate->cost = cost;
  candidate->value.assign(value.data(), value.size());
  candidate->content_value.assign(value.data(), value.size());
  candidate->key.assign(key.data(), key.size());
  candidate->content_key.assign(key.data(), key.size());
  candidate->description.assign(kEmoji);
  if (!description.empty()) {
    Util::AppendStringWithDelimiter(
        " ", description, &(candidate->description));
  }
  candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  candidate->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;

  return true;
}

// Merges two descriptions.  Connects them if one is not a substring of the
// other.
void AddDescription(StringPiece adding, std::vector<string> *descriptions) {
  DCHECK(descriptions);
  if (adding.empty()) {
    return;
  }

  // Add |adding| if it matches with no elements of |descriptions|.
  for (size_t i = 0; i < descriptions->size(); ++i) {
    if (adding == (*descriptions)[i]) {
      return;
    }
  }

  descriptions->emplace_back(adding.data(), adding.size());
}

bool InsertEmojiData(StringPiece key,
                     EmojiRewriter::EmojiDataIterator iter,
                     const SerializedStringArray &string_array,
                     int cost,
                     int32 available_carrier,
                     Segment *segment,
                     size_t *insert_position) {
  bool inserted = false;

  StringPiece utf8_emoji = string_array[iter.emoji_index()];

  // Fill a candidate of Unicode 6.0 emoji.
  if ((available_carrier & Request::UNICODE_EMOJI) && !utf8_emoji.empty()) {
    inserted |= InsertCandidate(
        key, utf8_emoji, string_array[iter.description_utf8_index()],
        cost, segment, insert_position);
  }

  std::vector<string> descriptions;
  if (available_carrier & Request::DOCOMO_EMOJI) {
    AddDescription(string_array[iter.description_docomo_index()],
                   &descriptions);
  }
  if (available_carrier & Request::SOFTBANK_EMOJI) {
    AddDescription(string_array[iter.description_softbank_index()],
                   &descriptions);
  }
  if (available_carrier & Request::KDDI_EMOJI) {
    AddDescription(string_array[iter.description_kddi_index()], &descriptions);
  }

  if (!descriptions.empty()) {
    // Encode the PUA code point to utf8 and fill it to candidate.
    string android_pua;
    string description;
    Util::UCS4ToUTF8Append(iter.android_pua(), &android_pua);
    Util::JoinStrings(descriptions, " ", &description);
    inserted |= InsertCandidate(
        key, android_pua, description.c_str(), cost, segment, insert_position);
  }

  return inserted;
}

int GetEmojiCost(const Segment &segment) {
  // Use the first candidate's cost (or 0 if not available).
  return segment.candidates_size() == 0 ? 0 : segment.candidate(0).cost;
}

bool InsertAllEmojiData(StringPiece key,
                        EmojiRewriter::EmojiDataIterator begin,
                        EmojiRewriter::EmojiDataIterator end,
                        const SerializedStringArray &string_array,
                        int32 available_carrier,
                        Segment *segment) {
  bool inserted = false;

  // Insert all candidates at the tail of the segment.
  size_t insert_position = segment->candidates_size();
  int cost = GetEmojiCost(*segment);
  for (; begin != end; ++begin) {
    inserted |= InsertEmojiData(key, begin, string_array, cost,
                                available_carrier,
                                segment, &insert_position);
  }
  return inserted;
}

bool InsertToken(StringPiece key,
                 EmojiRewriter::IteratorRange range,
                 const SerializedStringArray &string_array,
                 int32 available_carrier,
                 Segment *segment) {
  bool inserted = false;

  size_t insert_position =
      std::min(segment->candidates_size(), kDefaultInsertPos);
  int cost = GetEmojiCost(*segment);
  for (; range.first != range.second; ++range.first) {
    inserted |= InsertEmojiData(
        key, range.first, string_array, cost, available_carrier,
        segment, &insert_position);
  }
  return inserted;
}

}  // namespace

EmojiRewriter::EmojiRewriter(const DataManagerInterface &data_manager) {
  StringPiece string_array_data;
  data_manager.GetEmojiRewriterData(&token_array_data_, &string_array_data);
  DCHECK(SerializedStringArray::VerifyData(string_array_data));
  string_array_.Set(string_array_data);
}

EmojiRewriter::~EmojiRewriter() = default;

int EmojiRewriter::capability(const ConversionRequest &request) const {
  // The capability of the EmojiRewriter is up to the client's request.
  // Note that the bit representation of RewriterInterface::CapabilityType
  // and Request::RewriterCapability should exactly same, so it is ok
  // to just return the value as is.
  return request.request().emoji_rewriter_capability();
}

bool EmojiRewriter::Rewrite(const ConversionRequest &request,
                            Segments *segments) const {
  if (!request.config().use_emoji_conversion()) {
    VLOG(2) << "no use_emoji_conversion";
    return false;
  }

  int32 available_emoji_carrier = request.request().available_emoji_carrier();
  if (available_emoji_carrier == 0) {
    VLOG(2) << "No available emoji carrier.";
    return false;
  }

  CHECK(segments != NULL);
  return RewriteCandidates(available_emoji_carrier, segments);
}

void EmojiRewriter::Finish(const ConversionRequest &request,
                           Segments *segments) {
  if (!request.config().use_emoji_conversion()) {
    return;
  }

  // Update usage stats
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    const Segment &segment = segments->conversion_segment(i);
    // Ignores segments which are not converted or not committed.
    if (segment.candidates_size() == 0 ||
        segment.segment_type() != Segment::FIXED_VALUE) {
      continue;
    }

    // Check if the chosen candidate (index 0) is an emoji candidate.
    // The Mozc converter replaces committed candidates into the 0-th index.
    if (IsEmojiCandidate(segment.candidate(0))) {
      usage_stats::UsageStats::IncrementCount("CommitEmoji");
    }
  }
}

bool EmojiRewriter::IsEmojiCandidate(const Segment::Candidate &candidate) {
  return candidate.description.find(kEmoji) != string::npos;
}

std::pair<EmojiRewriter::EmojiDataIterator, EmojiRewriter::EmojiDataIterator>
EmojiRewriter::LookUpToken(StringPiece key) const {
  // Search string array for key.
  auto iter = std::lower_bound(string_array_.begin(), string_array_.end(), key);
  if (iter == string_array_.end() || *iter != key) {
    return std::pair<EmojiDataIterator, EmojiDataIterator>(end(), end());
  }
  // Search token array for the string index.
  return std::equal_range(begin(), end(), iter.index());
}

bool EmojiRewriter::RewriteCandidates(
    int32 available_emoji_carrier, Segments *segments) const {
  bool modified = false;
  string reading;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    Util::FullWidthAsciiToHalfWidthAscii(segment->key(), &reading);
    if (reading.empty()) {
      continue;
    }

    if (reading == kEmojiKey) {
      // When key is "えもじ", we expect to expand all Emoji characters.
      modified |= InsertAllEmojiData(reading, begin(), end(), string_array_,
                                     available_emoji_carrier, segment);
      continue;
    }
    const auto range = LookUpToken(reading);
    if (range.first == range.second) {
      VLOG(2) << "Token not found: " << reading;
      continue;
    }
    modified |= InsertToken(reading, range, string_array_,
                            available_emoji_carrier, segment);
  }
  return modified;
}

}  // namespace mozc

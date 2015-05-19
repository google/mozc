// Copyright 2010-2013, Google Inc.
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

#include "base/iterator_adapter.h"
#include "base/logging.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "session/commands.pb.h"
#include "usage_stats/usage_stats.h"

// EmojiRewriter:
// Converts HIRAGANA strings to emoji characters, if they are names of emojis.

namespace mozc {

using commands::Request;

namespace {

// Simple getter for Token::key.
struct GetTokenKey : public AdapterBase<const char *> {
  template<typename Iter>
  value_type operator()(Iter iter) const {
    return iter->key;
  }
};

// The lexicographical order comparator for the const char *.
struct ConstCharPtrLess {
  bool operator()(const char *s1, const char *s2) const {
    return strcmp(s1, s2) < 0;
  }
};

// "絵文字"
const char kEmoji[] = "\xE7\xB5\xB5\xE6\x96\x87\xE5\xAD\x97";
// "えもじ"
const char kEmojiKey[] = "\xE3\x81\x88\xE3\x82\x82\xE3\x81\x98";
// Where to insert emoji candidate by default.
const size_t kDefaultInsertPos = 6;

// Inserts a candidate to the segment at insert_position.
// Returns true if succeeded, otherwise false. Also, if succeeded, increments
// the insert_position to represent the next insert position.
bool InsertCandidate(const string &key,
                     const string &value,
                     const char *description,
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
  candidate->value = value;
  candidate->content_value = value;
  candidate->key = key;
  candidate->content_key = key;
  candidate->description.assign(kEmoji);
  if (description) {
    Util::AppendStringWithDelimiter(
        " ", description, &(candidate->description));
  }

  return true;
}

// Merges two descriptions.  Connects them if one is not a substring of the
// other.
void AddDescription(const char *adding, vector<string> *descriptions) {
  DCHECK(descriptions);
  if (adding == NULL) {
    return;
  }

  // Add |adding| if it matches with no elements of |descriptions|.
  for (size_t i = 0; i < descriptions->size(); ++i) {
    if (adding == (*descriptions)[i]) {
      return;
    }
  }

  descriptions->push_back(string(adding));
}

bool InsertEmojiData(const string &key,
                     const EmojiRewriter::EmojiData &emoji_data,
                     int cost,
                     int32 available_carrier,
                     Segment *segment,
                     size_t *insert_position) {
  bool inserted = false;

  // Fill a candidate of Unicode 6.0 emoji.
  if ((available_carrier & Request::UNICODE_EMOJI) &&
      emoji_data.unicode != NULL) {
    inserted |= InsertCandidate(
        key, emoji_data.unicode, emoji_data.description_unicode, cost, segment,
        insert_position);
  }

  vector<string> descriptions;
  if (available_carrier & Request::DOCOMO_EMOJI) {
    AddDescription(emoji_data.description_docomo, &descriptions);
  }
  if (available_carrier & Request::SOFTBANK_EMOJI) {
    AddDescription(emoji_data.description_softbank, &descriptions);
  }
  if (available_carrier & Request::KDDI_EMOJI) {
    AddDescription(emoji_data.description_kddi, &descriptions);
  }

  if (!descriptions.empty()) {
    // Encode the PUA code point to utf8 and fill it to candidate.
    string android_pua;
    string description;
    Util::UCS4ToUTF8Append(emoji_data.android_pua, &android_pua);
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

bool InsertAllEmojiData(const string &key,
                        const EmojiRewriter::EmojiData *emoji_data,
                        size_t emoji_data_size,
                        int32 available_carrier,
                        Segment *segment) {
  bool inserted = false;

  // Insert all candidates at the tail of the segment.
  size_t insert_position = segment->candidates_size();
  int cost = GetEmojiCost(*segment);
  for (size_t i = 0; i < emoji_data_size; ++i) {
    inserted |= InsertEmojiData(key, emoji_data[i], cost, available_carrier,
                                segment, &insert_position);
  }
  return inserted;
}

bool InsertToken(const string &key,
                 const EmojiRewriter::Token &token,
                 const EmojiRewriter::EmojiData *emoji_data,
                 int32 available_carrier,
                 Segment *segment) {
  bool inserted = false;

  size_t insert_position =
      min(segment->candidates_size(), kDefaultInsertPos);
  int cost = GetEmojiCost(*segment);
  for (size_t i = 0; i < token.value_size; ++i) {
    inserted |= InsertEmojiData(
        key, emoji_data[token.value[i]], cost, available_carrier,
        segment, &insert_position);
  }
  return inserted;
}

}  // namespace

EmojiRewriter::EmojiRewriter(
    const EmojiRewriter::EmojiData *emoji_data_list,
    size_t emoji_data_size,
    const EmojiRewriter::Token *token_list,
    size_t token_size,
    const uint16 *value_list)
    : emoji_data_list_(emoji_data_list), emoji_data_size_(emoji_data_size),
      token_list_(token_list), token_size_(token_size),
      value_list_(value_list) {
  DCHECK(emoji_data_list_ != NULL);
  DCHECK(token_list_ != NULL);
  DCHECK(value_list_ != NULL);
}

EmojiRewriter::~EmojiRewriter() {}

int EmojiRewriter::capability(const ConversionRequest &request) const {
  // The capability of the EmojiRewriter is up to the client's request.
  // Note that the bit representation of RewriterInterface::CapabilityType
  // and Request::RewriterCapability should exactly same, so it is ok
  // to just return the value as is.
  return request.request().emoji_rewriter_capability();
}

bool EmojiRewriter::Rewrite(const ConversionRequest &request,
                            Segments *segments) const {
  if (!mozc::config::ConfigHandler::GetConfig().use_emoji_conversion()) {
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

void EmojiRewriter::Finish(Segments *segments) {
  if (!mozc::config::ConfigHandler::GetConfig().use_emoji_conversion()) {
    return;
  }

  // Update usage stats
  for (size_t i = 0; i < segments->segments_size(); ++i) {
    const Segment &segment = segments->segment(i);
    // Ignores segments which are not converted or not committed.
    if (segment.candidates_size() <= 0 ||
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

const EmojiRewriter::Token *EmojiRewriter::LookUpToken(const string &key)
    const {
  const Token *token = lower_bound(
      MakeIteratorAdapter(token_list_, GetTokenKey()),
      MakeIteratorAdapter(token_list_ + token_size_, GetTokenKey()),
      key.c_str(),
      ConstCharPtrLess()).base();
  if (token == token_list_ + token_size_ || token->key != key) {
    // Not found.
    return NULL;
  }
  return token;
}

bool EmojiRewriter::RewriteCandidates(
    int32 available_emoji_carrier, Segments *segments) const {
  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    const string &reading = segment->key();
    if (reading.empty()) {
      LOG(ERROR) << "reading is empty";
      continue;
    }

    if (reading == kEmojiKey) {
      // When key is "えもじ", we expect to expand all Emoji characters.
      modified |= InsertAllEmojiData(
          reading, emoji_data_list_, emoji_data_size_,
          available_emoji_carrier, segment);
      continue;
    }

    const Token *token = LookUpToken(reading);
    if (token == NULL) {
      VLOG(2) << "Token not found: " << reading;
      continue;
    }
    modified |= InsertToken(
        reading, *token, emoji_data_list_, available_emoji_carrier, segment);
  }
  return modified;
}

}  // namespace mozc

// Copyright 2010, Google Inc.
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

#include "rewriter/fortune_rewriter.h"

#include <algorithm>
#include <ctime>
#include <string>
#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "converter/segments.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

enum FortuneType {
  FORTUNE_TYPE_EXCELLENT_LUCK  = 0,
  FORTUNE_TYPE_LUCK            = 1,
  FORTUNE_TYPE_MIDDLE_LUCK     = 2,
  FORTUNE_TYPE_LITTLE_LUCK     = 3,
  FORTUNE_TYPE_LUCK_AT_THE_END = 4,
  FORTUNE_TYPE_MISFORTUNE      = 5,
  NUM_FORTUNE_TYPES            = 6,
};

const int kMaxLevel = 100;
const int kNormalLevels[]     = { 20, 40, 60, 80, 90 };
const int kNewYearLevels[]    = { 30, 60, 80, 90, 95 };
const int kMyBirthdayLevels[] = { 30, 60, 80, 90, 95 };
const int kFriday13Levels[]   = { 10, 25, 40, 55, 70 };

bool IsValidFortuneType(FortuneType fortune_type) {
  return (fortune_type >= 0 && fortune_type < NUM_FORTUNE_TYPES);
}

class FortuneData {
 public:
  FortuneData()
      : fortune_type_(FORTUNE_TYPE_EXCELLENT_LUCK),
        last_update_yday_(-1),
        last_update_year_(-1) {
    ChangeFortune();
  }

  void ChangeFortune() {
    const int *levels = kNormalLevels;
    tm today;
    if (Util::GetCurrentTm(&today)) {
      // Modify once per one day
      if (today.tm_yday == last_update_yday_ &&
          today.tm_year == last_update_year_) {
        return;
      }
      last_update_yday_ = today.tm_yday;
      last_update_year_ = today.tm_year;
      // More happy at New Year's Day
      if (today.tm_yday == 0) {
        levels = kNewYearLevels;
      } else if (today.tm_mon == 2 && today.tm_mday == 3) {
        // It's my birthday :)
        levels = kMyBirthdayLevels;
      } else if (today.tm_mday == 13 && today.tm_wday == 5) {
        // Friday the 13th
        levels = kFriday13Levels;
      }
    }
    uint32 random = 0;
    Util::GetSecureRandomSequence(reinterpret_cast<char *>(&random),
                                  sizeof(random));
    const int level = random % kMaxLevel;
    for (int i = 0; i < arraysize(kNormalLevels); ++i) {
      if (level <= levels[i]) {
        fortune_type_ = static_cast<FortuneType>(i);
        break;
      }
    }
    DCHECK(IsValidFortuneType(fortune_type_));
  }

  FortuneType fortune_type() const { return fortune_type_; }

 private:
  FortuneType fortune_type_;
  int last_update_yday_;
  int last_update_year_;
};

// Insert Fortune message into the |segment|
// Only one fortune indicated by fortune_type is inserted at
// |insert_pos|. Return false if insersion is failed.
bool InsertCandidate(FortuneType fortune_type,
                     size_t insert_pos,
                     Segment *segment) {
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return false;
  }

  const Segment::Candidate& base_candidate = segment->candidate(0);
  size_t offset = min(insert_pos, segment->candidates_size());

  Segment::Candidate *c = segment->insert_candidate(offset);
  if (c == NULL) {
    LOG(ERROR) << "cannot insert candidate at " << offset;
    return false;
  }
  const Segment::Candidate &trigger_c = segment->candidate(offset - 1);

  string value;
  switch (fortune_type) {
    case FORTUNE_TYPE_EXCELLENT_LUCK:  // "大吉"
      value = "\xE5\xA4\xA7\xE5\x90\x89";
      break;
    case FORTUNE_TYPE_LUCK:  // "吉"
      value = "\xE5\x90\x89";
      break;
    case FORTUNE_TYPE_MIDDLE_LUCK:  // "中吉"
      value = "\xE4\xB8\xAD\xE5\x90\x89";
      break;
    case FORTUNE_TYPE_LITTLE_LUCK:  // "小吉"
      value = "\xE5\xB0\x8F\xE5\x90\x89";
      break;
    case FORTUNE_TYPE_LUCK_AT_THE_END:  // "末吉"
      value = "\xE6\x9C\xAB\xE5\x90\x89";
      break;
    case FORTUNE_TYPE_MISFORTUNE:  // "凶"
      value = "\xE5\x87\xB6";
      break;
    default:
      LOG(FATAL) << "undefined fortune type";
      return false;
  }

  c->Init();
  c->lid = trigger_c.lid;
  c->rid = trigger_c.rid;
  c->cost = trigger_c.cost;
  c->value = value;
  c->content_value = value;
  c->key = base_candidate.key;
  c->content_key = base_candidate.content_key;
  c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  c->attributes |= Segment::Candidate::NO_LEARNING;
  // discription "今日の運勢"
  c->description =
      "\xE4\xBB\x8A\xE6\x97\xA5\xE3\x81\xAE\xE9\x81\x8B\xE5\x8B\xA2";
  return true;
}

}  // namespace

FortuneRewriter::FortuneRewriter() {}

FortuneRewriter::~FortuneRewriter() {}

bool FortuneRewriter::Rewrite(Segments *segments) const {
  if (segments->conversion_segments_size() != 1) {
    return false;
  }

  const Segment &segment = segments->conversion_segment(0);
  const string &key = segment.key();
  if (key.empty()) {
    LOG(ERROR) << "Key is empty";
    return false;
  }

  // "おみくじ"
  if (key != "\xE3\x81\x8A\xE3\x81\xBF\xE3\x81\x8F\xE3\x81\x98") {
    return false;
  }
  FortuneData *fortune_data = Singleton<FortuneData>::get();
  fortune_data->ChangeFortune();
  // Insert a fortune candidate into the last of all candidates.
  return InsertCandidate(fortune_data->fortune_type(),
                         segment.candidates_size(),
                         segments->mutable_conversion_segment(0));
}
}  // namespace mozc

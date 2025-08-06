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

#include "rewriter/fortune_rewriter.h"

#include <algorithm>
#include <ctime>
#include <iterator>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "base/singleton.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"

namespace mozc {
namespace {

enum FortuneType {
  FORTUNE_TYPE_EXCELLENT_LUCK = 0,
  FORTUNE_TYPE_LUCK = 1,
  FORTUNE_TYPE_MIDDLE_LUCK = 2,
  FORTUNE_TYPE_LITTLE_LUCK = 3,
  FORTUNE_TYPE_LUCK_AT_THE_END = 4,
  FORTUNE_TYPE_MISFORTUNE = 5,
  NUM_FORTUNE_TYPES = 6,
};

constexpr int kMaxLevel = 100;
constexpr int kNormalLevels[] = {20, 40, 60, 80, 90};
constexpr int kNewYearLevels[] = {30, 60, 80, 90, 95};
constexpr int kMyBirthdayLevels[] = {30, 60, 80, 90, 95};
constexpr int kFriday13Levels[] = {10, 25, 40, 55, 70};

bool IsValidFortuneType(FortuneType fortune_type) {
  return (fortune_type < NUM_FORTUNE_TYPES);
}

class FortuneData {
 public:
  FortuneData() : fortune_type_(FORTUNE_TYPE_EXCELLENT_LUCK) {
    ChangeFortune();
  }

  void ChangeFortune() {
    const int *levels = kNormalLevels;

    const absl::Time at = Clock::GetAbslTime();
    const absl::TimeZone &tz = Clock::GetTimeZone();
    const absl::CivilDay today = absl::ToCivilDay(at, tz);

    // Modify once per one day
    if (today == last_updated_day_) {
      return;
    }
    last_updated_day_ = today;

    // More happy at New Year's Day
    if (today.month() == 1 && today.day() == 1) {
      levels = kNewYearLevels;
    } else if (today.month() == 3 && today.day() == 3) {
      // It's my birthday :)
      levels = kMyBirthdayLevels;
    } else if (today.day() == 13 &&
               absl::GetWeekday(today) == absl::Weekday::friday) {
      // Friday the 13th
      levels = kFriday13Levels;
    }
    const int level = absl::Uniform(gen_, 0, kMaxLevel);
    for (int i = 0; i < std::size(kNormalLevels); ++i) {
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
  absl::CivilDay last_updated_day_;
  absl::BitGen gen_;
};

// Insert Fortune message into the |segment|
// Only one fortune indicated by fortune_type is inserted at
// |insert_pos|. Return false if insersion is failed.
bool InsertCandidate(FortuneType fortune_type, size_t insert_pos,
                     Segment *segment) {
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return false;
  }

  const converter::Candidate &base_candidate = segment->candidate(0);
  size_t offset = std::min(insert_pos, segment->candidates_size());
  const converter::Candidate &trigger_c = segment->candidate(offset - 1);

  std::string value;
  switch (fortune_type) {
    case FORTUNE_TYPE_EXCELLENT_LUCK:
      value = "大吉";
      break;
    case FORTUNE_TYPE_LUCK:
      value = "吉";
      break;
    case FORTUNE_TYPE_MIDDLE_LUCK:
      value = "中吉";
      break;
    case FORTUNE_TYPE_LITTLE_LUCK:
      value = "小吉";
      break;
    case FORTUNE_TYPE_LUCK_AT_THE_END:
      value = "末吉";
      break;
    case FORTUNE_TYPE_MISFORTUNE:
      value = "凶";
      break;
    default:
      LOG(FATAL) << "undefined fortune type";
      return false;
  }

  converter::Candidate *c = segment->insert_candidate(offset);
  if (c == nullptr) {
    LOG(ERROR) << "cannot insert candidate at " << offset;
    return false;
  }
  c->lid = trigger_c.lid;
  c->rid = trigger_c.rid;
  c->cost = trigger_c.cost;
  c->value = value;
  c->content_value = value;
  c->key = base_candidate.key;
  c->content_key = base_candidate.content_key;
  c->attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;
  c->attributes |= converter::Attribute::NO_LEARNING;
  c->description = "今日の運勢";
  return true;
}

}  // namespace

FortuneRewriter::FortuneRewriter() = default;

FortuneRewriter::~FortuneRewriter() = default;

bool FortuneRewriter::Rewrite(const ConversionRequest &request,
                              Segments *segments) const {
  if (segments->conversion_segments_size() != 1) {
    return false;
  }

  const Segment &segment = segments->conversion_segment(0);
  absl::string_view key = segment.key();
  if (key.empty()) {
    LOG(ERROR) << "Key is empty";
    return false;
  }

  if (key != "おみくじ") {
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

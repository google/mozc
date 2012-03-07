// Copyright 2010-2012, Google Inc.
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

#include "rewriter/command_rewriter.h"

#include <algorithm>
#include <vector>
#include "base/base.h"
#include "converter/segments.h"
#include "config/config_handler.h"
#include "config/config.pb.h"

namespace mozc {
namespace {

// const char kPrefix[] = "【";
// const char kSuffix[] = "】";
const char kPrefix[] = "\xE3\x80\x90";
const char kSuffix[] = "\xE3\x80\x91";

// const char kDescription[] = "設定を変更します"
const char kDescription[] = "\xE8\xA8\xAD\xE5\xAE\x9A"
    "\xE3\x82\x92\xE5\xA4\x89\xE6\x9B\xB4"
    "\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x99";

// Trigger CommandRewriter if and only if the Segment::key is one of
// kTriggerKeys[]
const char *kTriggerKeys[] = {
  // "こまんど",
  "\xE3\x81\x93\xE3\x81\xBE\xE3\x82\x93\xE3\x81\xA9",
  // "しーくれっと",
  "\xE3\x81\x97\xE3\x83\xBC\xE3\x81\x8F\xE3\x82\x8C\xE3\x81\xA3\xE3\x81\xA8",
  // "しーくれっともーど",
  "\xE3\x81\x97\xE3\x83\xBC\xE3\x81\x8F\xE3\x82\x8C\xE3\x81\xA3\xE3\x81\xA8"
  "\xE3\x82\x82\xE3\x83\xBC\xE3\x81\xA9",
  // "ひみつ",
  "\xE3\x81\xB2\xE3\x81\xBF\xE3\x81\xA4",
  // "ぷらいばしー",
  "\xE3\x81\xB7\xE3\x82\x89\xE3\x81\x84\xE3\x81\xB0\xE3\x81\x97\xE3\x83\xBC",
  // "ぷらいべーと",
  "\xE3\x81\xB7\xE3\x82\x89\xE3\x81\x84\xE3\x81\xB9\xE3\x83\xBC\xE3\x81\xA8",
  // "さじぇすと"
  "\xE3\x81\x95\xE3\x81\x98\xE3\x81\x87\xE3\x81\x99\xE3\x81\xA8",
  // "ぷれぜんてーしょん"
  "\xE3\x81\xB7\xE3\x82\x8C\xE3\x81\x9C\xE3\x82\x93"
  "\xE3\x81\xA6\xE3\x83\xBC\xE3\x81\x97\xE3\x82\x87\xE3\x82\x93",
  // "ぷれぜん"
  "\xE3\x81\xB7\xE3\x82\x8C\xE3\x81\x9C\xE3\x82\x93",
  // "よそく"
  "\xE3\x82\x88\xE3\x81\x9D\xE3\x81\x8F",
  // "よそくにゅうりょく",
  "\xE3\x82\x88\xE3\x81\x9D\xE3\x81\x8F\xE3\x81\xAB\xE3\x82\x85\xE3\x81\x86"
  "\xE3\x82\x8A\xE3\x82\x87\xE3\x81\x8F",
  // "よそくへんかん",
  "\xE3\x82\x88\xE3\x81\x9D\xE3\x81\x8F\xE3\x81\xB8\xE3\x82\x93\xE3\x81\x8B"
  "\xE3\x82\x93",
  // "すいそくこうほ"
  "\xE3\x81\x99\xE3\x81\x84\xE3\x81\x9D\xE3\x81\x8F\xE3\x81\x93\xE3\x81\x86"
  "\xE3\x81\xBB"
};

// Trigger Values for all commands
const char *kCommandValues[] = {
  //  "コマンド"
  "\xE3\x82\xB3\xE3\x83\x9E\xE3\x83\xB3\xE3\x83\x89"
};

// Trigger Values for Incoginito Mode.
const char *kIncognitoModeValues[] = {
  // "秘密",
  "\xE7\xA7\x98\xE5\xAF\x86",
  // "シークレット",
  "\xE3\x82\xB7\xE3\x83\xBC\xE3\x82\xAF\xE3\x83\xAC\xE3\x83\x83\xE3\x83\x88",
  // "シークレットモード",
  "\xE3\x82\xB7\xE3\x83\xBC\xE3\x82\xAF\xE3\x83\xAC\xE3\x83\x83\xE3\x83\x88"
  "\xE3\x83\xA2\xE3\x83\xBC\xE3\x83\x89",
  // "プライバシー",
  "\xE3\x83\x97\xE3\x83\xA9\xE3\x82\xA4\xE3\x83\x90\xE3\x82\xB7\xE3\x83\xBC",
  // "プライベート"
  "\xE3\x83\x97\xE3\x83\xA9\xE3\x82\xA4\xE3\x83\x99\xE3\x83\xBC\xE3\x83\x88"
};

const char *kDisableAllSuggestionValues[] = {
  // "サジェスト",
  "\xE3\x82\xB5\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xB9\xE3\x83\x88",
  // "予測",
  "\xE4\xBA\x88\xE6\xB8\xAC",
  // "予測入力",
  "\xE4\xBA\x88\xE6\xB8\xAC\xE5\x85\xA5\xE5\x8A\x9B",
  // "予測変換",
  "\xE4\xBA\x88\xE6\xB8\xAC\xE5\xA4\x89\xE6\x8F\x9B",
  // "プレゼンテーション"
  "\xE3\x83\x97\xE3\x83\xAC\xE3\x82\xBC\xE3\x83\xB3"
  "\xE3\x83\x86\xE3\x83\xBC\xE3\x82\xB7\xE3\x83\xA7\xE3\x83\xB3",
  // "プレゼン"
  "\xE3\x83\x97\xE3\x83\xAC\xE3\x82\xBC\xE3\x83\xB3"
};

// const char kIncoginitoModeOn[] = "シークレットモードをオン";
const char kIncoginitoModeOn[] =
    "\xE3\x82\xB7\xE3\x83\xBC\xE3\x82\xAF\xE3\x83\xAC"
    "\xE3\x83\x83\xE3\x83\x88\xE3\x83\xA2\xE3\x83\xBC"
    "\xE3\x83\x89\xE3\x82\x92\xE3\x82\xAA\xE3\x83\xB3";

// const char kIncoginitoModeOff[] = "シークレットモードをオフ";
const char kIncoginitoModeOff[] =
    "\xE3\x82\xB7\xE3\x83\xBC\xE3\x82\xAF\xE3\x83\xAC"
    "\xE3\x83\x83\xE3\x83\x88\xE3\x83\xA2\xE3\x83\xBC"
    "\xE3\x83\x89\xE3\x82\x92\xE3\x82\xAA\xE3\x83\x95";

// const char kDisableAllSuggestionOn[] = "サジェスト機能の一時停止";
const char kDisableAllSuggestionOn[] =
    "\xE3\x82\xB5\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xB9"
    "\xE3\x83\x88\xE6\xA9\x9F\xE8\x83\xBD\xE3\x81\xAE"
    "\xE4\xB8\x80\xE6\x99\x82\xE5\x81\x9C\xE6\xAD\xA2";

// const char kDisableAllSuggestionOff[] = "サジェスト機能を元に戻す";
const char kDisableAllSuggestionOff[] =
    "\xE3\x82\xB5\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xB9"
    "\xE3\x83\x88\xE6\xA9\x9F\xE8\x83\xBD\xE3\x82\x92"
    "\xE5\x85\x83\xE3\x81\xAB\xE6\x88\xBB\xE3\x81\x99";

bool FindString(const string &query, const char **values, size_t size) {
  DCHECK(values);
  DCHECK_GT(size, 0);
  for (size_t i = 0; i < size; ++i) {
    if (query == values[i]) {
      return true;
    }
  }
  return false;
}

Segment::Candidate *InsertCommandCandidate(
    Segment *segment, size_t reference_pos, size_t insert_pos) {
  DCHECK(segment);
  Segment::Candidate *candidate =
      segment->insert_candidate(min(segment->candidates_size(), insert_pos));
  DCHECK(candidate);
  candidate->CopyFrom(segment->candidate(reference_pos));
  candidate->attributes |= Segment::Candidate::COMMAND_CANDIDATE;
  candidate->attributes |= Segment::Candidate::NO_LEARNING;
  candidate->description = kDescription;
  candidate->prefix = kPrefix;
  candidate->suffix = kSuffix;
  return candidate;
}

bool IsSuggestionEnabled() {
  return GET_CONFIG(use_history_suggest) ||
      GET_CONFIG(use_dictionary_suggest) ||
      GET_CONFIG(use_realtime_conversion);
}
}  // namespace

CommandRewriter::CommandRewriter() {}

CommandRewriter::~CommandRewriter() {}

int CommandRewriter::capability() const {
  return RewriterInterface::CONVERSION;
}

void CommandRewriter::InsertIncognitoModeToggleCommand(
    Segment *segment, size_t reference_pos, size_t insert_pos) const {
  Segment::Candidate *candidate = InsertCommandCandidate(segment, reference_pos,
                                                         insert_pos);
  DCHECK(candidate);
  if (GET_CONFIG(incognito_mode)) {
    candidate->value = kIncoginitoModeOff;
    candidate->command = Segment::Candidate::DISABLE_INCOGNITO_MODE;
  } else {
    candidate->value = kIncoginitoModeOn;
    candidate->command = Segment::Candidate::ENABLE_INCOGNITO_MODE;
  }
  candidate->content_value = candidate->value;
}

void CommandRewriter::InsertDisableAllSuggestionToggleCommand(
    Segment *segment, size_t reference_pos, size_t insert_pos) const {
  if (!IsSuggestionEnabled()) {
    return;
  }

  Segment::Candidate *candidate = InsertCommandCandidate(segment, reference_pos,
                                                         insert_pos);

  DCHECK(candidate);
  if (GET_CONFIG(presentation_mode)) {
    candidate->value = kDisableAllSuggestionOff;
    candidate->command = Segment::Candidate::DISABLE_PRESENTATION_MODE;
  } else {
    candidate->value = kDisableAllSuggestionOn;
    candidate->command = Segment::Candidate::ENABLE_PRESENTATION_MODE;
  }
  candidate->content_value = candidate->value;
}

void CommandRewriter::Finish(Segments *segments) {
  // Do nothing in finish.
}

bool CommandRewriter::RewriteSegment(Segment *segment) const {
  DCHECK(segment);

  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    const string &value = segment->candidate(i).value;
    if (FindString(value, kCommandValues, arraysize(kCommandValues))) {
      // insert command candidate at an fixed position.
      InsertDisableAllSuggestionToggleCommand(segment, i, 6);
      InsertIncognitoModeToggleCommand(segment, i, 6);
      return true;
    }
    if (FindString(value, kIncognitoModeValues,
                   arraysize(kIncognitoModeValues))) {
      InsertIncognitoModeToggleCommand(segment, i, i + 3);
      return true;
    }
    if (FindString(value, kDisableAllSuggestionValues,
                   arraysize(kDisableAllSuggestionValues))) {
      InsertDisableAllSuggestionToggleCommand(segment, i, i + 3);
      return true;
    }
  }

  return false;
}

bool CommandRewriter::Rewrite(Segments *segments) const {
  if (segments == NULL || segments->conversion_segments_size() != 1) {
    return false;
  }

  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);
  const string &key = segment->key();

  // TODO(taku): we want to replace the linear search with STL map when
  // kTriggerKeys become bigger.
  if (!FindString(key, kTriggerKeys, arraysize(kTriggerKeys))) {
    return false;
  }

  return RewriteSegment(segment);
}
}  // namespace mozc

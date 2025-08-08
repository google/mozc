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

#include "rewriter/command_rewriter.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"

namespace mozc {
namespace {

constexpr char kPrefix[] = "【";
constexpr char kSuffix[] = "】";
constexpr char kDescription[] = "設定を変更します";

constexpr char kIncoginitoModeOn[] = "シークレットモードをオン";
constexpr char kIncoginitoModeOff[] = "シークレットモードをオフ";
constexpr char kDisableAllSuggestionOn[] = "サジェスト機能の一時停止";
constexpr char kDisableAllSuggestionOff[] = "サジェスト機能を元に戻す";

// Trigger CommandRewriter if and only if the Segment::key is one of
// kTriggerKeys[]
constexpr absl::string_view kTriggerKeys[] = {
    "こまんど",      "しーくれっと", "しーくれっともーど", "ひみつ",
    "ぷらいばしー",  "ぷらいべーと", "さじぇすと",         "ぷれぜんてーしょん",
    "ぷれぜん",      "よそく",       "よそくにゅうりょく", "よそくへんかん",
    "すいそくこうほ"};

// Trigger Values for all commands
constexpr absl::string_view kCommandValues[] = {"コマンド"};

// Trigger Values for Incoginito Mode.
constexpr absl::string_view kIncognitoModeValues[] = {
    "秘密", "シークレット", "シークレットモード", "プライバシー",
    "プライベート"};

constexpr absl::string_view kDisableAllSuggestionValues[] = {
    "サジェスト",         "予測",    "予測入力", "予測変換",
    "プレゼンテーション", "プレゼン"};

bool FindString(const absl::string_view query,
                const absl::Span<const absl::string_view> values) {
  return absl::c_find(values, query) != values.end();
}

converter::Candidate *InsertCommandCandidate(Segment *segment,
                                             size_t reference_pos,
                                             size_t insert_pos) {
  DCHECK(segment);
  converter::Candidate *candidate = segment->insert_candidate(
      std::min(segment->candidates_size(), insert_pos));
  DCHECK(candidate);
  *candidate = segment->candidate(reference_pos);
  candidate->attributes |= converter::Attribute::COMMAND_CANDIDATE;
  candidate->attributes |= converter::Attribute::NO_LEARNING;
  candidate->description = kDescription;
  candidate->prefix = kPrefix;
  candidate->suffix = kSuffix;
  candidate->inner_segment_boundary.clear();
  return candidate;
}

bool IsSuggestionEnabled(const config::Config &config) {
  return config.use_history_suggest() || config.use_dictionary_suggest() ||
         config.use_realtime_conversion();
}
}  // namespace

void CommandRewriter::InsertIncognitoModeToggleCommand(
    const config::Config &config, Segment *segment, size_t reference_pos,
    size_t insert_pos) const {
  converter::Candidate *candidate =
      InsertCommandCandidate(segment, reference_pos, insert_pos);
  DCHECK(candidate);
  if (config.incognito_mode()) {
    candidate->value = kIncoginitoModeOff;
    candidate->command = converter::Candidate::DISABLE_INCOGNITO_MODE;
  } else {
    candidate->value = kIncoginitoModeOn;
    candidate->command = converter::Candidate::ENABLE_INCOGNITO_MODE;
  }
  candidate->content_value = candidate->value;
}

void CommandRewriter::InsertDisableAllSuggestionToggleCommand(
    const config::Config &config, Segment *segment, size_t reference_pos,
    size_t insert_pos) const {
  if (!IsSuggestionEnabled(config)) {
    return;
  }

  converter::Candidate *candidate =
      InsertCommandCandidate(segment, reference_pos, insert_pos);

  DCHECK(candidate);
  if (config.presentation_mode()) {
    candidate->value = kDisableAllSuggestionOff;
    candidate->command = converter::Candidate::DISABLE_PRESENTATION_MODE;
  } else {
    candidate->value = kDisableAllSuggestionOn;
    candidate->command = converter::Candidate::ENABLE_PRESENTATION_MODE;
  }
  candidate->content_value = candidate->value;
}

bool CommandRewriter::RewriteSegment(const config::Config &config,
                                     Segment *segment) const {
  DCHECK(segment);

  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    absl::string_view value = segment->candidate(i).value;
    if (FindString(value, kCommandValues)) {
      // insert command candidate at an fixed position.
      InsertDisableAllSuggestionToggleCommand(config, segment, i, 6);
      InsertIncognitoModeToggleCommand(config, segment, i, 6);
      return true;
    }
    if (FindString(value, kIncognitoModeValues)) {
      InsertIncognitoModeToggleCommand(config, segment, i, i + 3);
      return true;
    }
    if (FindString(value, kDisableAllSuggestionValues)) {
      InsertDisableAllSuggestionToggleCommand(config, segment, i, i + 3);
      return true;
    }
  }

  return false;
}

bool CommandRewriter::Rewrite(const ConversionRequest &request,
                              Segments *segments) const {
  if (segments == nullptr || segments->conversion_segments_size() != 1) {
    return false;
  }

  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);
  absl::string_view key = segment->key();

  // TODO(taku): we want to replace the linear search with STL map when
  // kTriggerKeys become bigger.
  if (!FindString(key, kTriggerKeys)) {
    return false;
  }

  return RewriteSegment(request.config(), segment);
}
}  // namespace mozc

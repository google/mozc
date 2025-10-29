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

#include "rewriter/correction_rewriter.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "base/japanese_util.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"


namespace mozc {

using ::mozc::converter::Attribute;
using ::mozc::converter::Candidate;

// Returns "アボ*カ*ド" from {content_key=あぼがど, content_value=アボカド}
// It extracts the minimum span where the reading is different.
std::optional<std::string> CorrectionRewriter::GetDisplayValueForKatakana(
    const Candidate& candidate) const {
  return std::nullopt;
}

// Returns "巣(そう)窟" from {"巣窟", "すくつ", "そうくつ"}
// It extracts the minimum span where the reading is different.
std::optional<std::string>
CorrectionRewriter::GetDisplayValueForReadingCorrection(
    const ReadingCorrectionItem& item) const {
  using Alignment =
      std::vector<std::pair<absl::string_view, absl::string_view>>;

  const Alignment error_alignment =
      modules_.GetSupplementalModel().GetReadingAlignment(item.value,
                                                          item.error);
  const Alignment correction_alignment =
      modules_.GetSupplementalModel().GetReadingAlignment(item.value,
                                                          item.correction);

  auto fallback_result = [&]() {
    return absl::StrCat(item.value, "(", item.correction, ")");
  };

  // reading aligner is not available.
  // TODO(taku): Consider using fallback result when reading aligner is not
  // available.
  if (error_alignment.empty() || correction_alignment.empty()) {
    return std::nullopt;
  }

  if (error_alignment.size() != correction_alignment.size()) {
    return fallback_result();
  }

  // The maximum range where the per-character-readings are different.
  int diff_start = error_alignment.size();
  int diff_end = -1;
  for (int i = 0; i < error_alignment.size(); ++i) {
    if (error_alignment[i].second != correction_alignment[i].second) {
      diff_start = std::min(diff_start, i);
      diff_end = i;
    }
  }

  if (diff_end == -1) {
    return fallback_result();
  }

  std::string result;

  // [0, diff_start) have the same value.
  for (int i = 0; i < diff_start; ++i) {
    absl::StrAppend(&result, correction_alignment[i].first);
  }

  {
    // [diff_start, diff_end] are different.
    std::string diff_value, diff_key;
    for (int i = diff_start; i <= diff_end; ++i) {
      absl::StrAppend(&diff_value, correction_alignment[i].first);
      absl::StrAppend(&diff_key, correction_alignment[i].second);
    }
    absl::StrAppend(&result, diff_value, "(", diff_key, ")");
  }

  // (diff_end, last) have the same value.
  for (int i = diff_end + 1; i < correction_alignment.size(); ++i) {
    absl::StrAppend(&result, correction_alignment[i].first);
  }

  return result;
}

void CorrectionRewriter::SetCandidate(
    const ReadingCorrectionItem& item,
    commands::Request::DisplayValueCapability capability,
    Candidate* candidate) const {
  candidate->attributes |= Attribute::SPELLING_CORRECTION;

  // Assigns display_value when the client supports it.
  if (capability == commands::Request::PLAIN_TEXT) {
    std::optional<std::string> display_value;
    if (!item.correction.empty()) {
      display_value = GetDisplayValueForReadingCorrection(item);
    } else {
      display_value = GetDisplayValueForKatakana(*candidate);
    }

    if (display_value.has_value()) {
      candidate->display_value = std::move(display_value.value());
      candidate->description.clear();
      return;
    }
  }

  // The legacy description-based annotation.
  constexpr absl::string_view kDidYouMean = "もしかして";
  candidate->prefix = "→ ";
  if (item.correction.empty()) {
    candidate->description = absl::StrCat("<", kDidYouMean, ">");
  } else {
    candidate->description =
        absl::StrCat("<", kDidYouMean, ": ", item.correction, ">");
  }
}

bool CorrectionRewriter::LookupCorrection(
    const absl::string_view key, const absl::string_view value,
    std::vector<ReadingCorrectionItem>* results) const {
  CHECK(results);
  results->clear();

  using Iter = SerializedStringArray::const_iterator;
  std::pair<Iter, Iter> range =
      std::equal_range(error_array_.begin(), error_array_.end(), key);
  for (; range.first != range.second; ++range.first) {
    const absl::string_view v = value_array_[range.first.index()];
    if (value.empty() || value == v) {
      results->emplace_back(v, *range.first,
                            correction_array_[range.first.index()]);
    }
  }
  return !results->empty();
}

CorrectionRewriter::CorrectionRewriter(const engine::Modules& modules,
                                       absl::string_view value_array_data,
                                       absl::string_view error_array_data,
                                       absl::string_view correction_array_data)
    : modules_(modules) {
  DCHECK(SerializedStringArray::VerifyData(value_array_data));
  DCHECK(SerializedStringArray::VerifyData(error_array_data));
  DCHECK(SerializedStringArray::VerifyData(correction_array_data));
  value_array_.Set(value_array_data);
  error_array_.Set(error_array_data);
  correction_array_.Set(correction_array_data);
  DCHECK_EQ(value_array_.size(), error_array_.size());
  DCHECK_EQ(value_array_.size(), correction_array_.size());
}

// static
std::unique_ptr<CorrectionRewriter>
CorrectionRewriter::CreateCorrectionRewriter(const engine::Modules& modules) {
  absl::string_view value_array_data, error_array_data, correction_array_data;
  modules.GetDataManager().GetReadingCorrectionData(
      &value_array_data, &error_array_data, &correction_array_data);
  return std::make_unique<CorrectionRewriter>(
      modules, value_array_data, error_array_data, correction_array_data);
}

bool CorrectionRewriter::Rewrite(const ConversionRequest& request,
                                 Segments* segments) const {
  if (!request.config().use_spelling_correction()) {
    return false;
  }

  bool modified = false;
  std::vector<ReadingCorrectionItem> results;

  const commands::Request::DisplayValueCapability capability =
      request.request().display_value_capability();

  for (Segment& segment : segments->conversion_segments()) {
    if (segment.candidates_size() == 0) {
      continue;
    }

    for (size_t j = 0; j < segment.candidates_size(); ++j) {
      // Check if the existing candidate is a corrected candidate.
      // In this case, update the candidate description.
      const converter::Candidate& candidate = segment.candidate(j);

      // Handles the spelling correction defined in the system dictionary.
      // mostly they are Katakana to Katakana correction.
      if (candidate.attributes & Attribute::SPELLING_CORRECTION) {
        // Sets empty correction item.
        SetCandidate(ReadingCorrectionItem{"", "", ""}, capability,
                     segment.mutable_candidate(j));
        continue;
      }

      if (!LookupCorrection(candidate.content_key, candidate.content_value,
                            &results)) {
        continue;
      }

      CHECK_GT(results.size(), 0);
      // results.size() should be 1, but we don't check it here.
      Candidate* mutable_candidate = segment.mutable_candidate(j);
      DCHECK(mutable_candidate);
      SetCandidate(results[0], capability, mutable_candidate);
      modified = true;
    }

    // Add correction candidates that have the same key as the top candidate.
    //
    // TODO(taku): Want to calculate the position more accurately by
    // taking the emission cost into consideration.
    // The cost of mis-reading candidate can simply be obtained by adding
    // some constant penalty to the original emission cost.
    //
    // TODO(taku): In order to provide all miss reading corrections
    // defined in the tsv file, we want to add miss-read entries to
    // the system dictionary.
    const size_t kInsertPosition =
        std::min<size_t>(3, segment.candidates_size());
    const Candidate& top_candidate = segment.candidate(0);
    if (!LookupCorrection(top_candidate.content_key, "", &results)) {
      continue;
    }
    for (const ReadingCorrectionItem result : results) {
      auto new_candidate = std::make_unique<Candidate>(top_candidate);
      DCHECK(new_candidate);
      new_candidate->key =
          absl::StrCat(result.error, top_candidate.functional_key());
      new_candidate->value =
          absl::StrCat(result.value, top_candidate.functional_value());
      new_candidate->inner_segment_boundary.clear();
      SetCandidate(result, capability, new_candidate.get());

      segment.insert_candidate(kInsertPosition, std::move(new_candidate));
      modified = true;
    }
  }

  return modified;
}
}  // namespace mozc

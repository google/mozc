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

#include "rewriter/correction_rewriter.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/logging.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"

namespace mozc {
namespace {

void SetCandidate(const ReadingCorrectionItem *item,
                  Segment::Candidate *candidate) {
  DCHECK(item);
  candidate->prefix = "\xE2\x86\x92 ";  // "→ "
  candidate->attributes |= Segment::Candidate::SPELLING_CORRECTION;
  candidate->description =
      // "もしかして"
      "<\xE3\x82\x82\xE3\x81\x97\xE3\x81\x8B\xE3\x81\x97\xE3\x81\xA6: " +
      string(item->correction) + ">";
  DCHECK(candidate->IsValid());
}

struct ReadingCorrectionItemCompare {
  bool operator()(const ReadingCorrectionItem &s1,
                  const ReadingCorrectionItem &s2) {
    return (strcmp(s1.error, s2.error) < 0);
  }
};
}  // namespace

bool CorrectionRewriter::LookupCorrection(
    const string &key,
    const string &value,
    vector<const ReadingCorrectionItem *> *results) const {
  CHECK(results);
  results->clear();
  ReadingCorrectionItem key_item;
  key_item.error = key.c_str();
  const ReadingCorrectionItem *result =
      lower_bound(reading_corrections_,
                  reading_corrections_ + size_,
                  key_item,
                  ReadingCorrectionItemCompare());
  if (result == (reading_corrections_ + size_) ||
      key != result->error) {
    return false;
  }

  for (; result != (reading_corrections_ + size_); ++result) {
    if (key != result->error) {
      break;
    }
    if (value.empty() || value == result->value) {
      results->push_back(result);
    }
  }

  return !results->empty();
}

CorrectionRewriter::CorrectionRewriter(
    const ReadingCorrectionItem *reading_corrections, const size_t array_size) :
    reading_corrections_(reading_corrections), size_(array_size) {}

// static
CorrectionRewriter *CorrectionRewriter::CreateCorrectionRewriter(
    const DataManagerInterface *data_manager) {
  const ReadingCorrectionItem *array = NULL;
  size_t array_size = 0;
  data_manager->GetReadingCorrectionData(&array, &array_size);
  return new CorrectionRewriter(array, array_size);
}

CorrectionRewriter::~CorrectionRewriter() {}

bool CorrectionRewriter::Rewrite(const ConversionRequest &request,
                                 Segments *segments) const {
  if (!GET_CONFIG(use_spelling_correction)) {
    return false;
  }

  bool modified = false;
  vector<const ReadingCorrectionItem *> results;

  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    DCHECK(segment);
    if (segment->candidates_size() == 0) {
      continue;
    }

    for (size_t j = 0; j < segment->candidates_size(); ++j) {
      const Segment::Candidate &candidate = segment->candidate(j);
      if (!LookupCorrection(candidate.content_key, candidate.content_value,
                            &results)) {
        continue;
      }
      CHECK_GT(results.size(), 0);
      // results.size() should be 1, but we don't check it here.
      Segment::Candidate *mutable_candidate = segment->mutable_candidate(j);
      DCHECK(mutable_candidate);
      SetCandidate(results[0], mutable_candidate);
      modified = true;
    }

    // TODO(taku): Want to calculate the position more accurately by
    // taking the emission cost into consideration.
    // The cost of mis-reading candidate can simply be obtained by adding
    // some constant penalty to the original emission cost.
    //
    // TODO(taku): In order to provide all miss reading corrections
    // defined in the tsv file, we want to add miss-read entries to
    // the system dictionary.
    const size_t kInsertPostion = min(static_cast<size_t>(3),
                                      segment->candidates_size());
    const Segment::Candidate &top_candidate = segment->candidate(0);
    if (!LookupCorrection(top_candidate.content_key, "", &results)) {
      continue;
    }
    for (size_t k = 0; k < results.size(); ++k) {
      Segment::Candidate *mutable_candidate =
          segment->insert_candidate(kInsertPostion);
      DCHECK(mutable_candidate);
      mutable_candidate->CopyFrom(top_candidate);
      mutable_candidate->key = results[k]->error +
          top_candidate.functional_key();
      mutable_candidate->value = results[k]->value +
          top_candidate.functional_value();
      mutable_candidate->inner_segment_boundary.clear();
      SetCandidate(results[k], mutable_candidate);
      modified = true;
    }
  }

  return modified;
}
}  // namespace mozc

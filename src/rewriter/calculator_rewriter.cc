// Copyright 2010-2011, Google Inc.
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

#include "rewriter/calculator_rewriter.h"

#include <algorithm>
#include <string>
#include "base/util.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "rewriter/calculator/calculator_interface.h"

namespace mozc {

CalculatorRewriter::CalculatorRewriter() {
}

CalculatorRewriter::~CalculatorRewriter() {}

// Rewrites candidates when conversion segments of |segments| represents an
// expression that can be calculated. In such case, if |segments| consists
// of multiple segments, it merges them by calling ConverterInterface::
// ResizeSegment(), otherwise do calculation and insertion.
// TODO(tok): It currently calculates same expression twice, if |segments| is
//            a valid expression.
bool CalculatorRewriter::Rewrite(Segments *segments) const {
  if (!GET_CONFIG(use_calculator)) {
    return false;
  }

  CalculatorInterface *calculator = CalculatorFactory::GetCalculator();

  const size_t segments_size = segments->conversion_segments_size();
  if (segments_size == 0) {
    return false;
  }

  // If |segments| has only one conversion segment, try calculation and insert
  // the result on success.
  if (segments_size == 1) {
    const string &key = segments->conversion_segment(0).key();
    string result;
    if (!calculator->CalculateString(key, &result)) {
      return false;
    }
    // Insert the result.
    if (!InsertCandidate(result, 0, segments->mutable_conversion_segment(0))) {
      return false;
    }
    return true;
  }

  // Merge keys of all conversion segments and try calculation.
  string merged_key;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    merged_key += segments->conversion_segment(i).key();
  }
  // The decision to calculate and calculation itself are both done by the
  // calculator.
  string result;
  if (!calculator->CalculateString(merged_key, &result)) {
    return false;
  }

  // Merge all conversion segments.
  ConverterInterface *converter = ConverterFactory::GetConverter();
  int offset = Util::CharsLen(merged_key) -
                   Util::CharsLen(segments->conversion_segment(0).key());
  // ConverterInterface::ResizeSegment() calls Rewriter::Rewrite(), so
  // CalculatorRewriter::Rewrite() is recursively called with merged
  // conversion segment.
  if (!converter->ResizeSegment(segments, 0, offset)) {
    LOG(ERROR) << "Failed to merge conversion segments";
    return false;
  }
  return true;
}

bool CalculatorRewriter::InsertCandidate(const string &value,
                                         size_t insert_pos,
                                         Segment *segment) const {
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return false;
  }

  const Segment::Candidate &base_candidate = segment->candidate(0);

  // Normalize the expression, used in description.
  string temp, temp2, expression;
  Util::FullWidthAsciiToHalfWidthAscii(base_candidate.content_key, &temp);
  // "・"
  Util::StringReplace(temp, "\xE3\x83\xBB", "/", true, &temp2);
  // "ー", onbiki
  Util::StringReplace(temp2, "\xE3\x83\xBC", "-", true, &expression);

  size_t offset = min(insert_pos, segment->candidates_size());

  for (int n = 0; n < 2; ++n) {
    int current_offset = offset + n;
    Segment::Candidate *candidate = segment->insert_candidate(
        current_offset);
    if (candidate == NULL) {
      LOG(ERROR) << "cannot insert candidate at " << current_offset;
      return false;
    }

    // Simply sets some member variables of the new candidate to ones of the
    // existing candidate next to it.
    size_t reference_index = current_offset + 1;
    if (reference_index >= segment->candidates_size()) {
      reference_index = current_offset - 1;
    }
    const Segment::Candidate &reference_candidate =
        segment->candidate(reference_index);

    candidate->Init();
    candidate->lid = reference_candidate.lid;
    candidate->rid = reference_candidate.rid;
    candidate->cost = reference_candidate.cost;
    candidate->key = base_candidate.key;
    candidate->content_key = base_candidate.content_key;
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    candidate->attributes |= Segment::Candidate::NO_LEARNING;
    // description "[expression] の計算結果"
    candidate->description = expression +
        " \xE3\x81\xAE\xE8\xA8\x88\xE7\xAE\x97\xE7\xB5\x90\xE6\x9E\x9C";

    if (n == 0) {   // without expression
      candidate->value = value;
      candidate->content_value = value;
    } else {       // with expression
      candidate->value = expression + value;
      candidate->content_value = expression + value;
    }
  }

  return true;
}
}  // namespace mozc

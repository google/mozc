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

#include "rewriter/dice_rewriter.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"

namespace mozc {
namespace {

// We use dice with 6 faces.
constexpr int kDiceFaces = 6;

// Last candidate index of one page.
constexpr size_t kLastCandidateIndex = 8;

// Insert a dice number into the |segment|
// The number indicated by |top_face_number| is inserted at
// |insert_pos|. Return false if insersion is failed.
bool InsertCandidate(int top_face_number, size_t insert_pos, Segment* segment) {
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return false;
  }

  const converter::Candidate& base_candidate = segment->candidate(0);
  size_t offset = std::min(insert_pos, segment->candidates_size());

  converter::Candidate* c = segment->insert_candidate(offset);
  if (c == nullptr) {
    LOG(ERROR) << "cannot insert candidate at " << offset;
    return false;
  }
  const converter::Candidate& trigger_c = segment->candidate(offset - 1);

  c->lid = trigger_c.lid;
  c->rid = trigger_c.rid;
  c->cost = trigger_c.cost;
  c->value = absl::StrFormat("%d", top_face_number);
  c->content_value = c->value;
  c->key = base_candidate.key;
  c->content_key = base_candidate.content_key;
  c->attributes |= converter::Attribute::NO_LEARNING;
  c->attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;
  c->description = "出た目の数";
  return true;
}

}  // namespace

DiceRewriter::DiceRewriter() = default;

DiceRewriter::~DiceRewriter() = default;

bool DiceRewriter::Rewrite(const ConversionRequest& request,
                           Segments* segments) const {
  if (segments->conversion_segments_size() != 1) {
    return false;
  }

  const Segment& segment = segments->conversion_segment(0);
  absl::string_view key = segment.key();
  if (key.empty()) {
    LOG(ERROR) << "key is empty";
    return false;
  }

  if (key != "さいころ") {
    return false;
  }

  // Insert position is the last of first page or the last of candidates
  const size_t insert_pos =
      std::min(kLastCandidateIndex, segment.candidates_size());

  // Get a random number whose range is [1, kDiceFaces]
  // Insert the number at |insert_pos|
  absl::BitGen bitgen;
  return InsertCandidate(
      absl::Uniform(absl::IntervalClosed, bitgen, 1, kDiceFaces), insert_pos,
      segments->mutable_conversion_segment(0));
}

}  // namespace mozc

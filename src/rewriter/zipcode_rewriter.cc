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

#include "rewriter/zipcode_rewriter.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"

namespace mozc {

bool ZipcodeRewriter::GetZipcodeCandidatePositions(const Segment &seg,
                                                   std::string &zipcode,
                                                   std::string &address,
                                                   size_t &insert_pos) const {
  for (size_t i = 0; i < seg.candidates_size(); ++i) {
    const converter::Candidate &c = seg.candidate(i);
    if (!pos_matcher_.IsZipcode(c.lid) || !pos_matcher_.IsZipcode(c.rid)) {
      continue;
    }
    zipcode = c.content_key;
    address = c.content_value;
    insert_pos = i + 1;
    return true;
  }
  return false;
}

// Insert zipcode into the |segment|
bool ZipcodeRewriter::InsertCandidate(const size_t insert_pos,
                                      std::string zipcode, std::string address,
                                      const ConversionRequest &request,
                                      Segment *segment) const {
  DCHECK(segment);
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return false;
  }

  bool is_full_width = true;
  switch (request.config().space_character_form()) {
    case config::Config::FUNDAMENTAL_INPUT_MODE:
      is_full_width = true;
      break;
    case config::Config::FUNDAMENTAL_FULL_WIDTH:
      is_full_width = true;
      break;
    case config::Config::FUNDAMENTAL_HALF_WIDTH:
      is_full_width = false;
      break;
    default:
      LOG(WARNING) << "Unknown input mode";
      is_full_width = true;
      break;
  }

  absl::string_view space;
  if (is_full_width) {
    space = "　";  // "　" (full-width space)
  } else {
    space = " ";
  }

  std::string value = absl::StrCat(zipcode, space, address);

  const size_t offset = std::min(insert_pos, segment->candidates_size());
  converter::Candidate *candidate = segment->insert_candidate(offset);
  if (candidate == nullptr) {
    LOG(ERROR) << "cannot insert candidate at " << offset;
    return false;
  }
  DCHECK_GE(offset, 1);
  const converter::Candidate &base_candidate = segment->candidate(offset - 1);
  candidate->lid = pos_matcher_.GetZipcodeId();
  candidate->rid = pos_matcher_.GetZipcodeId();
  candidate->cost = base_candidate.cost;
  candidate->value = value;
  candidate->content_value = std::move(value);
  candidate->key = zipcode;
  candidate->content_key = std::move(zipcode);
  candidate->attributes |= converter::Candidate::NO_VARIANTS_EXPANSION;
  candidate->attributes |= converter::Candidate::NO_LEARNING;
  candidate->description = "郵便番号と住所";

  return true;
}

bool ZipcodeRewriter::Rewrite(const ConversionRequest &request,
                              Segments *segments) const {
  if (segments->conversion_segments_size() != 1) {
    return false;
  }

  const Segment &segment = segments->conversion_segment(0);
  const absl::string_view key = segment.key();
  if (key.empty()) {
    LOG(ERROR) << "Key is empty";
    return false;
  }

  size_t insert_pos;
  std::string zipcode, address;
  if (!GetZipcodeCandidatePositions(segment, zipcode, address, insert_pos)) {
    return false;
  }

  return InsertCandidate(insert_pos, std::move(zipcode), std::move(address),
                         request, segments->mutable_conversion_segment(0));
}

}  // namespace mozc

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

#include "rewriter/dice_rewriter.h"

#include <algorithm>
#include <string>
#include "base/base.h"
#include "base/util.h"
#include "converter/segments.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

// We use dice with 6 faces.
const int kDiceFaces = 6;

// Last candidate index of one page.
const size_t kLastCandidateIndex = 8;

// Insert a dice number into the |segment|
// The number indicated by |top_face_number| is inserted at
// |insert_pos|. Return false if insersion is failed.
bool InsertCandidate(int top_face_number,
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

  c->Init();
  c->lid = trigger_c.lid;
  c->rid = trigger_c.rid;
  c->cost = trigger_c.cost;
  c->value = Util::StringPrintf("%d", top_face_number);
  c->content_value = c->value;
  c->key = base_candidate.key;
  c->content_key = base_candidate.content_key;
  c->attributes |= Segment::Candidate::NO_LEARNING;
  c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  // description "出た目の数"
  c->description =
      "\xE5\x87\xBA\xE3\x81\x9F\xE7\x9B\xAE\xE3\x81\xAE\xE6\x95\xB0";
  return true;
}
}  // namespace

DiceRewriter::DiceRewriter() {}

DiceRewriter::~DiceRewriter() {}

bool DiceRewriter::Rewrite(Segments *segments) const {
  if (segments->conversion_segments_size() != 1) {
    return false;
  }

  const Segment &segment = segments->conversion_segment(0);
  const string &key = segment.key();
  if (key.empty()) {
    LOG(ERROR) << "key is empty";
    return false;
  }

  // "さいころ"
  if (key != "\xE3\x81\x95\xE3\x81\x84\xE3\x81\x93\xE3\x82\x8D") {
    return false;
  }

  // Insert position is the last of first page or the last of candidates
  const size_t insert_pos = min(kLastCandidateIndex, segment.candidates_size());

  // Get a random number whose range is [1, kDiceFaces]
  // Insert the number at |insert_pos|
  return InsertCandidate(Util::Random(kDiceFaces) + 1,
                         insert_pos,
                         segments->mutable_conversion_segment(0));
}
}  // namespace mozc

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

#include "rewriter/number_rewriter.h"

#include <stdio.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/util.h"
#include "dictionary/pos_matcher.h"
#include "converter/segments.h"
#include "session/config_handler.h"
#include "session/config.pb.h"

namespace mozc {
namespace {

void PushBackCandidate(const string &value, const string &desc, uint16 style,
                       vector<Segment::Candidate> *results) {
  bool found = false;
  for (vector<Segment::Candidate>::const_iterator it = results->begin();
       it != results->end(); ++it) {
    if (it->value == value) {
      found = true;
      break;
    }
  }
  if (!found) {
    Segment::Candidate cand;
    cand.value = value;
    cand.description = desc;
    cand.style = style;
    results->push_back(cand);
  }
}

bool IsNumber(uint16 lid) {
  return
      (POSMatcher::IsNumber(lid) ||
       POSMatcher::IsKanjiNumber(lid));
}

// Return true if rewriter should insert numerical variants.
// *base_candidate_pos: candidate index of base_candidate. POS information
// for numerical variants are coped from the base_candidate.
// *insert_pos: the candidate index from which numerical variants
// should be inserted.
bool GetNumericCandidatePositions(Segment *seg, int *base_candidate_pos,
                                  int *insert_pos) {
  CHECK(base_candidate_pos);
  CHECK(insert_pos);
  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    const Segment::Candidate &c = seg->candidate(i);
    if (!IsNumber(c.lid)) {
      continue;
    }

    if (Util::GetScriptType(c.content_value) == Util::NUMBER) {
      *base_candidate_pos = i;
      // +2 as fullwidht/(or halfwidth) variant is on i + 1 postion.
      *insert_pos = i + 2;
      return true;
    }

    string kanji_number, arabic_number, half_width_new_content_value;
    Util::FullWidthToHalfWidth(c.content_key, &half_width_new_content_value);
    // try to get normalized kanji_number and arabic_number.
    // if it failed, do nothing.
    if (!Util::NormalizeNumbers(c.content_value, true, &kanji_number,
                                &arabic_number) ||
        arabic_number == half_width_new_content_value) {
      return false;
    }

    // Insert arabic number first
    Segment::Candidate *arabic_c = seg->insert_candidate(i + 1);
    DCHECK(arabic_c);
    const string suffix =
        c.value.substr(c.content_value.size(),
                       c.value.size() - c.content_value.size());
    arabic_c->Init();
    arabic_c->value = arabic_number + suffix;
    arabic_c->content_value = arabic_number;
    arabic_c->key = c.key;
    arabic_c->content_key = c.content_key;
    arabic_c->cost = c.cost;
    arabic_c->structure_cost = c.structure_cost;
    arabic_c->lid = c.lid;
    arabic_c->rid = c.rid;

    // If top candidate is Kanji numeric, we want to expand at least
    // 5 candidates here.
    // http://b/issue?id=2872048
    const int kArabicNumericOffset = 5;
    *base_candidate_pos = i + 1;
    *insert_pos = i + kArabicNumericOffset;
    return true;
  }

  return false;
}
}  // namespace

NumberRewriter::NumberRewriter() {}
NumberRewriter::~NumberRewriter() {}

bool NumberRewriter::Rewrite(Segments *segments) const {
  if (!GET_CONFIG(use_number_conversion)) {
    VLOG(2) << "no use_number_conversion";
    return false;
  }

  bool modified = false;
  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    int base_candidate_pos = 0;
    int insert_pos = 0;
    if (!GetNumericCandidatePositions(seg, &base_candidate_pos, &insert_pos)) {
      continue;
    }

    const Segment::Candidate &base_cand = seg->candidate(base_candidate_pos);

    if (base_cand.content_value.size() > base_cand.value.size()) {
      LOG(ERROR) << "Invalid content_value/value: ";
      continue;
    }

    string base_content_value;
    Util::FullWidthToHalfWidth(base_cand.content_value, &base_content_value);

    if (Util::GetScriptType(base_content_value) != Util::NUMBER) {
      LOG(ERROR) << "base_content_value is not number: " << base_content_value;
      continue;
    }

    insert_pos = min(insert_pos, static_cast<int>(seg->candidates_size()));

    modified = true;
    vector<Util::NumberString> output;
    vector<Segment::Candidate> converted_numbers;

    Util::ArabicToWideArabic(base_content_value, &output);
    Util::ArabicToSeparatedArabic(base_content_value, &output);
    Util::ArabicToKanji(base_content_value, &output);
    Util::ArabicToOtherForms(base_content_value, &output);

    if (segments->conversion_segments_size() == 1) {
      Util::ArabicToOtherRadixes(base_content_value, &output);
    }

    for (int i = 0; i < output.size(); i++) {
      PushBackCandidate(output[i].value, output[i].description, output[i].style,
                        &converted_numbers);
    }

    const string suffix =
        base_cand.value.substr(base_cand.content_value.size(),
                               base_cand.value.size() -
                               base_cand.content_value.size());

    for (vector<Segment::Candidate>::const_iterator iter =
             converted_numbers.begin();
         iter != converted_numbers.end(); ++iter) {
      Segment::Candidate *c = seg->insert_candidate(insert_pos++);
      DCHECK(c);
      c->lid = base_cand.lid;
      c->rid = base_cand.rid;
      c->cost = base_cand.cost;
      c->value = iter->value + suffix;
      c->content_value = iter->value;
      c->key = base_cand.key;
      c->content_key = base_cand.content_key;
      c->style = iter->style;
      c->description = iter->description;
      // Don't want to have FULL_WIDTH form for Hex/Oct/BIN..etc.
      if (c->style == Segment::Candidate::NUMBER_HEX ||
          c->style == Segment::Candidate::NUMBER_OCT ||
          c->style == Segment::Candidate::NUMBER_BIN) {
        c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
      }
    }
  }

  return modified;
}
}  // namespace mozc

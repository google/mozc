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

#include "rewriter/english_variants_rewriter.h"

#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "session/commands.pb.h"

namespace mozc {

EnglishVariantsRewriter::EnglishVariantsRewriter() {}

EnglishVariantsRewriter::~EnglishVariantsRewriter() {}

bool EnglishVariantsRewriter::ExpandEnglishVariants(
    const string &input,
    vector<string> *variants) const {
  DCHECK(variants);

  if (input.empty()) {
    return false;
  }

  // multi-word
  if (input.find(" ") != string::npos) {
    return false;
  }

  string lower = input;
  string upper = input;
  string capitalized = input;
  Util::LowerString(&lower);
  Util::UpperString(&upper);
  Util::CapitalizeString(&capitalized);

  if (lower == upper) {
    // given word is non-ascii.
    return false;
  }

  variants->clear();
  // If |input| is non-standard expression, like "iMac", only
  // expand lowercase.
  if (input != lower && input != upper && input != capitalized) {
    variants->push_back(lower);
    return true;
  }

  if (input != lower) {
    variants->push_back(lower);
  }
  if (input != capitalized) {
    variants->push_back(capitalized);
  }
  if (input != upper) {
    variants->push_back(upper);
  }

  return true;
}

bool EnglishVariantsRewriter::IsT13NCandidate(
    Segment::Candidate *candidate) const {
  return (Util::IsEnglishTransliteration(candidate->content_value) &&
          Util::GetScriptType(candidate->content_key) == Util::HIRAGANA);
}

bool EnglishVariantsRewriter::IsEnglishCandidate(
    Segment::Candidate *candidate) const {
  return (Util::IsEnglishTransliteration(candidate->content_value) &&
          Util::GetScriptType(candidate->content_key) == Util::ALPHABET);
}

bool EnglishVariantsRewriter::ExpandEnglishVariantsWithSegment(
    Segment *seg) const {
  CHECK(seg);

  bool modified = false;

  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    Segment::Candidate *original_candidate = seg->mutable_candidate(i);
    DCHECK(original_candidate);

    // http://b/issue?id=5137299
    // If the entry is comming from user dictionary,
    // expand English variants.
    if (original_candidate->attributes &
        Segment::Candidate::NO_VARIANTS_EXPANSION &&
        !(original_candidate->attributes &
          Segment::Candidate::USER_DICTIONARY)) {
      continue;
    }

    if (IsT13NCandidate(original_candidate)) {
      // Expand T13N candiadte variants
      modified = true;
      original_candidate->attributes |=
          Segment::Candidate::NO_VARIANTS_EXPANSION;
      vector<string> variants;
      if (ExpandEnglishVariants(original_candidate->content_value,
                                &variants)) {
        CHECK(!variants.empty());
        for (size_t j = 0; j < variants.size(); ++j) {
          Segment::Candidate *new_candidate = seg->insert_candidate(i + j + 1);
          DCHECK(new_candidate);
          new_candidate->Init();
          new_candidate->value = variants[j] +
              original_candidate->functional_value();
          new_candidate->key = original_candidate->key;
          new_candidate->content_value = variants[j];
          new_candidate->content_key = original_candidate->content_key;
          new_candidate->cost = original_candidate->cost;
          new_candidate->wcost = original_candidate->wcost;
          new_candidate->structure_cost =
              original_candidate->structure_cost;
          new_candidate->lid = original_candidate->lid;
          new_candidate->rid = original_candidate->rid;
          new_candidate->attributes |=
              Segment::Candidate::NO_VARIANTS_EXPANSION;
        }

        i += variants.size();
      }
    } else if (IsEnglishCandidate(original_candidate)) {
      // Fix variants for English candidate
      modified = true;
      original_candidate->attributes |=
          Segment::Candidate::NO_VARIANTS_EXPANSION;
    }
  }

  return modified;
}

int EnglishVariantsRewriter::capability() const {
  return RewriterInterface::CONVERSION;
}

bool EnglishVariantsRewriter::Rewrite(const ConversionRequest &request,
                                      Segments *segments) const {
  bool modified = false;
  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    modified |= ExpandEnglishVariantsWithSegment(seg);
  }

  return modified;
}
}  // namespace mozc

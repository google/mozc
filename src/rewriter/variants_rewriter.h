// Copyright 2010, Google Inc.
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

#ifndef MOZC_REWRITER_VARIANTS_REWRITER_H_
#define MOZC_REWRITER_VARIANTS_REWRITER_H_

#include <string>
#include "converter/segments.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class VariantsRewriter: public RewriterInterface  {
 public:
  VariantsRewriter();
  virtual ~VariantsRewriter();
  virtual bool Rewrite(Segments *segments) const;
  virtual void Finish(Segments *segments);
  virtual void Clear();

  static void SetDescriptionForCandidate(
      Segment::Candidate *candidate);
  static void SetDescriptionForTransliteration(
      Segment::Candidate *candidate);
  static void SetDescriptionForPrediction(
      Segment::Candidate *candidate);

 private:
  // 1) Full width / half width description
  // 2) CharForm (hiragana/katakana) description
  // 3) Platform dependent char (JISX0213..etc) description
  // 4) Zipcode description (XXX-XXXX)
  //     * note that this overrides other descriptions
  enum DescriptionType {
    FULL_HALF_WIDTH = 1,   // automatically detect full/haflwidth.
    FULL_HALF_WIDTH_WITH_UNKNOWN = 2,  // set half/full widith for symbols.
    // This flag must be used together with FULL_HALF_WIDTH.
    // If WITH_UNKNOWN is specified, assign FULL/HALF width annotation
    // more aggressively.
    HALF_WIDTH = 4,        // always set half width description.
    FULL_WIDTH = 8,        // always set full width description.
    CHARACTER_FORM = 16,    // Hiragana/Katakana..etc
    PLATFORM_DEPENDENT_CHARACTER = 32,
    ZIPCODE = 64,
    SPELLING_CORRECTION = 128
  };

  static void SetDescription(int description_type,
                             Segment::Candidate *candidate);

  bool RewriteSegment(Segment *seg) const;
};
}

#endif  // MOZC_REWRITER_VARIANTS_REWRITER_H_

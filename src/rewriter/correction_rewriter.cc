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

#include "rewriter/correction_rewriter.h"

#include "converter/conversion_request.h"
#include "converter/segments.h"

namespace mozc {
namespace {
// Include kReadingCorrections.
#include "rewriter/reading_correction_data.h"
}  // namespace

CorrectionRewriter::CorrectionRewriter(
    const ReadingCorrectionItem *reading_corrections, const size_t array_size) {
  for (size_t i = 0; i < array_size; ++i) {
    const ReadingCorrectionItem &item = reading_corrections[i];
    correction_map_[StrPair(item.error, item.value)] = item.correction;
  }
}

// static
CorrectionRewriter *CorrectionRewriter::CreateCorrectionRewriter() {
  // TODO(noriyukit, komatsu): kReadingCorrections should be
  // integrated into data manager and this function should cooperate
  // with each data maanger.  Otherwise there might be a gap between
  // the dictionary data set and correction data.  NOTE: This rewriter
  // and data does not affect the ranking or coverage of any
  // conversion.
  return new CorrectionRewriter(kReadingCorrections,
                                arraysize(kReadingCorrections));
}

CorrectionRewriter::~CorrectionRewriter() {}

bool CorrectionRewriter::LookupCorrection(const string &key,
                                          const string &value,
                                          string *correction) const {
  DCHECK(correction);
  const map<StrPair, string>::const_iterator it =
      correction_map_.find(StrPair(key, value));
  if (it == correction_map_.end()) {
    return false;
  }

  correction->assign(it->second);
  return true;
}


bool CorrectionRewriter::Rewrite(const ConversionRequest &request,
                                 Segments *segments) const {
  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    DCHECK(segment);
    for (size_t j = 0; j < segment->candidates_size(); ++j) {
      const Segment::Candidate &candidate = segment->candidate(j);
      string correction;
      if (!LookupCorrection(candidate.content_key,
                            candidate.content_value,
                            &correction)) {
        continue;
      }

      Segment::Candidate *mutable_candidate = segment->mutable_candidate(j);
      mutable_candidate->prefix = "\xE2\x86\x92 ";  // "→ "
      mutable_candidate->attributes |= Segment::Candidate::SPELLING_CORRECTION;
      mutable_candidate->description =
          // "もしかして"
          "<\xE3\x82\x82\xE3\x81\x97\xE3\x81\x8B\xE3\x81\x97\xE3\x81\xA6: " +
          correction + ">";
      modified = true;
    }
  }
  return modified;
}
}  // namespace mozc

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

#include "rewriter/single_hentaigana_rewriter.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "composer/composer.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "request/conversion_request.h"

namespace mozc {
struct Pair {
  // Hentaigana Glyph.
  std::string glyph;
  // The origin Kanji of Hentaigana, from which the glyph came. For example,
  // '𛀇'(U+1B007) comes from Kanji '伊'. Because people sometimes explain it
  // using the origin like '伊の変体仮名', this information is used to generate
  // description of the candidates. Note that this origin can be empty string.
  std::string origin;
};
namespace {
// Content of single_hentaigana_rewriter_data.inc is generated on build time,
// based on data/hentaigana/hentaigana.tsv.
const auto *kHentaiganaTable = new std::map<std::string, std::vector<Pair>>({
#include "rewriter/single_hentaigana_rewriter_data.inc"
});

bool EnsureSingleSegment(const ConversionRequest &request, Segments *segments,
                         const ConverterInterface *parent_converter,
                         const std::string &key) {
  if (segments->conversion_segments_size() == 1) {
    return true;
  }

  if (segments->resized()) {
    // The given segments are resized by user so don't modify anymore.
    return false;
  }

  const uint32_t resize_len =
      Util::CharsLen(key) -
      Util::CharsLen(segments->conversion_segment(0).key());
  if (!parent_converter->ResizeSegment(segments, request, 0, resize_len)) {
    return false;
  }
  DCHECK_EQ(1, segments->conversion_segments_size());
  return true;
}

void AddCandidate(const std::string &key, const std::string &description,
                  const std::string &value, Segment *segment) {
  DCHECK(segment);
  Segment::Candidate *candidate =
      segment->insert_candidate(segment->candidates_size());
  DCHECK(candidate);

  candidate->Init();
  segment->set_key(key);
  candidate->key = key;
  candidate->value = value;
  candidate->content_value = value;
  candidate->description = description;
  candidate->attributes |= (Segment::Candidate::NO_VARIANTS_EXPANSION);
}
}  // namespace

SingleHentaiganaRewriter::SingleHentaiganaRewriter(
    const ConverterInterface *parent_converter)
    : parent_converter_(parent_converter) {}

SingleHentaiganaRewriter::~SingleHentaiganaRewriter() {}

int SingleHentaiganaRewriter::capability(
    const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

void SingleHentaiganaRewriter::SetEnabled(const bool enabled) {
  // TODO(b/242276533): Replace this with better mechanism later. Right now this
  // rewriter is always disabled intentionally except for tests.
  enabled_ = enabled;
}

bool SingleHentaiganaRewriter::Rewrite(const ConversionRequest &request,
                                       Segments *segments) const {
  // If hentaigana rewriter is not requested to use, do nothing.
  if (!enabled_) {
    return false;
  }

  std::string key;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    key += segments->conversion_segment(i).key();
  }

  if (!EnsureSingleSegment(request, segments, parent_converter_, key)) {
    return false;
  }

  // Ensure table has entry for key.
  if (kHentaiganaTable->find(key) == kHentaiganaTable->end()) {
    return false;
  }
  const std::vector<Pair> &pairs = kHentaiganaTable->at(key);
  // Ensure pairs is not empty
  if (pairs.empty()) {
    return false;
  }

  // Generate candidate for each value in pairs.
  for (const Pair &pair : pairs) {
    const std::string &value = pair.glyph;
    Segment *segment = segments->mutable_conversion_segment(0);

    // If origin is not available, ignore it and set "変体仮名" for description.
    if (pair.origin.empty()) {
      AddCandidate(key, "変体仮名", value, segment);
    } else {
      const auto description = pair.origin + "の変体仮名";
      AddCandidate(key, description, value, segment);
    }
  }
  return true;
}
}  // namespace mozc

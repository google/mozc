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

#include "rewriter/single_kanji_rewriter.h"

#include <string>
#include <vector>
#include <set>
#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "converter/segments.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/embedded_dictionary.h"
#include "session/config_handler.h"
#include "session/config.pb.h"

namespace mozc {

namespace {

#include "rewriter/single_kanji_rewriter_data.h"

class SingleKanjiDictionary {
 public:
  SingleKanjiDictionary()
      : dic_(new EmbeddedDictionary(kSingleKanjiData_token_data,
                                    kSingleKanjiData_token_size)) {}

  ~SingleKanjiDictionary() {}

  EmbeddedDictionary *GetDictionary() const {
    return dic_.get();
  }

 private:
  scoped_ptr<EmbeddedDictionary> dic_;
};

// Insert SingleKanji into segment.
void InsertCandidate(Segment *segment,
                     bool is_single_segment,
                     const EmbeddedDictionary::Value *dict_values,
                     size_t dict_values_size) {
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return;
  }

  // Adding 5000 to the single kanji cost
  const int kOffsetDiff = 5000;

  const Segment::Candidate &base_candidate = segment->candidate(0);
  size_t idx_j = 0;

  if (is_single_segment) {
    // Merge default candidate and SingleKanji candidate.
    // This procedure makes dup, but we just ignore it, as
    // session layer removes dups
    size_t idx_i = 0;

    // we don't touch the first candidate if it is already fixed
    if (segment->segment_type() == Segment::FIXED_VALUE) {
      idx_i = 1;
    }

    // Find insertion point
    for (size_t i = 0; i < segment->candidates_size(); ++i) {
      const string &value = segment->candidate(i).value;
      // We want to insert under hiragana, katakana,
      // and single kanjis in system dictionary.
      if (Util::IsScriptType(value, Util::HIRAGANA) ||
          Util::IsScriptType(value, Util::KATAKANA) ||
          (Util::IsScriptType(value, Util::KANJI) &&
           Util::CharsLen(value) == 1)) {
        ++idx_i;
        continue;
      }
      break;
    }

    while (idx_i < segment->candidates_size() && idx_j < dict_values_size) {
      const int cost = dict_values[idx_j].cost + kOffsetDiff;
      if (cost >= segment->candidate(idx_i).cost) {
        ++idx_i;
        continue;
      }
      Segment::Candidate *c = segment->insert_candidate(idx_i);
      c->lid = dict_values[idx_j].lid;
      c->rid = dict_values[idx_j].rid;
      c->cost = dict_values[idx_j].cost + kOffsetDiff;
      c->content_value = dict_values[idx_j].value;
      c->key = base_candidate.key;
      c->content_key = base_candidate.content_key;
      c->value = dict_values[idx_j].value;
      c->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
      c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
      if (dict_values[idx_j].description != NULL) {
        c->description = dict_values[idx_j].description;
      }
      ++idx_i;
      ++idx_j;
    }
  }

  // append remaining single-kanji
  while (idx_j < dict_values_size) {
    Segment::Candidate *c = segment->push_back_candidate();
    c->lid = dict_values[idx_j].lid;
    c->rid = dict_values[idx_j].rid;
    c->cost = dict_values[idx_j].cost + kOffsetDiff;
    c->content_value = dict_values[idx_j].value;
    c->content_key = base_candidate.content_key;
    c->value = dict_values[idx_j].value;
    c->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    if (dict_values[idx_j].description != NULL) {
      c->description = dict_values[idx_j].description;
    }
    ++idx_j;
  }
}
}  // namespace

SingleKanjiRewriter::SingleKanjiRewriter() {}

SingleKanjiRewriter::~SingleKanjiRewriter() {}

bool SingleKanjiRewriter::Rewrite(Segments *segments) const {
  if (!GET_CONFIG(use_single_kanji_conversion)) {
    VLOG(2) << "no use_single_kanji_conversion";
    return false;
  }

  bool modified = false;
  const size_t segments_size = segments->conversion_segments_size();
  const bool is_single_segment = (segments_size == 1);
  for (size_t i = 0; i < segments_size; ++i) {
    const string &key = segments->conversion_segment(i).key();
    const EmbeddedDictionary::Token *token =
        Singleton<SingleKanjiDictionary>::get()->GetDictionary()->Lookup(key);
    if (token == NULL) {
      continue;
    }
    InsertCandidate(segments->mutable_conversion_segment(i),
                    is_single_segment,
                    token->value, token->value_size);
    modified = true;
  }

  return modified;
}
}  // namespace mozc

// Copyright 2010-2013, Google Inc.
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

#include "rewriter/emoticon_rewriter.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "rewriter/embedded_dictionary.h"
#include "rewriter/rewriter_interface.h"
#include "session/commands.pb.h"

namespace mozc {
namespace {

#include "rewriter/emoticon_rewriter_data.h"

class EmoticonDictionary {
 public:
  EmoticonDictionary()
      : dic_(new EmbeddedDictionary(kEmoticonData_token_data,
                                    kEmoticonData_token_size)) {}

  ~EmoticonDictionary() {}

  EmbeddedDictionary *GetDictionary() const {
    return dic_.get();
  }

 private:
  scoped_ptr<EmbeddedDictionary> dic_;
};

class ValueCostCompare {
 public:
  bool operator() (const EmbeddedDictionary::Value *a,
                   const EmbeddedDictionary::Value *b) const {
    return a->cost < b->cost;
  }
};

class IsEqualValue {
 public:
  bool operator() (const EmbeddedDictionary::Value *a,
                   const EmbeddedDictionary::Value *b) const {
    return strcmp(a->value, b->value) == 0;
  }
};

// Insert Emoticon into the |segment|
// Top |initial_insert_size| candidates are inserted from |initial_insert_pos|.
// Remained candidates are added to the buttom.
void InsertCandidates(const EmbeddedDictionary::Value *value,
                      size_t value_size,
                      size_t initial_insert_pos,
                      size_t initial_insert_size,
                      bool is_no_learning,
                      Segment *segment) {
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candiadtes_size is 0";
    return;
  }

  const Segment::Candidate &base_candidate = segment->candidate(0);
  size_t offset = min(initial_insert_pos, segment->candidates_size());

  // Sort values by cost just in case
  vector<const EmbeddedDictionary::Value *> sorted_value;
  for (size_t i = 0; i < value_size; ++i) {
    sorted_value.push_back(&value[i]);
  }

  sort(sorted_value.begin(), sorted_value.end(), ValueCostCompare());

  // after sorting the valeus by |cost|, adjacent candidates
  // will have the same value. It is almost OK to use std::unique to
  // remove dup entries, it is not a perfect way though.
  sorted_value.erase(unique(sorted_value.begin(),
                            sorted_value.end(),
                            IsEqualValue()),
                     sorted_value.end());

  for (size_t i = 0; i < sorted_value.size(); ++i) {
    Segment::Candidate *c = NULL;

    if (i < initial_insert_size) {
      c = segment->insert_candidate(offset);
      ++offset;
    } else {
      c = segment->push_back_candidate();
    }

    if (c == NULL) {
      LOG(ERROR) << "cannot insert candidate at " << offset;
      continue;
    }

    c->Init();
    // TODO(taku): set an appropriate POS here.
    c->lid = sorted_value[i]->lid;
    c->rid = sorted_value[i]->rid;
    c->cost = base_candidate.cost;
    c->value = sorted_value[i]->value;
    c->content_value = sorted_value[i]->value;
    c->key = base_candidate.key;
    c->content_key = base_candidate.content_key;
    // no full/half width normalizations
    c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    c->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    if (is_no_learning) {
      c->attributes |= Segment::Candidate::NO_LEARNING;
    }

    //  "顔文字";
    const char kBaseEmoticonDescription[]
        = "\xE9\xA1\x94\xE6\x96\x87\xE5\xAD\x97";

    if (sorted_value[i]->description == NULL) {
      c->description = kBaseEmoticonDescription;
    } else {
      string description = kBaseEmoticonDescription;
      description.append(" ");
      description.append(sorted_value[i]->description);
      c->description = description;
    }
  }
}

bool RewriteCandidate(Segments *segments) {
  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    const string &key = segments->conversion_segment(i).key();
    if (key.empty()) {
      LOG(ERROR) << "Key is empty";
      continue;
    }
    bool is_no_learning = false;
    const EmbeddedDictionary::Value *value = NULL;
    size_t value_size = 0;
    size_t initial_insert_size = 0;
    size_t initial_insert_pos = 0;

    // TODO(taku): Emoticon dictionary does not always include "facemark".
    // Displaying non-facemarks with "かおもじ" is not always correct.
    // We have to distinguish pure facemarks and other symbol marks.

    // "かおもじ"
    if (key == "\xE3\x81\x8B\xE3\x81\x8A\xE3\x82\x82\xE3\x81\x98") {
      // When key is "かおもじ", default candidate size should be small enough.
      // It is safe to expand all candidates at this time.
      const EmbeddedDictionary::Token *token
          = Singleton<EmoticonDictionary>::get()->GetDictionary()->AllToken();
      CHECK(token);
      // set large value(100) so that all candidates are pushed to the bottom
      value = token->value;
      value_size = token->value_size;
      initial_insert_pos = 100;
      initial_insert_size = token->value_size;
      // "かお"
    } else if (key == "\xE3\x81\x8B\xE3\x81\x8A") {
      // When key is "かお", expand all candidates in conservative way.
      const EmbeddedDictionary::Token *token
          = Singleton<EmoticonDictionary>::get()->GetDictionary()->AllToken();
      CHECK(token);
      // first 6 candidates are inserted at 4 th position.
      // Other candidates are pushed to the buttom.
      value = token->value;
      value_size = token->value_size;
      initial_insert_pos = 4;
      initial_insert_size = 6;
    } else if (key == "\xE3\x81\xB5\xE3\x81\x8F\xE3\x82\x8F"
               "\xE3\x82\x89\xE3\x81\x84") {   // "ふくわらい"
      // Choose one emoticon randomly from the dictionary.
      // TODO(taku): want to make it "generate" more funny emoticon.
      const EmbeddedDictionary::Token *token
          = Singleton<EmoticonDictionary>::get()->GetDictionary()->AllToken();
      CHECK(token);
      uint32 n = 0;
      // use secure random not to predict the next emoticon.
      Util::GetRandomSequence(reinterpret_cast<char *>(&n), sizeof(n));
      value = token->value + n % token->value_size;
      value_size = 1;
      initial_insert_pos = 4;
      initial_insert_size = 1;
      is_no_learning = true;   // do not learn this candidate.
    } else {
      const EmbeddedDictionary::Token *token
          = Singleton<EmoticonDictionary>::get()->GetDictionary()->Lookup(key);
      // by default, insert canidate at 7 th position.
      if (token != NULL) {
        value = token->value;
        value_size = token->value_size;
        initial_insert_pos = 6;
        initial_insert_size = token == NULL ? 0 : token->value_size;
      }
    }

    if (value == NULL || value_size == 0) {
      continue;
    }

    InsertCandidates(value, value_size,
                     initial_insert_pos,
                     initial_insert_size,
                     is_no_learning,
                     segments->mutable_conversion_segment(i));
    modified = true;
  }

  return modified;
}
}  // namespace

EmoticonRewriter::EmoticonRewriter() {}

EmoticonRewriter::~EmoticonRewriter() {}

int EmoticonRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool EmoticonRewriter::Rewrite(const ConversionRequest &request,
                               Segments *segments) const {
  if (!GET_CONFIG(use_emoticon_conversion)) {
    VLOG(2) << "no use_emoticon_conversion";
    return false;
  }
  return RewriteCandidate(segments);
}
}  // namespace mozc

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

#include "base/util.h"
#include "converter/segments.h"
#include "rewriter/usage_rewriter.h"

namespace mozc {
namespace {
struct ConjugationSuffix {
  const char *value_suffix;
  const char *key_suffix;
};
#include "rewriter/usage_rewriter_data.h"
}

UsageRewriter::UsageRewriter() {
  const UsageDictItem *item = kUsageData_value;
  for (; item->key != NULL; ++item) {
    for (size_t i = kConjugationSuffixDataIndex[item->conjugation_id];
         i < kConjugationSuffixDataIndex[item->conjugation_id + 1];
         ++i) {
      StrPair key_value(
        string(item->key) + kConjugationSuffixData[i].key_suffix,
        string(item->value) + kConjugationSuffixData[i].value_suffix);
      key_value_usageitem_map_[key_value] = item;
    }
  }
}

UsageRewriter::~UsageRewriter() {
}

const UsageDictItem* UsageRewriter::LookupUsage(const string &key,
                                               const string &value) const {
  StrPair key_value(key, value);
  const map<StrPair, const UsageDictItem *>::const_iterator itr =
    key_value_usageitem_map_.find(key_value);
  if (itr == key_value_usageitem_map_.end()) {
    return NULL;
  }
  return itr->second;
}

bool UsageRewriter::Rewrite(Segments *segments) const {
  DLOG(INFO) << segments->DebugString();
  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    DCHECK(segment);
    for (size_t j = 0; j < segment->candidates_size(); ++j) {
      const string &content_key = segment->candidate(j).content_key;
      const string &content_value = segment->candidate(j).content_value;
      const UsageDictItem *usage = LookupUsage(content_key, content_value);

      if (usage != NULL) {
        Segment::Candidate *candidate = segment->mutable_candidate(j);
        DCHECK(candidate);
        candidate->usage_id = usage->id;
        candidate->usage_title = string(usage->value)
          + kBaseConjugationSuffix[usage->conjugation_id].value_suffix;
        candidate->usage_description = usage->meaning;
        DLOG(INFO) << i << ":" << j << ":" <<
          content_key << ":" << content_value <<
          ":" << usage->key << ":" << usage->value <<
          ":" << usage->conjugation_id << ":" << usage->meaning;
        modified = true;
      }
    }
  }
  return modified;
}
}  // namespace mozc

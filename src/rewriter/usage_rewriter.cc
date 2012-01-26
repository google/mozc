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

#include "base/util.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
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
  // TODO(taku): To reduce memory footprint, better to replace it with
  // binary search over the kConjugationSuffixDataIndex diretly.
  for (; item->key != NULL; ++item) {
    for (size_t i = kConjugationSuffixDataIndex[item->conjugation_id];
         i < kConjugationSuffixDataIndex[item->conjugation_id + 1];
         ++i) {
      StrPair key_value1(
          string(item->key) + kConjugationSuffixData[i].key_suffix,
          string(item->value) + kConjugationSuffixData[i].value_suffix);
      key_value_usageitem_map_[key_value1] = item;
      StrPair key_value2(
          "",
          string(item->value) + kConjugationSuffixData[i].value_suffix);
      key_value_usageitem_map_[key_value2] = item;
    }
  }
}

UsageRewriter::~UsageRewriter() {
}

// static
// "合いました" => "合い"
string UsageRewriter::GetKanjiPrefixAndOneHiragana(const string &word) {
  const char *begin = word.data();
  const char *end = word.data() + word.size();

  string result;
  int pos = 0;
  bool has_kanji = false;
  bool has_hiragana = false;
  while (begin < end) {
    size_t mblen = 0;
    const char32 w = Util::UTF8ToUCS4(begin, end, &mblen);
    DCHECK_GT(mblen, 0);
    const Util::ScriptType s = Util::GetScriptType(w);
    begin += mblen;
    if (pos == 0 && s != Util::KANJI) {
      return "";
    } else if (pos >= 0 && pos <= 1 && s == Util::KANJI) {
      // length of kanji <= 2.
      has_kanji = true;
      ++pos;
      Util::UCS4ToUTF8Append(w, &result);
      continue;
    } else if (pos > 0 && s == Util::HIRAGANA) {
      has_hiragana = true;
      Util::UCS4ToUTF8Append(w, &result);
      break;
    } else {
      return "";
    }
  }

  if (has_hiragana && has_kanji) {
    return result;
  }

  return "";
}

const UsageDictItem* UsageRewriter::LookupUnmatchedUsageHeuristically(
    const Segment::Candidate &candidate) const {
  // We check Unknwon POS ("名詞,サ変接続") as well, since
  // target verbs/adjectives may be in web dictionary.
  if (!POSMatcher::IsContentWordWithConjugation(candidate.lid) &&
      !POSMatcher::IsUnknown(candidate.lid)) {
    return NULL;
  }

  const string value = GetKanjiPrefixAndOneHiragana(candidate.content_value);
  if (value.empty()) {
    return NULL;
  }

  // key is empty;
  StrPair key_value("", value);
  const map<StrPair, const UsageDictItem *>::const_iterator itr =
      key_value_usageitem_map_.find(key_value);
  // Check result key part is a prefix of the content_key.
  if (itr != key_value_usageitem_map_.end() &&
      Util::StartsWith(candidate.content_key, itr->second->key)) {
    return itr->second;
  }

  return NULL;
}

const UsageDictItem* UsageRewriter::LookupUsage(
    const Segment::Candidate &candidate) const {
  const string &key = candidate.content_key;
  const string &value = candidate.content_value;
  StrPair key_value(key, value);
  const map<StrPair, const UsageDictItem *>::const_iterator itr =
      key_value_usageitem_map_.find(key_value);
  if (itr != key_value_usageitem_map_.end()) {
    return itr->second;
  }

  return LookupUnmatchedUsageHeuristically(candidate);
}

bool UsageRewriter::Rewrite(Segments *segments) const {
  DLOG(INFO) << segments->DebugString();
  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    DCHECK(segment);
    for (size_t j = 0; j < segment->candidates_size(); ++j) {
      const UsageDictItem *usage = LookupUsage(segment->candidate(j));

      if (usage != NULL) {
        Segment::Candidate *candidate = segment->mutable_candidate(j);
        DCHECK(candidate);
        candidate->usage_id = usage->id;
        candidate->usage_title = string(usage->value)
          + kBaseConjugationSuffix[usage->conjugation_id].value_suffix;
        candidate->usage_description = usage->meaning;
        DLOG(INFO) << i << ":" << j << ":" <<
            candidate->content_key << ":" << candidate->content_value <<
            ":" << usage->key << ":" << usage->value <<
            ":" << usage->conjugation_id << ":" << usage->meaning;
        modified = true;
      }
    }
  }
  return modified;
}
}  // namespace mozc

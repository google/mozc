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

#include "rewriter/usage_rewriter.h"

#include <string>

#include "base/base.h"
#include "base/logging.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_dictionary.h"

namespace mozc {

UsageRewriter::UsageRewriter(const DataManagerInterface *data_manager,
                             const UserDictionary *user_dictionary)
    : pos_matcher_(data_manager->GetPOSMatcher()),
      user_dictionary_(user_dictionary),
      base_conjugation_suffix_(NULL) {
  const ConjugationSuffix *conjugation_suffix_data = NULL;
  const int *conjugation_suffix_data_index = NULL;
  const UsageDictItem *usage_data_value = NULL;
  data_manager->GetUsageRewriterData(&base_conjugation_suffix_,
                                     &conjugation_suffix_data,
                                     &conjugation_suffix_data_index,
                                     &usage_data_value);
  CHECK(base_conjugation_suffix_);
  CHECK(conjugation_suffix_data);
  CHECK(conjugation_suffix_data_index);
  CHECK(conjugation_suffix_data_index);
  const UsageDictItem *item = usage_data_value;
  // TODO(taku): To reduce memory footprint, better to replace it with
  // binary search over the conjugation_suffix_data diretly.
  for (; item->key != NULL; ++item) {
    for (size_t i = conjugation_suffix_data_index[item->conjugation_id];
         i < conjugation_suffix_data_index[item->conjugation_id + 1];
         ++i) {
      StrPair key_value1(
          string(item->key) + conjugation_suffix_data[i].key_suffix,
          string(item->value) + conjugation_suffix_data[i].value_suffix);
      key_value_usageitem_map_[key_value1] = item;
      StrPair key_value2(
          "",
          string(item->value) + conjugation_suffix_data[i].value_suffix);
      key_value_usageitem_map_[key_value2] = item;
    }
  }
}

UsageRewriter::~UsageRewriter() {
}

// static
// "合いました" => "合い"
string UsageRewriter::GetKanjiPrefixAndOneHiragana(const string &word) {
  // TODO(hidehiko): Refactor more based on ConstChar32Iterator.
  string result;
  int pos = 0;
  bool has_kanji = false;
  bool has_hiragana = false;
  for (ConstChar32Iterator iter(word); !iter.Done(); iter.Next()) {
    const char32 w = iter.Get();
    const Util::ScriptType s = Util::GetScriptType(w);
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
  if (!pos_matcher_->IsContentWordWithConjugation(candidate.lid) &&
      !pos_matcher_->IsUnknown(candidate.lid)) {
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

bool UsageRewriter::Rewrite(const ConversionRequest &request,
                            Segments *segments) const {
  VLOG(2) << segments->DebugString();

  const config::Config &config = config::ConfigHandler::GetConfig();
  // Default value of use_local_usage_dictionary() is true.
  // So if information_list_config() is not available in the config,
  // we don't need to return false here.
  if (config.has_information_list_config() &&
      !config.information_list_config().use_local_usage_dictionary()) {
    return false;
  }

  bool modified = false;
  // UsageIDs for embedded usage dictionary are generated in advance by
  // gen_usage_rewriter_dictionary_main.cc (which are just sequential numbers).
  // However, since user dictionary comments don't have such IDs, dynamically
  // genereate them so that they don't conflict with those of the embedded usage
  // dictionary.  Since just the uniqueness in one Segments is sufficient, for
  // usage from the user dictionary, we simply assign sequential numbers larger
  // than the maximum ID of the embedded usage dictionary.
  int32 usage_id_for_user_comment = key_value_usageitem_map_.size();
  string comment;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    DCHECK(segment);
    for (size_t j = 0; j < segment->candidates_size(); ++j) {
      ++usage_id_for_user_comment;

      // First, search the user dictionary for comment.
      if (user_dictionary_ != NULL) {
        user_dictionary_->LookupComment(segment->candidate(j).content_key,
                                        segment->candidate(j).content_value,
                                        &comment);
        if (!comment.empty()) {
          Segment::Candidate *candidate = segment->mutable_candidate(j);
          candidate->usage_id = usage_id_for_user_comment;
          candidate->usage_title = segment->candidate(j).content_value;
          candidate->usage_description = comment;
          modified = true;
          continue;
        }
      }

      // If comment isn't in the user dictionary, search the system usage
      // dictionary.
      const UsageDictItem *usage = LookupUsage(segment->candidate(j));
      if (usage != NULL) {
        Segment::Candidate *candidate = segment->mutable_candidate(j);
        DCHECK(candidate);
        candidate->usage_id = usage->id;
        candidate->usage_title
            .assign(usage->value)
            .append(
                base_conjugation_suffix_[usage->conjugation_id].value_suffix);
        candidate->usage_description = usage->meaning;
        VLOG(2) << i << ":" << j << ":" <<
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

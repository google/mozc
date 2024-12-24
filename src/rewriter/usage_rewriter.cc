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

#include "rewriter/usage_rewriter.h"

#ifndef NO_USAGE_REWRITER
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "base/util.h"
#include "base/vlog.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"

namespace mozc {

using ::mozc::dictionary::DictionaryInterface;

UsageRewriter::UsageRewriter(const DataManager *data_manager,
                             const DictionaryInterface *dictionary)
    : pos_matcher_(data_manager->GetPosMatcherData()),
      dictionary_(dictionary),
      base_conjugation_suffix_(nullptr) {
  absl::string_view base_conjugation_suffix_data;
  absl::string_view conjugation_suffix_data;
  absl::string_view conjugation_suffix_index_data;
  absl::string_view usage_items_data;
  absl::string_view string_array_data;
  data_manager->GetUsageRewriterData(
      &base_conjugation_suffix_data, &conjugation_suffix_data,
      &conjugation_suffix_index_data, &usage_items_data, &string_array_data);
  base_conjugation_suffix_ =
      reinterpret_cast<const uint32_t *>(base_conjugation_suffix_data.data());
  const uint32_t *conjugation_suffix =
      reinterpret_cast<const uint32_t *>(conjugation_suffix_data.data());
  const uint32_t *conjugation_suffix_data_index =
      reinterpret_cast<const uint32_t *>(conjugation_suffix_index_data.data());

  if (SerializedStringArray::VerifyData(string_array_data)) {
    string_array_.Set(string_array_data);
  } else {
    // \0\0\0\0 is the header value of the data size.
    string_array_.Set({"\0\0\0\0", 4});
  }

  UsageDictItemIterator begin(usage_items_data.data());
  UsageDictItemIterator end(usage_items_data.data() + usage_items_data.size());

  // TODO(taku): To reduce memory footprint, better to replace it with
  // binary search over the conjugation_suffix_data directly.
  for (; begin != end; ++begin) {
    for (size_t i = conjugation_suffix_data_index[begin.conjugation_id()];
         i < conjugation_suffix_data_index[begin.conjugation_id() + 1]; ++i) {
      const absl::string_view key = string_array_[begin.key_index()];
      const absl::string_view value = string_array_[begin.value_index()];
      const absl::string_view key_suffix =
          string_array_[conjugation_suffix[2 * i + 1]];
      const absl::string_view value_suffix =
          string_array_[conjugation_suffix[2 * i]];
      const StrPair key_value1(absl::StrCat(key, key_suffix),
                               absl::StrCat(value, value_suffix));
      key_value_usageitem_map_[key_value1] = begin;

      const StrPair key_value2("", key_value1.second);
      key_value_usageitem_map_[key_value2] = begin;
    }
  }
}

// static
// "合いました" => "合い"
std::string UsageRewriter::GetKanjiPrefixAndOneHiragana(
    const absl::string_view word) {
  // TODO(hidehiko): Refactor more based on ConstChar32Iterator.
  std::string result;
  int pos = 0;
  bool has_kanji = false;
  bool has_hiragana = false;
  for (ConstChar32Iterator iter(word); !iter.Done(); iter.Next()) {
    const char32_t codepoint = iter.Get();
    const Util::ScriptType s = Util::GetScriptType(codepoint);
    if (pos == 0 && s != Util::KANJI) {
      return "";
    } else if (pos >= 0 && pos <= 1 && s == Util::KANJI) {
      // length of kanji <= 2.
      has_kanji = true;
      ++pos;
      Util::CodepointToUtf8Append(codepoint, &result);
      continue;
    } else if (pos > 0 && s == Util::HIRAGANA) {
      has_hiragana = true;
      Util::CodepointToUtf8Append(codepoint, &result);
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

UsageRewriter::UsageDictItemIterator
UsageRewriter::LookupUnmatchedUsageHeuristically(
    const Segment::Candidate &candidate) const {
  // We check Unknown POS ("名詞,サ変接続") as well, since
  // target verbs/adjectives may be in web dictionary.
  if (!pos_matcher_.IsContentWordWithConjugation(candidate.lid) &&
      !pos_matcher_.IsUnknown(candidate.lid)) {
    return UsageDictItemIterator();
  }

  const std::string value =
      GetKanjiPrefixAndOneHiragana(candidate.content_value);
  if (value.empty()) {
    return UsageDictItemIterator();
  }

  // key is empty;
  StrPair key_value("", value);
  const auto itr = key_value_usageitem_map_.find(key_value);
  if (itr == key_value_usageitem_map_.end()) {
    return UsageDictItemIterator();
  }
  // Check result key part is a prefix of the content_key.
  const absl::string_view key = string_array_[itr->second.key_index()];
  if (absl::StartsWith(candidate.content_key, key)) {
    return itr->second;
  }

  return UsageDictItemIterator();
}

UsageRewriter::UsageDictItemIterator UsageRewriter::LookupUsage(
    const Segment::Candidate &candidate) const {
  const absl::string_view key = candidate.content_key;
  const absl::string_view value = candidate.content_value;
  StrPair key_value(key, value);
  const auto itr = key_value_usageitem_map_.find(key_value);
  if (itr != key_value_usageitem_map_.end()) {
    return itr->second;
  }

  return LookupUnmatchedUsageHeuristically(candidate);
}

bool UsageRewriter::Rewrite(const ConversionRequest &request,
                            Segments *segments) const {
  MOZC_VLOG(2) << segments->DebugString();

  const config::Config &config = request.config();
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
  // generate them so that they don't conflict with those of the embedded usage
  // dictionary.  Since just the uniqueness in one Segments is sufficient, for
  // usage from the user dictionary, we simply assign sequential numbers larger
  // than the maximum ID of the embedded usage dictionary.
  int32_t usage_id_for_user_comment = key_value_usageitem_map_.size();
  std::string comment;  // LookupComment rarely returns true.
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    DCHECK(segment);
    for (size_t j = 0; j < segment->candidates_size(); ++j) {
      ++usage_id_for_user_comment;

      // First, search the user dictionary for comment.
      if (dictionary_ != nullptr) {
        if (dictionary_->LookupComment(segment->candidate(j).content_key,
                                       segment->candidate(j).content_value,
                                       request, &comment)) {
          Segment::Candidate *candidate = segment->mutable_candidate(j);
          candidate->usage_id = usage_id_for_user_comment;
          candidate->usage_title = segment->candidate(j).content_value;
          candidate->usage_description = std::move(comment);
          comment.clear();
          modified = true;
          continue;
        }
      }

      // If comment isn't in the user dictionary, search the system usage
      // dictionary.
      const UsageDictItemIterator iter = LookupUsage(segment->candidate(j));
      if (iter.IsValid()) {
        Segment::Candidate *candidate = segment->mutable_candidate(j);
        DCHECK(candidate);
        candidate->usage_id = iter.usage_id();

        const absl::string_view value_suffix =
            string_array_[base_conjugation_suffix_[2 * iter.conjugation_id()]];
        candidate->usage_title.assign(string_array_[iter.value_index()].data(),
                                      string_array_[iter.value_index()].size());
        candidate->usage_title.append(value_suffix.data(), value_suffix.size());

        candidate->usage_description.assign(
            string_array_[iter.meaning_index()].data(),
            string_array_[iter.meaning_index()].size());

        MOZC_VLOG(2) << i << ":" << j << ":" << candidate->content_key << ":"
                     << candidate->content_value << ":"
                     << string_array_[iter.key_index()] << ":"
                     << string_array_[iter.value_index()] << ":"
                     << iter.conjugation_id() << ":"
                     << string_array_[iter.meaning_index()];
        modified = true;
      }
    }
  }
  return modified;
}

}  // namespace mozc

#endif  // NO_USAGE_REWRITER

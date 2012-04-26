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

#include "rewriter/collocation_rewriter.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/mmap.h"
#include "base/singleton.h"
#include "base/util.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/collocation_util.h"
#include "rewriter/rewriter_interface.h"
#include "storage/existence_filter.h"

DEFINE_bool(use_collocation, true, "use collocation rewrite");

namespace mozc {
namespace {
#include "rewriter/embedded_collocation_data.h"
#include "rewriter/embedded_collocation_suppression_data.h"

const size_t kCandidateSize = 12;

class CollocationFilter {
 public:
  CollocationFilter()
      : filter_(
          ExistenceFilter::Read(
              reinterpret_cast<const char *>(CollocationData::kExistenceData),
              CollocationData::kExistenceDataSize)) {}
  ~CollocationFilter() {}

  bool Exists(uint64 id) const {
    return filter_->Exists(id);
  }

 private:
  scoped_ptr<ExistenceFilter> filter_;
};

class SuppressionFilter {
 public:
  SuppressionFilter()
      : filter_(
          ExistenceFilter::Read(
              reinterpret_cast<const char *>(
                  CollocationSuppressionData::kExistenceData),
              CollocationSuppressionData::kExistenceDataSize)) {}
  ~SuppressionFilter() {}

  bool Exists(uint64 id) const {
    return filter_->Exists(id);
  }

 private:
  scoped_ptr<ExistenceFilter> filter_;
};

bool LookupPair(const string &left, const string &right) {
  if (left.empty() || right.empty()) {
    return false;
  }

  const string pair = left + right;
  const uint64 id = Util::Fingerprint(pair);
  return Singleton<CollocationFilter>::get()->Exists(id);
}

// Returns true if the content key and value of the candidate is "ateji".
// Atejis are stored using Bloom filter, so it is possible that non-ateji words
// are sometimes mistakenly classified as ateji, resulting in passing on the
// right collocations, though the possibility is very low (0.001% by default).
bool ContentIsAteji(const Segment::Candidate &cand) {
  const char kSeparator[] = "\t";
  const string key = cand.content_value + kSeparator + cand.content_key;
  const uint64 id = Util::Fingerprint(key);
  return Singleton<SuppressionFilter>::get()->Exists(id);
}

bool ContainsScriptType(const string &str, Util::ScriptType type) {
  const char *begin = str.data();
  const char *end = str.data() + str.size();
  size_t mblen = 0;
  while (begin < end) {
    const uint16 w = Util::UTF8ToUCS2(begin, end, &mblen);
    if (Util::GetScriptType(w) == type) {
      return true;
    }
    begin += mblen;
  }
  return false;
}

// returns true if the given string contains number including Kanji.
bool ContainsNumber(const string &str) {
  const char *begin = str.data();
  const char *end = str.data() + str.size();
  size_t mblen = 0;

  while (begin < end) {
    const uint16 wchar = Util::UTF8ToUCS2(begin, end, &mblen);
    if (CollocationUtil::IsNumber(wchar)) {
      return true;
    }
    begin += mblen;
  }
  return false;
}

bool IsEndWith(const string& str, const uint16 *pattern, int pat_len) {
  const char *begin = str.data();
  const char *end = str.data() + str.size();
  size_t mblen = 0;
  int pos = 0;

  while (begin < end) {
    const uint16 wchar = Util::UTF8ToUCS2(begin, end, &mblen);
    if (wchar == pattern[pos]) {
      ++pos;
    } else if (pos != 0) {
      return false;
    }

    begin += mblen;
    if (pos == pat_len) {
      if (begin == end) {
        return true;
      } else {
        return false;
      }
    }
  }
  return false;
}

bool ParseCompound(const string &value, const uint16 *pattern, int pat_len,
                   string *first_content, string *first_aux, string *second) {
  const char *begin = value.data();
  const char *end = value.data() + value.size();
  size_t mblen = 0;
  int pos = 0;

  while (begin < end) {
    const uint16 wchar = Util::UTF8ToUCS2(begin, end, &mblen);
    if (pos < pat_len && wchar == pattern[pos]) {
      Util::UCS2ToUTF8Append(wchar, first_aux);
      ++pos;
    } else if (pos == 0) {
      Util::UCS2ToUTF8Append(wchar, first_content);
    } else if (pos < pat_len) {
      return false;
    } else if (pos == pat_len) {
      Util::UCS2ToUTF8Append(wchar, second);
    }

    begin += mblen;
  }

  if (second->empty() ||
      !Util::IsScriptType(*first_content, Util::KANJI) ||
      !ContainsScriptType(*second, Util::KANJI)) {
    return false;
  } else {
    return true;
  }
}

// handles compound such as "本を読む"(one segment)
// we want to rewrite using it as if it was "<本|を><読む>"
// so that we can use collocation data like "暑い本"
void ResolveCompoundSegment(const string &top_value, const string &value,
                            bool is_first,
                            vector<string> *output) {
  // "格助詞"
  // see "http://ja.wikipedia.org/wiki/助詞"
  static const uint16 kPat1[] = {0x304C};  // "が"
  // "の" was not good...
  // static const uint16 kPat2[] = {0x306E};  // "の"
  static const uint16 kPat3[] = {0x3092};  // "を"
  static const uint16 kPat4[] = {0x306B};  // "に"
  static const uint16 kPat5[] = {0x3078};  // "へ"
  static const uint16 kPat6[] = {0x3068};  // "と"
  static const uint16 kPat7[] = {0x304B, 0x3089};  // "から"
  static const uint16 kPat8[] = {0x3088, 0x308A};  // "より"
  static const uint16 kPat9[] = {0x3067};  // "で"

  static const struct {
    const uint16 *pat;
    int len;
  } kParticles[] = {
    {kPat1, arraysize(kPat1)},
    //    {kPat2, arraysize(kPat2)},
    {kPat3, arraysize(kPat3)},
    {kPat4, arraysize(kPat4)},
    {kPat5, arraysize(kPat5)},
    {kPat6, arraysize(kPat6)},
    {kPat7, arraysize(kPat7)},
    {kPat8, arraysize(kPat8)},
    {kPat9, arraysize(kPat9)},
    {NULL, 0}
  };

  for (int i =0; kParticles[i].pat != NULL; ++i) {
    string first_content, first_aux, second;
    if (!ParseCompound(top_value, kParticles[i].pat, kParticles[i].len,
                       &first_content, &first_aux, &second)) {
      continue;
    }

    first_content.clear();
    first_aux.clear();
    second.clear();
    if (ParseCompound(value, kParticles[i].pat, kParticles[i].len,
                      &first_content, &first_aux, &second)) {
      if (is_first) {
        output->push_back(second);
        output->push_back(first_content + first_aux);
      } else {
        output->push_back(first_content);
      }
      return;
    }
  }
}

bool IsNaturalContent(const Segment::Candidate &cand,
                      const Segment::Candidate &top_cand,
                      bool is_first,
                      vector<string> *output) {
  const string& content = cand.content_value;
  const string& value = cand.value;
  const string& top_content = top_cand.content_value;
  const string& top_value = top_cand.value;

  if (!is_first &&
      value != top_value &&
      Util::CharsLen(top_content) >= 2 &&
      Util::CharsLen(content) == 1) {
    return false;
  }

  if (is_first) {
    output->push_back(value);
  } else {
    output->push_back(content);
    // "舞って" workaround
    // V+"て" is often treated as one compound.
    static const uint16 kPat[] = {0x3066};  // "て"
    if (IsEndWith(content, kPat, arraysize(kPat))) {
      output->push_back(
          Util::SubString(content, 0, Util::CharsLen(content) - 1));
    }
  }

  string aux_value =
      Util::SubString(value, Util::CharsLen(content), string::npos);
  string top_aux_value =
      Util::SubString(top_value, Util::CharsLen(top_content), string::npos);

  // we don't rewrite NUMBER to others and vice versa
  if (ContainsNumber(value) != ContainsNumber(top_value)) {
    return false;
  }

  // we don't rewrite KATAKANA segment
  // for example, we don't rewrite "コーヒー飲みます" to "珈琲飲みます"
  if (is_first &&
      Util::CharsLen(top_aux_value) == 0 &&
      top_value != value &&
      Util::IsScriptType(top_value, Util::KATAKANA)) {
    return false;
  }

  // special cases
  if (Util::CharsLen(top_content) == 1) {
    const char *begin = top_content.data();
    const char *end = top_content.data() + top_content.size();
    size_t mblen = 0;
    const uint16 wchar = Util::UTF8ToUCS2(begin, end, &mblen);

    switch (wchar) {
      case 0x304a:  // "お"
      case 0x5fa1:  // "御"
      case 0x3054:  // "ご"
        return true;
      default:
        break;
    }
  }

  string aux_normalized, top_aux_normalized;
  CollocationUtil::GetNormalizedScript(aux_value, &aux_normalized);
  CollocationUtil::GetNormalizedScript(top_aux_value, &top_aux_normalized);
  if (Util::CharsLen(aux_normalized) > 0 &&
      !Util::IsScriptType(aux_normalized, Util::HIRAGANA)) {
    if (!is_first) {
      return false;
    }
    if (aux_normalized != top_aux_normalized) {
      return false;
    }
  }

  ResolveCompoundSegment(top_value, value, is_first, output);

  // "<XXいる|>" can be rewrited to "<YY|いる>" and vice versa
  {
    static const uint16 kPat[] = {0x3044, 0x308B};  // "いる"
    if (Util::CharsLen(top_aux_value) == 0 &&
        IsEndWith(top_value, kPat, arraysize(kPat)) &&
        Util::CharsLen(aux_value) == 2 &&
        IsEndWith(aux_value, kPat, arraysize(kPat))) {
      if (!is_first) {
        // "YYいる" in addition to "YY"
        output->push_back(value);
      }
      return true;
    }
    if (Util::CharsLen(aux_value) == 0 &&
        IsEndWith(value, kPat, arraysize(kPat)) &&
        Util::CharsLen(top_aux_value) == 2 &&
        IsEndWith(top_aux_value, kPat, arraysize(kPat))) {
      if (!is_first) {
        // "YY" in addition to "YYいる"
        output->push_back(Util::SubString(value, 0,
                                          Util::CharsLen(value) - 2));
      }
      return true;
    }
  }

  // "<XXせる|>" can be rewrited to "<YY|せる>" and vice versa
  {
    static const uint16 kPat[] = {0x305B, 0x308B};  // "せる"
    if (Util::CharsLen(top_aux_value) == 0 &&
        IsEndWith(top_value, kPat, arraysize(kPat)) &&
        Util::CharsLen(aux_value) == 2 &&
        IsEndWith(aux_value, kPat, arraysize(kPat))) {
      if (!is_first) {
        // "YYせる" in addition to "YY"
        output->push_back(value);
      }
      return true;
    }
    if (Util::CharsLen(aux_value) == 0 &&
        IsEndWith(value, kPat, arraysize(kPat)) &&
        Util::CharsLen(top_aux_value) == 2 &&
        IsEndWith(top_aux_value, kPat, arraysize(kPat))) {
      if (!is_first) {
        // "YY" in addition to "YYせる"
        output->push_back(Util::SubString(value, 0,
                                          Util::CharsLen(value) - 2));
      }
      return true;
    }
  }

  // "<XX|する>" can be rewrited using "<XXす|る>" and "<XX|する>"
  // in "<XX|する>", XX must be single script type
  // "評する"
  {
    static const uint16 kPat[] = { 0x3059, 0x308B};  // "する"
    if (Util::CharsLen(aux_value) == 2 &&
        IsEndWith(aux_value, kPat, arraysize(kPat))) {
      if (!Util::IsScriptType(content, Util::KATAKANA) &&
          !Util::IsScriptType(content, Util::HIRAGANA) &&
          !Util::IsScriptType(content, Util::KANJI) &&
          !Util::IsScriptType(content, Util::ALPHABET)) {
        return false;
      }
      if (!is_first) {
        // "YYす" in addition to "YY"
        output->push_back(Util::SubString(value, 0,
                                          Util::CharsLen(value) - 1));
      }
      return true;
    }
  }

  // "<XXる>" can be rewrited using "<XX|る>"
  // "まとめる", "衰える"
  {
    static const uint16 kPat[] = {0x308B};  // "る"
    if (Util::CharsLen(aux_value) == 0 &&
        IsEndWith(value, kPat, arraysize(kPat))) {
      if (!is_first) {
        // "YY" in addition to "YYる"
        output->push_back(Util::SubString(value, 0,
                                          Util::CharsLen(value) - 1));
      }
      return true;
    }
  }

  // "<XXす>" can be rewrited using "XXする"
  {
    static const uint16 kPat[] = {0x3059};  // "す"
    if (IsEndWith(value, kPat, arraysize(kPat)) &&
        Util::IsScriptType(
            Util::SubString(value, 0, Util::CharsLen(value) - 1),
            Util::KANJI)) {
      if (!is_first) {
        string val = value;
        Util::UCS2ToUTF8Append(0x308B, &val);  // "る"
        // "YYする" in addition to "YY"
        output->push_back(val);
      }
      return true;
    }
  }

  // "<XXし|た>" can be rewrited using "<XX|した>"
  {
    static const uint16 kPat[] = {0x3057};  // "し"
    static const string aux = "\xe3\x81\x9f";  // "た"
    if (IsEndWith(content, kPat, arraysize(kPat)) &&
        aux_value == aux &&
        IsEndWith(top_content, kPat, arraysize(kPat)) &&
        top_aux_value == aux) {
      if (!is_first) {
        string val = Util::SubString(content, 0, Util::CharsLen(content) - 1);
        // XX must be KANJI
        if (Util::IsScriptType(val, Util::KANJI)) {
          output->push_back(val);
        }
      }
      return true;
    }
  }

  int aux_len = Util::CharsLen(value) - Util::CharsLen(content);
  int top_aux_len = Util::CharsLen(top_value) - Util::CharsLen(top_content);
  if (aux_len != top_aux_len) {
    return false;
  }

  // we don't rewrite HIRAGANA to KATAKANA
  if (Util::IsScriptType(top_content, Util::HIRAGANA) &&
      Util::IsScriptType(content, Util::KATAKANA)) {
    return false;
  }

  // we don't rewrite second KATAKANA
  // for example, we don't rewrite "このコーヒー" to "この珈琲"
  if (!is_first &&
      value != top_value &&
      Util::IsScriptType(top_content, Util::KATAKANA)) {
    return false;
  }

  if (Util::CharsLen(top_content) == 1 &&
      Util::IsScriptType(top_content, Util::HIRAGANA)) {
    return false;
  }

  // suppress "<身|ています>" etc.
  if (top_content != content &&
      Util::CharsLen(top_content) == 1 &&
      Util::CharsLen(content) == 1 &&
      Util::CharsLen(top_aux_value) >= 2 &&
      Util::CharsLen(aux_value) >= 2 &&
      Util::IsScriptType(top_content, Util::KANJI) &&
      Util::IsScriptType(content, Util::KANJI)) {
    return false;
  }

  return true;
}

bool IsKeyUnknown(const Segment &seg) {
  return Util::IsScriptType(seg.key(), Util::UNKNOWN_SCRIPT);
}
}  // namespace

bool CollocationRewriter::RewriteCollocation(Segments *segments) const {
  // return false if at least one segment is fixed.
  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    if (segments->segment(i).segment_type() == Segment::FIXED_VALUE) {
      return false;
    }
  }

  vector<bool> segs_changed =
      vector<bool>(segments->segments_size(), false);
  bool changed = false;

  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    bool rewrited_next = false;

    if (IsKeyUnknown(segments->segment(i))) {
      continue;
    }

    if (i + 1 < segments->segments_size() &&
        RewriteUsingNextSegment(segments->mutable_segment(i + 1),
                                segments->mutable_segment(i))) {
      changed = true;
      rewrited_next = true;
      segs_changed[i] = true;
      segs_changed[i + 1] = true;
    }

    if (!segs_changed[i] &&
        !rewrited_next &&
        i > 0 &&
        RewriteFromPrevSegment(segments->segment(i - 1).candidate(0),
                               segments->mutable_segment(i))) {
      changed = true;
      segs_changed[i - 1] = true;
      segs_changed[i] = true;
    }

    const Segment::Candidate &cand = segments->segment(i).candidate(0);
    if (i >= 2 &&
        // Don't cross over the number candidate
        !ContainsNumber(segments->segment(i - 1).candidate(0).value) &&
        (cand.content_value != cand.value ||
         cand.value != "\xe3\x83\xbb")) {  // "・" workaround
      if (!segs_changed[i - 2] &&
          !segs_changed[i] &&
          RewriteUsingNextSegment(segments->mutable_segment(i),
                                  segments->mutable_segment(i - 2))) {
        changed = true;
        segs_changed[i] = true;
        segs_changed[i - 2] = true;
      } else if (!segs_changed[i] &&
                 RewriteFromPrevSegment(
                     segments->segment(i - 2).candidate(0),
                     segments->mutable_segment(i))) {
        changed = true;
        segs_changed[i] = true;
        segs_changed[i - 2] = true;
      }
    }
  }

  return changed;
}

CollocationRewriter::CollocationRewriter(const POSMatcher &pos_matcher)
    : first_name_id_(pos_matcher.GetFirstNameId()),
      last_name_id_(pos_matcher.GetLastNameId()) {}

CollocationRewriter::~CollocationRewriter() {}

bool CollocationRewriter::Rewrite(const ConversionRequest &request,
                                  Segments *segments) const {
  return RewriteCollocation(segments);
}

bool CollocationRewriter::IsName(const Segment::Candidate &cand) const {
  const bool ret = (cand.lid == last_name_id_ || cand.lid == first_name_id_);
  if (ret) {
    VLOG(3) << cand.value << " is name sagment";
  }
  return ret;
}

bool CollocationRewriter::RewriteFromPrevSegment(
    const Segment::Candidate &prev_cand,
    Segment *seg) const {
  string prev;
  CollocationUtil::GetNormalizedScript(prev_cand.value, &prev);

  const size_t i_max = min(seg->candidates_size(), kCandidateSize);
  for (size_t i = 0; i < i_max; ++i) {
    vector<string> curs;
    if (!IsNaturalContent(seg->candidate(i), seg->candidate(0),
                          false, &curs)) {
      continue;
    }
    if (IsName(seg->candidate(i))) {
      continue;
    }
    if (ContentIsAteji(seg->candidate(i))) {
      continue;
    }

    for (int j = 0; j < curs.size(); ++j) {
      string cur;
      CollocationUtil::GetNormalizedScript(curs[j], &cur);
      if (LookupPair(prev, cur)) {
        if (i != 0) {
          VLOG(3) << prev << cur << " "
                  << seg->candidate(0).value << "->"
                  << seg->candidate(i).value;
        }
        seg->move_candidate(i, 0);
        seg->mutable_candidate(0)->attributes
            |= Segment::Candidate::CONTEXT_SENSITIVE;
        return true;
      }
    }
  }
  return false;
}

bool CollocationRewriter::RewriteUsingNextSegment(Segment *next_seg,
                                                  Segment *seg) const {
  const size_t i_max = min(seg->candidates_size(), kCandidateSize);
  const size_t j_max = min(next_seg->candidates_size(), kCandidateSize);

  // Cache the results for the next segment
  vector<int> next_seg_ok(j_max);  // Avoiding vector<bool>
  vector<vector<string> > normalized_string(j_max);

  for (size_t j = 0; j < j_max; ++j) {
    next_seg_ok[j] = 0;
    vector<string> nexts;

    if (!IsNaturalContent(next_seg->candidate(j),
                          next_seg->candidate(0), false, &nexts)) {
      continue;
    }
    if (IsName(next_seg->candidate(j))) {
      continue;
    }
    if (ContentIsAteji(next_seg->candidate(j))) {
      continue;
    }
    next_seg_ok[j] = 1;
    for (vector<string>::const_iterator it = nexts.begin();
         it != nexts.end(); ++it) {
      string next_normalized;
      CollocationUtil::GetNormalizedScript(*it, &next_normalized);
      normalized_string[j].push_back(next_normalized);
    }
  }

  for (size_t i = 0; i < i_max; ++i) {
    vector<string> curs;
    if (!IsNaturalContent(seg->candidate(i), seg->candidate(0), true, &curs)) {
      continue;
    }
    if (IsName(seg->candidate(i))) {
      continue;
    }
    if (ContentIsAteji(seg->candidate(i))) {
      continue;
    }

    for (int k = 0; k < curs.size(); ++k) {
      string cur;
      CollocationUtil::GetNormalizedScript(curs[k], &cur);
      for (size_t j = 0; j < j_max; ++j) {
        if (!next_seg_ok[j]) {
          continue;
        }

        for (int l = 0; l < normalized_string[j].size(); ++l) {
          const string &next = normalized_string[j][l];
          if (LookupPair(cur, next)) {
            vector<string> nexts;
            DCHECK(IsNaturalContent(next_seg->candidate(j),
                                    next_seg->candidate(0), false, &nexts))
                << "IsNaturalContent() should not fail here.";

            VLOG(3) << curs[k] << nexts[l] << " " << cur << next;
            seg->move_candidate(i, 0);
            seg->mutable_candidate(0)->attributes
                |= Segment::Candidate::CONTEXT_SENSITIVE;
            next_seg->move_candidate(j, 0);
            next_seg->mutable_candidate(0)->attributes
                |= Segment::Candidate::CONTEXT_SENSITIVE;
            return true;
          }
        }
      }
    }
  }
  return false;
}

}  // namespace mozc

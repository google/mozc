// Copyright 2010-2014, Google Inc.
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
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/util.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/collocation_util.h"
#include "storage/existence_filter.h"

DEFINE_bool(use_collocation, true, "use collocation rewrite");

namespace mozc {

using mozc::storage::ExistenceFilter;

namespace {
const size_t kCandidateSize = 12;

// For collocation, we use two segments.
enum SegmentLookupType {
  LEFT,
  RIGHT,
};

// returns true if the given string contains number including Kanji.
bool ContainsNumber(const string &str) {
  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    if (CollocationUtil::IsNumber(iter.Get())) {
      return true;
    }
  }
  return false;
}

// Returns true if value matches the pattern XXXPPPYYY, where XXX is a Kanji
// sequence, PPP is the given pattern, and YYY is a sequence containing at least
// one Kanji character. In the value matches the pattern, XXX and YYY are
// substituted to |first_content| and |second|, respectively. Returns false if
// the value isn't of the form XXXPPPYYY.
bool ParseCompound(const StringPiece value, const StringPiece pattern,
                   StringPiece *first_content, StringPiece *second) {
  DCHECK(!value.empty());
  DCHECK(!pattern.empty());

  // Find the |first_content| candidate and check if it consists of Kanji only.
  StringPiece::const_iterator pattern_begin =
      find(value.begin(), value.end(), pattern[0]);
  if (pattern_begin == value.end()) {
    return false;
  }
  first_content->set(value.data(), distance(value.begin(), pattern_begin));
  if (!Util::IsScriptType(*first_content, Util::KANJI)) {
    return false;
  }

  // Check if the middle part matches |pattern|.
  const StringPiece remaining_value = value.substr(first_content->size());
  if (!Util::StartsWith(remaining_value, pattern)) {
    return false;
  }

  // Check if the last substring is eligible for |second|.
  *second = remaining_value.substr(pattern.size());
  if (second->empty() || !Util::ContainsScriptType(*second, Util::KANJI)) {
    return false;
  }

  // Just verify that |value| = |first_content| + |pattern| + |second|.
  DCHECK_EQ(
      value,
      first_content->as_string() + pattern.as_string() + second->as_string());
  return true;
}

// Fast way of pushing back a string piece to a vector.
inline void PushBackStringPiece(const StringPiece s, vector<string> *v) {
  v->push_back(string());
  v->back().assign(s.data(), s.size());
}

// Fast way of pushing back the concatenated string of two string pieces to a
// vector.
inline void PushBackJoinedStringPieces(
    const StringPiece s1, const StringPiece s2, vector<string> *v) {
  v->push_back(string());
  v->back().reserve(s1.size() + s2.size());
  v->back().assign(s1.data(), s1.size()).append(s2.data(), s2.size());
}

// Handles compound such as "本を読む"(one segment)
// we want to rewrite using it as if it was "<本|を><読む>"
// so that we can use collocation data like "厚い本"
void ResolveCompoundSegment(const string &top_value, const string &value,
                            SegmentLookupType type,
                            vector<string> *output) {
  // "格助詞"
  // see "http://ja.wikipedia.org/wiki/助詞"
  static const char kPat1[] = "\xE3\x81\x8C";  // "が"
  // "の" was not good...
  // static const char kPat2[] = "\xE3\x81\xAE";  // "の"
  static const char kPat3[] = "\xE3\x82\x92";  // "を"
  static const char kPat4[] = "\xE3\x81\xAB";  // "に"
  static const char kPat5[] = "\xE3\x81\xB8";  // "へ"
  static const char kPat6[] = "\xE3\x81\xA8";  // "と"
  static const char kPat7[] = "\xE3\x81\x8B\xE3\x82\x89";  // "から"
  static const char kPat8[] = "\xE3\x82\x88\xE3\x82\x8A";  // "より"
  static const char kPat9[] = "\xE3\x81\xA7";  // "で"

  static const struct {
    const char *pat;
    size_t len;
  } kParticles[] = {
    {kPat1, arraysize(kPat1) - 1},
    //    {kPat2, arraysize(kPat2) - 1},
    {kPat3, arraysize(kPat3) - 1},
    {kPat4, arraysize(kPat4) - 1},
    {kPat5, arraysize(kPat5) - 1},
    {kPat6, arraysize(kPat6) - 1},
    {kPat7, arraysize(kPat7) - 1},
    {kPat8, arraysize(kPat8) - 1},
    {kPat9, arraysize(kPat9) - 1},
    {NULL, 0}
  };

  for (size_t i = 0; kParticles[i].pat != NULL; ++i) {
    const StringPiece particle(kParticles[i].pat, kParticles[i].len);
    StringPiece first_content, second;
    if (!ParseCompound(top_value, particle, &first_content, &second)) {
      continue;
    }
    if (ParseCompound(value, particle, &first_content, &second)) {
      if (type == LEFT) {
        PushBackStringPiece(second, output);
        PushBackJoinedStringPieces(first_content, particle, output);
      } else {
        PushBackStringPiece(first_content, output);
      }
      return;
    }
  }
}

bool IsNaturalContent(const Segment::Candidate &cand,
                      const Segment::Candidate &top_cand,
                      SegmentLookupType type,
                      vector<string> *output) {
  const string &content = cand.content_value;
  const string &value = cand.value;
  const string &top_content = top_cand.content_value;
  const string &top_value = top_cand.value;

  const size_t top_content_len = Util::CharsLen(top_content);
  const size_t content_len = Util::CharsLen(content);

  if (type == RIGHT &&
      value != top_value &&
      top_content_len >= 2 &&
      content_len == 1) {
    return false;
  }

  if (type == LEFT) {
    output->push_back(value);
  } else {
    output->push_back(content);
    // "舞って" workaround
    // V+"て" is often treated as one compound.
    static const char kPat[] = "\xE3\x81\xA6";  // "て"
    if (Util::EndsWith(content, StringPiece(kPat, arraysize(kPat) - 1))) {
      PushBackStringPiece(
          Util::SubStringPiece(content, 0, content_len - 1), output);
    }
  }

  // we don't rewrite NUMBER to others and vice versa
  if (ContainsNumber(value) != ContainsNumber(top_value)) {
    return false;
  }

  const StringPiece top_aux_value =
      Util::SubStringPiece(top_value, top_content_len, string::npos);
  const size_t top_aux_value_len = Util::CharsLen(top_aux_value);
  const Util::ScriptType top_value_script_type = Util::GetScriptType(top_value);

  // we don't rewrite KATAKANA segment
  // for example, we don't rewrite "コーヒー飲みます" to "珈琲飲みます"
  if (type == LEFT &&
      top_aux_value_len == 0 &&
      top_value != value &&
      top_value_script_type == Util::KATAKANA) {
    return false;
  }

  // special cases
  if (top_content_len == 1) {
    const char *begin = top_content.data();
    const char *end = top_content.data() + top_content.size();
    size_t mblen = 0;
    const char32 wchar = Util::UTF8ToUCS4(begin, end, &mblen);

    switch (wchar) {
      case 0x304a:  // "お"
      case 0x5fa1:  // "御"
      case 0x3054:  // "ご"
        return true;
      default:
        break;
    }
  }

  const StringPiece aux_value =
      Util::SubStringPiece(value, content_len, string::npos);

  // Remove number in normalization for the left segment.
  string aux_normalized, top_aux_normalized;
  CollocationUtil::GetNormalizedScript(
      aux_value, (type == LEFT), &aux_normalized);
  CollocationUtil::GetNormalizedScript(
      top_aux_value, (type == LEFT), &top_aux_normalized);
  if (!aux_normalized.empty() &&
      !Util::IsScriptType(aux_normalized, Util::HIRAGANA)) {
    if (type == RIGHT) {
      return false;
    }
    if (aux_normalized != top_aux_normalized) {
      return false;
    }
  }

  ResolveCompoundSegment(top_value, value, type, output);

  const size_t aux_value_len = Util::CharsLen(aux_value);
  const size_t value_len = Util::CharsLen(value);

  // "<XXいる|>" can be rewrited to "<YY|いる>" and vice versa
  {
    static const char kPat[] = "\xE3\x81\x84\xE3\x82\x8B";  // "いる"
    const StringPiece kSuffix(kPat, arraysize(kPat) - 1);
    if (top_aux_value_len == 0 &&
        aux_value_len == 2 &&
        Util::EndsWith(top_value, kSuffix) &&
        Util::EndsWith(aux_value, kSuffix)) {
      if (type == RIGHT) {
        // "YYいる" in addition to "YY"
        output->push_back(value);
      }
      return true;
    }
    if (aux_value_len == 0 &&
        top_aux_value_len == 2 &&
        Util::EndsWith(value, kSuffix) &&
        Util::EndsWith(top_aux_value, kSuffix)) {
      if (type == RIGHT) {
        // "YY" in addition to "YYいる"
        PushBackStringPiece(
            Util::SubStringPiece(value, 0, value_len - 2), output);
      }
      return true;
    }
  }

  // "<XXせる|>" can be rewrited to "<YY|せる>" and vice versa
  {
    const char kPat[] = "\xE3\x81\x9B\xE3\x82\x8B";  // "せる"
    const StringPiece kSuffix(kPat, arraysize(kPat) - 1);
    if (top_aux_value_len == 0 &&
        aux_value_len == 2 &&
        Util::EndsWith(top_value, kSuffix) &&
        Util::EndsWith(aux_value, kSuffix)) {
      if (type == RIGHT) {
        // "YYせる" in addition to "YY"
        output->push_back(value);
      }
      return true;
    }
    if (aux_value_len == 0 &&
        top_aux_value_len == 2 &&
        Util::EndsWith(value, kSuffix) &&
        Util::EndsWith(top_aux_value, kSuffix)) {
      if (type == RIGHT) {
        // "YY" in addition to "YYせる"
        PushBackStringPiece(
            Util::SubStringPiece(value, 0, value_len - 2), output);
      }
      return true;
    }
  }

  const Util::ScriptType content_script_type = Util::GetScriptType(content);

  // "<XX|する>" can be rewrited using "<XXす|る>" and "<XX|する>"
  // in "<XX|する>", XX must be single script type
  // "評する"
  {
    static const char kPat[] = "\xE3\x81\x99\xE3\x82\x8B";  // "する"
    const StringPiece kSuffix(kPat, arraysize(kPat) - 1);
    if (aux_value_len == 2 &&
        Util::EndsWith(aux_value, kSuffix)) {
      if (content_script_type != Util::KATAKANA &&
          content_script_type != Util::HIRAGANA &&
          content_script_type != Util::KANJI &&
          content_script_type != Util::ALPHABET) {
        return false;
      }
      if (type == RIGHT) {
        // "YYす" in addition to "YY"
        PushBackStringPiece(
            Util::SubStringPiece(value, 0, value_len - 1), output);
      }
      return true;
    }
  }

  // "<XXる>" can be rewrited using "<XX|る>"
  // "まとめる", "衰える"
  {
    static const char kPat[] = "\xE3\x82\x8B";  // "る"
    const StringPiece kSuffix(kPat, arraysize(kPat) - 1);
    if (aux_value_len == 0 &&
        Util::EndsWith(value, kSuffix)) {
      if (type == RIGHT) {
        // "YY" in addition to "YYる"
        PushBackStringPiece(
            Util::SubStringPiece(value, 0, value_len - 1), output);
      }
      return true;
    }
  }

  // "<XXす>" can be rewrited using "XXする"
  {
    static const char kPat[] = "\xE3\x81\x99";  // "す"
    const StringPiece kSuffix(kPat, arraysize(kPat) - 1);
    if (Util::EndsWith(value, kSuffix) &&
        Util::IsScriptType(
            Util::SubStringPiece(value, 0, value_len - 1),
            Util::KANJI)) {
      if (type == RIGHT) {
        const char kRu[] = "\xE3\x82\x8B";
        // "YYする" in addition to "YY"
        PushBackJoinedStringPieces(
            value, StringPiece(kRu, arraysize(kRu) - 1), output);
      }
      return true;
    }
  }

  // "<XXし|た>" can be rewrited using "<XX|した>"
  {
    static const char kPat[] = "\xE3\x81\x97\xE3\x81\x9F";  // "した"
    const StringPiece kShi(kPat, 3), kTa(kPat + 3, 3);
    if (Util::EndsWith(content, kShi) &&
        aux_value == kTa &&
        Util::EndsWith(top_content, kShi) &&
        top_aux_value == kTa) {
      if (type == RIGHT) {
        const StringPiece val =
            Util::SubStringPiece(content, 0, content_len - 1);
        // XX must be KANJI
        if (Util::IsScriptType(val, Util::KANJI)) {
          PushBackStringPiece(val, output);
        }
      }
      return true;
    }
  }

  const int aux_len = value_len - content_len;
  const int top_aux_len = Util::CharsLen(top_value) - top_content_len;
  if (aux_len != top_aux_len) {
    return false;
  }

  const Util::ScriptType top_content_script_type =
      Util::GetScriptType(top_content);

  // we don't rewrite HIRAGANA to KATAKANA
  if (top_content_script_type == Util::HIRAGANA &&
      content_script_type == Util::KATAKANA) {
    return false;
  }

  // we don't rewrite second KATAKANA
  // for example, we don't rewrite "このコーヒー" to "この珈琲"
  if (type == RIGHT &&
      top_content_script_type == Util::KATAKANA &&
      value != top_value) {
    return false;
  }

  if (top_content_len == 1 &&
      top_content_script_type == Util::HIRAGANA) {
    return false;
  }

  // suppress "<身|ています>" etc.
  if (top_content_len == 1 &&
      content_len == 1 &&
      top_aux_value_len >= 2 &&
      aux_value_len >= 2 &&
      top_content_script_type == Util::KANJI &&
      content_script_type == Util::KANJI &&
      top_content != content) {
    return false;
  }

  return true;
}

// Just a wrapper of IsNaturalContent for debug.
bool VerifyNaturalContent(const Segment::Candidate &cand,
                          const Segment::Candidate &top_cand,
                          SegmentLookupType type) {
  vector<string> nexts;
  return IsNaturalContent(cand, top_cand, RIGHT, &nexts);
}

inline bool IsKeyUnknown(const Segment &seg) {
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

  vector<bool> segs_changed(segments->segments_size(), false);
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
        // Cross over only adverbs
        // Segment is adverb if;
        //  1) lid and rid is adverb.
        //  2) or rid is adverb suffix.
        ((pos_matcher_->IsAdverb(segments->segment(i - 1).candidate(0).lid) &&
          pos_matcher_->IsAdverb(segments->segment(i - 1).candidate(0).rid)) ||
         pos_matcher_->IsAdverbSegmentSuffix(
             segments->segment(i - 1).candidate(0).rid)) &&
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

class CollocationRewriter::CollocationFilter {
 public:
  CollocationFilter(const char *existence_data, size_t size)
      : filter_(ExistenceFilter::Read(existence_data, size)) {
  }
  ~CollocationFilter() {
  }

  bool Exists(const string &left, const string &right) const {
    if (left.empty() || right.empty()) {
      return false;
    }
    string key;
    key.reserve(left.size() + right.size());
    key.assign(left).append(right);
    const uint64 id = Util::Fingerprint(key);
    return filter_->Exists(id);
  }

 private:
  scoped_ptr<ExistenceFilter> filter_;

  DISALLOW_COPY_AND_ASSIGN(CollocationFilter);
};

class CollocationRewriter::SuppressionFilter {
 public:
  SuppressionFilter(const char *suppression_data, size_t size)
      : filter_(ExistenceFilter::Read(suppression_data, size)) {
  }
  ~SuppressionFilter() {
  }

  bool Exists(const Segment::Candidate &cand) const {
    // TODO(noriyukit): We should share key generation rule with
    // gen_collocation_suppression_data_main.cc.
    string key;
    key.reserve(cand.content_value.size() + 1 + cand.content_key.size());
    key.assign(cand.content_value).append("\t").append(cand.content_key);
    const uint64 id = Util::Fingerprint(key);
    return filter_->Exists(id);
  }

 private:
  scoped_ptr<ExistenceFilter> filter_;

  DISALLOW_COPY_AND_ASSIGN(SuppressionFilter);
};

CollocationRewriter::CollocationRewriter(
    const DataManagerInterface *data_manager)
    : pos_matcher_(data_manager->GetPOSMatcher()),
      first_name_id_(pos_matcher_->GetFirstNameId()),
      last_name_id_(pos_matcher_->GetLastNameId()) {
  const char *data = NULL;
  size_t size = 0;

  data_manager->GetCollocationData(&data, &size);
  collocation_filter_.reset(new CollocationFilter(data, size));

  data_manager->GetCollocationSuppressionData(&data, &size);
  suppression_filter_.reset(new SuppressionFilter(data, size));
}

CollocationRewriter::~CollocationRewriter() {}

bool CollocationRewriter::Rewrite(const ConversionRequest &request,
                                  Segments *segments) const {
  return RewriteCollocation(segments);
}

bool CollocationRewriter::IsName(const Segment::Candidate &cand) const {
  const bool ret = (cand.lid == last_name_id_ || cand.lid == first_name_id_);
  VLOG_IF(3, ret) << cand.value << " is name sagment";
  return ret;
}

bool CollocationRewriter::RewriteFromPrevSegment(
    const Segment::Candidate &prev_cand,
    Segment *seg) const {
  string prev;
  CollocationUtil::GetNormalizedScript(prev_cand.value, true, &prev);

  const size_t i_max = min(seg->candidates_size(), kCandidateSize);

  // Reuse |curs| and |cur| in the loop as this method is performance critical.
  vector<string> curs;
  string cur;
  for (size_t i = 0; i < i_max; ++i) {
    if (IsName(seg->candidate(i))) {
      continue;
    }
    if (suppression_filter_->Exists(seg->candidate(i))) {
      continue;
    }
    curs.clear();
    if (!IsNaturalContent(seg->candidate(i), seg->candidate(0), RIGHT, &curs)) {
      continue;
    }

    for (int j = 0; j < curs.size(); ++j) {
      cur.clear();
      CollocationUtil::GetNormalizedScript(curs[j], false, &cur);
      if (collocation_filter_->Exists(prev, cur)) {
        VLOG_IF(3, i != 0) << prev << cur << " "
                           << seg->candidate(0).value << "->"
                           << seg->candidate(i).value;
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

  // Reuse |nexts| in the loop as this method is performance critical.
  vector<string> nexts;
  for (size_t j = 0; j < j_max; ++j) {
    next_seg_ok[j] = 0;

    if (IsName(next_seg->candidate(j))) {
      continue;
    }
    if (suppression_filter_->Exists(next_seg->candidate(j))) {
      continue;
    }
    nexts.clear();
    if (!IsNaturalContent(next_seg->candidate(j),
                          next_seg->candidate(0), RIGHT, &nexts)) {
      continue;
    }

    next_seg_ok[j] = 1;
    for (vector<string>::const_iterator it = nexts.begin();
         it != nexts.end(); ++it) {
      normalized_string[j].push_back(string());
      CollocationUtil::GetNormalizedScript(
          *it, false, &normalized_string[j].back());
    }
  }

  // Reuse |curs| and |cur| in the loop as this method is performance critical.
  vector<string> curs;
  string cur;
  for (size_t i = 0; i < i_max; ++i) {
    if (IsName(seg->candidate(i))) {
      continue;
    }
    if (suppression_filter_->Exists(seg->candidate(i))) {
      continue;
    }
    curs.clear();
    if (!IsNaturalContent(seg->candidate(i), seg->candidate(0), LEFT, &curs)) {
      continue;
    }

    for (int k = 0; k < curs.size(); ++k) {
      cur.clear();
      CollocationUtil::GetNormalizedScript(curs[k], true, &cur);
      for (size_t j = 0; j < j_max; ++j) {
        if (!next_seg_ok[j]) {
          continue;
        }

        for (int l = 0; l < normalized_string[j].size(); ++l) {
          const string &next = normalized_string[j][l];
          if (collocation_filter_->Exists(cur, next)) {
            DCHECK(VerifyNaturalContent(
                next_seg->candidate(j), next_seg->candidate(0), RIGHT))
                << "IsNaturalContent() should not fail here.";
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

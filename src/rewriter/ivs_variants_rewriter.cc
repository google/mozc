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

#include "rewriter/ivs_variants_rewriter.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/util.h"
#include "converter/segments.h"
#include "absl/strings/str_cat.h"

namespace mozc {
namespace {
// {"reading", "base surface"}, {"IVS surface", "additional description"}},
const auto *kIvsExpansionTable =
    new std::map<std::pair<std::string, std::string>,
                 std::pair<std::string, std::string>>{
        {{"かつらぎし", "葛城市"}, {"葛\U000E0100城市", "正式字体"}},  // 葛󠄀城市
        {{"ぎおん", "祇園"}, {"祇\U000E0100園", ""}}  // 祇󠄀園
        // TODO(b/246668402): Add other IVS words here.
    };

constexpr char kIvsVariantDescription[] = "環境依存(IVS)";

bool ExpandIvsVariantsWithSegment(Segment *seg) {
  CHECK(seg);

  bool modified = false;
  for (int i = seg->candidates_size() - 1; i >= 0; --i) {
    const Segment::Candidate &original_candidate = seg->candidate(i);
    auto it = kIvsExpansionTable->find(std::pair(
        original_candidate.content_key, original_candidate.content_value));
    if (it == kIvsExpansionTable->end()) {
      continue;
    }
    modified = true;

    auto new_candidate =
        std::make_unique<Segment::Candidate>(original_candidate);
    // "は" for "葛城市は"
    const absl::string_view non_content_value(
        original_candidate.value.data() +
        original_candidate.content_value.size());
    new_candidate->value = absl::StrCat(it->second.first, non_content_value);
    new_candidate->content_value = it->second.first;
    new_candidate->description =
        it->second.second.empty()
            ? kIvsVariantDescription
            : absl::StrCat(kIvsVariantDescription, " ", it->second.second);
    seg->insert_candidate(i + 1, std::move(new_candidate));
  }
  return modified;
}
}  // namespace

int IvsVariantsRewriter::capability(const ConversionRequest &request) const {
  return RewriterInterface::ALL;
}

bool IvsVariantsRewriter::Rewrite(const ConversionRequest &request,
                                  Segments *segments) const {
  bool modified = false;
  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    modified |= ExpandIvsVariantsWithSegment(seg);
  }

  return modified;
}

}  // namespace mozc

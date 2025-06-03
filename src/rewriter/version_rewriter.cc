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

#include "rewriter/version_rewriter.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/const.h"
#include "base/version.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"

namespace mozc {
namespace {

constexpr struct {
  absl::string_view key;
  absl::string_view base_candidate;
} kKeyCandList[] = {
    {
        "う゛ぁーじょん",
        "ヴァージョン",
    },
    {
        "ゔぁーじょん",
        "ヴァージョン",
    },
    {
        "ばーじょん",
        "バージョン",
    },
    {
        "Version",
        "Version",
    },
};

}  // namespace

VersionRewriter::VersionRewriter(absl::string_view data_version) {
  std::string version_string =
      absl::StrCat(kVersionRewriterVersionPrefix, Version::GetMozcVersion(),
                   "+", data_version);
  for (const auto &[key, base_candidate] : kKeyCandList) {
    entries_.emplace(
        key, VersionEntry{std::string(base_candidate), version_string, 9});
  }
}

bool VersionRewriter::Rewrite(const ConversionRequest &request,
                              Segments *segments) const {
  bool result = false;
  for (Segment &segment : segments->conversion_segments()) {
    const auto it = entries_.find(segment.key());
    if (it != entries_.end()) {
      for (size_t j = 0; j < segment.candidates_size(); ++j) {
        const VersionEntry &ent = it->second;
        const converter::Candidate &c = segment.candidate(static_cast<int>(j));
        if (c.value == ent.base_candidate) {
          converter::Candidate *new_cand = segment.insert_candidate(
              std::min<int>(segment.candidates_size(), ent.rank));
          if (new_cand != nullptr) {
            new_cand->lid = c.lid;
            new_cand->rid = c.rid;
            new_cand->cost = c.cost;
            new_cand->value = ent.output;
            new_cand->content_value = ent.output;
            new_cand->key = segment.key();
            new_cand->content_key = segment.key();
            // we don't learn version
            new_cand->attributes |= converter::Candidate::NO_LEARNING;
            result = true;
          }
          break;
        }
      }
    }
  }
  return result;
}

}  // namespace mozc

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
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/const.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/version.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

const struct {
  const char *key;
  const char *base_candidate;
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

class VersionRewriter::VersionDataImpl {
 public:
  class VersionEntry {
   public:
    VersionEntry(const std::string &base_candidate, const std::string &output,
                 size_t rank)
        : base_candidate_(base_candidate), output_(output), rank_(rank) {}

    const std::string &base_candidate() const { return base_candidate_; }
    const std::string &output() const { return output_; }
    size_t rank() const { return rank_; }

   private:
    std::string base_candidate_;
    std::string output_;
    size_t rank_;
  };

  const VersionEntry *Lookup(const std::string &key) const {
    const auto it = entries_.find(key);
    if (it == entries_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

  explicit VersionDataImpl(absl::string_view data_version) {
    std::string version_string = kVersionRewriterVersionPrefix;
    version_string.append(Version::GetMozcVersion());
    version_string.append(1, '+');
    version_string.append(data_version.data(), data_version.size());
    for (int i = 0; i < std::size(kKeyCandList); ++i) {
      entries_[kKeyCandList[i].key] = std::make_unique<VersionEntry>(
          kKeyCandList[i].base_candidate, version_string, 9);
    }
  }

 private:
  std::map<std::string, std::unique_ptr<VersionEntry>> entries_;
};

VersionRewriter::VersionRewriter(absl::string_view data_version)
    : impl_(new VersionDataImpl(data_version)) {}

VersionRewriter::~VersionRewriter() = default;

int VersionRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool VersionRewriter::Rewrite(const ConversionRequest &request,
                              Segments *segments) const {
  bool result = false;
  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    const VersionDataImpl::VersionEntry *ent = impl_->Lookup(seg->key());
    if (ent != nullptr) {
      for (size_t j = 0; j < seg->candidates_size(); ++j) {
        const Segment::Candidate &c = seg->candidate(static_cast<int>(j));
        if (c.value == ent->base_candidate()) {
          Segment::Candidate *new_cand = seg->insert_candidate(
              std::min<int>(seg->candidates_size(), ent->rank()));
          if (new_cand != nullptr) {
            new_cand->lid = c.lid;
            new_cand->rid = c.rid;
            new_cand->cost = c.cost;
            new_cand->value = ent->output();
            new_cand->content_value = ent->output();
            new_cand->key = seg->key();
            new_cand->content_key = seg->key();
            // we don't learn version
            new_cand->attributes |= Segment::Candidate::NO_LEARNING;
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

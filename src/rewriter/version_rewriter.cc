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

#include "rewriter/version_rewriter.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/version.h"
#include "base/singleton.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "session/commands.pb.h"
#include "session/request_handler.h"

namespace mozc {
namespace {

class VersionDataImpl {
 public:
  struct VersionEntry {
    string base_candidate_;
    string output_;
    size_t rank_;
    VersionEntry(const string& base_candidate,
                 const string& output,
                 size_t rank)
        : base_candidate_(base_candidate),
          output_(output), rank_(rank) {}
  };

  const VersionEntry *Lookup(const string &key) const {
    map<string, VersionEntry *>::const_iterator it = entries_.find(key);
    if (it == entries_.end()) {
      return NULL;
    }
    return it->second;
  }

  VersionDataImpl() {
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
    const string branding = "GoogleJapaneseInput";
#else
    const string branding = "Mozc";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
    const string version_string = branding + "-" + Version::GetMozcVersion();
    entries_["\xe3\x81\x86\xe3\x82\x9b\xe3\x81\x81\xe3\x83\xbc"
             "\xe3\x81\x98\xe3\x82\x87\xe3\x82\x93"] =
        new VersionEntry("\xe3\x83\xb4\xe3\x82\xa1\xe3\x83\xbc"
                         "\xe3\x82\xb8\xe3\x83\xa7\xe3\x83\xb3",
                         version_string, 9);
    entries_["\xe3\x82\x94\xe3\x81\x81\xe3\x83\xbc"
             "\xe3\x81\x98\xe3\x82\x87\xe3\x82\x93"] =
        new VersionEntry("\xe3\x83\xb4\xe3\x82\xa1\xe3\x83\xbc"
                         "\xe3\x82\xb8\xe3\x83\xa7\xe3\x83\xb3",
                         version_string, 9);
    entries_["\xe3\x81\xb0\xe3\x83\xbc\xe3\x81\x98"
             "\xe3\x82\x87\xe3\x82\x93"] =
        new VersionEntry("\xe3\x83\x90\xe3\x83\xbc\xe3\x82\xb8"
                         "\xe3\x83\xa7\xe3\x83\xb3", version_string, 9);

    //   entries_["う゛ぁーじょん"] =
    //     new VersionEntry("ヴァージョン", Version::GetMozcVersion(), 9);
    //   entries_["ゔぁーじょん"] =
    //     new VersionEntry("ヴァージョン", Version::GetMozcVersion(), 9);
    //   entries_["ばーじょん"] =
    //     new VersionEntry("バージョン", Version::GetMozcVersion(), 9);
  }

  ~VersionDataImpl() {
    for (map<string, VersionEntry *>::iterator it = entries_.begin();
         it != entries_.end();
         ++it) {
      delete it->second;
    }
    entries_.clear();
  }

 private:
  map<string, VersionEntry *> entries_;
};
}  // namespace

VersionRewriter::VersionRewriter() {}

VersionRewriter::~VersionRewriter() {}

int VersionRewriter::capability() const {
  if (GET_REQUEST(mixed_conversion)) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool VersionRewriter::Rewrite(const ConversionRequest &request,
                              Segments *segments) const {
  bool result = false;
  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment* seg = segments->mutable_segment(i);
    DCHECK(seg);
    const VersionDataImpl::VersionEntry *ent
        = Singleton<VersionDataImpl>::get()->Lookup(seg->key());
    if (ent != NULL) {
      for (size_t j = 0; j < seg->candidates_size(); ++j) {
        const Segment::Candidate& c = seg->candidate(static_cast<int>(j));
        if (c.value == ent->base_candidate_) {
          Segment::Candidate* new_cand =
              seg->insert_candidate(static_cast<int>(
                  min(seg->candidates_size(),
                      ent->rank_)));
          if (new_cand != NULL) {
            new_cand->lid = c.lid;
            new_cand->rid = c.rid;
            new_cand->cost = c.cost;
            new_cand->value = ent->output_;
            new_cand->content_value = ent->output_;
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

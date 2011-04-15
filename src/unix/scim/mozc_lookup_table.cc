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

#include "unix/scim/mozc_lookup_table.h"

namespace mozc_unix_scim {

MozcLookupTable::MozcLookupTable(const vector<scim::WideString> &labels,
                                 const vector<scim::WideString> &values,
                                 const vector<int32> *ids,
                                 uint32 size, uint32 focused)
    : scim::CommonLookupTable(labels.size()),
      ids_(ids), size_(size), focused_(focused) {
  DCHECK(ids != NULL);
  CHECK_EQ(labels.size(), values.size());
  CHECK_EQ(labels.size(), ids_->size());
  set_candidate_labels(labels);
  for (int i = 0; i < values.size(); ++i) {
    append_candidate(values[i]);
  }
  // Fix the window size.
  // We don't override IMEngineInstanceBase::update_lookup_table_page_size().
  fix_page_size();
}

MozcLookupTable::~MozcLookupTable() {
}

int32 MozcLookupTable::GetId(int index) const {
  if (!ids_.get()) {
    return kBadCandidateId;
  }
  if (ids_->size() <= index) {
    LOG(ERROR) << "Index out of bounds: size="
               << ids_->size() << ", index=" << index;
    return kBadCandidateId;
  }
  return ids_->at(index);
}

uint32 MozcLookupTable::size() const {
  return size_;
}

uint32 MozcLookupTable::focused() const {
  return focused_;
}

}  // namespace mozc_unix_scim

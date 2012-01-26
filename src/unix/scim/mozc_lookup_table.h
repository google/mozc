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

#ifndef MOZC_UNIX_SCIM_MOZC_LOOKUP_TABLE_H_
#define MOZC_UNIX_SCIM_MOZC_LOOKUP_TABLE_H_

#define Uses_SCIM_FRONTEND_MODULE

#include <scim.h>

#include <string>
#include <vector>

#include "base/base.h"  // for DISALLOW_COPY_AND_ASSIGN.

namespace mozc_unix_scim {

// TODO(yusukes): Obtain the "bad" id in a better way.
const int32 kBadCandidateId = -12345;

class MozcLookupTable : public scim::CommonLookupTable {
 public:
  MozcLookupTable(const vector<scim::WideString> &labels,
                  const vector<scim::WideString> &vaues,
                  const vector<int32> *ids,  // take ownership
                  uint32 size, uint32 focused);
  virtual ~MozcLookupTable();

  int32 GetId(int index) const;

  uint32 size() const;
  uint32 focused() const;

 private:
  // Unique number specifing the candidate.
  scoped_ptr<const vector<int32> > ids_;

  // Total # of candidates (written in the 'Output' protobuf).
  const uint32 size_;
  // The index of the focused candidate written in the protobuf. 1-origin.
  const uint32 focused_;

  DISALLOW_COPY_AND_ASSIGN(MozcLookupTable);
};

}  // namespace mozc_unix_scim

#endif  // MOZC_UNIX_SCIM_MOZC_LOOKUP_TABLE_H_

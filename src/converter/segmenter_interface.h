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

#ifndef MOZC_CONVERTER_SEGMENTER_INTERFACE_H_
#define MOZC_CONVERTER_SEGMENTER_INTERFACE_H_

#include "base/base.h"

namespace mozc {

struct Node;

class SegmenterInterface {
 public:
  virtual ~SegmenterInterface() {}

  // Returns true if there is a segment boundary between |lnode| and |rnode|.
  // If |is_single_segment| is true, this function basically reutrns false
  // unless |lnode| or |rnode| is BOS/EOS.  |is_single_segment| is used for
  // prediction/suggestion mode.
  virtual bool IsBoundary(const Node *lnode, const Node *rnode,
                          bool is_single_segment) const = 0;

  virtual bool IsBoundary(uint16 rid, uint16 lid) const = 0;

  // Returns cost penalty of the word prefix.  We can add cost penalty if a
  // node->lid exists at the begging of user input.
  virtual int32 GetPrefixPenalty(uint16 lid) const = 0;

  // Returns cost penalty of the word suffix.  We can add cost penalty if a
  // node->rid exists at the end of user input.
  virtual int32 GetSuffixPenalty(uint16 rid) const = 0;

 protected:
  SegmenterInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SegmenterInterface);
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_SEGMENTER_INTERFACE_H_

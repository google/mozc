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

#ifndef MOZC_CONVERTER_SEGMENTER_H_
#define MOZC_CONVERTER_SEGMENTER_H_

#include "base/base.h"
#include "converter/node.h"

namespace mozc {

class Segmenter {
 public:
  // return true if there is a segment boundary between
  // |lnode| and |rnode|.
  // if |is_single_segment| is true, this function basically
  // reutrns false unless |lnode| or |rnode| is BOS/EOS.
  // |is_single_segment| is used for prediction/suggestion mode.
  static bool IsBoundary(const Node *lnode, const Node *rnode,
                         bool is_single_segment);

  static bool IsBoundary(uint16 rid, uint16 lid);

  // return cost penalty of the word prefix.
  // we can add cost penalty if a node->lid
  // exists at the begging of user input.
  static int32 GetPrefixPenalty(uint16 lid);

  // return cost penalty of the word suffix.
  // we can add cost penalty if a node->rid
  // exists at the end of user input.
  static int32 GetSuffixPenalty(uint16 rid);

 private:
  Segmenter() {}
  virtual ~Segmenter() {}
};
}  // namespace mozc
#endif  // MOZC_CONVERTER_SEGMENTER_H_

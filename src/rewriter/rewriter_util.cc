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

#include "rewriter/rewriter_util.h"

#include <algorithm>
#include <cstddef>

#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"

namespace mozc {

// Candidates from user history: "h"
// Other existing candidates : "o"
// Inserting candidates from the rewriter: "R"
// The number of inserting "R": 2
// offset: 2
//
// The output candidates would be:
// [o, o, R, R, o, o, o, o, ...]
// [h, o, o, R, R, o, o, o, ...]
// [h, h, o, o, R, R, o, o, ...]
// [h, h, h, o, o, R, R, o, ...]
// [h, h, h, h, o, o, R, R  ...]
// For the number of history candidates.
size_t RewriterUtil::CalculateInsertPosition(const Segment &segment,
                                             size_t offset) {
  size_t existing_history_candidates_num = 0;
  for (int i = 0; i < segment.candidates_size(); ++i) {
    // Assume that the user history prediction candidates are inserted
    // sequentially from top.
    if (segment.candidate(i).attributes &
        converter::Attribute::USER_HISTORY_PREDICTION) {
      ++existing_history_candidates_num;
    } else if (existing_history_candidates_num > 0) {
      break;
    }
  }
  return std::min(offset + existing_history_candidates_num,
                  segment.candidates_size());
}

}  // namespace mozc

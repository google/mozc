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

#include "rewriter/user_dictionary_rewriter.h"

#include <cstddef>

#include "absl/log/check.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"

namespace mozc {

// User-dictionary candidates are not always placed at the top.
// Since user expects that user-dictionary candidates may appear
// on the top, we simply move user-dictionary-candidate just
// "after" the top candidate.
bool UserDictionaryRewriter::Rewrite(const ConversionRequest &request,
                                     Segments *segments) const {
  DCHECK(segments);

  bool modified = false;
  for (Segment &segment : segments->conversion_segments()) {
    // final destination of the user dictionary candidate.
    int move_to_start = 1;

    for (size_t move_from = 2; move_from < segment.candidates_size();
         ++move_from) {
      if (!(segment.candidate(move_from).attributes &
            converter::Candidate::USER_DICTIONARY)) {
        continue;
      }

      // find the final destination of user dictionary
      // from [move_to_start .. move_from).
      int move_to = -1;
      for (int j = move_to_start; j < static_cast<int>(move_from); ++j) {
        if (!(segment.candidate(j).attributes &
              converter::Candidate::USER_DICTIONARY)) {
          move_to = j;
          break;
        }
      }

      // if move_to is not nil, move the candidate.
      if (move_to == -1) {
        move_to_start = move_from + 1;
      } else {
        segment.move_candidate(move_from, move_to);
        modified = true;
        move_to_start = move_to + 1;
      }
    }
  }

  return modified;
}
}  // namespace mozc

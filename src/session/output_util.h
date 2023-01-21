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

#ifndef MOZC_SESSION_OUTPUT_UTIL_H_
#define MOZC_SESSION_OUTPUT_UTIL_H_

#include <cstdint>

#include "base/port.h"

namespace mozc {
namespace commands {
class Output;
}  // namespace commands

// This class consists of utility functions for mozc::commands::Output.
// TODO(yukawa): Consider to move this class under client/*.
class OutputUtil {
 public:
  OutputUtil() = delete;
  OutputUtil(const OutputUtil &) = delete;
  OutputUtil &operator=(const OutputUtil &) = delete;

  // Converts a candidate id into candidate index.
  // Returns true if a valid candidate index is returned in |candidate_index|.
  static bool GetCandidateIndexById(const mozc::commands::Output &output,
                                    int32_t mozc_candidate_id,
                                    int32_t *candidate_index);

  // Converts a candidate index into candidate id.
  // Returns true if a valid candidate id is returned in |candidate_id|.
  static bool GetCandidateIdByIndex(const mozc::commands::Output &output,
                                    int32_t candidate_index,
                                    int32_t *mozc_candidate_id);

  // Converts a candidate id into candidate index.
  // Returns true if a valid candidate index is returned in |candidate_index|.
  static bool GetFocusedCandidateId(const mozc::commands::Output &output,
                                    int32_t *mozc_candidate_id);
};

}  // namespace mozc
#endif  // MOZC_SESSION_OUTPUT_UTIL_H_

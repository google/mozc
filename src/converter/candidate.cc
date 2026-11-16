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

#include "converter/candidate.h"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/number_util.h"
#include "converter/inner_segment.h"

namespace mozc {
namespace converter {

void Candidate::Clear() {
  key.clear();
  value.clear();
  content_value.clear();
  content_key.clear();
  consumed_key_size = 0;
  converted_segment_count = 1;
  prefix.clear();
  suffix.clear();
  description.clear();
  display_value.clear();
  usage_title.clear();
  usage_description.clear();
  cost = 0;
  structure_cost = 0;
  wcost = 0;
  lid = 0;
  rid = 0;
  usage_id = 0;
  attributes = 0;
  style = NumberUtil::NumberString::DEFAULT_STYLE;
  command = DEFAULT_COMMAND;
  inner_segment_boundary.clear();
#ifdef MOZC_CANDIDATE_DEBUG
  log.clear();
#endif  // MOZC_CANDIDATE_DEBUG
}

#ifdef MOZC_CANDIDATE_DEBUG
void Candidate::Dlog(absl::string_view filename, int line,
                     absl::string_view message) const {
  absl::StrAppend(&log, filename, ":", line, " ", message, "\n");
}
#endif  // MOZC_CANDIDATE_DEBUG

std::string Candidate::DebugString() const {
  std::string result = absl::StrFormat(
      "(key=%s ckey=%s val=%s cval=%s cost=%d scost=%d wcost=%d lid=%d rid=%d"
      " attributes=%s consumed_key_size=%u converted_segment_count=%u",
      key, content_key, value, content_value, cost, structure_cost, wcost, lid,
      rid, std::bitset<16>(attributes).to_string(), consumed_key_size,
      converted_segment_count);
  if (!prefix.empty()) {
    absl::StrAppend(&result, " prefix=", prefix);
  }
  if (!suffix.empty()) {
    absl::StrAppend(&result, " suffix=", suffix);
  }
  if (!description.empty()) {
    absl::StrAppend(&result, " description=", description);
  }
  if (!display_value.empty()) {
    absl::StrAppend(&result, " display_value=", display_value);
  }
  if (!inner_segment_boundary.empty()) {
    absl::StrAppend(&result, " segbdd=");
    for (const auto& iter : inner_segments()) {
      absl::StrAppendFormat(
          &result, "<%d,%d,%d,%d>", iter.GetKey().size(),
          iter.GetValue().size(), iter.GetContentKey().size(),
          iter.GetContentValue().size());
    }
  }
  absl::StrAppend(&result, ")\n");
  return result;
}

}  // namespace converter
}  // namespace mozc

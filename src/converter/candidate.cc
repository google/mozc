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
#include <ostream>
#include <sstream>  // For DebugString()
#include <string>
#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/number_util.h"
#include "converter/inner_segment.h"

#ifdef MOZC_CANDIDATE_DEBUG
#include "absl/strings/str_cat.h"
#endif  // MOZC_CANDIDATE_DEBUG

namespace mozc {
namespace converter {

void Candidate::Clear() {
  key.clear();
  value.clear();
  content_value.clear();
  content_key.clear();
  consumed_key_size = 0;
  prefix.clear();
  suffix.clear();
  description.clear();
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
  std::stringstream os;
  os << "(key=" << key << " ckey=" << content_key << " val=" << value
     << " cval=" << content_value << " cost=" << cost
     << " scost=" << structure_cost << " wcost=" << wcost << " lid=" << lid
     << " rid=" << rid << " attributes=" << std::bitset<16>(attributes)
     << " consumed_key_size=" << consumed_key_size;
  if (!prefix.empty()) {
    os << " prefix=" << prefix;
  }
  if (!suffix.empty()) {
    os << " suffix=" << suffix;
  }
  if (!description.empty()) {
    os << " description=" << description;
  }
  if (!inner_segment_boundary.empty()) {
    os << " segbdd=";
    for (const auto& iter : inner_segments()) {
      os << absl::StreamFormat(
          "<%d,%d,%d,%d>", iter.GetKey().size(), iter.GetValue().size(),
          iter.GetContentKey().size(), iter.GetContentValue().size());
    }
  }
  os << ")" << std::endl;
  return os.str();
}

}  // namespace converter
}  // namespace mozc

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
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/number_util.h"

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
  source_info = SOURCE_INFO_NONE;
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

bool Candidate::IsValid() const {
  if (inner_segment_boundary.empty()) {
    return true;
  }
  // The sums of the lengths of key, value components must coincide with those
  // of key, value, respectively.
  size_t sum_key_len = 0, sum_value_len = 0;
  for (InnerSegmentIterator iter(this); !iter.Done(); iter.Next()) {
    sum_key_len += iter.GetKey().size();
    sum_value_len += iter.GetValue().size();
  }
  return sum_key_len == key.size() && sum_value_len == value.size();
}

bool Candidate::EncodeLengths(size_t key_len, size_t value_len,
                                       size_t content_key_len,
                                       size_t content_value_len,
                                       uint32_t *result) {
  if (key_len > std::numeric_limits<uint8_t>::max() ||
      value_len > std::numeric_limits<uint8_t>::max() ||
      content_key_len > std::numeric_limits<uint8_t>::max() ||
      content_value_len > std::numeric_limits<uint8_t>::max()) {
    return false;
  }
  *result = (static_cast<uint32_t>(key_len) << 24) |
            (static_cast<uint32_t>(value_len) << 16) |
            (static_cast<uint32_t>(content_key_len) << 8) |
            static_cast<uint32_t>(content_value_len);
  return true;
}

bool Candidate::PushBackInnerSegmentBoundary(
    size_t key_len, size_t value_len, size_t content_key_len,
    size_t content_value_len) {
  uint32_t encoded;
  if (EncodeLengths(key_len, value_len, content_key_len, content_value_len,
                    &encoded)) {
    inner_segment_boundary.push_back(encoded);
    return true;
  }
  return false;
}

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
    for (size_t i = 0; i < inner_segment_boundary.size(); ++i) {
      const uint32_t encoded_lengths = inner_segment_boundary[i];
      const int key_len = encoded_lengths >> 24;
      const int value_len = (encoded_lengths >> 16) & 0xff;
      const int content_key_len = (encoded_lengths >> 8) & 0xff;
      const int content_value_len = encoded_lengths & 0xff;
      os << absl::StreamFormat("<%d,%d,%d,%d>", key_len, value_len,
                               content_key_len, content_value_len);
    }
  }
  os << ")" << std::endl;
  return os.str();
}

void Candidate::InnerSegmentIterator::Next() {
  DCHECK_LT(index_, inner_segment_boundary_.size());
  const uint32_t encoded_lengths = inner_segment_boundary_[index_++];
  key_offset_ += encoded_lengths >> 24;
  value_offset_ += (encoded_lengths >> 16) & 0xff;
}

absl::string_view Candidate::InnerSegmentIterator::GetKey() const {
  DCHECK_LT(index_, inner_segment_boundary_.size());
  const uint32_t encoded_lengths = inner_segment_boundary_[index_];
  return absl::string_view(key_offset_, encoded_lengths >> 24);
}

absl::string_view Candidate::InnerSegmentIterator::GetValue() const {
  DCHECK_LT(index_, inner_segment_boundary_.size());
  const uint32_t encoded_lengths = inner_segment_boundary_[index_];
  return absl::string_view(value_offset_, (encoded_lengths >> 16) & 0xff);
}

absl::string_view Candidate::InnerSegmentIterator::GetContentKey()
    const {
  DCHECK_LT(index_, inner_segment_boundary_.size());
  const uint32_t encoded_lengths = inner_segment_boundary_[index_];
  return absl::string_view(key_offset_, (encoded_lengths >> 8) & 0xff);
}

absl::string_view Candidate::InnerSegmentIterator::GetContentValue()
    const {
  DCHECK_LT(index_, inner_segment_boundary_.size());
  const uint32_t encoded_lengths = inner_segment_boundary_[index_];
  return absl::string_view(value_offset_, encoded_lengths & 0xff);
}

absl::string_view Candidate::InnerSegmentIterator::GetFunctionalKey()
    const {
  DCHECK_LT(index_, inner_segment_boundary_.size());
  const uint32_t encoded_lengths = inner_segment_boundary_[index_];
  const int key_len = encoded_lengths >> 24;
  const int content_key_len = (encoded_lengths >> 8) & 0xff;
  if (const int key_size = key_len - content_key_len; key_size > 0) {
    return absl::string_view(key_offset_ + content_key_len, key_size);
  }
  return absl::string_view();
}

absl::string_view Candidate::InnerSegmentIterator::GetFunctionalValue()
    const {
  DCHECK_LT(index_, inner_segment_boundary_.size());
  const uint32_t encoded_lengths = inner_segment_boundary_[index_];
  const int value_len = (encoded_lengths >> 16) & 0xff;
  const int content_value_len = encoded_lengths & 0xff;
  if (const int value_size = value_len - content_value_len; value_size > 0) {
    return absl::string_view(value_offset_ + content_value_len, value_size);
  }
  return absl::string_view();
}

}  // namespace converter
}  // namespace mozc

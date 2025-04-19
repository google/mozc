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

#include "base/container/serialized_string_array.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/file_util.h"

namespace mozc {

static_assert(std::endian::native == std::endian::little,
              "Little endian is assumed");

bool SerializedStringArray::Init(
    absl::string_view data_aligned_at_4byte_boundary) {
  if (VerifyData(data_aligned_at_4byte_boundary)) {
    data_ = data_aligned_at_4byte_boundary;
    return true;
  }
  clear();
  return false;
}

void SerializedStringArray::Set(
    absl::string_view data_aligned_at_4byte_boundary) {
  DCHECK(VerifyData(data_aligned_at_4byte_boundary));
  data_ = data_aligned_at_4byte_boundary;
}

bool SerializedStringArray::VerifyData(absl::string_view data) {
  if (data.size() < 4) {
    LOG(ERROR) << "Array size is missing";
    return false;
  }
  const uint32_t *u32_array = reinterpret_cast<const uint32_t *>(data.data());
  const size_type size = u32_array[0];

  const size_type min_required_data_size = 4 + (4 + 4) * size;
  if (data.size() < min_required_data_size) {
    LOG(ERROR) << "Lack of data.  At least " << min_required_data_size
               << " bytes are required";
    return false;
  }

  difference_type prev_str_end = min_required_data_size;
  for (difference_type i = 0; i < size; ++i) {
    const difference_type offset = u32_array[2 * i + 1];
    const difference_type len = u32_array[2 * i + 2];
    if (offset < prev_str_end) {
      LOG(ERROR) << "Invalid offset for string " << i << ": len = " << len
                 << ", offset = " << offset;
      return false;
    }
    if (len >= data.size() || offset > data.size() - len) {
      LOG(ERROR) << "Invalid length for string " << i << ": len = " << len
                 << ", offset = " << offset << ", " << data.size();
      return false;
    }
    if (data[offset + len] != '\0') {
      LOG(ERROR) << "string[" << i << "] is not null-terminated";
      return false;
    }
    prev_str_end = offset + len + 1;
  }

  return true;
}

absl::string_view SerializedStringArray::SerializeToBuffer(
    const absl::Span<const absl::string_view> strs,
    std::unique_ptr<uint32_t[]> *buffer) {
  const size_type header_byte_size = 4 * (1 + 2 * strs.size());

  // Calculate the offsets of each string.
  std::unique_ptr<uint32_t[]> offsets(new uint32_t[strs.size()]);
  difference_type current_offset =
      header_byte_size;  // The offset for first string.
  for (difference_type i = 0; i < strs.size(); ++i) {
    offsets[i] = static_cast<uint32_t>(current_offset);
    // The next string is written after terminating '\0', so increment one byte
    // in addition to the string byte length.
    current_offset += strs[i].size() + 1;
  }

  // At this point, |current_offset| is the byte length of the whole binary
  // image.  Allocate a necessary buffer as uint32_t array.
  buffer->reset(new uint32_t[(current_offset + 3) / 4]);

  (*buffer)[0] = static_cast<uint32_t>(strs.size());
  for (difference_type i = 0; i < strs.size(); ++i) {
    // Fill offset and length.
    (*buffer)[2 * i + 1] = offsets[i];
    (*buffer)[2 * i + 2] = static_cast<uint32_t>(strs[i].size());

    // Copy string buffer at the calculated offset.  Guarantee that the buffer
    // is null-terminated.
    char *dest = reinterpret_cast<char *>(buffer->get()) + offsets[i];
    memcpy(dest, strs[i].data(), strs[i].size());
    dest[strs[i].size()] = '\0';
  }

  return absl::string_view(reinterpret_cast<const char *>(buffer->get()),
                           current_offset);
}

void SerializedStringArray::SerializeToFile(
    const absl::Span<const absl::string_view> strs,
    const std::string &filepath) {
  std::unique_ptr<uint32_t[]> buffer;
  const absl::string_view data = SerializeToBuffer(strs, &buffer);
  CHECK_OK(FileUtil::SetContents(filepath, data));
}

}  // namespace mozc

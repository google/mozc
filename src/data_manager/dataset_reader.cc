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

#include "data_manager/dataset_reader.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "base/unverified_sha1.h"
#include "base/util.h"
#include "data_manager/dataset.pb.h"

namespace mozc {
namespace {

// The size of the file footer, which contains some metadata; see dataset.proto.
constexpr size_t kFooterSize = 36;

}  // namespace

bool DataSetReader::Init(absl::string_view memblock, absl::string_view magic) {
  memblock_ = memblock;
  name_to_data_map_.clear();

  // Initializes |name_to_data_map_| from |memblock|.  For binary data format,
  // see dataset.proto.

  // Check the file magic string.
  if (!memblock.starts_with(magic)) {
    LOG(ERROR) << "Invalid format: magic number doesn't match: "
               << absl::CHexEscape(memblock.substr(0, magic.size())) << " vs "
               << absl::CHexEscape(magic);
    return false;
  }

  return Init(memblock, magic.size());
}

bool DataSetReader::Init(absl::string_view memblock, size_t magic_length) {
  // Check minimum required data size.
  if (memblock.size() < magic_length + kFooterSize) {
    LOG(ERROR) << "Broken: data is too small";
    return false;
  }

  // Check the file size.
  uint64_t filesize = 0;
  if (!Util::DeserializeUint64(
          absl::ClippedSubstr(memblock, memblock.size() - 8, 8), &filesize)) {
    LOG(ERROR) << "Broken: failed to read filesize";
    return false;
  }
  if (filesize != memblock.size()) {
    LOG(ERROR) << "Broken: filesize mismatch.  " << filesize << " vs "
               << memblock.size();
    return false;
  }

  // Checksum is not checked.

  // Read the metadata size.
  uint64_t metadata_size = 0;
  if (!Util::DeserializeUint64(
          absl::ClippedSubstr(memblock, memblock.size() - kFooterSize, 8),
          &metadata_size)) {
    LOG(ERROR) << "Broken: failed to read metadata size";
    return false;
  }

  // Note: This subtraction doesn't cause underflow by the above check.
  const uint64_t content_and_metadata_size =
      memblock.size() - magic_length - kFooterSize;
  if (metadata_size == 0 || content_and_metadata_size < metadata_size) {
    LOG(ERROR) << "Broken: metadata size is broken or metadata is broken";
    return false;
  }

  // Note: This subtraction doesn't cause underflow by the above check.
  const uint64_t metadata_offset =
      memblock.size() - kFooterSize - metadata_size;

  // Open metadata.
  DataSetMetadata metadata;
  const absl::string_view metadata_chunk =
      absl::ClippedSubstr(memblock, metadata_offset, metadata_size);
  if (!metadata.ParseFromArray(metadata_chunk.data(), metadata_chunk.size())) {
    LOG(ERROR) << "Broken: Failed to parse metadata";
    return false;
  }

  // Construct a mapping from name to data chunk.
  uint64_t prev_chunk_end = magic_length;
  for (int i = 0; i < metadata.entries_size(); ++i) {
    const auto& e = metadata.entries(i);
    if (e.offset() < prev_chunk_end || e.offset() >= metadata_offset) {
      LOG(ERROR) << "Broken: Offset is out of range: " << e
                 << ", metadata offset = " << metadata_offset;
      return false;
    }
    // Check the condition e.offset() + e.size() <= metadata_offset, i.e., data
    // chunk must point to a block before metadata.
    if (e.size() > metadata_offset || e.offset() > metadata_offset - e.size()) {
      LOG(ERROR) << "Broken: Size exceeds the metadata offset: " << e
                 << ", metadata offset = " << metadata_offset;
      return false;
    }
    name_to_data_map_[e.name()] =
        absl::ClippedSubstr(memblock, e.offset(), e.size());
    prev_chunk_end = e.offset() + e.size();
  }

  return true;
}

bool DataSetReader::Get(absl::string_view name, absl::string_view* data) const {
  auto iter = name_to_data_map_.find(name);
  if (iter == name_to_data_map_.end()) {
    return false;
  }
  *data = iter->second;
  return true;
}

std::optional<std::pair<size_t, size_t>> DataSetReader::GetOffsetAndSize(
    absl::string_view name) const {
  absl::string_view data;
  if (!Get(name, &data)) {
    return std::nullopt;
  }
  const size_t offset = data.data() - memblock_.data();
  return std::make_pair(offset, data.size());
}

bool DataSetReader::VerifyChecksum(absl::string_view memblock) {
  if (memblock.size() < kFooterSize) {
    return false;
  }
  // Checksum is computed for all but last 28 bytes.
  const std::string& actual_checksum = internal::UnverifiedSHA1::MakeDigest(
      memblock.substr(0, memblock.size() - 28));

  // Extract the stored SHA1; see dataset.proto for file format.
  const std::size_t kSHA1Length = 20;
  absl::string_view expected_checksum =
      absl::ClippedSubstr(memblock, memblock.size() - 28, kSHA1Length);

  return actual_checksum == expected_checksum;
}

}  // namespace mozc

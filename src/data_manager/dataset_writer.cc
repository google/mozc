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

#include "data_manager/dataset_writer.h"

#include <bit>
#include <ostream>
#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/file_util.h"
#include "base/unverified_sha1.h"
#include "base/util.h"
#include "base/vlog.h"
#include "data_manager/dataset.pb.h"

namespace mozc {
namespace {

// Checks if `a` is a power of 2 greater than or equal to 8.
bool IsValidAlignment(int a) {
  return a >= 8 && std::has_single_bit<unsigned int>(a);
}

}  // namespace

void DataSetWriter::Add(const std::string& name, int alignment,
                        absl::string_view data) {
  CHECK(seen_names_.insert(name).second) << name << " was already added";
  AppendPadding(alignment);
  DataSetMetadata::Entry* entry = metadata_.add_entries();
  entry->set_name(name);
  entry->set_offset(image_.size());
  entry->set_size(data.size());
  image_.append(data.data(), data.size());
}

void DataSetWriter::AddFile(const std::string& name, int alignment,
                            const std::string& filepath) {
  absl::StatusOr<std::string> content = FileUtil::GetContents(filepath);
  CHECK_OK(content);
  Add(name, alignment, *content);
}

void DataSetWriter::Finish(std::ostream* output) {
  const std::string s = metadata_.SerializeAsString();
  image_.append(s);                                // Metadata
  image_.append(Util::SerializeUint64(s.size()));  // Metadata size

  // SHA1 checksum
  image_.append(mozc::internal::UnverifiedSHA1::MakeDigest(image_));

  // File size.  Note that the final file size becomes image_.size() + 8 after
  // writing this file size.
  image_.append(Util::SerializeUint64(image_.size() + 8));

  CHECK(output->write(image_.data(), image_.size()));
  MOZC_VLOG(1) << "Wrote data set of " << image_.size() << " bytes:\n"
               << metadata_;
}

void DataSetWriter::AppendPadding(int alignment) {
  CHECK(IsValidAlignment(alignment)) << "Invalid alignment: " << alignment;
  alignment /= 8;  // To byte
  if (image_.size() % alignment > 0) {
    image_.append(alignment - image_.size() % alignment, '\0');
  }
}

}  // namespace mozc

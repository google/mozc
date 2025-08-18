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

#ifndef MOZC_DATA_MANAGER_DATASET_WRITER_H_
#define MOZC_DATA_MANAGER_DATASET_WRITER_H_

#include <ostream>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "data_manager/dataset.pb.h"

namespace mozc {

// Creates a data set file that packs multiple files into one.  For file format,
// see dataset.proto.
class DataSetWriter {
 public:
  // Creates a writer with specified magic number.
  explicit DataSetWriter(absl::string_view magic) : image_(magic) {}

  // Adds a binary image to the packed file so that data is aligned at the
  // specified bit boundary (8, 16, 32, 64, ...).
  void Add(const std::string& name, int alignment, absl::string_view data);

  // Similar to Add() for absl::string_view but data is read from file.
  void AddFile(const std::string& name, int alignment,
               const std::string& filepath);

  // Writes the image to output.  If |output| is a file, it should be opened in
  // binary mode.
  void Finish(std::ostream* output);

  const DataSetMetadata& metadata() const { return metadata_; }

 private:
  void AppendPadding(int alignment);

  std::string image_;
  DataSetMetadata metadata_;
  absl::flat_hash_set<std::string> seen_names_;
};

}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_DATASET_WRITER_H_

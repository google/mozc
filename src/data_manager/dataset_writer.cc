// Copyright 2010-2016, Google Inc.
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

#include <string>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"

namespace mozc {
namespace {

bool IsValidAlignment(int a) {
  return a == 8 || a == 16 || a == 32 || a == 64;
}

}  // namespace

DataSetWriter::DataSetWriter(StringPiece magic, ostream *output)
    : ofs_(output), bytes_written_(0) {
  Write(magic);
}

DataSetWriter::~DataSetWriter() = default;

void DataSetWriter::Add(const string &name, int alignment, StringPiece data) {
  CHECK(seen_names_.insert(name).second) << name << " was already added";
  WritePadding(alignment);
  DataSetMetadata::Entry *entry = metadata_.add_entries();
  entry->set_name(name);
  entry->set_offset(bytes_written_);
  entry->set_size(data.size());
  Write(data);
}

void DataSetWriter::AddFile(const string &name, int alignment,
                            const string &filepath) {
  mozc::InputFileStream ifs(filepath.c_str(), ios_base::in | ios_base::binary);
  CHECK(ifs.good()) << "Failed to open " << name;
  Add(name, alignment, ifs.Read());
}

void DataSetWriter::Finish() {
  const string s = metadata_.SerializeAsString();
  Write(s);
  Write(Util::SerializeUint64(s.size()));
  VLOG(1) << "Wrote data set of " << bytes_written_ << " bytes:\n"
          << metadata_.Utf8DebugString();
}

void DataSetWriter::Write(StringPiece data) {
  CHECK(ofs_->write(data.data(), data.size()).good())
      << "Failed to write data of length " << data.size();
  bytes_written_ += data.size();
}

void DataSetWriter::WritePadding(int alignment) {
  CHECK(IsValidAlignment(alignment)) << "Invalid alignment: " << alignment;
  alignment /= 8;  // To byte
  if (bytes_written_ % alignment > 0) {
    Write(string(alignment - bytes_written_ % alignment, '\0'));
  }
}

}  // namespace mozc

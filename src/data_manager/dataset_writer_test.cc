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

#include <sstream>
#include <string>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/unverified_sha1.h"
#include "base/util.h"
#include "data_manager/dataset.pb.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/flags/flag.h"

namespace mozc {
namespace {

void SetEntry(const string &name, uint64 offset, uint64 size,
              DataSetMetadata::Entry *entry) {
  entry->set_name(name);
  entry->set_offset(offset);
  entry->set_size(size);
}

TEST(DatasetWriterTest, Write) {
  // Create a dummy file to be packed.
  const string &in =
      FileUtil::JoinPath({absl::GetFlag(FLAGS_test_tmpdir), "in"});
  {
    OutputFileStream f(in.c_str(), std::ios_base::out | std::ios_base::binary);
    f.write("m\0zc\xEF", 5);
    f.close();
  }

  // Generate a packed file into |actual|.
  string actual;
  {
    DataSetWriter w("magic");

    w.Add("data8", 8, string("data8 \x00\x01", 8));
    w.Add("data16", 16, "data16 \xAB\xCD\xEF");
    w.Add("data32", 32, string("data32 \x00\xAB\n\r\n", 12));
    w.Add("data64", 64, string("data64 \t\t\x00\x00", 11));

    w.AddFile("file8", 8, in);
    w.AddFile("file16", 16, in);
    w.AddFile("file32", 32, in);
    w.AddFile("file64", 64, in);

    std::stringstream out;
    w.Finish(&out);
    actual = out.str();
  }

  // Define the expected binary image.
  const char data_chunk[] =
      "magic"                  // offset 0, size 5
      "data8 \x00\x01"         // offset 5, size 8
      "\0"                     // offset 13, size 1 (padding)
      "data16 \xAB\xCD\xEF"    // offset 14, size 10
      "data32 \x00\xAB\n\r\n"  // offset 24, size 12
      "\0\0\0\0"               // offset 36, size 4 (padding)
      "data64 \t\t\x00\x00"    // offset 40, size 11
      "m\0zc\xEF"              // offset 51, size 5
      "m\0zc\xEF"              // offset 56, size 5
      "\0\0\0"                 // offset 61, size 3 (padding)
      "m\0zc\xEF"              // offset 64, size 5
      "\0\0\0"                 // offset 69, size 3 (padding)
      "m\0zc\xEF";             // offset 72, size 5
  DataSetMetadata metadata;
  SetEntry("data8", 5, 8, metadata.add_entries());
  SetEntry("data16", 14, 10, metadata.add_entries());
  SetEntry("data32", 24, 12, metadata.add_entries());
  SetEntry("data64", 40, 11, metadata.add_entries());
  SetEntry("file8", 51, 5, metadata.add_entries());
  SetEntry("file16", 56, 5, metadata.add_entries());
  SetEntry("file32", 64, 5, metadata.add_entries());
  SetEntry("file64", 72, 5, metadata.add_entries());
  const string &metadata_chunk = metadata.SerializeAsString();
  const string &metadata_size = Util::SerializeUint64(metadata_chunk.size());
  // Append data_chunk except for the last '\0'.
  string expected(data_chunk, sizeof(data_chunk) - 1);
  expected.append(metadata_chunk.data(), metadata_chunk.size());
  expected.append(metadata_size.data(), metadata_size.size());
  expected.append(internal::UnverifiedSHA1::MakeDigest(expected));
  expected.append(Util::SerializeUint64(expected.size() + 8));

  // Compare the results.
  EXPECT_EQ(expected, actual);
}

}  // namespace
}  // namespace mozc

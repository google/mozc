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

#include <cstdint>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "base/unverified_sha1.h"
#include "base/util.h"
#include "data_manager/dataset.pb.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

void SetEntry(absl::string_view name, uint64_t offset, uint64_t size,
              DataSetMetadata::Entry* entry) {
  entry->set_name(name);
  entry->set_offset(offset);
  entry->set_size(size);
}

TEST(DatasetWriterTest, Write) {
  // Create a dummy file to be packed.
  TempFile in = testing::MakeTempFileOrDie();
  absl::Status s =
      FileUtil::SetContents(in.path(), absl::string_view("m\0zc\xEF", 5));
  ASSERT_OK(s);

  // Generate a packed file into |actual|.
  std::string actual;
  {
    DataSetWriter w("magic");

    w.Add("data8", 8, std::string("data8 \x00\x01", 8));
    w.Add("data16", 16, "data16 \xAB\xCD\xEF");
    w.Add("data32", 32, std::string("data32 \x00\xAB\n\r\n", 12));
    w.Add("data64", 64, std::string("data64 \t\t\x00\x00", 11));
    w.Add("data128", 128, "data128 abcdefg");
    w.Add("data256", 256, "data256 xyz");

    w.AddFile("file8", 8, in.path());
    w.AddFile("file16", 16, in.path());
    w.AddFile("file32", 32, in.path());
    w.AddFile("file64", 64, in.path());
    w.AddFile("file128", 128, in.path());
    w.AddFile("file256", 256, in.path());

    std::stringstream out;
    w.Finish(&out);
    actual = out.str();
  }

  // Define the expected binary image.
  constexpr char data_chunk[] =
      "magic"                               // offset 0, size 5
      "data8 \x00\x01"                      // offset 5, size 8
      "\0"                                  // offset 13, size 1 (padding)
      "data16 \xAB\xCD\xEF"                 // offset 14, size 10
      "data32 \x00\xAB\n\r\n"               // offset 24, size 12
      "\0\0\0\0"                            // offset 36, size 4 (padding)
      "data64 \t\t\x00\x00"                 // offset 40, size 11
      "\0\0\0\0\0\0\0\0\0\0\0\0\0"          // offset 51, size 13 (padding)
      "data128 abcdefg"                     // offset 64, size 15
      "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"  // offset 79, size 17 (padding)
      "data256 xyz"                         // offset 96, size 11
      "m\0zc\xEF"                           // offset 107, size 5 (file8)
      "m\0zc\xEF"                           // offset 112, size 5 (file16)
      "\0\0\0"                              // offset 117, size 3 (padding)
      "m\0zc\xEF"                           // offset 120, size 5 (file32)
      "\0\0\0"                              // offset 125, size 3 (padding)
      "m\0zc\xEF"                           // offset 128, size 5 (file64)
      "\0\0\0\0\0\0\0\0\0\0\0"              // offset 133, size 11 (padding)
      "m\0zc\xEF"                           // offset 144 size 5 (file128)
      "\0\0\0\0\0\0\0\0\0\0\0"              // offset 149, size 11 (padding)
      "m\0zc\xEF";                          // offset 160, size 5 (file256)
  DataSetMetadata metadata;
  SetEntry("data8", 5, 8, metadata.add_entries());
  SetEntry("data16", 14, 10, metadata.add_entries());
  SetEntry("data32", 24, 12, metadata.add_entries());
  SetEntry("data64", 40, 11, metadata.add_entries());
  SetEntry("data128", 64, 15, metadata.add_entries());
  SetEntry("data256", 96, 11, metadata.add_entries());
  SetEntry("file8", 107, 5, metadata.add_entries());
  SetEntry("file16", 112, 5, metadata.add_entries());
  SetEntry("file32", 120, 5, metadata.add_entries());
  SetEntry("file64", 128, 5, metadata.add_entries());
  SetEntry("file128", 144, 5, metadata.add_entries());
  SetEntry("file256", 160, 5, metadata.add_entries());
  const std::string& metadata_chunk = metadata.SerializeAsString();
  const std::string& metadata_size =
      Util::SerializeUint64(metadata_chunk.size());
  // Append data_chunk except for the last '\0'.
  std::string expected(data_chunk, sizeof(data_chunk) - 1);
  expected.append(metadata_chunk.data(), metadata_chunk.size());
  expected.append(metadata_size.data(), metadata_size.size());
  expected.append(internal::UnverifiedSHA1::MakeDigest(expected));
  expected.append(Util::SerializeUint64(expected.size() + 8));

  // Compare the results.
  EXPECT_EQ(actual, expected);
}

}  // namespace
}  // namespace mozc

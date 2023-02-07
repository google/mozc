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

#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

#include "base/port.h"
#include "base/util.h"
#include "data_manager/dataset_writer.h"
#include "testing/gunit.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

std::string GenerateRandomBytes(size_t len) {
  std::string s;
  s.resize(len);
  for (size_t i = 0; i < len; ++i) {
    s[i] = static_cast<char>(Util::Random(256));
  }
  return s;
}

std::string GetTestMagicNumber() { return std::string("ma\0gic", 6); }

TEST(DataSetReaderTest, ValidData) {
  const absl::string_view kGoogle("GOOGLE"), kMozc("m\0zc\xEF", 5);
  std::string image;
  {
    DataSetWriter w(GetTestMagicNumber());
    w.Add("google", 16, kGoogle);
    w.Add("mozc", 64, kMozc);
    std::stringstream out;
    w.Finish(&out);
    image = out.str();
  }

  DataSetReader r;
  ASSERT_TRUE(DataSetReader::VerifyChecksum(image));
  ASSERT_TRUE(r.Init(image, GetTestMagicNumber()));

  absl::string_view data;
  EXPECT_TRUE(r.Get("google", &data));
  EXPECT_EQ(data, kGoogle);
  EXPECT_TRUE(r.Get("mozc", &data));
  EXPECT_EQ(data, kMozc);

  EXPECT_FALSE(r.Get("", &data));
  EXPECT_FALSE(r.Get("foo", &data));
}

TEST(DataSetReaderTest, InvalidMagicString) {
  const std::string &magic = GetTestMagicNumber();
  DataSetReader r;
  EXPECT_FALSE(r.Init("", magic));
  EXPECT_FALSE(r.Init("abc", magic));
  EXPECT_FALSE(r.Init("this is a text file", magic));
}

TEST(DataSetReaderTest, BrokenMetadata) {
  const std::string &magic = GetTestMagicNumber();
  DataSetReader r;

  // Only magic number, no metadata.
  EXPECT_FALSE(DataSetReader::VerifyChecksum(magic));
  EXPECT_FALSE(r.Init(magic, magic));

  // Metadata size is too small.
  std::string data = magic;
  data.append("content and metadata");
  data.append(Util::SerializeUint64(0));
  EXPECT_FALSE(DataSetReader::VerifyChecksum(data));
  EXPECT_FALSE(r.Init(data, magic));

  // Metadata size is too small.
  data = magic;
  data.append("content and metadata");
  data.append(Util::SerializeUint64(4));
  EXPECT_FALSE(DataSetReader::VerifyChecksum(data));
  EXPECT_FALSE(r.Init(data, magic));

  // Metadata size is too large.
  data = magic;
  data.append("content and metadata");
  data.append(Util::SerializeUint64(std::numeric_limits<uint64_t>::max()));
  EXPECT_FALSE(DataSetReader::VerifyChecksum(data));
  EXPECT_FALSE(r.Init(data, magic));

  // Metadata chunk is not a serialied protobuf.
  data = magic;
  data.append("content and metadata");
  data.append(Util::SerializeUint64(strlen("content and metadata")));
  EXPECT_FALSE(DataSetReader::VerifyChecksum(data));
  EXPECT_FALSE(r.Init(data, magic));
}

TEST(DataSetReaderTest, BrokenMetadataFields) {
  const std::string &magic = GetTestMagicNumber();
  const absl::string_view kGoogle("GOOGLE"), kMozc("m\0zc\xEF", 5);
  std::string content;
  {
    DataSetWriter w(magic);
    w.Add("google", 16, kGoogle);
    w.Add("mozc", 64, kMozc);
    std::stringstream out;
    w.Finish(&out);

    // Remove the metadata chunk at the bottom, which will be appended in each
    // test below.
    content = out.str();
    const size_t metadata_size = w.metadata().SerializeAsString().size();
    content.erase(content.size() - metadata_size - 8);
  }
  {
    // Create an image with broken metadata.
    DataSetMetadata md;
    auto e = md.add_entries();
    e->set_name("google");
    e->set_offset(content.size() + 3);  // Invalid offset
    e->set_size(kGoogle.size());
    const std::string &md_str = md.SerializeAsString();
    std::string image = content;
    image.append(md_str);
    image.append(Util::SerializeUint64(md_str.size()));

    DataSetReader r;
    EXPECT_FALSE(DataSetReader::VerifyChecksum(image));
    EXPECT_FALSE(r.Init(image, magic));
  }
  {
    // Create an image with broken size.
    DataSetMetadata md;
    auto e = md.add_entries();
    e->set_name("google");
    e->set_offset(content.size());
    e->set_size(std::numeric_limits<uint64_t>::max());  // Too big size
    const std::string &md_str = md.SerializeAsString();
    std::string image = content;
    image.append(md_str);
    image.append(Util::SerializeUint64(md_str.size()));

    DataSetReader r;
    EXPECT_FALSE(DataSetReader::VerifyChecksum(image));
    EXPECT_FALSE(r.Init(image, magic));
  }
}

TEST(DataSetReaderTest, OneBitError) {
  const char *kTestMagicNumber = "Dummy magic number\r\n";

  // Create data at random.
  std::string image;
  {
    constexpr int kAlignments[] = {8, 16, 32, 64};
    DataSetWriter w(kTestMagicNumber);
    for (int i = 0; i < 10; ++i) {
      w.Add(absl::StrFormat("key%d", i), kAlignments[Util::Random(4)],
            GenerateRandomBytes(1 + Util::Random(1024)));
    }
    std::stringstream out;
    w.Finish(&out);
    image = out.str();
  }

  DataSetReader r;
  ASSERT_TRUE(DataSetReader::VerifyChecksum(image));
  ASSERT_TRUE(r.Init(image, kTestMagicNumber));

  // Flip each bit and test if VerifyChecksum fails.
  for (size_t i = 0; i < image.size(); ++i) {
    const char orig = image[i];
    for (size_t j = 0; j < 8; ++j) {
      image[i] = orig ^ (1 << j);  // Flip (j + 1)-th bit

      // Since checksum is computed from the bytes up to metadata size, errors
      // in the last 8 bytes (where file size is stored) cannot be tested using
      // the checksum.  However, in such case, Init() should fail due to file
      // size mismatch.  Thus, either VerifyChecksum() or Init() fails.
      EXPECT_FALSE(DataSetReader::VerifyChecksum(image) &&
                   r.Init(image, kTestMagicNumber));
    }
    image[i] = orig;  // Recover the original data.
  }
}

}  // namespace
}  // namespace mozc

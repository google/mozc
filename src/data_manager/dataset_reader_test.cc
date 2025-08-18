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
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <string>

#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "base/random.h"
#include "base/util.h"
#include "data_manager/dataset.pb.h"
#include "data_manager/dataset_writer.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

using ::testing::Optional;
using ::testing::Pair;

constexpr absl::string_view kTestMagicNumber = {"ma\0gic", 6};

TEST(DataSetReaderTest, ValidData) {
  constexpr absl::string_view kGoogle("GOOGLE"), kMozc("m\0zc\xEF", 5);
  std::string image;
  {
    DataSetWriter w(kTestMagicNumber);
    w.Add("google", 16, kGoogle);
    w.Add("mozc", 64, kMozc);
    std::stringstream out;
    w.Finish(&out);
    image = out.str();
  }

  DataSetReader r;
  ASSERT_TRUE(DataSetReader::VerifyChecksum(image));
  ASSERT_TRUE(r.Init(image, kTestMagicNumber));

  absl::string_view data;
  EXPECT_TRUE(r.Get("google", &data));
  EXPECT_EQ(data, kGoogle);
  EXPECT_THAT(r.GetOffsetAndSize("google"), Optional(Pair(6, kGoogle.size())));
  EXPECT_TRUE(r.Get("mozc", &data));
  EXPECT_EQ(data, kMozc);
  EXPECT_THAT(r.GetOffsetAndSize("mozc"), Optional(Pair(16, kMozc.size())));

  EXPECT_FALSE(r.Get("", &data));
  EXPECT_FALSE(r.Get("foo", &data));
  EXPECT_EQ(r.GetOffsetAndSize(""), std::nullopt);
  EXPECT_EQ(r.GetOffsetAndSize("foo"), std::nullopt);
}

TEST(DataSetReaderTest, InvalidMagicString) {
  DataSetReader r;
  EXPECT_FALSE(r.Init("", kTestMagicNumber));
  EXPECT_FALSE(r.Init("abc", kTestMagicNumber));
  EXPECT_FALSE(r.Init("this is a text file", kTestMagicNumber));
}

TEST(DataSetReaderTest, BrokenMetadata) {
  DataSetReader r;
  constexpr absl::string_view kContent = "content and metadata";

  // Only magic number, no metadata.
  EXPECT_FALSE(DataSetReader::VerifyChecksum(kTestMagicNumber));
  EXPECT_FALSE(r.Init(kTestMagicNumber, kTestMagicNumber));

  // Metadata size is too small.
  std::string data =
      absl::StrCat(kTestMagicNumber, kContent, Util::SerializeUint64(0));
  EXPECT_FALSE(DataSetReader::VerifyChecksum(data));
  EXPECT_FALSE(r.Init(data, kTestMagicNumber));

  // Metadata size is too small.
  data = absl::StrCat(kTestMagicNumber, kContent, Util::SerializeUint64(4));
  EXPECT_FALSE(DataSetReader::VerifyChecksum(data));
  EXPECT_FALSE(r.Init(data, kTestMagicNumber));

  // Metadata size is too large.
  data =
      absl::StrCat(kTestMagicNumber, kContent,
                   Util::SerializeUint64(std::numeric_limits<uint64_t>::max()));
  EXPECT_FALSE(DataSetReader::VerifyChecksum(data));
  EXPECT_FALSE(r.Init(data, kTestMagicNumber));

  // Metadata chunk is not a serialized protobuf.
  data = absl::StrCat(kTestMagicNumber, kContent,
                      Util::SerializeUint64(kContent.size()));
  EXPECT_FALSE(DataSetReader::VerifyChecksum(data));
  EXPECT_FALSE(r.Init(data, kTestMagicNumber));
}

TEST(DataSetReaderTest, BrokenMetadataFields) {
  constexpr absl::string_view kGoogle("GOOGLE"), kMozc("m\0zc\xEF", 5);
  std::string content;
  {
    DataSetWriter w(kTestMagicNumber);
    w.Add("google", 16, kGoogle);
    w.Add("mozc", 64, kMozc);
    std::stringstream out;
    w.Finish(&out);

    // Remove the metadata chunk at the bottom, which will be appended in each
    // test below.
    content = out.str();
    const size_t metadata_size = w.metadata().ByteSizeLong();
    content.erase(content.size() - metadata_size - 8);
  }
  {
    // Create an image with broken metadata.
    DataSetMetadata md;
    auto e = md.add_entries();
    e->set_name("google");
    e->set_offset(content.size() + 3);  // Invalid offset
    e->set_size(kGoogle.size());
    const std::string& md_str = md.SerializeAsString();
    std::string image =
        absl::StrCat(content, md_str, Util::SerializeUint64(md_str.size()));

    DataSetReader r;
    EXPECT_FALSE(DataSetReader::VerifyChecksum(image));
    EXPECT_FALSE(r.Init(image, kTestMagicNumber));
  }
  {
    // Create an image with broken size.
    DataSetMetadata md;
    auto e = md.add_entries();
    e->set_name("google");
    e->set_offset(content.size());
    e->set_size(std::numeric_limits<uint64_t>::max());  // Too big size
    const std::string& md_str = md.SerializeAsString();
    std::string image =
        absl::StrCat(content, md_str, Util::SerializeUint64(md_str.size()));

    DataSetReader r;
    EXPECT_FALSE(DataSetReader::VerifyChecksum(image));
    EXPECT_FALSE(r.Init(image, kTestMagicNumber));
  }
}

TEST(DataSetReaderTest, OneBitError) {
  constexpr absl::string_view kTestMagicNumber = "Dummy magic number\r\n";

  // Create data at random.
  std::string image;
  {
    constexpr int kAlignments[] = {8, 16, 32, 64};
    DataSetWriter w(kTestMagicNumber);
    Random random;
    for (int i = 0; i < 10; ++i) {
      w.Add(absl::StrFormat("key%d", i),
            kAlignments[absl::Uniform(random, 0u, std::size(kAlignments))],
            random.ByteString(absl::Uniform(random, 1, 1024)));
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

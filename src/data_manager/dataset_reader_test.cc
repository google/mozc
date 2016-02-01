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

#include "data_manager/dataset_reader.h"

#include <sstream>
#include <string>

#include "base/util.h"
#include "data_manager/dataset_writer.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

string GetTestMagicNumber() {
  return string("ma\0gic", 6);
}

TEST(DataSetReaderTest, ValidData) {
  const StringPiece kGoogle("GOOGLE"), kMozc("m\0zc\xEF", 5);
  string image;
  {
    stringstream out;
    DataSetWriter w(GetTestMagicNumber(), &out);
    w.Add("google", 16, kGoogle);
    w.Add("mozc", 64, kMozc);
    w.Finish();
    image = out.str();
  }

  DataSetReader r;
  ASSERT_TRUE(r.Init(image, GetTestMagicNumber()));

  StringPiece data;
  EXPECT_TRUE(r.Get("google", &data));
  EXPECT_EQ(kGoogle, data);
  EXPECT_TRUE(r.Get("mozc", &data));
  EXPECT_EQ(kMozc, data);

  EXPECT_FALSE(r.Get("", &data));
  EXPECT_FALSE(r.Get("foo", &data));
}

TEST(DataSetReaderTest, InvalidMagicString) {
  const string &magic = GetTestMagicNumber();
  DataSetReader r;
  EXPECT_FALSE(r.Init("", magic));
  EXPECT_FALSE(r.Init("abc", magic));
  EXPECT_FALSE(r.Init("this is a text file", magic));
}

TEST(DataSetReaderTest, BrokenMetadata) {
  const string &magic = GetTestMagicNumber();
  DataSetReader r;

  // Only magic number, no metadata.
  EXPECT_FALSE(r.Init(magic, magic));

  // Metadata size is too small.
  string data = magic;
  data.append("content and metadata");
  data.append(Util::SerializeUint64(0));
  EXPECT_FALSE(r.Init(data, magic));

  // Metadata size is too small.
  data = magic;
  data.append("content and metadata");
  data.append(Util::SerializeUint64(4));
  EXPECT_FALSE(r.Init(data, magic));

  // Metadata size is too large.
  data = magic;
  data.append("content and metadata");
  data.append(Util::SerializeUint64(kuint64max));
  EXPECT_FALSE(r.Init(data, magic));

  // Metadata chunk is not a serialied protobuf.
  data = magic;
  data.append("content and metadata");
  data.append(Util::SerializeUint64(strlen("content and metadata")));
  EXPECT_FALSE(r.Init(data, magic));
}

TEST(DataSetReaderTest, BrokenMetadataFields) {
  const string &magic = GetTestMagicNumber();
  const StringPiece kGoogle("GOOGLE"), kMozc("m\0zc\xEF", 5);
  string content;
  {
    stringstream out;
    DataSetWriter w(magic, &out);
    w.Add("google", 16, kGoogle);
    w.Add("mozc", 64, kMozc);
    w.Finish();

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
    const string &md_str = md.SerializeAsString();
    string image = content;
    image.append(md_str);
    image.append(Util::SerializeUint64(md_str.size()));

    DataSetReader r;
    EXPECT_FALSE(r.Init(image, magic));
  }
  {
    // Create an image with broken size.
    DataSetMetadata md;
    auto e = md.add_entries();
    e->set_name("google");
    e->set_offset(content.size());
    e->set_size(kuint64max);        // Too big size
    const string &md_str = md.SerializeAsString();
    string image = content;
    image.append(md_str);
    image.append(Util::SerializeUint64(md_str.size()));

    DataSetReader r;
    EXPECT_FALSE(r.Init(image, magic));
  }
}

}  // namespace
}  // namespace mozc

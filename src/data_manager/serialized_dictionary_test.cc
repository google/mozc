// Copyright 2010-2020, Google Inc.
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

#include "data_manager/serialized_dictionary.h"

#include <sstream>
#include <string>

#include "base/port.h"
#include "base/serialized_string_array.h"
#include "testing/base/public/gunit.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

const char *kTestInput =
    "key1\t10\t20\t3000\tvalue1\tdesc1\tadesc1\n"  // Entry 1
    "key1\t50\t60\t2000\tvalue2\tdesc2\tadesc2\n"  // Entry 2
    "key2\t50\t60\t2000\tvalue3\n";                // Entry 3

class SerializedDictionaryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::stringstream ifs(kTestInput);
    const std::pair<absl::string_view, absl::string_view> data =
        SerializedDictionary::Compile(&ifs, &buf1_, &buf2_);
    token_array_data_ = data.first;
    string_array_data_ = data.second;
  }

  absl::string_view token_array_data_;   // Pointing to a block of buf1_
  absl::string_view string_array_data_;  // Pointing to a block of buf2_

 private:
  std::unique_ptr<uint32[]> buf1_;
  std::unique_ptr<uint32[]> buf2_;
};

TEST_F(SerializedDictionaryTest, Compile) {
  // Compile() is called in SetUp()

  SerializedStringArray string_array;
  ASSERT_TRUE(string_array.Init(string_array_data_));
  ASSERT_EQ(10, string_array.size());
  EXPECT_EQ("", string_array[0]);
  EXPECT_EQ("adesc1", string_array[1]);
  EXPECT_EQ("adesc2", string_array[2]);
  EXPECT_EQ("desc1", string_array[3]);
  EXPECT_EQ("desc2", string_array[4]);
  EXPECT_EQ("key1", string_array[5]);
  EXPECT_EQ("key2", string_array[6]);
  EXPECT_EQ("value1", string_array[7]);
  EXPECT_EQ("value2", string_array[8]);
  EXPECT_EQ("value3", string_array[9]);

  // Recall that entries are sorted first by key then by cost.
  const char kExpectedTokenArray[] =
      // Entry 2
      "\x05\x00\x00\x00"  // key = "key1", index = 5
      "\x08\x00\x00\x00"  // value = "value2", index = 8
      "\x04\x00\x00\x00"  // description = "desc2", index = 4
      "\x02\x00\x00\x00"  // additional_description = "adesc2", index = 2
      "\x32\x00"          // lid = 50
      "\x3c\x00"          // rid = 60
      "\xd0\x07"          // cost = 2000
      "\x00\x00"          // padding
      // Entry 1
      "\x05\x00\x00\x00"  // key = "key1", index = 5
      "\x07\x00\x00\x00"  // value = "value1", index = 7
      "\x03\x00\x00\x00"  // description = "desc1", index = 3
      "\x01\x00\x00\x00"  // additional_description = "adesc1", index = 1
      "\x0a\x00"          // lid = 10
      "\x14\x00"          // rid = 20
      "\xb8\x0b"          // cost = 3000
      "\x00\x00"          // padding
      // Entry 3
      "\x06\x00\x00\x00"  // key = "key2", index = 6
      "\x09\x00\x00\x00"  // value = "value3", index = 9
      "\x00\x00\x00\x00"  // description = "", index = 0
      "\x00\x00\x00\x00"  // additional_description = "", index = 0
      "\x32\x00"          // lid = 50
      "\x3c\x00"          // rid = 60
      "\xd0\x07"          // cost = 2000
      "\x00\x00";         // padding
  ASSERT_EQ(
      std::string(kExpectedTokenArray, arraysize(kExpectedTokenArray) - 1),
      token_array_data_);
}

TEST_F(SerializedDictionaryTest, Iterator) {
  SerializedDictionary dic(token_array_data_, string_array_data_);
  auto iter = dic.begin();

  ASSERT_NE(dic.end(), iter);
  EXPECT_EQ("key1", iter.key());
  EXPECT_EQ("value2", iter.value());
  EXPECT_EQ("desc2", iter.description());
  EXPECT_EQ("adesc2", iter.additional_description());
  EXPECT_EQ(50, iter.lid());
  EXPECT_EQ(60, iter.rid());
  EXPECT_EQ(2000, iter.cost());

  ++iter;
  ASSERT_NE(dic.end(), iter);
  EXPECT_EQ("key1", iter.key());
  EXPECT_EQ("value1", iter.value());
  EXPECT_EQ("desc1", iter.description());
  EXPECT_EQ("adesc1", iter.additional_description());
  EXPECT_EQ(10, iter.lid());
  EXPECT_EQ(20, iter.rid());
  EXPECT_EQ(3000, iter.cost());

  ++iter;
  ASSERT_NE(dic.end(), iter);
  EXPECT_EQ("key2", iter.key());
  EXPECT_EQ("value3", iter.value());
  EXPECT_TRUE(iter.description().empty());
  EXPECT_TRUE(iter.additional_description().empty());
  EXPECT_EQ(50, iter.lid());
  EXPECT_EQ(60, iter.rid());
  EXPECT_EQ(2000, iter.cost());

  ++iter;
  EXPECT_EQ(dic.end(), iter);
}

TEST_F(SerializedDictionaryTest, EqualRange) {
  SerializedDictionary dic(token_array_data_, string_array_data_);
  {
    auto range = dic.equal_range("key1");
    ASSERT_NE(range.first, range.second);
    EXPECT_EQ("key1", range.first.key());
    EXPECT_EQ("value2", range.first.value());

    ++range.first;
    ASSERT_NE(range.first, range.second);
    EXPECT_EQ("key1", range.first.key());
    EXPECT_EQ("value1", range.first.value());

    ++range.first;
    EXPECT_EQ(range.first, range.second);
  }
  {
    auto range = dic.equal_range("key2");
    ASSERT_NE(range.first, range.second);
    EXPECT_EQ("key2", range.first.key());
    EXPECT_EQ("value3", range.first.value());

    ++range.first;
    EXPECT_EQ(range.first, range.second);
  }
  {
    auto range = dic.equal_range("mozc");
    EXPECT_EQ(range.first, range.second);
  }
}

}  // namespace
}  // namespace mozc

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

#include "data_manager/serialized_dictionary.h"

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

const char* kTestInput =
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
  std::unique_ptr<uint32_t[]> buf1_;
  std::unique_ptr<uint32_t[]> buf2_;
};

TEST_F(SerializedDictionaryTest, Compile) {
  // Compile() is called in SetUp()

  SerializedStringArray string_array;
  ASSERT_TRUE(string_array.Init(string_array_data_));
  ASSERT_EQ(string_array.size(), 10);
  EXPECT_EQ(string_array[0], "");
  EXPECT_EQ(string_array[1], "adesc1");
  EXPECT_EQ(string_array[2], "adesc2");
  EXPECT_EQ(string_array[3], "desc1");
  EXPECT_EQ(string_array[4], "desc2");
  EXPECT_EQ(string_array[5], "key1");
  EXPECT_EQ(string_array[6], "key2");
  EXPECT_EQ(string_array[7], "value1");
  EXPECT_EQ(string_array[8], "value2");
  EXPECT_EQ(string_array[9], "value3");

  // Recall that entries are sorted first by key then by cost.
  constexpr char kExpectedTokenArray[] =
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
  ASSERT_EQ(token_array_data_, std::string(kExpectedTokenArray,
                                           std::size(kExpectedTokenArray) - 1));
}

TEST_F(SerializedDictionaryTest, Iterator) {
  SerializedDictionary dic(token_array_data_, string_array_data_);
  auto iter = dic.begin();

  ASSERT_NE(iter, dic.end());
  EXPECT_EQ(iter.key(), "key1");
  EXPECT_EQ(iter.value(), "value2");
  EXPECT_EQ(iter.description(), "desc2");
  EXPECT_EQ(iter.additional_description(), "adesc2");
  EXPECT_EQ(iter.lid(), 50);
  EXPECT_EQ(iter.rid(), 60);
  EXPECT_EQ(iter.cost(), 2000);

  ++iter;
  ASSERT_NE(iter, dic.end());
  EXPECT_EQ(iter.key(), "key1");
  EXPECT_EQ(iter.value(), "value1");
  EXPECT_EQ(iter.description(), "desc1");
  EXPECT_EQ(iter.additional_description(), "adesc1");
  EXPECT_EQ(iter.lid(), 10);
  EXPECT_EQ(iter.rid(), 20);
  EXPECT_EQ(iter.cost(), 3000);

  ++iter;
  ASSERT_NE(iter, dic.end());
  EXPECT_EQ(iter.key(), "key2");
  EXPECT_EQ(iter.value(), "value3");
  EXPECT_TRUE(iter.description().empty());
  EXPECT_TRUE(iter.additional_description().empty());
  EXPECT_EQ(iter.lid(), 50);
  EXPECT_EQ(iter.rid(), 60);
  EXPECT_EQ(iter.cost(), 2000);

  ++iter;
  EXPECT_EQ(iter, dic.end());
}

TEST_F(SerializedDictionaryTest, EqualRange) {
  SerializedDictionary dic(token_array_data_, string_array_data_);
  {
    auto range = dic.equal_range("key1");
    ASSERT_NE(range.first, range.second);
    EXPECT_EQ(range.first.key(), "key1");
    EXPECT_EQ(range.first.value(), "value2");

    ++range.first;
    ASSERT_NE(range.first, range.second);
    EXPECT_EQ(range.first.key(), "key1");
    EXPECT_EQ(range.first.value(), "value1");

    ++range.first;
    EXPECT_EQ(range.second, range.first);
  }
  {
    auto range = dic.equal_range("key2");
    ASSERT_NE(range.first, range.second);
    EXPECT_EQ(range.first.key(), "key2");
    EXPECT_EQ(range.first.value(), "value3");

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

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

#include "prediction/zero_query_dict.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/serialized_string_array.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

constexpr char kTestTokenArray[] =
    // The last two items must be 0x00, because they are now unused field.
    // {"あ", "", ZERO_QUERY_EMOJI, 0x00, 0x00}
    "\x04\x00\x00\x00"
    "\x00\x00\x00\x00"
    "\x03\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"あ", "❕", ZERO_QUERY_EMOJI, 0x00, 0x00},
    "\x04\x00\x00\x00"
    "\x02\x00\x00\x00"
    "\x03\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"あ", "❣", ZERO_QUERY_NONE, 0x00, 0x00},
    "\x04\x00\x00\x00"
    "\x03\x00\x00\x00"
    "\x00\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"ああ", "( •̀ㅁ•́;)", ZERO_QUERY_EMOTICON, 0x00, 0x00}
    "\x05\x00\x00\x00"
    "\x01\x00\x00\x00"
    "\x02\x00"
    "\x00\x00"
    "\x00\x00\x00\x00";

constexpr absl::string_view kTestStrings[] = {
    "", "( •̀ㅁ•́;)", "❕", "❣", "あ", "ああ",
};

// Initializes a ZeroQueryDict from the above test data.  Note that the returned
// buffer contains the internal data used by |dict|, so it must outlive |dict|.
std::unique_ptr<uint32_t[]> InitTestZeroQueryDict(ZeroQueryDict* dict) {
  // kTestTokenArray contains a trailing '\0', so create a absl::string_view
  // that excludes it by subtracting 1.
  const absl::string_view token_array_data(kTestTokenArray,
                                           std::size(kTestTokenArray) - 1);
  std::unique_ptr<uint32_t[]> string_data_buffer;
  const absl::string_view string_array_data =
      SerializedStringArray::SerializeToBuffer(kTestStrings,
                                               &string_data_buffer);
  dict->Init(token_array_data, string_array_data);
  return string_data_buffer;
}

TEST(ZeroQueryDictTest, IterateForwardByPreIncrement) {
  ZeroQueryDict dict;
  const auto buf = InitTestZeroQueryDict(&dict);
  absl::Span<const ZeroQueryEntry> entries = dict.GetZeroQueryEntreis();
  EXPECT_EQ(entries.size(), 4);

  EXPECT_EQ(entries[0].key_index, 4);    // Index to "あ"
  EXPECT_EQ(entries[0].value_index, 0);  // Index to ""
  EXPECT_EQ(entries[0].type, ZERO_QUERY_EMOJI);
  EXPECT_EQ(dict.key(entries[0]), "あ");
  EXPECT_EQ(dict.value(entries[0]), "");

  EXPECT_EQ(entries[1].key_index, 4);    // Index to "あ"
  EXPECT_EQ(entries[1].value_index, 2);  // Index to "❕"
  EXPECT_EQ(entries[1].type, ZERO_QUERY_EMOJI);
  EXPECT_EQ(dict.key(entries[1]), "あ");
  EXPECT_EQ(dict.value(entries[1]), "❕");

  EXPECT_EQ(entries[2].key_index, 4);    // Index to "あ"
  EXPECT_EQ(entries[2].value_index, 3);  // Index to "❣"
  EXPECT_EQ(entries[2].type, ZERO_QUERY_NONE);
  EXPECT_EQ(dict.key(entries[2]), "あ");
  EXPECT_EQ(dict.value(entries[2]), "❣");

  EXPECT_EQ(entries[3].key_index, 5);    // Index to "ああ"
  EXPECT_EQ(entries[3].value_index, 1);  // Index to "( •̀ㅁ•́;)"
  EXPECT_EQ(entries[3].type, ZERO_QUERY_EMOTICON);
  EXPECT_EQ(dict.key(entries[3]), "ああ");
  EXPECT_EQ(dict.value(entries[3]), "( •̀ㅁ•́;)");
}

TEST(ZeroQueryDict, EqualRange) {
  ZeroQueryDict dict;
  const auto buf = InitTestZeroQueryDict(&dict);

  EXPECT_EQ(dict.equal_range("あ").size(), 3);
  EXPECT_EQ(dict.equal_range("ああ").size(), 1);
  EXPECT_TRUE(dict.equal_range("This key is not found").empty());
}

TEST(ZeroQueryDict, UninitializedTest) {
  ZeroQueryDict dict;
  EXPECT_TRUE(dict.GetZeroQueryEntreis().empty());
  EXPECT_TRUE(dict.equal_range("あ").empty());
}

}  // namespace
}  // namespace mozc

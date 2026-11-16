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

#include "dictionary/pos_id_map.h"

#include <cstddef>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>

#include "absl/strings/string_view.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc::dictionary {
namespace {

TEST(PosIdMapTest, LoadFromGeneratedBinary) {
  std::string file_path = mozc::testing::GetSourceFileOrDie(
      {"data_manager", "testing", "pos_id_map.data"});

  std::ifstream ifs(file_path, std::ios::binary);
  ASSERT_TRUE(ifs.is_open());
  std::stringstream ss;
  ss << ifs.rdbuf();
  std::string data = ss.str();

  PosIdMap pos_id_map(data);

  EXPECT_GT(pos_id_map.GetPosIdCount(), 2000);
  EXPECT_EQ(pos_id_map.GetPosString(0), "BOS/EOS,*,*,*,*,*,*");
  EXPECT_EQ(pos_id_map.GetPosString(pos_id_map.GetPosIdCount() + 100), "");

  // Verify that some known POS string exists and matches.
  bool found_noun_general = false;
  for (size_t pos_id = 0; pos_id < pos_id_map.GetPosIdCount(); ++pos_id) {
    if (pos_id_map.GetPosString(pos_id).starts_with("名詞,一般")) {
      found_noun_general = true;
      break;
    }
  }
  EXPECT_TRUE(found_noun_general);
}

TEST(PosIdMapTest, SimpleNullSeparatedData) {
  // 3 entries: "BOS/EOS", "名詞,一般", "動詞"
  const char data[] = "BOS/EOS\0名詞,一般\0動詞";
  PosIdMap pos_id_map(absl::string_view(data, sizeof(data) - 1));

  EXPECT_EQ(pos_id_map.GetPosIdCount(), 3);
  EXPECT_EQ(pos_id_map.GetPosString(0), "BOS/EOS");
  EXPECT_EQ(pos_id_map.GetPosString(1), "名詞,一般");
  EXPECT_EQ(pos_id_map.GetPosString(2), "動詞");
  EXPECT_EQ(pos_id_map.GetPosString(3), "");
  EXPECT_EQ(pos_id_map.GetPosString(100), "");
}

TEST(PosIdMapTest, EmptyDataHandledGracefully) {
  PosIdMap pos_id_map("");
  EXPECT_EQ(pos_id_map.GetPosIdCount(), 0);
  EXPECT_EQ(pos_id_map.GetPosString(0), "");
  EXPECT_EQ(pos_id_map.GetPosString(100), "");
}

}  // namespace
}  // namespace mozc::dictionary

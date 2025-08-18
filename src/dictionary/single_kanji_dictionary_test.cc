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

#include "dictionary/single_kanji_dictionary.h"

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "absl/strings/string_view.h"
#include "data_manager/testing/mock_data_manager.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace dictionary {
namespace {

class SingleKanjiDictionaryTest : public testing::TestWithTempUserProfile {
 protected:
  SingleKanjiDictionaryTest()
      : data_manager_(std::make_unique<testing::MockDataManager>()) {}

  std::unique_ptr<testing::MockDataManager> data_manager_;
};

TEST_F(SingleKanjiDictionaryTest, GenerateDescription) {
  SingleKanjiDictionary dictionary(*data_manager_);
  std::string description;
  // variant of "亜".
  EXPECT_TRUE(dictionary.GenerateDescription("亞", &description));
  EXPECT_EQ(description, "亜の旧字体");

  // no entry
  EXPECT_FALSE(dictionary.GenerateDescription("あ", &description));
  EXPECT_FALSE(dictionary.GenerateDescription("ABC", &description));
  EXPECT_FALSE(dictionary.GenerateDescription("", &description));
}

TEST_F(SingleKanjiDictionaryTest, LookupNounPrefixEntries) {
  SingleKanjiDictionary dictionary(*data_manager_);
  {
    auto [begin, end] = dictionary.LookupNounPrefixEntries("ご");
    EXPECT_NE(begin, end);
    for (auto& iter = begin; iter != end; ++iter) {
      EXPECT_FALSE(iter.value().empty());
      EXPECT_TRUE(iter.cost() == 0 || iter.cost() == 1);
    }
  }
  {
    // no entry
    auto [begin, end] = dictionary.LookupNounPrefixEntries("てすと");
    EXPECT_EQ(begin, end);
    std::tie(begin, end) = dictionary.LookupNounPrefixEntries("");
    EXPECT_EQ(begin, end);
  }
}

TEST_F(SingleKanjiDictionaryTest, LookupKanjiEntries) {
  SingleKanjiDictionary dictionary(*data_manager_);

  auto contains = [](std::vector<std::string> entries,
                     absl::string_view value) {
    auto it = std::find(entries.begin(), entries.end(), value);
    return it != entries.end();
  };

  std::vector<std::string> entries;
  {
    entries = dictionary.LookupKanjiEntries("かみ",
                                            /* use_svs = */ true);
    EXPECT_FALSE(entries.empty());
    EXPECT_TRUE(contains(entries, "神"));
    // 神︀ SVS character.
    EXPECT_TRUE(contains(entries, "\u795E\uFE00"));
    // 神 CJK compat ideograph.
    EXPECT_FALSE(contains(entries, "\uFA19"));
  }
  {
    entries = dictionary.LookupKanjiEntries("かみ",
                                            /* use_svs = */ false);
    EXPECT_FALSE(entries.empty());
    EXPECT_TRUE(contains(entries, "神"));
    // 神︀ SVS character.
    EXPECT_FALSE(contains(entries, "\u795E\uFE00"));
    // 神 CJK compat ideograph.
    EXPECT_TRUE(contains(entries, "\uFA19"));
  }
  {
    entries = dictionary.LookupKanjiEntries("",
                                            /* use_svs = */ false);
    EXPECT_TRUE(entries.empty());
    dictionary.LookupKanjiEntries("unknown reading",
                                  /* use_svs = */ false);
    EXPECT_TRUE(entries.empty());
  }
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc

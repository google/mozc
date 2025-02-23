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

#include "composer/internal/composition.h"

#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "composer/internal/char_chunk.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/transliterators.h"
#include "composer/table.h"
#include "testing/gunit.h"

namespace mozc {
namespace composer {
namespace {

std::string GetRawString(const Composition& composition) {
  return composition.GetStringWithTransliterator(Transliterators::RAW_STRING);
}

size_t InsertCharacters(const std::string& input, size_t pos,
                        Composition& composition) {
  for (size_t i = 0; i < input.size(); ++i) {
    pos = composition.InsertAt(pos, input.substr(i, 1));
  }
  return pos;
}

}  // namespace

class CompositionTest : public testing::Test {
 protected:
  CompositionTest() : table_(std::make_shared<Table>()), composition_(table_) {
    composition_.SetInputMode(Transliterators::CONVERSION_STRING);
  }

  void SetInput(const absl::string_view raw, std::string conversion,
                const bool is_new_input, CompositionInput* input) {
    input->Clear();
    input->set_raw(table_->ParseSpecialKey(raw));
    if (!conversion.empty()) {
      input->set_conversion(std::move(conversion));
    }
    input->set_is_new_input(is_new_input);
  }

  std::shared_ptr<Table> table_;
  Composition composition_;
};

static int InitComposition(Composition& comp) {
  static const struct TestCharChunk {
    const char* conversion;
    const char* pending;
    const char* raw;
  } test_chunks[] = {
      // "あ ky き った っty"  (9 chars)
      // a ky ki tta tty
      {"あ", "", "a"},     {"", "ky", "ky"},    {"き", "", "ki"},
      {"った", "", "tta"}, {"っ", "ty", "tty"},
  };
  static const int test_chunks_size = std::size(test_chunks);
  CharChunkList::iterator it = comp.MaybeSplitChunkAt(0);
  for (int i = 0; i < test_chunks_size; ++i) {
    const TestCharChunk& data = test_chunks[i];
    CharChunk& chunk = *comp.InsertChunk(it);
    chunk.set_conversion(data.conversion);
    chunk.set_pending(data.pending);
    chunk.set_raw(data.raw);
  }
  return test_chunks_size;
}

static CharChunk& AppendChunk(const char* conversion, const char* pending,
                              const char* raw, Composition& comp) {
  CharChunkList::iterator it = comp.MaybeSplitChunkAt(comp.GetLength());

  CharChunk& chunk = *comp.InsertChunk(it);
  chunk.set_conversion(conversion);
  chunk.set_pending(pending);
  chunk.set_raw(raw);
  return chunk;
}

TEST_F(CompositionTest, GetChunkLength) {
  static const struct TestCase {
    const char* conversion;
    const char* pending;
    const char* raw;
    const int expected_conv_length;
    const int expected_raw_length;
  } test_cases[] = {
      {"あ", "", "a", 1, 1},     {"", "ky", "ky", 2, 2},
      {"き", "", "ki", 1, 2},    {"った", "", "tta", 2, 3},
      {"っ", "ty", "tty", 3, 3},
  };
  CharChunk& chunk = AppendChunk("", "", "", composition_);

  for (int i = 0; i < std::size(test_cases); ++i) {
    const TestCase& test = test_cases[i];
    chunk.set_conversion(test.conversion);
    chunk.set_pending(test.pending);
    chunk.set_raw(test.raw);

    const int conv_length = chunk.GetLength(Transliterators::CONVERSION_STRING);
    EXPECT_EQ(conv_length, test.expected_conv_length);

    const int raw_length = chunk.GetLength(Transliterators::RAW_STRING);
    EXPECT_EQ(raw_length, test.expected_raw_length);
  }
}

namespace {
bool TestGetChunkAt(Composition& comp,
                    Transliterators::Transliterator transliterator,
                    const size_t index, const size_t expected_index,
                    const size_t expected_inner_position) {
  auto expected_it = comp.GetCharChunkList().begin();
  for (int j = 0; j < expected_index; ++j, ++expected_it) {
  }

  size_t inner_position;
  auto it = comp.GetChunkAt(index, transliterator, &inner_position);
  if (it == comp.GetCharChunkList().end()) {
    EXPECT_TRUE(expected_it == it);
    EXPECT_EQ(inner_position, expected_inner_position);
  } else {
    std::string result, expected_result;
    EXPECT_EQ(it->conversion(), expected_it->conversion());
    EXPECT_EQ(it->pending(), expected_it->pending());
    EXPECT_EQ(it->raw(), expected_it->raw());
    EXPECT_EQ(inner_position, expected_inner_position);
  }
  return true;
}
}  // namespace

TEST_F(CompositionTest, GetChunkAt) {
  InitComposition(composition_);

  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 0, 0, 0);
  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 1, 0, 1);
  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 2, 1, 1);
  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 3, 1, 2);
  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 4, 2, 1);
  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 5, 3, 1);
  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 6, 3, 2);
  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 7, 4, 1);
  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 8, 4, 2);
  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 9, 4, 3);
  // end
  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 10, 4, 3);
  // end
  TestGetChunkAt(composition_, Transliterators::CONVERSION_STRING, 11, 4, 3);

  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 0, 0, 0);
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 1, 0, 1);
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 2, 1, 1);
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 3, 1, 2);
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 4, 2, 1);
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 5, 2, 2);
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 6, 3, 1);
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 7, 3, 2);
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 8, 3, 3);
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 9, 4, 1);
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 10, 4, 2);
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 11, 4, 3);
  // end
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 12, 4, 3);
  // end
  TestGetChunkAt(composition_, Transliterators::RAW_STRING, 13, 4, 3);
}

TEST_F(CompositionTest, GetString) {
  InitComposition(composition_);

  const size_t dummy_position = 0;

  // Test RAW mode
  composition_.SetDisplayMode(dummy_position, Transliterators::RAW_STRING);
  std::string composition = composition_.GetString();
  EXPECT_EQ(composition, "akykittatty");

  // Test CONVERSION mode
  composition_.SetDisplayMode(dummy_position,
                              Transliterators::CONVERSION_STRING);
  composition = composition_.GetString();
  EXPECT_EQ(composition, "あkyきったっty");
}

TEST_F(CompositionTest, GetStringWithDisplayMode) {
  AppendChunk("も", "", "mo", composition_);
  AppendChunk("ず", "", "z", composition_);
  AppendChunk("く", "", "c", composition_);

  std::string composition = composition_.GetStringWithTransliterator(
      Transliterators::CONVERSION_STRING);
  EXPECT_EQ(composition, "もずく");

  composition =
      composition_.GetStringWithTransliterator(Transliterators::RAW_STRING);
  EXPECT_EQ(composition, "mozc");
}

TEST_F(CompositionTest, SplitRawChunk) {
  static const struct TestCase {
    const char* conversion;
    const char* pending;
    const char* raw;
    const int position;
    const char* expected_left_conversion;
    const char* expected_left_pending;
    const char* expected_left_raw;
    const char* expected_right_conversion;
    const char* expected_right_pending;
    const char* expected_right_raw;
  } test_cases[] = {
      {"あ", "", "a", 0, "", "", "", "あ", "", "a"},
      {"", "ky", "ky", 1, "", "k", "k", "", "y", "y"},
      {"き", "", "ki", 1, "k", "", "k", "i", "", "i"},
      {"った", "", "tta", 1, "t", "", "t", "ta", "", "ta"},
      {"った", "", "tta", 2, "tt", "", "tt", "a", "", "a"},
      {"っ", "ty", "tty", 1, "っ", "", "t", "", "ty", "ty"},
      {"っ", "ty", "tty", 2, "っ", "t", "tt", "", "y", "y"},
      {"っ", "ty", "tty", 3, "", "", "", "っ", "ty", "tty"},
  };
  for (int i = 0; i < std::size(test_cases); ++i) {
    const TestCase& test = test_cases[i];
    CharChunk right_orig_chunk(Transliterators::CONVERSION_STRING, table_);
    right_orig_chunk.set_conversion(test.conversion);
    right_orig_chunk.set_pending(test.pending);
    right_orig_chunk.set_raw(test.raw);
    absl::StatusOr<CharChunk> left_new_chunk =
        right_orig_chunk.SplitChunk(Transliterators::RAW_STRING, test.position);

    if (left_new_chunk.ok()) {
      EXPECT_EQ(left_new_chunk->conversion(), test.expected_left_conversion);
      EXPECT_EQ(left_new_chunk->pending(), test.expected_left_pending);
      EXPECT_EQ(left_new_chunk->raw(), test.expected_left_raw);
    }

    EXPECT_EQ(right_orig_chunk.conversion(), test.expected_right_conversion);
    EXPECT_EQ(right_orig_chunk.pending(), test.expected_right_pending);
    EXPECT_EQ(right_orig_chunk.raw(), test.expected_right_raw);
  }
}

TEST_F(CompositionTest, SplitConversionChunk) {
  static const struct TestCase {
    const char* conversion;
    const char* pending;
    const char* raw;
    const int position;
    const char* expected_left_conversion;
    const char* expected_left_pending;
    const char* expected_left_raw;
    const char* expected_right_conversion;
    const char* expected_right_pending;
    const char* expected_right_raw;
  } test_cases[] = {
      {"あ", "", "a", 0, "", "", "", "あ", "", "a"},
      {"", "ky", "ky", 1, "", "k", "k", "", "y", "y"},
      {"きょ", "", "kyo", 1, "き", "", "き", "ょ", "", "ょ"},
      {"っ", "t", "tt", 1, "っ", "", "t", "", "t", "t"},
      {"った", "", "tta", 1, "っ", "", "っ", "た", "", "た"},
      {"っ", "ty", "tty", 1, "っ", "", "t", "", "ty", "ty"},
      {"っ", "ty", "tty", 2, "っ", "t", "tt", "", "y", "y"},
      {"っ", "ty", "tty", 3, "", "", "", "っ", "ty", "tty"},
  };
  for (int i = 0; i < std::size(test_cases); ++i) {
    const TestCase& test = test_cases[i];
    CharChunk right_orig_chunk(Transliterators::CONVERSION_STRING, table_);
    right_orig_chunk.set_conversion(test.conversion);
    right_orig_chunk.set_pending(test.pending);
    right_orig_chunk.set_raw(test.raw);
    absl::StatusOr<CharChunk> left_new_chunk = right_orig_chunk.SplitChunk(
        Transliterators::CONVERSION_STRING, test.position);

    if (left_new_chunk.ok()) {
      EXPECT_EQ(left_new_chunk->conversion(), test.expected_left_conversion);
      EXPECT_EQ(left_new_chunk->pending(), test.expected_left_pending);
      EXPECT_EQ(left_new_chunk->raw(), test.expected_left_raw);
    }

    EXPECT_EQ(right_orig_chunk.conversion(), test.expected_right_conversion);
    EXPECT_EQ(right_orig_chunk.pending(), test.expected_right_pending);
    EXPECT_EQ(right_orig_chunk.raw(), test.expected_right_raw);
  }
}

TEST_F(CompositionTest, GetLength) {
  table_->AddRule("a", "A", "");
  table_->AddRule("ka", "K", "");

  EXPECT_EQ(composition_.GetLength(), 0);

  InsertCharacters("aka", 0, composition_);
  EXPECT_EQ(composition_.GetLength(), 2);
}

TEST_F(CompositionTest, MaybeSplitChunkAt) {
  static const struct TestCase {
    const int position;
    const int expected_raw_chunks_size;
    const int expected_conv_chunks_size;
  } test_cases[] = {
      // "あ ky き った っty"  (9 chars)
      // a ky ki tta tty (11 chars)
      {0, 5, 5},  {1, 5, 5},  {2, 6, 6},  {3, 5, 5}, {4, 6, 5},
      {5, 5, 6},  {6, 6, 5},  {7, 6, 6},  {8, 5, 6}, {9, 6, 5},
      {10, 6, 5}, {11, 5, 5}, {12, 5, 5},
  };
  const size_t dummy_position = 0;
  for (int i = 0; i < std::size(test_cases); ++i) {
    const TestCase& test = test_cases[i];

    {  // Test RAW mode
      Composition raw_comp(table_);
      InitComposition(raw_comp);
      raw_comp.SetDisplayMode(dummy_position, Transliterators::RAW_STRING);
      raw_comp.MaybeSplitChunkAt(test.position);
      const size_t raw_chunks_size = raw_comp.GetCharChunkList().size();
      EXPECT_EQ(raw_chunks_size, test.expected_raw_chunks_size);
    }

    {  // Test CONVERSION mode
      Composition conv_comp(table_);
      InitComposition(conv_comp);
      conv_comp.SetDisplayMode(dummy_position,
                               Transliterators::CONVERSION_STRING);
      conv_comp.MaybeSplitChunkAt(test.position);
      const size_t conv_chunks_size = conv_comp.GetCharChunkList().size();
      EXPECT_EQ(conv_chunks_size, test.expected_conv_chunks_size);
    }
  }
}

namespace {
std::string GetDeletedString(Transliterators::Transliterator t12r,
                             const int position) {
  auto table = std::make_shared<Table>();
  Composition comp(table);

  InitComposition(comp);
  comp.SetDisplayMode(0, t12r);
  comp.DeleteAt(position);
  return comp.GetString();
}
}  // namespace

TEST_F(CompositionTest, DeleteAt) {
  // "あkyきったっty" is the original string
  EXPECT_EQ(GetDeletedString(Transliterators::CONVERSION_STRING, 0),
            "kyきったっty");
  EXPECT_EQ(GetDeletedString(Transliterators::CONVERSION_STRING, 1),
            "あyきったっty");
  EXPECT_EQ(GetDeletedString(Transliterators::CONVERSION_STRING, 2),
            "あkきったっty");
  EXPECT_EQ(GetDeletedString(Transliterators::CONVERSION_STRING, 3),
            "あkyったっty");
  EXPECT_EQ(GetDeletedString(Transliterators::CONVERSION_STRING, 4),
            "あkyきたっty");
  EXPECT_EQ(GetDeletedString(Transliterators::CONVERSION_STRING, 5),
            "あkyきっっty");
  EXPECT_EQ(GetDeletedString(Transliterators::CONVERSION_STRING, 6),
            "あkyきったty");
  EXPECT_EQ(GetDeletedString(Transliterators::CONVERSION_STRING, 7),
            "あkyきったっy");
  EXPECT_EQ(GetDeletedString(Transliterators::CONVERSION_STRING, 8),
            "あkyきったっt");
  // end
  EXPECT_EQ(GetDeletedString(Transliterators::CONVERSION_STRING, 9),
            "あkyきったっty");
  EXPECT_EQ(GetDeletedString(Transliterators::CONVERSION_STRING, -1),
            "あkyきったっty");

  // "akykittatty" is the original string
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 0), "kykittatty");
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 1), "aykittatty");
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 2), "akkittatty");
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 3), "akyittatty");
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 4), "akykttatty");
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 5), "akykitatty");
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 6), "akykitatty");
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 7), "akykitttty");
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 8), "akykittaty");
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 9), "akykittaty");
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 10), "akykittatt");
  // end
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, 11), "akykittatty");
  EXPECT_EQ(GetDeletedString(Transliterators::RAW_STRING, -1), "akykittatty");
}

TEST_F(CompositionTest, DeleteAtInvisibleCharacter) {
  using ChunkData = std::vector<std::pair<std::string, std::string>>;
  auto init_chunk = [&](Composition& composition, const ChunkData& data) {
    composition.Erase();
    CharChunkList::iterator it = composition.MaybeSplitChunkAt(0);
    for (const auto& item : data) {
      CharChunk& chunk = *composition.InsertChunk(it);
      chunk.set_raw(table_->ParseSpecialKey(item.first));
      chunk.set_pending(table_->ParseSpecialKey(item.second));
    }
  };

  {
    init_chunk(composition_, {{"1", "{1}"}, {"2", "{2}2"}, {"3", "3"}});

    // Now the CharChunks in the comp are expected to be following;
    // (raw, pending) = [ ("1", "{1}"), ("2", "{2}2"), ("3", "3") ]
    // {} means invisible characters.

    composition_.DeleteAt(0);
    const std::string composition = composition_.GetString();
    EXPECT_EQ(composition, "3");
    EXPECT_EQ(composition_.GetCharChunkList().size(), 1);
  }
  {
    init_chunk(composition_, {{"1", "{1}"}, {"2", "{2}2"}, {"3", "3"}});

    composition_.DeleteAt(1);
    const std::string composition = composition_.GetString();
    EXPECT_EQ(composition, "2");
    const CharChunkList& chunks = composition_.GetCharChunkList();
    EXPECT_EQ(chunks.size(), 2);
    const CharChunk& chunk0 = *(chunks.begin());
    EXPECT_EQ(chunk0.raw(), "1");
    EXPECT_EQ(chunk0.pending(), table_->ParseSpecialKey("{1}"));
    const CharChunk& chunk1 = *(std::next(chunks.begin()));
    EXPECT_EQ(chunk1.raw(), "2");
    EXPECT_EQ(chunk1.pending(), table_->ParseSpecialKey("{2}2"));
  }

  {
    init_chunk(composition_, {{"1", "{1}"}, {"2", "2{2}"}, {"3", "3"}});

    composition_.DeleteAt(0);
    const std::string composition = composition_.GetString();
    // 0-th character is "2".
    // As "{1}" is a character between the head and "2", it is removed.
    // All "2{2}" is also removed because they are in the same chunk.
    EXPECT_EQ(composition, "3");
    EXPECT_EQ(composition_.GetCharChunkList().size(), 1);
  }

  {
    init_chunk(composition_, {{"1", "{1}"}, {"2", "2{2}"}, {"3", "{3}"}});

    composition_.DeleteAt(0);
    const std::string composition = composition_.GetString();
    EXPECT_EQ(composition, "");
    EXPECT_EQ(composition_.GetCharChunkList().size(), 1);
    EXPECT_EQ(composition_.GetCharChunkList().begin()->raw(), "3");
  }
}

namespace {

void InitTable(Table* table) {
  table->AddRule("i", "い", "");
  table->AddRule("ki", "き", "");
  table->AddRule("kyi", "きぃ", "");
  table->AddRule("ti", "ち", "");
  table->AddRule("tya", "ちゃ", "");
  table->AddRule("tyi", "ちぃ", "");
  table->AddRule("ya", "や", "");
  table->AddRule("yy", "っ", "y");
}

std::string GetInsertedString(Transliterators::Transliterator t12r,
                              const size_t position, std::string input) {
  auto table = std::make_shared<Table>();
  InitTable(table.get());
  Composition comp(table);
  InitComposition(comp);

  comp.SetTable(table);
  comp.SetDisplayMode(0, t12r);
  comp.InsertAt(position, std::move(input));

  return comp.GetString();
}
}  // namespace

TEST_F(CompositionTest, InsertAt) {
  // "あkyきったっty" is the original string
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 0, "i"),
            "いあkyきったっty");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 1, "i"),
            "あいkyきったっty");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 2, "i"),
            "あきyきったっty");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 3, "i"),
            "あきぃきったっty");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 4, "i"),
            "あkyきいったっty");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 5, "i"),
            "あkyきっいたっty");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 9, "i"),
            "あkyきったっちぃ");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 0, "y"),
            "yあkyきったっty");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 1, "y"),
            "あykyきったっty");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 2, "y"),
            "あkyyきったっty");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 3, "y"),
            "あkyyきったっty");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 4, "y"),
            "あkyきyったっty");
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 5, "y"),
            "あkyきっyたっty");
  // end
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 9, "i"),
            "あkyきったっちぃ");
  // end
  EXPECT_EQ(GetInsertedString(Transliterators::CONVERSION_STRING, 9, "y"),
            "あkyきったっtyy");

  // "akykittatty" is the original string
  EXPECT_EQ(GetInsertedString(Transliterators::RAW_STRING, 0, "i"),
            "iakykittatty");

  EXPECT_EQ(GetInsertedString(Transliterators::RAW_STRING, 1, "i"),
            "aikykittatty");

  EXPECT_EQ(GetInsertedString(Transliterators::RAW_STRING, 2, "i"),
            "akiykittatty");

  EXPECT_EQ(GetInsertedString(Transliterators::RAW_STRING, 3, "i"),
            "akyikittatty");

  EXPECT_EQ(GetInsertedString(Transliterators::RAW_STRING, 4, "i"),
            "akykiittatty");

  EXPECT_EQ(GetInsertedString(Transliterators::RAW_STRING, 5, "i"),
            "akykiittatty");

  EXPECT_EQ(GetInsertedString(Transliterators::RAW_STRING, 11, "i"),
            "akykittattyi");  // end
}

TEST_F(CompositionTest, GetExpandedStrings) {
  InitTable(table_.get());
  InitComposition(composition_);

  // a ky ki tta tty
  // auto = std::pair<std::string, absl::btree_set<std::string>>
  const auto [base, expanded] = composition_.GetExpandedStrings();
  EXPECT_EQ(base, "あkyきったっ");
  EXPECT_EQ(expanded.size(), 2);
  // You cannot use EXPECT_NE here because it causes compile error in gtest
  // when the compiler is Visual C++. b/5655673
  EXPECT_TRUE(expanded.find("ちぃ") != expanded.end());
  // You cannot use EXPECT_NE here because it causes compile error in gtest
  // when the compiler is Visual C++. b/5655673
  EXPECT_TRUE(expanded.find("ちゃ") != expanded.end());
}

TEST_F(CompositionTest, ConvertPosition) {
  // Test against http://b/1550597

  // Invalid positions.
  EXPECT_EQ(composition_.ConvertPosition(static_cast<size_t>(-1),
                                         Transliterators::CONVERSION_STRING,
                                         Transliterators::RAW_STRING),
            0);
  EXPECT_EQ(composition_.ConvertPosition(0, Transliterators::CONVERSION_STRING,
                                         Transliterators::RAW_STRING),
            0);
  EXPECT_EQ(composition_.ConvertPosition(1, Transliterators::CONVERSION_STRING,
                                         Transliterators::RAW_STRING),
            0);
  EXPECT_EQ(composition_.ConvertPosition(0, Transliterators::RAW_STRING,
                                         Transliterators::CONVERSION_STRING),
            0);
  EXPECT_EQ(composition_.ConvertPosition(static_cast<size_t>(-1),
                                         Transliterators::RAW_STRING,
                                         Transliterators::CONVERSION_STRING),
            0);
  EXPECT_EQ(composition_.ConvertPosition(1, Transliterators::RAW_STRING,
                                         Transliterators::CONVERSION_STRING),
            0);

  AppendChunk("ね", "", "ne", composition_);
  AppendChunk("っと", "", "tto", composition_);

  // "|ねっと" -> "|netto"
  EXPECT_EQ(composition_.ConvertPosition(0, Transliterators::CONVERSION_STRING,
                                         Transliterators::RAW_STRING),
            0);
  // "ね|っと" -> "ne|tto"
  EXPECT_EQ(composition_.ConvertPosition(1, Transliterators::CONVERSION_STRING,
                                         Transliterators::RAW_STRING),
            2);
  // "ねっ|と" -> "net|to"
  EXPECT_EQ(composition_.ConvertPosition(2, Transliterators::CONVERSION_STRING,
                                         Transliterators::RAW_STRING),
            3);
  // "ねっと|" -> "netto|"
  EXPECT_EQ(composition_.ConvertPosition(3, Transliterators::CONVERSION_STRING,
                                         Transliterators::RAW_STRING),
            5);

  // Invalid positions.
  EXPECT_EQ(composition_.ConvertPosition(static_cast<size_t>(-1),
                                         Transliterators::CONVERSION_STRING,
                                         Transliterators::RAW_STRING),
            5);
  EXPECT_EQ(composition_.ConvertPosition(4, Transliterators::CONVERSION_STRING,
                                         Transliterators::RAW_STRING),
            5);

  // "|netto" -> "|ねっと"
  EXPECT_EQ(composition_.ConvertPosition(0, Transliterators::RAW_STRING,
                                         Transliterators::CONVERSION_STRING),
            0);
  // "n|etto" -> "ね|っと"
  EXPECT_EQ(composition_.ConvertPosition(1, Transliterators::RAW_STRING,
                                         Transliterators::CONVERSION_STRING),
            1);
  // "ne|tto" -> "ね|っと"
  EXPECT_EQ(composition_.ConvertPosition(2, Transliterators::RAW_STRING,
                                         Transliterators::CONVERSION_STRING),
            1);
  // "net|to" -> "ねっ|と"
  EXPECT_EQ(composition_.ConvertPosition(3, Transliterators::RAW_STRING,
                                         Transliterators::CONVERSION_STRING),
            2);
  // "nett|o" -> "ねっと|"
  EXPECT_EQ(composition_.ConvertPosition(4, Transliterators::RAW_STRING,
                                         Transliterators::CONVERSION_STRING),
            3);
  // "netto|" -> "ねっと|"
  EXPECT_EQ(composition_.ConvertPosition(5, Transliterators::RAW_STRING,
                                         Transliterators::CONVERSION_STRING),
            3);
  // Invalid positions.
  EXPECT_EQ(composition_.ConvertPosition(static_cast<size_t>(-1),
                                         Transliterators::RAW_STRING,
                                         Transliterators::CONVERSION_STRING),
            3);
  EXPECT_EQ(composition_.ConvertPosition(6, Transliterators::RAW_STRING,
                                         Transliterators::CONVERSION_STRING),
            3);

  size_t inner_position;
  auto chunk_it =
      composition_.GetChunkAt(5, Transliterators::RAW_STRING, &inner_position);

  EXPECT_EQ(chunk_it->raw(), "tto");
  EXPECT_EQ(inner_position, 3);
}

TEST_F(CompositionTest, SetDisplayMode) {
  AppendChunk("も", "", "mo", composition_);
  AppendChunk("ず", "", "zu", composition_);
  AppendChunk("く", "", "ku", composition_);

  size_t inner_position;
  auto chunk_it = composition_.GetChunkAt(0, Transliterators::CONVERSION_STRING,
                                          &inner_position);
  EXPECT_EQ(chunk_it->raw(), "mo");
  EXPECT_EQ(inner_position, 0);
  chunk_it = composition_.GetChunkAt(1, Transliterators::CONVERSION_STRING,
                                     &inner_position);
  EXPECT_EQ(chunk_it->raw(), "mo");
  EXPECT_EQ(inner_position, 1);
  chunk_it = composition_.GetChunkAt(2, Transliterators::CONVERSION_STRING,
                                     &inner_position);
  EXPECT_EQ(chunk_it->raw(), "zu");
  EXPECT_EQ(inner_position, 1);
  chunk_it = composition_.GetChunkAt(3, Transliterators::CONVERSION_STRING,
                                     &inner_position);
  EXPECT_EQ(chunk_it->raw(), "ku");
  EXPECT_EQ(inner_position, 1);

  EXPECT_EQ(composition_.SetDisplayMode(1, Transliterators::RAW_STRING), 6);
  EXPECT_EQ(composition_.SetDisplayMode(2, Transliterators::CONVERSION_STRING),
            3);
  EXPECT_EQ(composition_.SetDisplayMode(2, Transliterators::RAW_STRING), 6);
}

TEST_F(CompositionTest, GetStringWithTrimMode) {
  table_->AddRule("ka", "か", "");
  table_->AddRule("n", "ん", "");
  // This makes the above rule ambiguous.
  table_->AddRule("na", "な", "");

  std::string output_empty = composition_.GetStringWithTrimMode(TRIM);
  EXPECT_TRUE(output_empty.empty());

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "k");
  pos = composition_.InsertAt(pos, "a");
  pos = composition_.InsertAt(pos, "n");

  const std::string output_trim = composition_.GetStringWithTrimMode(TRIM);
  EXPECT_EQ(output_trim, "か");

  const std::string output_asis = composition_.GetStringWithTrimMode(ASIS);
  EXPECT_EQ(output_asis, "かn");

  const std::string output_fix = composition_.GetStringWithTrimMode(FIX);
  EXPECT_EQ(output_fix, "かん");
}

TEST_F(CompositionTest, InsertKeyAndPreeditAt) {
  table_->AddRule("す゛", "ず", "");
  table_->AddRule("く゛", "ぐ", "");

  size_t pos = 0;
  pos = composition_.InsertKeyAndPreeditAt(pos, "m", "も");
  pos = composition_.InsertKeyAndPreeditAt(pos, "r", "す");
  pos = composition_.InsertKeyAndPreeditAt(pos, "@", "゛");
  pos = composition_.InsertKeyAndPreeditAt(pos, "h", "く");
  pos = composition_.InsertKeyAndPreeditAt(pos, "!", "!");

  const std::string comp_str = composition_.GetString();
  EXPECT_EQ(comp_str, "もずく!");

  const std::string comp_ascii_str =
      composition_.GetStringWithTransliterator(Transliterators::RAW_STRING);
  EXPECT_EQ(comp_ascii_str, "mr@h!");
}

TEST_F(CompositionTest, InsertKeyForN) {
  table_->AddRule("a", "[A]", "");
  table_->AddRule("n", "[N]", "");
  table_->AddRule("nn", "[N]", "");
  table_->AddRule("na", "[NA]", "");
  table_->AddRule("nya", "[NYA]", "");
  table_->AddRule("ya", "[YA]", "");
  table_->AddRule("ka", "[KA]", "");

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "n");
  pos = composition_.InsertAt(pos, "y");
  pos = composition_.InsertAt(pos, "n");
  pos = composition_.InsertAt(pos, "y");
  pos = composition_.InsertAt(pos, "a");

  const std::string comp_str = composition_.GetString();
  EXPECT_EQ(comp_str, "ny[NYA]");
}

TEST_F(CompositionTest, GetStringWithDisplayModeForKana) {
  size_t pos = 0;
  pos = composition_.InsertKeyAndPreeditAt(pos, "m", "も");

  const std::string comp_str =
      composition_.GetStringWithTransliterator(Transliterators::RAW_STRING);
  EXPECT_EQ(comp_str, "m");
}

TEST_F(CompositionTest, InputMode) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("ka", "か", "");

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "k");

  std::string result = composition_.GetString();
  EXPECT_EQ(result, "k");

  composition_.SetInputMode(Transliterators::FULL_KATAKANA);
  pos = composition_.InsertAt(pos, "a");
  result = composition_.GetString();
  // If a vowel and a consonant were typed with different
  // transliterators, these characters should not be combined.
  EXPECT_EQ(result, "kア");

  composition_.SetInputMode(Transliterators::HALF_ASCII);
  pos = composition_.InsertAt(pos, "k");
  result = composition_.GetString();
  EXPECT_EQ(result, "kアk");

  composition_.SetInputMode(Transliterators::HIRAGANA);
  pos = composition_.InsertAt(pos, "a");
  result = composition_.GetString();
  EXPECT_EQ(result, "kアkあ");

  EXPECT_EQ(composition_.GetTransliterator(0),
            Transliterators::CONVERSION_STRING);
  EXPECT_EQ(composition_.GetTransliterator(1),
            Transliterators::CONVERSION_STRING);
  EXPECT_EQ(composition_.GetTransliterator(2), Transliterators::FULL_KATAKANA);
  EXPECT_EQ(composition_.GetTransliterator(3), Transliterators::HALF_ASCII);
  EXPECT_EQ(composition_.GetTransliterator(4), Transliterators::HIRAGANA);
  EXPECT_EQ(composition_.GetTransliterator(5), Transliterators::HIRAGANA);
  EXPECT_EQ(composition_.GetTransliterator(10), Transliterators::HIRAGANA);
}

TEST_F(CompositionTest, SetTable) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("ka", "か", "");

  auto table2 = std::make_shared<Table>();
  table2->AddRule("a", "あ", "");
  table2->AddRule("ka", "か", "");

  composition_.SetInputMode(Transliterators::HIRAGANA);

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "k");

  std::string result = composition_.GetString();
  EXPECT_EQ(result, "ｋ");

  composition_.SetTable(table2);

  pos = composition_.InsertAt(pos, "a");
  result = composition_.GetString();
  EXPECT_EQ(result, "ｋあ");
}

TEST_F(CompositionTest, Transliterator) {
  table_->AddRule("a", "あ", "");

  // Insert "a" which is converted to "あ".
  size_t pos = 0;
  pos = composition_.InsertAt(pos, "a");
  EXPECT_EQ(pos, 1);
  std::string result = composition_.GetString();
  EXPECT_EQ(result, "あ");

  // Set transliterator for Half Ascii.
  composition_.SetTransliterator(0, pos, Transliterators::HALF_ASCII);
  result = composition_.GetString();
  EXPECT_EQ(result, "a");

  // Insert "a" again.
  pos = composition_.InsertAt(pos, "a");
  EXPECT_EQ(pos, 2);
  result = composition_.GetString();
  EXPECT_EQ(result, "aあ");

  // Set transliterator for Full Katakana.
  composition_.SetTransliterator(0, pos, Transliterators::FULL_KATAKANA);
  result = composition_.GetString();
  EXPECT_EQ(result, "アア");
}

TEST_F(CompositionTest, HalfAsciiTransliterator) {
  table_->AddRule("-", "ー", "");
  composition_.SetInputMode(Transliterators::HALF_ASCII);

  size_t pos = 0;
  pos = composition_.InsertKeyAndPreeditAt(pos, "-", "-");
  EXPECT_EQ(pos, 1);
  EXPECT_EQ(composition_.GetString(), "-");

  pos = composition_.InsertKeyAndPreeditAt(pos, "-", "-");
  EXPECT_EQ(pos, 2);
  EXPECT_EQ(composition_.GetString(), "--");
}

TEST_F(CompositionTest, ShouldCommit) {
  table_->AddRuleWithAttributes("ka", "[KA]", "", DIRECT_INPUT);
  table_->AddRuleWithAttributes("tt", "[X]", "t", DIRECT_INPUT);
  table_->AddRuleWithAttributes("ta", "[TA]", "", NO_TABLE_ATTRIBUTE);

  size_t pos = 0;

  pos = composition_.InsertAt(pos, "k");
  EXPECT_FALSE(composition_.ShouldCommit());

  pos = composition_.InsertAt(pos, "a");
  EXPECT_TRUE(composition_.ShouldCommit());

  pos = composition_.InsertAt(pos, "t");
  EXPECT_FALSE(composition_.ShouldCommit());

  pos = composition_.InsertAt(pos, "t");
  EXPECT_FALSE(composition_.ShouldCommit());

  pos = composition_.InsertAt(pos, "a");
  EXPECT_TRUE(composition_.ShouldCommit());

  pos = composition_.InsertAt(pos, "t");
  EXPECT_FALSE(composition_.ShouldCommit());

  pos = composition_.InsertAt(pos, "a");
  EXPECT_FALSE(composition_.ShouldCommit());
  EXPECT_EQ(composition_.GetString(), "[KA][X][TA][TA]");
}

TEST_F(CompositionTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  size_t pos = 0;

  composition_.SetInputMode(Transliterators::FULL_ASCII);
  pos = composition_.InsertKeyAndPreeditAt(pos, "a", "ち");
  EXPECT_EQ(composition_.GetString(), "ａ");

  pos = composition_.InsertAt(pos, " ");
  EXPECT_EQ(composition_.GetString(), "ａ　");
}

TEST_F(CompositionTest, Issue1817410) {
  // This is a unittest against http://b/2190364
  table_->AddRule("ss", "っ", "s");

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "s");
  pos = composition_.InsertAt(pos, "s");

  const std::string preedit = composition_.GetString();
  EXPECT_EQ(preedit, "っs");

  EXPECT_EQ(composition_.ConvertPosition(0, Transliterators::LOCAL,
                                         Transliterators::HALF_ASCII),
            0);
  EXPECT_EQ(composition_.ConvertPosition(1, Transliterators::LOCAL,
                                         Transliterators::HALF_ASCII),
            1);
  EXPECT_EQ(composition_.ConvertPosition(2, Transliterators::LOCAL,
                                         Transliterators::HALF_ASCII),
            2);

  {  // "s|s"
    size_t inner_position;
    auto chunk_it =
        composition_.GetChunkAt(1, Transliterators::LOCAL, &inner_position);
    EXPECT_EQ(inner_position, 1);
    EXPECT_EQ(chunk_it->GetLength(Transliterators::LOCAL), 2);

    EXPECT_EQ(composition_.GetPosition(Transliterators::HALF_ASCII, chunk_it),
              0);
    EXPECT_EQ(chunk_it->GetLength(Transliterators::HALF_ASCII), 2);
  }

  {  // "ss|"
    size_t inner_position;
    auto chunk_it =
        composition_.GetChunkAt(2, Transliterators::LOCAL, &inner_position);
    EXPECT_EQ(inner_position, 2);
    EXPECT_EQ(chunk_it->GetLength(Transliterators::LOCAL), 2);

    EXPECT_EQ(composition_.GetPosition(Transliterators::HALF_ASCII, chunk_it),
              0);
    EXPECT_EQ(chunk_it->GetLength(Transliterators::HALF_ASCII), 2);
  }
}

TEST_F(CompositionTest, Issue2209634) {
  // This is a unittest against http://b/2209634
  // "q@" becomes "qた@".
  table_->AddRule("q", "", "た");
  table_->AddRule("た@", "だ", "");

  composition_.SetInputMode(Transliterators::HALF_ASCII);

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "q");
  pos = composition_.InsertAt(pos, "@");

  const std::string preedit = composition_.GetString();
  EXPECT_EQ(preedit, "q@");
}

TEST_F(CompositionTest, Issue2330530) {
  // This is a unittest against http://b/2330530
  // "Win" + Numpad7 becomes "Win77" instead of "Win7".
  table_->AddRule("wi", "うぃ", "");
  table_->AddRule("i", "い", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");

  composition_.SetInputMode(Transliterators::HALF_ASCII);

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "W");
  pos = composition_.InsertAt(pos, "i");
  pos = composition_.InsertAt(pos, "n");

  std::string preedit = composition_.GetString();
  EXPECT_EQ(preedit, "Win");

  pos = composition_.InsertKeyAndPreeditAt(pos, "7", "7");
  preedit = composition_.GetString();
  EXPECT_EQ(preedit, "Win7");
}

TEST_F(CompositionTest, Issue2819580) {
  // This is a unittest against http://b/2819580.
  // 'y' after 'n' disappears.
  table_->AddRule("po", "ぽ", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");
  table_->AddRule("ya", "や", "");
  table_->AddRule("nya", "にゃ", "");
  table_->AddRule("byo", "びょ", "");

  composition_.SetInputMode(Transliterators::HIRAGANA);

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "n");
  pos = composition_.InsertAt(pos, "y");
  {
    std::string output = composition_.GetStringWithTrimMode(FIX);
    EXPECT_EQ(output, "ｎｙ");

    output = composition_.GetStringWithTrimMode(ASIS);
    EXPECT_EQ(output, "ｎｙ");

    output = composition_.GetStringWithTrimMode(TRIM);
    EXPECT_EQ(output, "");
  }
}

TEST_F(CompositionTest, Issue2990253) {
  // SplitChunk fails.
  // Ambiguous text is left in rhs CharChunk invalidly.
  table_->AddRule("po", "ぽ", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");
  table_->AddRule("ya", "や", "");
  table_->AddRule("nya", "にゃ", "");
  table_->AddRule("byo", "びょ", "");

  composition_.SetInputMode(Transliterators::HIRAGANA);

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "n");
  pos = composition_.InsertAt(pos, "y");
  pos = 1;
  pos = composition_.InsertAt(pos, "b");
  {
    std::string output = composition_.GetStringWithTrimMode(FIX);
    EXPECT_EQ(output, "んｂｙ");

    output = composition_.GetStringWithTrimMode(ASIS);
    EXPECT_EQ(output, "んｂｙ");

    output = composition_.GetStringWithTrimMode(TRIM);
    // doubtful result. should be "ん"
    // May relate to http://b/2990358
    EXPECT_EQ(output, "んｂ");
  }
}

TEST_F(CompositionTest, InsertionIntoPreeditMakesInvalidText1) {
  // http://b/2990358
  // Test for mainly Composition::InsertAt()
  table_->AddRule("po", "ぽ", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");
  table_->AddRule("ya", "や", "");
  table_->AddRule("nya", "にゃ", "");
  table_->AddRule("byo", "びょ", "");

  composition_.SetInputMode(Transliterators::HIRAGANA);

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "n");
  pos = composition_.InsertAt(pos, "y");
  pos = 1;
  pos = composition_.InsertAt(pos, "b");
  pos = 3;
  pos = composition_.InsertAt(pos, "o");
  {
    std::string output = composition_.GetStringWithTrimMode(FIX);
    EXPECT_EQ(output, "んびょ");

    output = composition_.GetStringWithTrimMode(ASIS);
    EXPECT_EQ(output, "んびょ");

    output = composition_.GetStringWithTrimMode(TRIM);
    EXPECT_EQ(output, "んびょ");
  }
}

TEST_F(CompositionTest, InsertionIntoPreeditMakesInvalidText2) {
  // http://b/2990358
  // Test for mainly Composition::InsertKeyAndPreeditAt()

  table_->AddRule("す゛", "ず", "");
  table_->AddRule("く゛", "ぐ", "");

  size_t pos = 0;
  pos = composition_.InsertKeyAndPreeditAt(pos, "m", "も");
  pos = composition_.InsertKeyAndPreeditAt(pos, "r", "す");
  pos = composition_.InsertKeyAndPreeditAt(pos, "h", "く");
  pos = composition_.InsertKeyAndPreeditAt(2, "@", "゛");
  pos = composition_.InsertKeyAndPreeditAt(5, "!", "!");

  const std::string comp_str = composition_.GetString();
  EXPECT_EQ(comp_str, "もずく!");

  const std::string comp_ascii_str =
      composition_.GetStringWithTransliterator(Transliterators::RAW_STRING);
  EXPECT_EQ(comp_ascii_str, "mr@h!");
}

TEST_F(CompositionTest, CombinePendingChunks) {
  table_->AddRule("po", "ぽ", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");
  table_->AddRule("ya", "や", "");
  table_->AddRule("nya", "にゃ", "");
  table_->AddRule("byo", "びょ", "");

  {
    // empty chunks + "n" -> empty chunks + "n"
    Composition comp(table_);
    comp.SetInputMode(Transliterators::HIRAGANA);

    size_t pos = 0;

    CharChunkList::iterator it = comp.MaybeSplitChunkAt(pos);
    CharChunkList::iterator chunk_it = comp.GetInsertionChunk(it);

    CompositionInput input;
    SetInput("n", "", false, &input);
    comp.CombinePendingChunks(chunk_it, input);
    EXPECT_EQ(chunk_it->pending(), "");
    EXPECT_EQ(chunk_it->conversion(), "");
    EXPECT_EQ(chunk_it->raw(), "");
    EXPECT_EQ(chunk_it->ambiguous(), "");
  }
  {
    // [x] + "n" -> [x] + "n"
    // No combination performed.
    Composition comp(table_);
    comp.SetInputMode(Transliterators::HIRAGANA);

    size_t pos = 0;
    pos = comp.InsertAt(pos, "x");

    CharChunkList::iterator it = comp.MaybeSplitChunkAt(pos);
    CharChunkList::iterator chunk_it = comp.GetInsertionChunk(it);
    CompositionInput input;
    SetInput("n", "", false, &input);

    comp.CombinePendingChunks(chunk_it, input);
    EXPECT_EQ(chunk_it->pending(), "");
    EXPECT_EQ(chunk_it->conversion(), "");
    EXPECT_EQ(chunk_it->raw(), "");
    EXPECT_EQ(chunk_it->ambiguous(), "");
  }
  {
    // Append "a" to [n][y] -> [ny] + "a"
    // Combination performed.
    Composition comp(table_);
    comp.SetInputMode(Transliterators::HIRAGANA);

    size_t pos = 0;
    pos = comp.InsertAt(pos, "y");
    pos = comp.InsertAt(0, "n");

    CharChunkList::iterator it = comp.MaybeSplitChunkAt(2);
    CharChunkList::iterator chunk_it = comp.GetInsertionChunk(it);
    CompositionInput input;
    SetInput("a", "", false, &input);

    comp.CombinePendingChunks(chunk_it, input);
    EXPECT_EQ(chunk_it->pending(), "ny");
    EXPECT_EQ(chunk_it->conversion(), "");
    EXPECT_EQ(chunk_it->raw(), "ny");
    EXPECT_EQ(chunk_it->ambiguous(), "んy");
  }
  {
    // Append "a" to [x][n][y] -> [x][ny] + "a"
    // Combination performed.
    Composition comp(table_);
    comp.SetInputMode(Transliterators::HIRAGANA);

    size_t pos = 0;
    pos = comp.InsertAt(pos, "x");
    pos = comp.InsertAt(pos, "y");
    pos = comp.InsertAt(1, "n");

    CharChunkList::iterator it = comp.MaybeSplitChunkAt(3);
    CharChunkList::iterator chunk_it = comp.GetInsertionChunk(it);
    CompositionInput input;
    SetInput("a", "", false, &input);

    comp.CombinePendingChunks(chunk_it, input);
    EXPECT_EQ(chunk_it->pending(), "ny");
    EXPECT_EQ(chunk_it->conversion(), "");
    EXPECT_EQ(chunk_it->raw(), "ny");
    EXPECT_EQ(chunk_it->ambiguous(), "んy");
  }

  {
    // Append "a" of conversion value to [x][n][y] -> [x][ny] + "a"
    // Combination performed.  If composition input contains a
    // conversion, the conversion is used rather than a raw value.
    Composition comp(table_);
    comp.SetInputMode(Transliterators::HIRAGANA);

    size_t pos = 0;
    pos = comp.InsertAt(pos, "x");
    pos = comp.InsertAt(pos, "y");
    pos = comp.InsertAt(1, "n");

    CharChunkList::iterator it = comp.MaybeSplitChunkAt(3);
    CharChunkList::iterator chunk_it = comp.GetInsertionChunk(it);
    CompositionInput input;
    SetInput("x", "a", false, &input);

    comp.CombinePendingChunks(chunk_it, input);
    EXPECT_EQ(chunk_it->pending(), "ny");
    EXPECT_EQ(chunk_it->conversion(), "");
    EXPECT_EQ(chunk_it->raw(), "ny");
  }
}

TEST_F(CompositionTest, NewChunkBehaviors) {
  table_->AddRule("n", "", "ん");
  table_->AddRule("na", "", "な");
  table_->AddRuleWithAttributes("a", "", "あ", NEW_CHUNK);
  table_->AddRule("ん*", "", "猫");
  table_->AddRule("*", "", "");
  table_->AddRule("ん#", "", "猫");

  CompositionInput input;
  {
    size_t pos = 0;
    composition_.Erase();
    SetInput("n", "", true, &input);
    pos = composition_.InsertInput(pos, input);
    SetInput("a", "", true, &input);
    pos = composition_.InsertInput(pos, input);
    EXPECT_EQ(composition_.GetString(), "nあ");
  }
  {
    size_t pos = 0;
    composition_.Erase();
    SetInput("n", "", false, &input);
    pos = composition_.InsertInput(pos, input);
    SetInput("a", "", false, &input);
    pos = composition_.InsertInput(pos, input);
    EXPECT_EQ(composition_.GetString(), "な");
  }

  {
    size_t pos = 0;
    composition_.Erase();
    SetInput("n", "", true, &input);
    pos = composition_.InsertInput(pos, input);
    SetInput("*", "", true, &input);
    pos = composition_.InsertInput(pos, input);
    EXPECT_EQ(composition_.GetString(), "猫");
  }
  {
    size_t pos = 0;
    composition_.Erase();
    SetInput("n", "", false, &input);
    pos = composition_.InsertInput(pos, input);
    SetInput("*", "", false, &input);
    pos = composition_.InsertInput(pos, input);
    EXPECT_EQ(composition_.GetString(), "猫");
  }

  {
    size_t pos = 0;
    composition_.Erase();
    SetInput("n", "", true, &input);
    pos = composition_.InsertInput(pos, input);
    SetInput("#", "", true, &input);
    pos = composition_.InsertInput(pos, input);
    EXPECT_EQ(composition_.GetString(), "n#");
  }
  {
    size_t pos = 0;
    composition_.Erase();
    SetInput("n", "", false, &input);
    pos = composition_.InsertInput(pos, input);
    SetInput("#", "", false, &input);
    pos = composition_.InsertInput(pos, input);
    EXPECT_EQ(composition_.GetString(), "猫");
  }

  {
    size_t pos = 0;
    composition_.Erase();
    SetInput("n", "", true, &input);
    pos = composition_.InsertInput(pos, input);
    SetInput("1", "", true, &input);
    pos = composition_.InsertInput(pos, input);
    EXPECT_EQ(composition_.GetString(), "n1");
  }
  {
    size_t pos = 0;
    composition_.Erase();
    SetInput("n", "", false, &input);
    pos = composition_.InsertInput(pos, input);
    SetInput("1", "", false, &input);
    pos = composition_.InsertInput(pos, input);
    EXPECT_EQ(composition_.GetString(), "ん1");
  }
}

TEST_F(CompositionTest, TwelveKeysInput) {
  // Simulates flick + toggle input mode.

  table_->AddRule("n", "", "ん");
  table_->AddRule("na", "", "な");
  table_->AddRule("a", "", "あ");
  table_->AddRule("*", "", "");
  table_->AddRule("ほ*", "", "ぼ");
  table_->AddRuleWithAttributes("7", "", "は", NEW_CHUNK);
  table_->AddRule("は7", "", "ひ");
  table_->AddRule("ひ*", "", "び");

  CompositionInput input;
  size_t pos = 0;

  SetInput("n", "", true, &input);
  pos = composition_.InsertInput(pos, input);

  SetInput("a", "", false, &input);
  pos = composition_.InsertInput(pos, input);

  SetInput("7", "ほ", false, &input);
  pos = composition_.InsertInput(pos, input);

  SetInput("*", "", true, &input);
  pos = composition_.InsertInput(pos, input);

  SetInput("7", "ひ", false, &input);
  pos = composition_.InsertInput(pos, input);

  SetInput("7", "", true, &input);
  pos = composition_.InsertInput(pos, input);

  SetInput("7", "は", false, &input);
  pos = composition_.InsertInput(pos, input);

  SetInput("7", "", true, &input);
  pos = composition_.InsertInput(pos, input);

  SetInput("7", "は", false, &input);
  pos = composition_.InsertInput(pos, input);

  EXPECT_EQ(composition_.GetString(), "なぼひはははは");
}

TEST_F(CompositionTest, SpecialKeysInput) {
  table_->AddRule("{*}j", "お", "");

  CompositionInput input;
  size_t pos = 0;

  SetInput("{*}", "", true, &input);
  pos = composition_.InsertInput(pos, input);
  EXPECT_EQ(composition_.GetString(), "");
  EXPECT_EQ(composition_.GetCharChunkList().size(), 1);
  EXPECT_EQ(pos, 0);

  SetInput("j", "", false, &input);
  pos = composition_.InsertInput(pos, input);
  EXPECT_EQ(composition_.GetString(), "お");
}

TEST_F(CompositionTest, SpecialKeysInputWithReplacedKey) {
  table_->AddRule("r", "", "{r}");
  table_->AddRule("{r}j", "お", "");

  CompositionInput input;
  size_t pos = 0;

  SetInput("r", "", true, &input);
  pos = composition_.InsertInput(pos, input);
  EXPECT_EQ(composition_.GetString(), "");
  EXPECT_EQ(composition_.GetCharChunkList().size(), 1);
  EXPECT_EQ(pos, 0);

  SetInput("j", "", false, &input);
  pos = composition_.InsertInput(pos, input);
  EXPECT_EQ(composition_.GetString(), "お");
  EXPECT_EQ(composition_.GetCharChunkList().size(), 1);
  EXPECT_EQ(pos, 1);
}

TEST_F(CompositionTest, SpecialKeysInputWithLeadingPendingKey) {
  table_->AddRule("{*}j", "お", "");

  CompositionInput input;
  size_t pos = 0;

  SetInput("q", "", true, &input);
  pos = composition_.InsertInput(pos, input);
  EXPECT_EQ(composition_.GetString(), "q");
  EXPECT_EQ(composition_.GetCharChunkList().size(), 1);
  EXPECT_EQ(pos, 1);

  SetInput("{*}", "", true, &input);
  pos = composition_.InsertInput(pos, input);
  EXPECT_EQ(composition_.GetString(), "q");
  EXPECT_EQ(composition_.GetCharChunkList().size(), 2);
  EXPECT_EQ(pos, 1);

  SetInput("j", "", false, &input);
  pos = composition_.InsertInput(pos, input);
  EXPECT_EQ(composition_.GetString(), "qお");
}

TEST_F(CompositionTest, DifferentRulesForSamePendingWithSpecialKeys) {
  table_->AddRule("4", "", "[ta]");
  table_->AddRule("[to]4", "", "[x]{#1}");
  table_->AddRule("[x]{#1}4", "", "[ta]");

  table_->AddRule("*", "", "");
  table_->AddRule("[tu]*", "", "[x]{#2}");
  table_->AddRule("[x]{#2}*", "", "[tu]");

  {
    composition_.Erase();
    size_t pos = 0;
    pos = composition_.InsertAt(pos, "[to]4");
    EXPECT_EQ(pos, 3);
    EXPECT_EQ(composition_.GetString(), "[x]");
    EXPECT_EQ(GetRawString(composition_), "[to]4");

    pos = composition_.InsertAt(pos, "4");
    EXPECT_EQ(pos, 4);
    EXPECT_EQ(composition_.GetString(), "[ta]");
    EXPECT_EQ(GetRawString(composition_), "[to]44");
  }

  {
    composition_.Erase();
    size_t pos = 0;
    pos = composition_.InsertAt(pos, "[to]4");
    EXPECT_EQ(pos, 3);
    EXPECT_EQ(composition_.GetString(), "[x]");
    EXPECT_EQ(GetRawString(composition_), "[to]4");

    pos = composition_.InsertAt(pos, "*");
    EXPECT_EQ(pos, 3);
    EXPECT_EQ(composition_.GetString(), "[x]");
    EXPECT_EQ(GetRawString(composition_), "[to]4*");
  }

  {
    composition_.Erase();
    size_t pos = 0;
    pos = composition_.InsertAt(pos, "[tu]*");
    EXPECT_EQ(pos, 3);
    EXPECT_EQ(composition_.GetString(), "[x]");
    EXPECT_EQ(GetRawString(composition_), "[tu]*");

    pos = composition_.InsertAt(pos, "*");
    EXPECT_EQ(pos, 4);
    EXPECT_EQ(composition_.GetString(), "[tu]");
    EXPECT_EQ(GetRawString(composition_), "[tu]**");
  }

  {
    composition_.Erase();
    size_t pos = 0;
    pos = composition_.InsertAt(pos, "[tu]*");
    EXPECT_EQ(pos, 3);
    EXPECT_EQ(composition_.GetString(), "[x]");
    EXPECT_EQ(GetRawString(composition_), "[tu]*");

    pos = composition_.InsertAt(pos, "*");
    EXPECT_EQ(pos, 4);
    EXPECT_EQ(composition_.GetString(), "[tu]");
    EXPECT_EQ(GetRawString(composition_), "[tu]**");
  }
}

TEST_F(CompositionTest, LoopingRuleFor12KeysWithSpecialKeys) {
  table_->AddRule("2", "", "a");
  table_->AddRule("a2", "", "b");
  table_->AddRule("b2", "", "c");
  table_->AddRule("c2", "", "{2}2");
  table_->AddRule("{2}22", "", "a");

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "2");
  EXPECT_EQ(composition_.GetString(), "a");
  EXPECT_EQ(GetRawString(composition_), "2");

  pos = composition_.InsertAt(pos, "2");
  EXPECT_EQ(composition_.GetString(), "b");
  EXPECT_EQ(GetRawString(composition_), "22");

  pos = composition_.InsertAt(pos, "2");
  EXPECT_EQ(composition_.GetString(), "c");
  EXPECT_EQ(GetRawString(composition_), "222");

  pos = composition_.InsertAt(pos, "2");
  EXPECT_EQ(composition_.GetString(), "2");
  EXPECT_EQ(GetRawString(composition_), "2222");

  pos = composition_.InsertAt(pos, "2");
  EXPECT_EQ(composition_.GetString(), "a");
  EXPECT_EQ(GetRawString(composition_), "22222");

  pos = composition_.InsertAt(pos, "2");
  EXPECT_EQ(composition_.GetString(), "b");
  EXPECT_EQ(GetRawString(composition_), "222222");

  pos = composition_.InsertAt(pos, "2");
  EXPECT_EQ(composition_.GetString(), "c");
  EXPECT_EQ(GetRawString(composition_), "2222222");
}

TEST_F(CompositionTest, AlphanumericOfSSH) {
  // This is a unittest against http://b/3199626
  // 'ssh' (っｓｈ) + F10 should be 'ssh'.
  table_->AddRule("ss", "っ", "s");
  table_->AddRule("shi", "し", "");

  composition_.SetInputMode(Transliterators::HIRAGANA);
  size_t pos = 0;
  pos = composition_.InsertAt(pos, "s");
  pos = composition_.InsertAt(pos, "s");
  pos = composition_.InsertAt(pos, "h");
  EXPECT_EQ(pos, 3);

  std::string output = composition_.GetStringWithTrimMode(FIX);
  EXPECT_EQ(output, "っｓｈ");
}

TEST_F(CompositionTest, GrassHack) {
  table_->AddRule("ww", "っ", "w");
  table_->AddRule("we", "うぇ", "");
  table_->AddRule("www", "w", "ww");

  composition_.SetInputMode(Transliterators::HIRAGANA);
  size_t pos = 0;
  pos = composition_.InsertAt(pos, "w");
  pos = composition_.InsertAt(pos, "w");
  pos = composition_.InsertAt(pos, "w");

  EXPECT_EQ(composition_.GetString(), "ｗｗｗ");

  pos = composition_.InsertAt(pos, "e");
  EXPECT_EQ(composition_.GetString(), "ｗっうぇ");
}

TEST_F(CompositionTest, RulesForFirstKeyEvents) {
  table_->AddRuleWithAttributes("a", "[A]", "", NEW_CHUNK);
  table_->AddRule("n", "[N]", "");
  table_->AddRule("nn", "[N]", "");
  table_->AddRule("na", "[NA]", "");
  table_->AddRuleWithAttributes("ni", "[NI]", "", NEW_CHUNK);

  CompositionInput input;

  {
    SetInput("a", "", true, &input);
    composition_.InsertInput(0, input);
    EXPECT_EQ(composition_.GetString(), "[A]");
  }

  {
    composition_.Erase();
    CompositionInput input;
    SetInput("anaa", "", true, &input);
    composition_.InsertInput(0, input);
    EXPECT_EQ(composition_.GetString(), "[A][NA][A]");
  }

  {
    composition_.Erase();

    CompositionInput input;
    SetInput("an", "", true, &input);
    const size_t position_an = composition_.InsertInput(0, input);

    SetInput("a", "", true, &input);
    composition_.InsertInput(position_an, input);
    EXPECT_EQ(composition_.GetString(), "[A]n[A]");

    // This input should be treated as a part of "NA".
    SetInput("a", "", false, &input);
    composition_.InsertInput(position_an, input);
    EXPECT_EQ(composition_.GetString(), "[A][NA][A]");

    const std::string raw_t13n =
        composition_.GetStringWithTransliterator(Transliterators::RAW_STRING);
    EXPECT_EQ(raw_t13n, "anaa");
  }

  {
    composition_.Erase();

    CompositionInput input;
    SetInput("an", "", true, &input);
    const size_t position_an = composition_.InsertInput(0, input);

    SetInput("ni", "", true, &input);
    composition_.InsertInput(position_an, input);
    EXPECT_EQ(composition_.GetString(), "[A]n[NI]");

    const std::string raw_t13n =
        composition_.GetStringWithTransliterator(Transliterators::RAW_STRING);
    EXPECT_EQ(raw_t13n, "anni");
  }
}

TEST_F(CompositionTest, NoTransliteration) {
  table_->AddRuleWithAttributes("0", "0", "", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("1", "1", "", NO_TRANSLITERATION);
  table_->AddRuleWithAttributes("kk", "っ", "k", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("ka", "か", "", NO_TRANSLITERATION);
  table_->AddRuleWithAttributes("ss", "っ", "s", NO_TRANSLITERATION);
  table_->AddRuleWithAttributes("sa", "さ", "", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("tt", "っ", "t", NO_TRANSLITERATION);
  table_->AddRuleWithAttributes("ta", "た", "", NO_TRANSLITERATION);

  composition_.SetInputMode(Transliterators::FULL_KATAKANA);

  InsertCharacters("01kkassatta", 0, composition_);
  EXPECT_EQ(composition_.GetString(), "０1ッカっさった");
}

TEST_F(CompositionTest, NoTransliterationIssue3497962) {
  table_->AddRuleWithAttributes("2", "", "a", NEW_CHUNK | NO_TRANSLITERATION);
  table_->AddRuleWithAttributes("a2", "", "b", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("b2", "", "c", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("c2", "", "{2}2", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("{2}22", "", "a", NO_TABLE_ATTRIBUTE);

  composition_.SetInputMode(Transliterators::HIRAGANA);

  int pos = 0;
  pos = composition_.InsertAt(pos, "2");
  EXPECT_EQ(composition_.GetString(), "a");

  pos = composition_.InsertAt(pos, "2");
  EXPECT_EQ(composition_.GetString(), "b");
}

TEST_F(CompositionTest, SetTransliteratorOnEmpty) {
  composition_.SetTransliterator(0, 0, Transliterators::HIRAGANA);
  CompositionInput input;
  SetInput("a", "", true, &input);
  composition_.InsertInput(0, input);
  EXPECT_EQ(1, composition_.GetLength());
}

TEST_F(CompositionTest, Copy) {
  Composition src(table_);
  src.SetInputMode(Transliterators::FULL_KATAKANA);

  AppendChunk("も", "", "mo", src);
  AppendChunk("ず", "", "z", src);
  AppendChunk("く", "", "c", src);

  EXPECT_EQ(table_, src.table_for_testing());
  EXPECT_EQ(src.input_t12r(), Transliterators::FULL_KATAKANA);
  EXPECT_EQ(src.chunks().size(), 3);

  const Composition copy(src);
  EXPECT_EQ(copy, src);

  Composition copy2;
  copy2 = src;
  EXPECT_EQ(copy2, src);
}

TEST_F(CompositionTest, IsToggleable) {
  constexpr int kAttrs =
      TableAttribute::NEW_CHUNK | TableAttribute::NO_TRANSLITERATION;
  table_->AddRuleWithAttributes("1", "", "{?}あ", kAttrs);
  table_->AddRule("{?}あ1", "", "{*}あ");

  size_t pos = 0;
  pos = composition_.InsertAt(pos, "1");
  EXPECT_TRUE(composition_.IsToggleable(0));

  pos = composition_.InsertAt(pos, "1");
  EXPECT_FALSE(composition_.IsToggleable(0));
}

}  // namespace composer
}  // namespace mozc

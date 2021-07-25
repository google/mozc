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

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "composer/internal/char_chunk.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/transliterators.h"
#include "composer/table.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

namespace mozc {
namespace composer {
namespace {

std::string GetString(const Composition& composition) {
  std::string output;
  composition.GetString(&output);
  return output;
}

std::string GetRawString(const Composition& composition) {
  std::string output;
  composition.GetStringWithTransliterator(Transliterators::RAW_STRING, &output);
  return output;
}

void SetInput(const std::string& raw, const std::string& conversion,
              const bool is_new_input, CompositionInput* input) {
  input->Clear();
  input->set_raw(raw);
  if (!conversion.empty()) {
    input->set_conversion(conversion);
  }
  input->set_is_new_input(is_new_input);
}

size_t InsertCharacters(const std::string& input, size_t pos,
                        Composition* composition) {
  for (size_t i = 0; i < input.size(); ++i) {
    pos = composition->InsertAt(pos, input.substr(i, 1));
  }
  return pos;
}

}  // namespace

class CompositionTest : public testing::Test {
 protected:
  void SetUp() override {
    table_ = absl::make_unique<Table>();
    composition_ = absl::make_unique<Composition>(table_.get());
    composition_->SetInputMode(Transliterators::CONVERSION_STRING);
  }

  std::unique_ptr<Table> table_;
  std::unique_ptr<Composition> composition_;
};

static int InitComposition(Composition* comp) {
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
  static const int test_chunks_size = arraysize(test_chunks);
  CharChunkList::iterator it;
  comp->MaybeSplitChunkAt(0, &it);
  for (int i = 0; i < test_chunks_size; ++i) {
    const TestCharChunk& data = test_chunks[i];
    CharChunk* chunk = *comp->InsertChunk(&it);
    chunk->set_conversion(data.conversion);
    chunk->set_pending(data.pending);
    chunk->set_raw(data.raw);
  }
  return test_chunks_size;
}

static CharChunk* AppendChunk(const char* conversion, const char* pending,
                              const char* raw, Composition* comp) {
  CharChunkList::iterator it;
  comp->MaybeSplitChunkAt(comp->GetLength(), &it);

  CharChunk* chunk = *comp->InsertChunk(&it);
  chunk->set_conversion(conversion);
  chunk->set_pending(pending);
  chunk->set_raw(raw);
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
  CharChunk* chunk = AppendChunk("", "", "", composition_.get());

  for (int i = 0; i < arraysize(test_cases); ++i) {
    const TestCase& test = test_cases[i];
    chunk->set_conversion(test.conversion);
    chunk->set_pending(test.pending);
    chunk->set_raw(test.raw);

    const int conv_length =
        chunk->GetLength(Transliterators::CONVERSION_STRING);
    EXPECT_EQ(test.expected_conv_length, conv_length);

    const int raw_length = chunk->GetLength(Transliterators::RAW_STRING);
    EXPECT_EQ(test.expected_raw_length, raw_length);
  }
}

namespace {
bool TestGetChunkAt(Composition* comp,
                    Transliterators::Transliterator transliterator,
                    const size_t index, const size_t expected_index,
                    const size_t expected_inner_position) {
  auto expected_it = comp->GetCharChunkList().begin();
  for (int j = 0; j < expected_index; ++j, ++expected_it) {
  }

  size_t inner_position;
  auto it = comp->GetChunkAt(index, transliterator, &inner_position);
  if (it == comp->GetCharChunkList().end()) {
    EXPECT_TRUE(expected_it == it);
    EXPECT_EQ(expected_inner_position, inner_position);
  } else {
    std::string result, expected_result;
    EXPECT_EQ((*expected_it)->conversion(), (*it)->conversion());
    EXPECT_EQ((*expected_it)->pending(), (*it)->pending());
    EXPECT_EQ((*expected_it)->raw(), (*it)->raw());
    EXPECT_EQ(expected_inner_position, inner_position);
  }
  return true;
}
}  // namespace

TEST_F(CompositionTest, GetChunkAt) {
  InitComposition(composition_.get());

  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 0, 0,
                 0);
  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 1, 0,
                 1);
  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 2, 1,
                 1);
  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 3, 1,
                 2);
  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 4, 2,
                 1);
  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 5, 3,
                 1);
  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 6, 3,
                 2);
  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 7, 4,
                 1);
  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 8, 4,
                 2);
  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 9, 4,
                 3);
  // end
  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 10, 4,
                 3);
  // end
  TestGetChunkAt(composition_.get(), Transliterators::CONVERSION_STRING, 11, 4,
                 3);

  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 0, 0, 0);
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 1, 0, 1);
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 2, 1, 1);
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 3, 1, 2);
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 4, 2, 1);
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 5, 2, 2);
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 6, 3, 1);
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 7, 3, 2);
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 8, 3, 3);
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 9, 4, 1);
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 10, 4, 2);
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 11, 4, 3);
  // end
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 12, 4, 3);
  // end
  TestGetChunkAt(composition_.get(), Transliterators::RAW_STRING, 13, 4, 3);
}

TEST_F(CompositionTest, GetString) {
  InitComposition(composition_.get());
  std::string composition;

  const size_t dummy_position = 0;

  // Test RAW mode
  composition_->SetDisplayMode(dummy_position, Transliterators::RAW_STRING);
  composition_->GetString(&composition);
  EXPECT_EQ("akykittatty", composition);

  // Test CONVERSION mode
  composition_->SetDisplayMode(dummy_position,
                               Transliterators::CONVERSION_STRING);
  composition_->GetString(&composition);
  EXPECT_EQ("あkyきったっty", composition);
}

TEST_F(CompositionTest, GetStringWithDisplayMode) {
  AppendChunk("も", "", "mo", composition_.get());
  AppendChunk("ず", "", "z", composition_.get());
  AppendChunk("く", "", "c", composition_.get());

  std::string composition;
  composition_->GetStringWithTransliterator(Transliterators::CONVERSION_STRING,
                                            &composition);
  EXPECT_EQ("もずく", composition);

  composition_->GetStringWithTransliterator(Transliterators::RAW_STRING,
                                            &composition);
  EXPECT_EQ("mozc", composition);
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
  for (int i = 0; i < arraysize(test_cases); ++i) {
    const TestCase& test = test_cases[i];
    CharChunk right_orig_chunk(Transliterators::CONVERSION_STRING, nullptr);
    right_orig_chunk.set_conversion(test.conversion);
    right_orig_chunk.set_pending(test.pending);
    right_orig_chunk.set_raw(test.raw);
    CharChunk* left_new_chunk_ptr = nullptr;
    right_orig_chunk.SplitChunk(Transliterators::RAW_STRING, test.position,
                                &left_new_chunk_ptr);
    std::unique_ptr<CharChunk> left_new_chunk(left_new_chunk_ptr);

    if (left_new_chunk != nullptr) {
      EXPECT_EQ(test.expected_left_conversion, left_new_chunk->conversion());
      EXPECT_EQ(test.expected_left_pending, left_new_chunk->pending());
      EXPECT_EQ(test.expected_left_raw, left_new_chunk->raw());
    }

    EXPECT_EQ(test.expected_right_conversion, right_orig_chunk.conversion());
    EXPECT_EQ(test.expected_right_pending, right_orig_chunk.pending());
    EXPECT_EQ(test.expected_right_raw, right_orig_chunk.raw());
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
  for (int i = 0; i < arraysize(test_cases); ++i) {
    const TestCase& test = test_cases[i];
    CharChunk right_orig_chunk(Transliterators::CONVERSION_STRING, nullptr);
    right_orig_chunk.set_conversion(test.conversion);
    right_orig_chunk.set_pending(test.pending);
    right_orig_chunk.set_raw(test.raw);
    CharChunk* left_new_chunk_ptr = nullptr;
    right_orig_chunk.SplitChunk(Transliterators::CONVERSION_STRING,
                                test.position, &left_new_chunk_ptr);
    std::unique_ptr<CharChunk> left_new_chunk(left_new_chunk_ptr);

    if (left_new_chunk != nullptr) {
      EXPECT_EQ(test.expected_left_conversion, left_new_chunk->conversion());
      EXPECT_EQ(test.expected_left_pending, left_new_chunk->pending());
      EXPECT_EQ(test.expected_left_raw, left_new_chunk->raw());
    }

    EXPECT_EQ(test.expected_right_conversion, right_orig_chunk.conversion());
    EXPECT_EQ(test.expected_right_pending, right_orig_chunk.pending());
    EXPECT_EQ(test.expected_right_raw, right_orig_chunk.raw());
  }
}

TEST_F(CompositionTest, GetLength) {
  table_->AddRule("a", "A", "");
  table_->AddRule("ka", "K", "");

  EXPECT_EQ(0, composition_->GetLength());

  InsertCharacters("aka", 0, composition_.get());
  EXPECT_EQ(2, composition_->GetLength());
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
  for (int i = 0; i < arraysize(test_cases); ++i) {
    const TestCase& test = test_cases[i];

    {  // Test RAW mode
      Composition raw_comp(table_.get());
      InitComposition(&raw_comp);
      raw_comp.SetDisplayMode(dummy_position, Transliterators::RAW_STRING);
      CharChunkList::iterator raw_it;

      raw_comp.MaybeSplitChunkAt(test.position, &raw_it);
      const size_t raw_chunks_size = raw_comp.GetCharChunkList().size();
      EXPECT_EQ(test.expected_raw_chunks_size, raw_chunks_size);
    }

    {  // Test CONVERSION mode
      Composition conv_comp(table_.get());
      InitComposition(&conv_comp);
      conv_comp.SetDisplayMode(dummy_position,
                               Transliterators::CONVERSION_STRING);
      CharChunkList::iterator conv_it;

      conv_comp.MaybeSplitChunkAt(test.position, &conv_it);
      const size_t conv_chunks_size = conv_comp.GetCharChunkList().size();
      EXPECT_EQ(test.expected_conv_chunks_size, conv_chunks_size);
    }
  }
}

namespace {
std::string GetDeletedString(Transliterators::Transliterator t12r,
                             const int position) {
  std::unique_ptr<Table> table(new Table);
  std::unique_ptr<Composition> comp(new Composition(table.get()));

  InitComposition(comp.get());
  comp->SetDisplayMode(0, t12r);
  comp->DeleteAt(position);
  std::string composition;
  comp->GetString(&composition);

  comp.reset();
  table.reset();
  return composition;
}
}  // namespace

TEST_F(CompositionTest, DeleteAt) {
  // "あkyきったっty" is the original string
  EXPECT_EQ("kyきったっty",
            GetDeletedString(Transliterators::CONVERSION_STRING, 0));
  EXPECT_EQ("あyきったっty",
            GetDeletedString(Transliterators::CONVERSION_STRING, 1));
  EXPECT_EQ("あkきったっty",
            GetDeletedString(Transliterators::CONVERSION_STRING, 2));
  EXPECT_EQ("あkyったっty",
            GetDeletedString(Transliterators::CONVERSION_STRING, 3));
  EXPECT_EQ("あkyきたっty",
            GetDeletedString(Transliterators::CONVERSION_STRING, 4));
  EXPECT_EQ("あkyきっっty",
            GetDeletedString(Transliterators::CONVERSION_STRING, 5));
  EXPECT_EQ("あkyきったty",
            GetDeletedString(Transliterators::CONVERSION_STRING, 6));
  EXPECT_EQ("あkyきったっy",
            GetDeletedString(Transliterators::CONVERSION_STRING, 7));
  EXPECT_EQ("あkyきったっt",
            GetDeletedString(Transliterators::CONVERSION_STRING, 8));
  // end
  EXPECT_EQ("あkyきったっty",
            GetDeletedString(Transliterators::CONVERSION_STRING, 9));
  EXPECT_EQ("あkyきったっty",
            GetDeletedString(Transliterators::CONVERSION_STRING, -1));

  // "akykittatty" is the original string
  EXPECT_EQ("kykittatty", GetDeletedString(Transliterators::RAW_STRING, 0));
  EXPECT_EQ("aykittatty", GetDeletedString(Transliterators::RAW_STRING, 1));
  EXPECT_EQ("akkittatty", GetDeletedString(Transliterators::RAW_STRING, 2));
  EXPECT_EQ("akyittatty", GetDeletedString(Transliterators::RAW_STRING, 3));
  EXPECT_EQ("akykttatty", GetDeletedString(Transliterators::RAW_STRING, 4));
  EXPECT_EQ("akykitatty", GetDeletedString(Transliterators::RAW_STRING, 5));
  EXPECT_EQ("akykitatty", GetDeletedString(Transliterators::RAW_STRING, 6));
  EXPECT_EQ("akykitttty", GetDeletedString(Transliterators::RAW_STRING, 7));
  EXPECT_EQ("akykittaty", GetDeletedString(Transliterators::RAW_STRING, 8));
  EXPECT_EQ("akykittaty", GetDeletedString(Transliterators::RAW_STRING, 9));
  EXPECT_EQ("akykittatt", GetDeletedString(Transliterators::RAW_STRING, 10));
  // end
  EXPECT_EQ("akykittatty", GetDeletedString(Transliterators::RAW_STRING, 11));
  EXPECT_EQ("akykittatty", GetDeletedString(Transliterators::RAW_STRING, -1));
}

TEST_F(CompositionTest, DeleteAt_InvisibleCharacter) {
  CharChunkList::iterator it;
  CharChunk* chunk;

  composition_->MaybeSplitChunkAt(0, &it);

  chunk = *composition_->InsertChunk(&it);
  chunk->set_raw("1");
  chunk->set_pending(Table::ParseSpecialKey("{1}"));

  chunk = *composition_->InsertChunk(&it);
  chunk->set_raw("2");
  chunk->set_pending(Table::ParseSpecialKey("{2}2"));

  chunk = *composition_->InsertChunk(&it);
  chunk->set_raw("3");
  chunk->set_pending("3");

  // Now the CharChunks in the comp are expected to be following;
  // (raw, pending) = [ ("1", "{1}"), ("2", "{2}2"), ("3", "3") ]
  // {} means invisible characters.

  composition_->DeleteAt(0);
  std::string composition;
  composition_->GetString(&composition);
  EXPECT_EQ("3", composition);
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
                              const size_t position, const std::string& input) {
  std::unique_ptr<Table> table(new Table);
  InitTable(table.get());
  std::unique_ptr<Composition> comp(new Composition(table.get()));
  InitComposition(comp.get());

  comp->SetTable(table.get());
  comp->SetDisplayMode(0, t12r);
  comp->InsertAt(position, input);

  std::string composition;
  comp->GetString(&composition);

  comp.reset();
  table.reset();

  return composition;
}
}  // namespace

TEST_F(CompositionTest, InsertAt) {
  // "あkyきったっty" is the original string
  EXPECT_EQ("いあkyきったっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 0, "i"));
  EXPECT_EQ("あいkyきったっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 1, "i"));
  EXPECT_EQ("あきyきったっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 2, "i"));
  EXPECT_EQ("あきぃきったっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 3, "i"));
  EXPECT_EQ("あkyきいったっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 4, "i"));
  EXPECT_EQ("あkyきっいたっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 5, "i"));
  EXPECT_EQ("あkyきったっちぃ",
            GetInsertedString(Transliterators::CONVERSION_STRING, 9, "i"));
  EXPECT_EQ("yあkyきったっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 0, "y"));
  EXPECT_EQ("あykyきったっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 1, "y"));
  EXPECT_EQ("あkyyきったっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 2, "y"));
  EXPECT_EQ("あkyyきったっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 3, "y"));
  EXPECT_EQ("あkyきyったっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 4, "y"));
  EXPECT_EQ("あkyきっyたっty",
            GetInsertedString(Transliterators::CONVERSION_STRING, 5, "y"));
  // end
  EXPECT_EQ("あkyきったっちぃ",
            GetInsertedString(Transliterators::CONVERSION_STRING, 9, "i"));
  // end
  EXPECT_EQ("あkyきったっtyy",
            GetInsertedString(Transliterators::CONVERSION_STRING, 9, "y"));

  // "akykittatty" is the original string
  EXPECT_EQ("iakykittatty",
            GetInsertedString(Transliterators::RAW_STRING, 0, "i"));

  EXPECT_EQ("aikykittatty",
            GetInsertedString(Transliterators::RAW_STRING, 1, "i"));

  EXPECT_EQ("akiykittatty",
            GetInsertedString(Transliterators::RAW_STRING, 2, "i"));

  EXPECT_EQ("akyikittatty",
            GetInsertedString(Transliterators::RAW_STRING, 3, "i"));

  EXPECT_EQ("akykiittatty",
            GetInsertedString(Transliterators::RAW_STRING, 4, "i"));

  EXPECT_EQ("akykiittatty",
            GetInsertedString(Transliterators::RAW_STRING, 5, "i"));

  EXPECT_EQ("akykittattyi",
            GetInsertedString(Transliterators::RAW_STRING, 11, "i"));  // end
}

TEST_F(CompositionTest, GetExpandedStrings) {
  Table table;
  InitTable(&table);
  composition_->SetTable(&table);
  InitComposition(composition_.get());

  // a ky ki tta tty
  std::string base;
  std::set<std::string> expanded;
  composition_->GetExpandedStrings(&base, &expanded);
  EXPECT_EQ("あkyきったっ", base);
  EXPECT_EQ(2, expanded.size());
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
  EXPECT_EQ(0, composition_->ConvertPosition(static_cast<size_t>(-1),
                                             Transliterators::CONVERSION_STRING,
                                             Transliterators::RAW_STRING));
  EXPECT_EQ(0,
            composition_->ConvertPosition(0, Transliterators::CONVERSION_STRING,
                                          Transliterators::RAW_STRING));
  EXPECT_EQ(0,
            composition_->ConvertPosition(1, Transliterators::CONVERSION_STRING,
                                          Transliterators::RAW_STRING));
  EXPECT_EQ(0,
            composition_->ConvertPosition(0, Transliterators::RAW_STRING,
                                          Transliterators::CONVERSION_STRING));
  EXPECT_EQ(0, composition_->ConvertPosition(
                   static_cast<size_t>(-1), Transliterators::RAW_STRING,
                   Transliterators::CONVERSION_STRING));
  EXPECT_EQ(0,
            composition_->ConvertPosition(1, Transliterators::RAW_STRING,
                                          Transliterators::CONVERSION_STRING));

  AppendChunk("ね", "", "ne", composition_.get());
  AppendChunk("っと", "", "tto", composition_.get());

  // "|ねっと" -> "|netto"
  EXPECT_EQ(0,
            composition_->ConvertPosition(0, Transliterators::CONVERSION_STRING,
                                          Transliterators::RAW_STRING));
  // "ね|っと" -> "ne|tto"
  EXPECT_EQ(2,
            composition_->ConvertPosition(1, Transliterators::CONVERSION_STRING,
                                          Transliterators::RAW_STRING));
  // "ねっ|と" -> "net|to"
  EXPECT_EQ(3,
            composition_->ConvertPosition(2, Transliterators::CONVERSION_STRING,
                                          Transliterators::RAW_STRING));
  // "ねっと|" -> "netto|"
  EXPECT_EQ(5,
            composition_->ConvertPosition(3, Transliterators::CONVERSION_STRING,
                                          Transliterators::RAW_STRING));

  // Invalid positions.
  EXPECT_EQ(5, composition_->ConvertPosition(static_cast<size_t>(-1),
                                             Transliterators::CONVERSION_STRING,
                                             Transliterators::RAW_STRING));
  EXPECT_EQ(5,
            composition_->ConvertPosition(4, Transliterators::CONVERSION_STRING,
                                          Transliterators::RAW_STRING));

  // "|netto" -> "|ねっと"
  EXPECT_EQ(0,
            composition_->ConvertPosition(0, Transliterators::RAW_STRING,
                                          Transliterators::CONVERSION_STRING));
  // "n|etto" -> "ね|っと"
  EXPECT_EQ(1,
            composition_->ConvertPosition(1, Transliterators::RAW_STRING,
                                          Transliterators::CONVERSION_STRING));
  // "ne|tto" -> "ね|っと"
  EXPECT_EQ(1,
            composition_->ConvertPosition(2, Transliterators::RAW_STRING,
                                          Transliterators::CONVERSION_STRING));
  // "net|to" -> "ねっ|と"
  EXPECT_EQ(2,
            composition_->ConvertPosition(3, Transliterators::RAW_STRING,
                                          Transliterators::CONVERSION_STRING));
  // "nett|o" -> "ねっと|"
  EXPECT_EQ(3,
            composition_->ConvertPosition(4, Transliterators::RAW_STRING,
                                          Transliterators::CONVERSION_STRING));
  // "netto|" -> "ねっと|"
  EXPECT_EQ(3,
            composition_->ConvertPosition(5, Transliterators::RAW_STRING,
                                          Transliterators::CONVERSION_STRING));
  // Invalid positions.
  EXPECT_EQ(3, composition_->ConvertPosition(
                   static_cast<size_t>(-1), Transliterators::RAW_STRING,
                   Transliterators::CONVERSION_STRING));
  EXPECT_EQ(3,
            composition_->ConvertPosition(6, Transliterators::RAW_STRING,
                                          Transliterators::CONVERSION_STRING));

  size_t inner_position;
  auto chunk_it =
      composition_->GetChunkAt(5, Transliterators::RAW_STRING, &inner_position);

  EXPECT_EQ("tto", (*chunk_it)->raw());
  EXPECT_EQ(3, inner_position);
}

TEST_F(CompositionTest, SetDisplayMode) {
  AppendChunk("も", "", "mo", composition_.get());
  AppendChunk("ず", "", "zu", composition_.get());
  AppendChunk("く", "", "ku", composition_.get());

  size_t inner_position;
  auto chunk_it = composition_->GetChunkAt(
      0, Transliterators::CONVERSION_STRING, &inner_position);
  EXPECT_EQ("mo", (*chunk_it)->raw());
  EXPECT_EQ(0, inner_position);
  chunk_it = composition_->GetChunkAt(1, Transliterators::CONVERSION_STRING,
                                      &inner_position);
  EXPECT_EQ("mo", (*chunk_it)->raw());
  EXPECT_EQ(1, inner_position);
  chunk_it = composition_->GetChunkAt(2, Transliterators::CONVERSION_STRING,
                                      &inner_position);
  EXPECT_EQ("zu", (*chunk_it)->raw());
  EXPECT_EQ(1, inner_position);
  chunk_it = composition_->GetChunkAt(3, Transliterators::CONVERSION_STRING,
                                      &inner_position);
  EXPECT_EQ("ku", (*chunk_it)->raw());
  EXPECT_EQ(1, inner_position);

  EXPECT_EQ(6, composition_->SetDisplayMode(1, Transliterators::RAW_STRING));
  EXPECT_EQ(
      3, composition_->SetDisplayMode(2, Transliterators::CONVERSION_STRING));
  EXPECT_EQ(6, composition_->SetDisplayMode(2, Transliterators::RAW_STRING));
}

TEST_F(CompositionTest, GetStringWithTrimMode) {
  Table table;
  table.AddRule("ka", "か", "");
  table.AddRule("n", "ん", "");
  // This makes the above rule ambiguous.
  table.AddRule("na", "な", "");
  composition_->SetTable(&table);

  std::string output_empty;
  composition_->GetStringWithTrimMode(TRIM, &output_empty);
  EXPECT_TRUE(output_empty.empty());

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "k");
  pos = composition_->InsertAt(pos, "a");
  pos = composition_->InsertAt(pos, "n");

  std::string output_trim;
  composition_->GetStringWithTrimMode(TRIM, &output_trim);
  EXPECT_EQ("か", output_trim);

  std::string output_asis;
  composition_->GetStringWithTrimMode(ASIS, &output_asis);
  EXPECT_EQ("かn", output_asis);

  std::string output_fix;
  composition_->GetStringWithTrimMode(FIX, &output_asis);
  EXPECT_EQ("かん", output_asis);
}

TEST_F(CompositionTest, InsertKeyAndPreeditAt) {
  Table table;
  table.AddRule("す゛", "ず", "");
  table.AddRule("く゛", "ぐ", "");
  composition_->SetTable(&table);

  size_t pos = 0;
  pos = composition_->InsertKeyAndPreeditAt(pos, "m", "も");
  pos = composition_->InsertKeyAndPreeditAt(pos, "r", "す");
  pos = composition_->InsertKeyAndPreeditAt(pos, "@", "゛");
  pos = composition_->InsertKeyAndPreeditAt(pos, "h", "く");
  pos = composition_->InsertKeyAndPreeditAt(pos, "!", "!");

  std::string comp_str;
  composition_->GetString(&comp_str);
  EXPECT_EQ("もずく!", comp_str);

  std::string comp_ascii_str;
  composition_->GetStringWithTransliterator(Transliterators::RAW_STRING,
                                            &comp_ascii_str);
  EXPECT_EQ("mr@h!", comp_ascii_str);
}

TEST_F(CompositionTest, InsertKey_ForN) {
  Table table;
  table.AddRule("a", "[A]", "");
  table.AddRule("n", "[N]", "");
  table.AddRule("nn", "[N]", "");
  table.AddRule("na", "[NA]", "");
  table.AddRule("nya", "[NYA]", "");
  table.AddRule("ya", "[YA]", "");
  table.AddRule("ka", "[KA]", "");
  composition_->SetTable(&table);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "n");
  pos = composition_->InsertAt(pos, "y");
  pos = composition_->InsertAt(pos, "n");
  pos = composition_->InsertAt(pos, "y");
  pos = composition_->InsertAt(pos, "a");

  std::string comp_str;
  composition_->GetString(&comp_str);
  EXPECT_EQ("ny[NYA]", comp_str);
}

TEST_F(CompositionTest, GetStringWithDisplayMode_ForKana) {
  Table table;
  // Empty table is OK.
  composition_->SetTable(&table);

  size_t pos = 0;
  pos = composition_->InsertKeyAndPreeditAt(pos, "m", "も");

  std::string comp_str;
  composition_->GetStringWithTransliterator(Transliterators::RAW_STRING,
                                            &comp_str);
  EXPECT_EQ("m", comp_str);
}

TEST_F(CompositionTest, InputMode) {
  Table table;
  table.AddRule("a", "あ", "");
  table.AddRule("ka", "か", "");
  composition_->SetTable(&table);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "k");

  std::string result;
  composition_->GetString(&result);
  EXPECT_EQ("k", result);

  composition_->SetInputMode(Transliterators::FULL_KATAKANA);
  pos = composition_->InsertAt(pos, "a");
  composition_->GetString(&result);
  // If a vowel and a consonant were typed with different
  // transliterators, these characters should not be combined.
  EXPECT_EQ("kア", result);

  composition_->SetInputMode(Transliterators::HALF_ASCII);
  pos = composition_->InsertAt(pos, "k");
  composition_->GetString(&result);
  EXPECT_EQ("kアk", result);

  composition_->SetInputMode(Transliterators::HIRAGANA);
  pos = composition_->InsertAt(pos, "a");
  composition_->GetString(&result);
  EXPECT_EQ("kアkあ", result);

  EXPECT_EQ(Transliterators::CONVERSION_STRING,
            composition_->GetTransliterator(0));
  EXPECT_EQ(Transliterators::CONVERSION_STRING,
            composition_->GetTransliterator(1));
  EXPECT_EQ(Transliterators::FULL_KATAKANA, composition_->GetTransliterator(2));
  EXPECT_EQ(Transliterators::HALF_ASCII, composition_->GetTransliterator(3));
  EXPECT_EQ(Transliterators::HIRAGANA, composition_->GetTransliterator(4));
  EXPECT_EQ(Transliterators::HIRAGANA, composition_->GetTransliterator(5));
  EXPECT_EQ(Transliterators::HIRAGANA, composition_->GetTransliterator(10));
}

TEST_F(CompositionTest, SetTable) {
  Table table;
  table.AddRule("a", "あ", "");
  table.AddRule("ka", "か", "");

  Table table2;
  table2.AddRule("a", "あ", "");
  table2.AddRule("ka", "か", "");

  composition_->SetTable(&table);
  composition_->SetInputMode(Transliterators::HIRAGANA);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "k");

  std::string result;
  composition_->GetString(&result);
  EXPECT_EQ("ｋ", result);

  composition_->SetTable(&table2);

  pos = composition_->InsertAt(pos, "a");
  composition_->GetString(&result);
  EXPECT_EQ("ｋあ", result);
}

TEST_F(CompositionTest, Transliterator) {
  Table table;
  table.AddRule("a", "あ", "");
  composition_->SetTable(&table);

  // Insert "a" which is converted to "あ".
  size_t pos = 0;
  pos = composition_->InsertAt(pos, "a");
  EXPECT_EQ(1, pos);
  std::string result;
  composition_->GetString(&result);
  EXPECT_EQ("あ", result);

  // Set transliterator for Half Ascii.
  composition_->SetTransliterator(0, pos, Transliterators::HALF_ASCII);
  composition_->GetString(&result);
  EXPECT_EQ("a", result);

  // Insert "a" again.
  pos = composition_->InsertAt(pos, "a");
  EXPECT_EQ(2, pos);
  result.clear();
  composition_->GetString(&result);
  EXPECT_EQ("aあ", result);

  // Set transliterator for Full Katakana.
  composition_->SetTransliterator(0, pos, Transliterators::FULL_KATAKANA);
  composition_->GetString(&result);
  EXPECT_EQ("アア", result);
}

TEST_F(CompositionTest, HalfAsciiTransliterator) {
  Table table;
  table.AddRule("-", "ー", "");
  composition_->SetTable(&table);
  composition_->SetInputMode(Transliterators::HALF_ASCII);

  size_t pos = 0;
  pos = composition_->InsertKeyAndPreeditAt(pos, "-", "-");
  EXPECT_EQ(1, pos);
  EXPECT_EQ("-", GetString(*composition_));

  pos = composition_->InsertKeyAndPreeditAt(pos, "-", "-");
  EXPECT_EQ(2, pos);
  EXPECT_EQ("--", GetString(*composition_));
}

TEST_F(CompositionTest, ShouldCommit) {
  table_->AddRuleWithAttributes("ka", "[KA]", "", DIRECT_INPUT);
  table_->AddRuleWithAttributes("tt", "[X]", "t", DIRECT_INPUT);
  table_->AddRuleWithAttributes("ta", "[TA]", "", NO_TABLE_ATTRIBUTE);

  size_t pos = 0;

  pos = composition_->InsertAt(pos, "k");
  EXPECT_FALSE(composition_->ShouldCommit());

  pos = composition_->InsertAt(pos, "a");
  EXPECT_TRUE(composition_->ShouldCommit());

  pos = composition_->InsertAt(pos, "t");
  EXPECT_FALSE(composition_->ShouldCommit());

  pos = composition_->InsertAt(pos, "t");
  EXPECT_FALSE(composition_->ShouldCommit());

  pos = composition_->InsertAt(pos, "a");
  EXPECT_TRUE(composition_->ShouldCommit());

  pos = composition_->InsertAt(pos, "t");
  EXPECT_FALSE(composition_->ShouldCommit());

  pos = composition_->InsertAt(pos, "a");
  EXPECT_FALSE(composition_->ShouldCommit());
  EXPECT_EQ("[KA][X][TA][TA]", GetString(*composition_));
}

TEST_F(CompositionTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  Table table;
  composition_->SetTable(&table);

  size_t pos = 0;

  composition_->SetInputMode(Transliterators::FULL_ASCII);
  pos = composition_->InsertKeyAndPreeditAt(pos, "a", "ち");
  EXPECT_EQ("ａ", GetString(*composition_));

  pos = composition_->InsertAt(pos, " ");
  EXPECT_EQ("ａ　", GetString(*composition_));
}

TEST_F(CompositionTest, Issue1817410) {
  // This is a unittest against http://b/2190364
  Table table;
  table.AddRule("ss", "っ", "s");
  composition_->SetTable(&table);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "s");
  pos = composition_->InsertAt(pos, "s");

  std::string preedit;
  composition_->GetString(&preedit);
  EXPECT_EQ("っs", preedit);

  EXPECT_EQ(0, composition_->ConvertPosition(0, Transliterators::LOCAL,
                                             Transliterators::HALF_ASCII));
  EXPECT_EQ(1, composition_->ConvertPosition(1, Transliterators::LOCAL,
                                             Transliterators::HALF_ASCII));
  EXPECT_EQ(2, composition_->ConvertPosition(2, Transliterators::LOCAL,
                                             Transliterators::HALF_ASCII));

  {  // "s|s"
    size_t inner_position;
    auto chunk_it =
        composition_->GetChunkAt(1, Transliterators::LOCAL, &inner_position);
    EXPECT_EQ(1, inner_position);
    EXPECT_EQ(2, (*chunk_it)->GetLength(Transliterators::LOCAL));

    EXPECT_EQ(0,
              composition_->GetPosition(Transliterators::HALF_ASCII, chunk_it));
    EXPECT_EQ(2, (*chunk_it)->GetLength(Transliterators::HALF_ASCII));
  }

  {  // "ss|"
    size_t inner_position;
    auto chunk_it =
        composition_->GetChunkAt(2, Transliterators::LOCAL, &inner_position);
    EXPECT_EQ(2, inner_position);
    EXPECT_EQ(2, (*chunk_it)->GetLength(Transliterators::LOCAL));

    EXPECT_EQ(0,
              composition_->GetPosition(Transliterators::HALF_ASCII, chunk_it));
    EXPECT_EQ(2, (*chunk_it)->GetLength(Transliterators::HALF_ASCII));
  }
}

TEST_F(CompositionTest, Issue2209634) {
  // This is a unittest against http://b/2209634
  // "q@" becomes "qた@".
  Table table;
  table.AddRule("q", "", "た");
  table.AddRule("た@", "だ", "");
  composition_->SetTable(&table);

  composition_->SetInputMode(Transliterators::HALF_ASCII);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "q");
  pos = composition_->InsertAt(pos, "@");

  std::string preedit;
  composition_->GetString(&preedit);
  EXPECT_EQ("q@", preedit);
}

TEST_F(CompositionTest, Issue2330530) {
  // This is a unittest against http://b/2330530
  // "Win" + Numpad7 becomes "Win77" instead of "Win7".
  Table table;
  table.AddRule("wi", "うぃ", "");
  table.AddRule("i", "い", "");
  table.AddRule("n", "ん", "");
  table.AddRule("na", "な", "");
  composition_->SetTable(&table);

  composition_->SetInputMode(Transliterators::HALF_ASCII);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "W");
  pos = composition_->InsertAt(pos, "i");
  pos = composition_->InsertAt(pos, "n");

  std::string preedit;
  composition_->GetString(&preedit);
  EXPECT_EQ("Win", preedit);

  pos = composition_->InsertKeyAndPreeditAt(pos, "7", "7");
  composition_->GetString(&preedit);
  EXPECT_EQ("Win7", preedit);
}

TEST_F(CompositionTest, Issue2819580) {
  // This is a unittest against http://b/2819580.
  // 'y' after 'n' disappears.
  Table table;
  table.AddRule("po", "ぽ", "");
  table.AddRule("n", "ん", "");
  table.AddRule("na", "な", "");
  table.AddRule("ya", "や", "");
  table.AddRule("nya", "にゃ", "");
  table.AddRule("byo", "びょ", "");

  composition_->SetTable(&table);

  composition_->SetInputMode(Transliterators::HIRAGANA);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "n");
  pos = composition_->InsertAt(pos, "y");
  {
    std::string output;
    composition_->GetStringWithTrimMode(FIX, &output);
    EXPECT_EQ("んｙ", output);

    composition_->GetStringWithTrimMode(ASIS, &output);
    EXPECT_EQ("ｎｙ", output);

    composition_->GetStringWithTrimMode(TRIM, &output);
    EXPECT_EQ("", output);
  }
}

TEST_F(CompositionTest, Issue2990253) {
  // SplitChunk fails.
  // Ambiguous text is left in rhs CharChunk invalidly.
  Table table;
  table.AddRule("po", "ぽ", "");
  table.AddRule("n", "ん", "");
  table.AddRule("na", "な", "");
  table.AddRule("ya", "や", "");
  table.AddRule("nya", "にゃ", "");
  table.AddRule("byo", "びょ", "");

  composition_->SetTable(&table);

  composition_->SetInputMode(Transliterators::HIRAGANA);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "n");
  pos = composition_->InsertAt(pos, "y");
  pos = 1;
  pos = composition_->InsertAt(pos, "b");
  {
    std::string output;
    composition_->GetStringWithTrimMode(FIX, &output);
    EXPECT_EQ("んｂｙ", output);

    composition_->GetStringWithTrimMode(ASIS, &output);
    EXPECT_EQ("んｂｙ", output);

    composition_->GetStringWithTrimMode(TRIM, &output);
    // doubtful result. should be "ん"
    // May relate to http://b/2990358
    EXPECT_EQ("んｂ", output);
  }
}

TEST_F(CompositionTest, InsertionIntoPreeditMakesInvalidText_1) {
  // http://b/2990358
  // Test for mainly Composition::InsertAt()

  Table table;
  table.AddRule("po", "ぽ", "");
  table.AddRule("n", "ん", "");
  table.AddRule("na", "な", "");
  table.AddRule("ya", "や", "");
  table.AddRule("nya", "にゃ", "");
  table.AddRule("byo", "びょ", "");

  composition_->SetTable(&table);

  composition_->SetInputMode(Transliterators::HIRAGANA);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "n");
  pos = composition_->InsertAt(pos, "y");
  pos = 1;
  pos = composition_->InsertAt(pos, "b");
  pos = 3;
  pos = composition_->InsertAt(pos, "o");
  {
    std::string output;
    composition_->GetStringWithTrimMode(FIX, &output);
    EXPECT_EQ("んびょ", output);

    composition_->GetStringWithTrimMode(ASIS, &output);
    EXPECT_EQ("んびょ", output);

    composition_->GetStringWithTrimMode(TRIM, &output);
    EXPECT_EQ("んびょ", output);
  }
}

TEST_F(CompositionTest, InsertionIntoPreeditMakesInvalidText_2) {
  // http://b/2990358
  // Test for mainly Composition::InsertKeyAndPreeditAt()

  Table table;
  table.AddRule("す゛", "ず", "");
  table.AddRule("く゛", "ぐ", "");
  composition_->SetTable(&table);

  size_t pos = 0;
  pos = composition_->InsertKeyAndPreeditAt(pos, "m", "も");
  pos = composition_->InsertKeyAndPreeditAt(pos, "r", "す");
  pos = composition_->InsertKeyAndPreeditAt(pos, "h", "く");
  pos = composition_->InsertKeyAndPreeditAt(2, "@", "゛");
  pos = composition_->InsertKeyAndPreeditAt(5, "!", "!");

  std::string comp_str;
  composition_->GetString(&comp_str);
  EXPECT_EQ("もずく!", comp_str);

  std::string comp_ascii_str;
  composition_->GetStringWithTransliterator(Transliterators::RAW_STRING,
                                            &comp_ascii_str);
  EXPECT_EQ("mr@h!", comp_ascii_str);
}

TEST_F(CompositionTest, CombinePendingChunks) {
  Table table;
  table.AddRule("po", "ぽ", "");
  table.AddRule("n", "ん", "");
  table.AddRule("na", "な", "");
  table.AddRule("ya", "や", "");
  table.AddRule("nya", "にゃ", "");
  table.AddRule("byo", "びょ", "");

  {
    // empty chunks + "n" -> empty chunks + "n"
    Composition comp(&table);
    comp.SetInputMode(Transliterators::HIRAGANA);

    size_t pos = 0;

    CharChunkList::iterator it;
    comp.MaybeSplitChunkAt(pos, &it);
    CharChunkList::iterator chunk_it = comp.GetInsertionChunk(&it);

    CompositionInput input;
    SetInput("n", "", false, &input);
    comp.CombinePendingChunks(chunk_it, input);
    EXPECT_EQ("", (*chunk_it)->pending());
    EXPECT_EQ("", (*chunk_it)->conversion());
    EXPECT_EQ("", (*chunk_it)->raw());
    EXPECT_EQ("", (*chunk_it)->ambiguous());
  }
  {
    // [x] + "n" -> [x] + "n"
    // No combination performed.
    Composition comp(&table);
    comp.SetInputMode(Transliterators::HIRAGANA);

    size_t pos = 0;
    pos = comp.InsertAt(pos, "x");

    CharChunkList::iterator it;
    comp.MaybeSplitChunkAt(pos, &it);
    CharChunkList::iterator chunk_it = comp.GetInsertionChunk(&it);
    CompositionInput input;
    SetInput("n", "", false, &input);

    comp.CombinePendingChunks(chunk_it, input);
    EXPECT_EQ("", (*chunk_it)->pending());
    EXPECT_EQ("", (*chunk_it)->conversion());
    EXPECT_EQ("", (*chunk_it)->raw());
    EXPECT_EQ("", (*chunk_it)->ambiguous());
  }
  {
    // Append "a" to [n][y] -> [ny] + "a"
    // Combination performed.
    Composition comp(&table);
    comp.SetInputMode(Transliterators::HIRAGANA);

    size_t pos = 0;
    pos = comp.InsertAt(pos, "y");
    pos = comp.InsertAt(0, "n");

    CharChunkList::iterator it;
    comp.MaybeSplitChunkAt(2, &it);
    CharChunkList::iterator chunk_it = comp.GetInsertionChunk(&it);
    CompositionInput input;
    SetInput("a", "", false, &input);

    comp.CombinePendingChunks(chunk_it, input);
    EXPECT_EQ("ny", (*chunk_it)->pending());
    EXPECT_EQ("", (*chunk_it)->conversion());
    EXPECT_EQ("ny", (*chunk_it)->raw());
    EXPECT_EQ("んy", (*chunk_it)->ambiguous());
  }
  {
    // Append "a" to [x][n][y] -> [x][ny] + "a"
    // Combination performed.
    Composition comp(&table);
    comp.SetTable(&table);
    comp.SetInputMode(Transliterators::HIRAGANA);

    size_t pos = 0;
    pos = comp.InsertAt(pos, "x");
    pos = comp.InsertAt(pos, "y");
    pos = comp.InsertAt(1, "n");

    CharChunkList::iterator it;
    comp.MaybeSplitChunkAt(3, &it);
    CharChunkList::iterator chunk_it = comp.GetInsertionChunk(&it);
    CompositionInput input;
    SetInput("a", "", false, &input);

    comp.CombinePendingChunks(chunk_it, input);
    EXPECT_EQ("ny", (*chunk_it)->pending());
    EXPECT_EQ("", (*chunk_it)->conversion());
    EXPECT_EQ("ny", (*chunk_it)->raw());
    EXPECT_EQ("んy", (*chunk_it)->ambiguous());
  }

  {
    // Append "a" of conversion value to [x][n][y] -> [x][ny] + "a"
    // Combination performed.  If composition input contains a
    // conversion, the conversion is used rather than a raw value.
    Composition comp(&table);
    comp.SetInputMode(Transliterators::HIRAGANA);

    size_t pos = 0;
    pos = comp.InsertAt(pos, "x");
    pos = comp.InsertAt(pos, "y");
    pos = comp.InsertAt(1, "n");

    CharChunkList::iterator it;
    comp.MaybeSplitChunkAt(3, &it);
    CharChunkList::iterator chunk_it = comp.GetInsertionChunk(&it);
    CompositionInput input;
    SetInput("x", "a", false, &input);

    comp.CombinePendingChunks(chunk_it, input);
    EXPECT_EQ("ny", (*chunk_it)->pending());
    EXPECT_EQ("", (*chunk_it)->conversion());
    EXPECT_EQ("ny", (*chunk_it)->raw());
  }
}

TEST_F(CompositionTest, NewChunkBehaviors) {
  Table table;
  table.AddRule("n", "", "ん");
  table.AddRule("na", "", "な");
  table.AddRuleWithAttributes("a", "", "あ", NEW_CHUNK);
  table.AddRule("ん*", "", "猫");
  table.AddRule("*", "", "");
  table.AddRule("ん#", "", "猫");

  composition_->SetTable(&table);

  CompositionInput input;
  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("a", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    EXPECT_EQ("nあ", GetString(*composition_));
  }
  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("a", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    EXPECT_EQ("な", GetString(*composition_));
  }

  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("*", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    EXPECT_EQ("猫", GetString(*composition_));
  }
  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("*", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    EXPECT_EQ("猫", GetString(*composition_));
  }

  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("#", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    EXPECT_EQ("n#", GetString(*composition_));
  }
  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("#", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    EXPECT_EQ("猫", GetString(*composition_));
  }

  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("1", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    EXPECT_EQ("n1", GetString(*composition_));
  }
  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("1", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    EXPECT_EQ(
        "ん"
        "1",
        GetString(*composition_));
  }
}

TEST_F(CompositionTest, TwelveKeysInput) {
  // Simulates flick + toggle input mode.

  Table table;
  table.AddRule("n", "", "ん");
  table.AddRule("na", "", "な");
  table.AddRule("a", "", "あ");
  table.AddRule("*", "", "");
  table.AddRule("ほ*", "", "ぼ");
  table.AddRuleWithAttributes("7", "", "は", NEW_CHUNK);
  table.AddRule(
      "は"
      "7",
      "", "ひ");
  table.AddRule("ひ*", "", "び");

  composition_->SetTable(&table);

  CompositionInput input;
  size_t pos = 0;

  SetInput("n", "", true, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("a", "", false, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("7", "ほ", false, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("*", "", true, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("7", "ひ", false, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("7", "", true, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("7", "は", false, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("7", "", true, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("7", "は", false, &input);
  pos = composition_->InsertInput(pos, input);

  EXPECT_EQ("なぼひはははは", GetString(*composition_));
}

TEST_F(CompositionTest, DifferentRulesForSamePendingWithSpecialKeys) {
  table_->AddRule("4", "", "[ta]");
  table_->AddRule("[to]4", "", "[x]{#1}");
  table_->AddRule("[x]{#1}4", "", "[ta]");

  table_->AddRule("*", "", "");
  table_->AddRule("[tu]*", "", "[x]{#2}");
  table_->AddRule("[x]{#2}*", "", "[tu]");

  {
    composition_->Erase();
    size_t pos = 0;
    pos = composition_->InsertAt(pos, "[to]4");
    EXPECT_EQ(3, pos);
    EXPECT_EQ("[x]", GetString(*composition_));
    EXPECT_EQ("[to]4", GetRawString(*composition_));

    pos = composition_->InsertAt(pos, "4");
    EXPECT_EQ(4, pos);
    EXPECT_EQ("[ta]", GetString(*composition_));
    EXPECT_EQ("[to]44", GetRawString(*composition_));
  }

  {
    composition_->Erase();
    size_t pos = 0;
    pos = composition_->InsertAt(pos, "[to]4");
    EXPECT_EQ(3, pos);
    EXPECT_EQ("[x]", GetString(*composition_));
    EXPECT_EQ("[to]4", GetRawString(*composition_));

    pos = composition_->InsertAt(pos, "*");
    EXPECT_EQ(3, pos);
    EXPECT_EQ("[x]", GetString(*composition_));
    EXPECT_EQ("[to]4*", GetRawString(*composition_));
  }

  {
    composition_->Erase();
    size_t pos = 0;
    pos = composition_->InsertAt(pos, "[tu]*");
    EXPECT_EQ(3, pos);
    EXPECT_EQ("[x]", GetString(*composition_));
    EXPECT_EQ("[tu]*", GetRawString(*composition_));

    pos = composition_->InsertAt(pos, "*");
    EXPECT_EQ(4, pos);
    EXPECT_EQ("[tu]", GetString(*composition_));
    EXPECT_EQ("[tu]**", GetRawString(*composition_));
  }

  {
    composition_->Erase();
    size_t pos = 0;
    pos = composition_->InsertAt(pos, "[tu]*");
    EXPECT_EQ(3, pos);
    EXPECT_EQ("[x]", GetString(*composition_));
    EXPECT_EQ("[tu]*", GetRawString(*composition_));

    pos = composition_->InsertAt(pos, "*");
    EXPECT_EQ(4, pos);
    EXPECT_EQ("[tu]", GetString(*composition_));
    EXPECT_EQ("[tu]**", GetRawString(*composition_));
  }
}

TEST_F(CompositionTest, LoopingRuleFor12KeysWithSpecialKeys) {
  table_->AddRule("2", "", "a");
  table_->AddRule("a2", "", "b");
  table_->AddRule("b2", "", "c");
  table_->AddRule("c2", "", "{2}2");
  table_->AddRule("{2}22", "", "a");

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "2");
  EXPECT_EQ("a", GetString(*composition_));
  EXPECT_EQ("2", GetRawString(*composition_));

  pos = composition_->InsertAt(pos, "2");
  EXPECT_EQ("b", GetString(*composition_));
  EXPECT_EQ("22", GetRawString(*composition_));

  pos = composition_->InsertAt(pos, "2");
  EXPECT_EQ("c", GetString(*composition_));
  EXPECT_EQ("222", GetRawString(*composition_));

  pos = composition_->InsertAt(pos, "2");
  EXPECT_EQ("2", GetString(*composition_));
  EXPECT_EQ("2222", GetRawString(*composition_));

  pos = composition_->InsertAt(pos, "2");
  EXPECT_EQ("a", GetString(*composition_));
  EXPECT_EQ("22222", GetRawString(*composition_));

  pos = composition_->InsertAt(pos, "2");
  EXPECT_EQ("b", GetString(*composition_));
  EXPECT_EQ("222222", GetRawString(*composition_));

  pos = composition_->InsertAt(pos, "2");
  EXPECT_EQ("c", GetString(*composition_));
  EXPECT_EQ("2222222", GetRawString(*composition_));
}

TEST_F(CompositionTest, AlphanumericOfSSH) {
  // This is a unittest against http://b/3199626
  // 'ssh' (っｓｈ) + F10 should be 'ssh'.
  Table table;
  table.AddRule("ss", "っ", "s");
  table.AddRule("shi", "し", "");

  composition_->SetTable(&table);
  composition_->SetInputMode(Transliterators::HIRAGANA);
  size_t pos = 0;
  pos = composition_->InsertAt(pos, "s");
  pos = composition_->InsertAt(pos, "s");
  pos = composition_->InsertAt(pos, "h");
  EXPECT_EQ(3, pos);

  std::string output;
  composition_->GetStringWithTrimMode(FIX, &output);
  EXPECT_EQ("っｓｈ", output);
}

TEST_F(CompositionTest, GrassHack) {
  Table table;
  table.AddRule("ww", "っ", "w");
  table.AddRule("we", "うぇ", "");
  table.AddRule("www", "w", "ww");

  composition_->SetTable(&table);
  composition_->SetInputMode(Transliterators::HIRAGANA);
  size_t pos = 0;
  pos = composition_->InsertAt(pos, "w");
  pos = composition_->InsertAt(pos, "w");
  pos = composition_->InsertAt(pos, "w");

  EXPECT_EQ("ｗｗｗ", GetString(*composition_));

  pos = composition_->InsertAt(pos, "e");
  EXPECT_EQ("ｗっうぇ", GetString(*composition_));
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
    composition_->InsertInput(0, input);
    EXPECT_EQ("[A]", GetString(*composition_));
  }

  {
    composition_->Erase();
    CompositionInput input;
    SetInput("anaa", "", true, &input);
    composition_->InsertInput(0, input);
    EXPECT_EQ("[A][NA][A]", GetString(*composition_));
  }

  {
    composition_->Erase();

    CompositionInput input;
    SetInput("an", "", true, &input);
    const size_t position_an = composition_->InsertInput(0, input);

    SetInput("a", "", true, &input);
    composition_->InsertInput(position_an, input);
    EXPECT_EQ("[A]n[A]", GetString(*composition_));

    // This input should be treated as a part of "NA".
    SetInput("a", "", false, &input);
    composition_->InsertInput(position_an, input);
    EXPECT_EQ("[A][NA][A]", GetString(*composition_));

    std::string raw_t13n;
    composition_->GetStringWithTransliterator(Transliterators::RAW_STRING,
                                              &raw_t13n);
    EXPECT_EQ("anaa", raw_t13n);
  }

  {
    composition_->Erase();

    CompositionInput input;
    SetInput("an", "", true, &input);
    const size_t position_an = composition_->InsertInput(0, input);

    SetInput("ni", "", true, &input);
    composition_->InsertInput(position_an, input);
    EXPECT_EQ("[A]n[NI]", GetString(*composition_));

    std::string raw_t13n;
    composition_->GetStringWithTransliterator(Transliterators::RAW_STRING,
                                              &raw_t13n);
    EXPECT_EQ("anni", raw_t13n);
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

  composition_->SetInputMode(Transliterators::FULL_KATAKANA);

  InsertCharacters("01kkassatta", 0, composition_.get());
  EXPECT_EQ("０1ッカっさった", GetString(*composition_));
}

TEST_F(CompositionTest, NoTransliteration_Issue3497962) {
  table_->AddRuleWithAttributes("2", "", "a", NEW_CHUNK | NO_TRANSLITERATION);
  table_->AddRuleWithAttributes("a2", "", "b", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("b2", "", "c", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("c2", "", "{2}2", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("{2}22", "", "a", NO_TABLE_ATTRIBUTE);

  composition_->SetInputMode(Transliterators::HIRAGANA);

  int pos = 0;
  pos = composition_->InsertAt(pos, "2");
  EXPECT_EQ("a", GetString(*composition_));

  pos = composition_->InsertAt(pos, "2");
  EXPECT_EQ("b", GetString(*composition_));
}

TEST_F(CompositionTest, SetTransliteratorOnEmpty) {
  composition_->SetTransliterator(0, 0, Transliterators::HIRAGANA);
  CompositionInput input;
  SetInput("a", "", true, &input);
  composition_->InsertInput(0, input);
  EXPECT_EQ(1, composition_->GetLength());
}

TEST_F(CompositionTest, Clone) {
  Composition src(table_.get());
  src.SetInputMode(Transliterators::FULL_KATAKANA);

  AppendChunk("も", "", "mo", &src);
  AppendChunk("ず", "", "z", &src);
  AppendChunk("く", "", "c", &src);

  EXPECT_EQ(table_.get(), src.table());
  EXPECT_EQ(Transliterators::FULL_KATAKANA, src.input_t12r());
  EXPECT_EQ(3, src.chunks().size());

  std::unique_ptr<Composition> dest(src.Clone());
  ASSERT_NE(nullptr, dest.get());
  EXPECT_EQ(src.table(), dest->table());
  EXPECT_EQ(src.input_t12r(), dest->input_t12r());
  ASSERT_EQ(src.chunks().size(), dest->chunks().size());

  CharChunkList::const_iterator src_it = src.chunks().begin();
  CharChunkList::const_iterator dest_it = dest->chunks().begin();
  for (; src_it != src.chunks().end(); ++src_it, ++dest_it) {
    EXPECT_EQ((*src_it)->raw(), (*dest_it)->raw());
  }
}

TEST_F(CompositionTest, IsToggleable) {
  constexpr int kAttrs =
      TableAttribute::NEW_CHUNK | TableAttribute::NO_TRANSLITERATION;
  table_->AddRuleWithAttributes("1", "", "{?}あ", kAttrs);
  table_->AddRule("{?}あ1", "", "{*}あ");

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "1");
  EXPECT_TRUE(composition_->IsToggleable(0));

  pos = composition_->InsertAt(pos, "1");
  EXPECT_FALSE(composition_->IsToggleable(0));
}

}  // namespace composer
}  // namespace mozc

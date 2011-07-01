// Copyright 2010-2011, Google Inc.
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

#include "composer/internal/char_chunk.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/transliterators_ja.h"
#include "composer/table.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace composer {

namespace {
const TransliteratorInterface *kNullT12r = NULL;
const TransliteratorInterface *kRawT12r =
    Transliterators::GetRawStringSelector();
const TransliteratorInterface *kConvT12r =
    Transliterators::GetConversionStringSelector();
const TransliteratorInterface *kFullKatakanaT12r =
    TransliteratorsJa::GetFullKatakanaTransliterator();
const TransliteratorInterface *kHalfAsciiT12r =
    TransliteratorsJa::GetHalfAsciiTransliterator();
const TransliteratorInterface *kFullAsciiT12r =
    TransliteratorsJa::GetFullAsciiTransliterator();
const TransliteratorInterface *kHiraganaT12r =
    TransliteratorsJa::GetHiraganaTransliterator();
const TransliteratorInterface *kDefaultT12r =
    Transliterators::GetConversionStringSelector();

string GetString(const Composition &composition) {
  string output;
  composition.GetString(&output);
  return output;
}

string GetRawString(const Composition &composition) {
  string output;
  composition.GetStringWithTransliterator(kRawT12r, &output);
  return output;
}

void SetInput(const string &raw,
              const string &conversion,
              const bool is_new_input,
              CompositionInput *input) {
  input->Clear();
  input->set_raw(raw);
  if (!conversion.empty()) {
    input->set_conversion(conversion);
  }
  input->set_is_new_input(is_new_input);
}

size_t InsertCharacters(const string &input,
                        size_t pos,
                        Composition *composition) {
  for (size_t i = 0; i < input.size(); ++i) {
    pos = composition->InsertAt(pos, input.substr(i, 1));
  }
  return pos;
}

}  // anonymous namespace

class CompositionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    table_.reset(new Table);
    composition_.reset(new Composition);
    composition_->SetTable(table_.get());
    composition_->SetInputMode(kConvT12r);
  }

  scoped_ptr<Table> table_;
  scoped_ptr<Composition> composition_;
};

static int InitComposition(Composition* comp) {
  static const struct TestCharChunk {
    const char* conversion;
    const char* pending;
    const char* raw;
  } test_chunks[] = {
    // "あ ky き った っty"  (9 chars)
    // a ky ki tta tty
    // "あ"
    { "\xe3\x81\x82", "", "a" },
    { "", "ky", "ky" },
    // "き"
    { "\xe3\x81\x8d", "", "ki" },
    // "った"
    { "\xe3\x81\xa3\xe3\x81\x9f", "", "tta" },
    // "っ"
    { "\xe3\x81\xa3", "ty", "tty" },
  };
  static const int test_chunks_size = ARRAYSIZE(test_chunks);
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

static CharChunk *AppendChunk(const char *conversion,
                              const char *pending,
                              const char *raw,
                              Composition *comp) {
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
    // "あ"
    { "\xe3\x81\x82", "", "a", 1, 1 },
    { "", "ky", "ky", 2, 2 },
    // "き"
    { "\xe3\x81\x8d", "", "ki", 1, 2 },
    // "った"
    { "\xe3\x81\xa3\xe3\x81\x9f", "", "tta", 2, 3 },
    // "っ"
    { "\xe3\x81\xa3", "ty", "tty", 3, 3 },
  };
  Composition comp;
  CharChunk *chunk = AppendChunk("", "", "", &comp);

  for (int i = 0; i < ARRAYSIZE(test_cases); ++i) {
    const TestCase& test = test_cases[i];
    chunk->set_conversion(test.conversion);
    chunk->set_pending(test.pending);
    chunk->set_raw(test.raw);

    const int conv_length = chunk->GetLength(kConvT12r);
    EXPECT_EQ(test.expected_conv_length, conv_length);

    const int raw_length = chunk->GetLength(kRawT12r);
    EXPECT_EQ(test.expected_raw_length, raw_length);
  }
}

namespace {
bool TestGetChunkAt(Composition *comp,
                    const TransliteratorInterface *transliterator,
                    const size_t index,
                    const size_t expected_index,
                    const size_t expected_inner_position) {
  CharChunkList::iterator it;
  CharChunkList::const_iterator expected_it;
  size_t inner_position;

  expected_it = comp->GetCharChunkList().begin();
  for (int j = 0; j < expected_index; ++j, ++expected_it);

  comp->GetChunkAt(index, transliterator, &it, &inner_position);
  if (it == comp->GetCharChunkList().end()) {
    EXPECT_TRUE(expected_it == it);
    EXPECT_EQ(expected_inner_position, inner_position);
  } else {
    string result, expected_result;
    EXPECT_EQ((*expected_it)->conversion(), (*it)->conversion());
    EXPECT_EQ((*expected_it)->pending(), (*it)->pending());
    EXPECT_EQ((*expected_it)->raw(), (*it)->raw());
    EXPECT_EQ(expected_inner_position, inner_position);
  }
  return true;
}
}  // anonymous namespace

TEST_F(CompositionTest, GetChunkAt) {
  Composition comp;
  InitComposition(&comp);
  const TransliteratorInterface *transliterator;

  transliterator = Transliterators::GetConversionStringSelector();
  TestGetChunkAt(&comp, transliterator, 0, 0, 0);
  TestGetChunkAt(&comp, transliterator, 1, 0, 1);
  TestGetChunkAt(&comp, transliterator, 2, 1, 1);
  TestGetChunkAt(&comp, transliterator, 3, 1, 2);
  TestGetChunkAt(&comp, transliterator, 4, 2, 1);
  TestGetChunkAt(&comp, transliterator, 5, 3, 1);
  TestGetChunkAt(&comp, transliterator, 6, 3, 2);
  TestGetChunkAt(&comp, transliterator, 7, 4, 1);
  TestGetChunkAt(&comp, transliterator, 8, 4, 2);
  TestGetChunkAt(&comp, transliterator, 9, 4, 3);
  TestGetChunkAt(&comp, transliterator, 10, 4, 3);  // end
  TestGetChunkAt(&comp, transliterator, 11, 4, 3);  // end

  transliterator = Transliterators::GetRawStringSelector();
  TestGetChunkAt(&comp, transliterator, 0, 0, 0);
  TestGetChunkAt(&comp, transliterator, 1, 0, 1);
  TestGetChunkAt(&comp, transliterator, 2, 1, 1);
  TestGetChunkAt(&comp, transliterator, 3, 1, 2);
  TestGetChunkAt(&comp, transliterator, 4, 2, 1);
  TestGetChunkAt(&comp, transliterator, 5, 2, 2);
  TestGetChunkAt(&comp, transliterator, 6, 3, 1);
  TestGetChunkAt(&comp, transliterator, 7, 3, 2);
  TestGetChunkAt(&comp, transliterator, 8, 3, 3);
  TestGetChunkAt(&comp, transliterator, 9, 4, 1);
  TestGetChunkAt(&comp, transliterator, 10, 4, 2);
  TestGetChunkAt(&comp, transliterator, 11, 4, 3);
  TestGetChunkAt(&comp, transliterator, 12, 4, 3);  // end
  TestGetChunkAt(&comp, transliterator, 13, 4, 3);  // end
}

TEST_F(CompositionTest, GetString) {
  Composition comp;
  InitComposition(&comp);
  string composition;

  const size_t dummy_position = 0;

  // Test RAW mode
  comp.SetDisplayMode(dummy_position,
                      Transliterators::GetRawStringSelector());
  comp.GetString(&composition);
  EXPECT_EQ("akykittatty", composition);

  // Test CONVERSION mode
  comp.SetDisplayMode(dummy_position,
                      Transliterators::GetConversionStringSelector());
  comp.GetString(&composition);
  // "あkyきったっty"
  EXPECT_EQ("\xe3\x81\x82\x6b\x79\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\x9f\xe3\x81"
            "\xa3\x74\x79", composition);
}

TEST_F(CompositionTest, GetStringWithDisplayMode) {
  Composition comp;
  // "も"
  AppendChunk("\xe3\x82\x82", "", "mo", &comp);
  // "ず"
  AppendChunk("\xe3\x81\x9a", "", "z", &comp);
  // "く"
  AppendChunk("\xe3\x81\x8f", "", "c", &comp);

  string composition;
  comp.GetStringWithTransliterator(
      Transliterators::GetConversionStringSelector(), &composition);
  // "もずく"
  EXPECT_EQ("\xe3\x82\x82\xe3\x81\x9a\xe3\x81\x8f", composition);

  comp.GetStringWithTransliterator(
      Transliterators::GetRawStringSelector(), &composition);
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
    // "あ", "あ"
    { "\xe3\x81\x82", "", "a", 0, "", "", "", "\xe3\x81\x82", "", "a" },
    { "", "ky", "ky", 1, "", "k", "k", "", "y", "y" },
    // "き"
    { "\xe3\x81\x8d", "", "ki", 1, "k", "", "k", "i", "", "i" },
    // "った"
    { "\xe3\x81\xa3\xe3\x81\x9f", "", "tta", 1, "t", "", "t", "ta", "", "ta" },
    // "った"
    { "\xe3\x81\xa3\xe3\x81\x9f", "", "tta", 2, "tt", "", "tt", "a", "", "a" },
    // "っ", "っ"
    { "\xe3\x81\xa3", "ty", "tty", 1, "\xe3\x81\xa3", "", "t", "", "ty", "ty" },
    // "っ", "っ"
    { "\xe3\x81\xa3", "ty", "tty", 2, "\xe3\x81\xa3", "t", "tt", "", "y", "y" },
    // "っ", "っ"
    { "\xe3\x81\xa3", "ty", "tty", 3, "", "", "", "\xe3\x81\xa3", "ty", "tty" },
  };
  for (int i = 0; i < ARRAYSIZE(test_cases); ++i) {
    const TestCase& test = test_cases[i];
    CharChunk right_orig_chunk;
    CharChunk left_new_chunk;
    right_orig_chunk.set_conversion(test.conversion);
    right_orig_chunk.set_pending(test.pending);
    right_orig_chunk.set_raw(test.raw);
    right_orig_chunk.SplitChunk(kRawT12r, test.position, &left_new_chunk);

    EXPECT_EQ(test.expected_left_conversion, left_new_chunk.conversion());
    EXPECT_EQ(test.expected_left_pending, left_new_chunk.pending());
    EXPECT_EQ(test.expected_left_raw, left_new_chunk.raw());

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
    // "あ", "あ"
    { "\xe3\x81\x82", "", "a", 0, "", "", "", "\xe3\x81\x82", "", "a" },
    { "", "ky", "ky", 1, "", "k", "k", "", "y", "y" },
    // "きょ", "き", "き", "ょ", "ょ"
    { "\xe3\x81\x8d\xe3\x82\x87", "", "kyo", 1, "\xe3\x81\x8d", "",
      "\xe3\x81\x8d", "\xe3\x82\x87", "", "\xe3\x82\x87" },
    // "っ", "っ"
    { "\xe3\x81\xa3", "t", "tt", 1, "\xe3\x81\xa3", "", "t", "", "t", "t" },
    // "った", "っ", "っ", "た", "た"
    { "\xe3\x81\xa3\xe3\x81\x9f", "", "tta", 1, "\xe3\x81\xa3", "",
      "\xe3\x81\xa3", "\xe3\x81\x9f", "", "\xe3\x81\x9f" },
    // "っ", "っ"
    { "\xe3\x81\xa3", "ty", "tty", 1, "\xe3\x81\xa3", "", "t", "", "ty", "ty" },
    // "っ", "っ"
    { "\xe3\x81\xa3", "ty", "tty", 2, "\xe3\x81\xa3", "t", "tt", "", "y", "y" },
    // "っ", "っ"
    { "\xe3\x81\xa3", "ty", "tty", 3, "", "", "", "\xe3\x81\xa3", "ty", "tty" },
  };
  for (int i = 0; i < ARRAYSIZE(test_cases); ++i) {
    const TestCase& test = test_cases[i];
    CharChunk right_orig_chunk;
    CharChunk left_new_chunk;
    right_orig_chunk.set_conversion(test.conversion);
    right_orig_chunk.set_pending(test.pending);
    right_orig_chunk.set_raw(test.raw);
    right_orig_chunk.SplitChunk(kConvT12r, test.position, &left_new_chunk);

    EXPECT_EQ(test.expected_left_conversion, left_new_chunk.conversion());
    EXPECT_EQ(test.expected_left_pending, left_new_chunk.pending());
    EXPECT_EQ(test.expected_left_raw, left_new_chunk.raw());

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
    const bool expected_raw_result;
    const bool expected_conv_result;
    const int expected_raw_chunks_size;
    const int expected_conv_chunks_size;
  } test_cases[] = {
    // "あ ky き った っty"  (9 chars)
    // a ky ki tta tty (11 chars)
    {  0, false, false, 5, 5 },
    {  1, false, false, 5, 5 },
    {  2, true,  true,  6, 6 },
    {  3, false, false, 5, 5 },
    {  4, true,  false, 6, 5 },
    {  5, false, true,  5, 6 },
    {  6, true,  false, 6, 5 },
    {  7, true,  true,  6, 6 },
    {  8, false, true,  5, 6 },
    {  9, true,  false, 6, 5 },
    { 10, true,  false, 6, 5 },
    { 11, false, false, 5, 5 },
    { 12, false, false, 5, 5 },
  };
  const size_t dummy_position = 0;
  for (int i = 0; i < ARRAYSIZE(test_cases); ++i) {
    const TestCase& test = test_cases[i];

    // Test RAW mode
    Composition raw_comp;
    InitComposition(&raw_comp);
    raw_comp.SetDisplayMode(dummy_position, kRawT12r);
    CharChunkList::iterator raw_it;

    raw_comp.MaybeSplitChunkAt(test.position, &raw_it);
    const size_t raw_chunks_size = raw_comp.GetCharChunkList().size();
    EXPECT_EQ(test.expected_raw_chunks_size, raw_chunks_size);

    // Test CONVERSION mode
    Composition conv_comp;
    InitComposition(&conv_comp);
    conv_comp.SetDisplayMode(dummy_position, kConvT12r);
    CharChunkList::iterator conv_it;

    conv_comp.MaybeSplitChunkAt(test.position, &conv_it);
    const size_t conv_chunks_size = conv_comp.GetCharChunkList().size();
    EXPECT_EQ(test.expected_conv_chunks_size, conv_chunks_size);
  }
}

namespace {
string GetDeletedString(const TransliteratorInterface* t12r,
                        const int position) {
  Composition comp;
  InitComposition(&comp);
  comp.SetDisplayMode(0, t12r);
  comp.DeleteAt(position);
  string composition;
  comp.GetString(&composition);
  return composition;
}
}  // anonymous namespace

TEST_F(CompositionTest, DeleteAt) {
  // "あkyきったっty" is the original string

  // "kyきったっty"
  EXPECT_EQ("\x6b\x79\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\x9f\xe3\x81\xa3\x74\x79",
            GetDeletedString(kConvT12r, 0));
  // "あyきったっty"
  EXPECT_EQ("\xe3\x81\x82\x79\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\x9f\xe3\x81\xa3"
            "\x74\x79",
            GetDeletedString(kConvT12r, 1));
  // "あkきったっty"
  EXPECT_EQ("\xe3\x81\x82\x6b\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\x9f\xe3\x81\xa3"
            "\x74\x79",
            GetDeletedString(kConvT12r, 2));
  // "あkyったっty"
  EXPECT_EQ("\xe3\x81\x82\x6b\x79\xe3\x81\xa3\xe3\x81\x9f\xe3\x81\xa3\x74\x79",
            GetDeletedString(kConvT12r, 3));
  // "あkyきたっty"
  EXPECT_EQ("\xe3\x81\x82\x6b\x79\xe3\x81\x8d\xe3\x81\x9f\xe3\x81\xa3\x74\x79",
            GetDeletedString(kConvT12r, 4));
  // "あkyきっっty"
  EXPECT_EQ("\xe3\x81\x82\x6b\x79\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\xa3\x74\x79",
            GetDeletedString(kConvT12r, 5));
  // "あkyきったty"
  EXPECT_EQ("\xe3\x81\x82\x6b\x79\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\x9f\x74\x79",
            GetDeletedString(kConvT12r, 6));
  // "あkyきったっy"
  EXPECT_EQ("\xe3\x81\x82\x6b\x79\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\x9f\xe3\x81"
            "\xa3\x79",
            GetDeletedString(kConvT12r, 7));
  // "あkyきったっt"
  EXPECT_EQ("\xe3\x81\x82\x6b\x79\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\x9f\xe3\x81"
            "\xa3\x74",
            GetDeletedString(kConvT12r, 8));
  // "あkyきったっty"
  EXPECT_EQ("\xe3\x81\x82\x6b\x79\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\x9f"
            "\xe3\x81\xa3\x74\x79",
            GetDeletedString(kConvT12r, 9));  // end
  // "あkyきったっty"
  EXPECT_EQ("\xe3\x81\x82\x6b\x79\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\x9f"
            "\xe3\x81\xa3\x74\x79",
            GetDeletedString(kConvT12r, -1));

  // "akykittatty" is the original string
  EXPECT_EQ("kykittatty", GetDeletedString(kRawT12r, 0));
  EXPECT_EQ("aykittatty", GetDeletedString(kRawT12r, 1));
  EXPECT_EQ("akkittatty", GetDeletedString(kRawT12r, 2));
  EXPECT_EQ("akyittatty", GetDeletedString(kRawT12r, 3));
  EXPECT_EQ("akykttatty", GetDeletedString(kRawT12r, 4));
  EXPECT_EQ("akykitatty", GetDeletedString(kRawT12r, 5));
  EXPECT_EQ("akykitatty", GetDeletedString(kRawT12r, 6));
  EXPECT_EQ("akykitttty", GetDeletedString(kRawT12r, 7));
  EXPECT_EQ("akykittaty", GetDeletedString(kRawT12r, 8));
  EXPECT_EQ("akykittaty", GetDeletedString(kRawT12r, 9));
  EXPECT_EQ("akykittatt", GetDeletedString(kRawT12r, 10));
  EXPECT_EQ("akykittatty", GetDeletedString(kRawT12r, 11));  // end
  EXPECT_EQ("akykittatty", GetDeletedString(kRawT12r, -1));
}

namespace {

void InitTable(Table* table) {
  // "い"
  table->AddRule("i",  "\xe3\x81\x84", "");
  // "き"
  table->AddRule("ki", "\xe3\x81\x8d", "");
  // "きぃ"
  table->AddRule("kyi", "\xe3\x81\x8d\xe3\x81\x83", "");
  // "ち"
  table->AddRule("ti", "\xe3\x81\xa1", "");
  // "ちぁ"
  table->AddRule("tya", "\xe3\x81\xa1\xe3\x81\x81", "");
  // "ちぃ"
  table->AddRule("tyi", "\xe3\x81\xa1\xe3\x81\x83", "");
  // "や"
  table->AddRule("ya", "\xe3\x82\x84", "");
  // "っ"
  table->AddRule("yy", "\xe3\x81\xa3", "y");
}

string GetInsertedString(const TransliteratorInterface* t12r,
                         const size_t position,
                         const string &input) {
  Composition comp;
  InitComposition(&comp);

  Table table;
  InitTable(&table);
  comp.SetTable(&table);
  comp.SetDisplayMode(0, t12r);
  comp.InsertAt(position, input);

  string composition;
  comp.GetString(&composition);
  return composition;
}
}  // anonymous namespace

TEST_F(CompositionTest, InsertAt) {
  // "あkyきったっty" is the original string

  // "いあkyきったっty"
  EXPECT_EQ("\xe3\x81\x84\xe3\x81\x82\x6b\x79\xe3\x81\x8d\xe3\x81\xa3"
            "\xe3\x81\x9f\xe3\x81\xa3\x74\x79",
            GetInsertedString(kConvT12r, 0, "i"));

  // "あいkyきったっty"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\x6B\x79\xE3\x81\x8D\xE3\x81\xA3"
            "\xE3\x81\x9F\xE3\x81\xA3\x74\x79",
            GetInsertedString(kConvT12r, 1, "i"));

  // "あきyきったっty"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\x8D\x79\xE3\x81\x8D\xE3\x81\xA3"
            "\xE3\x81\x9F\xE3\x81\xA3\x74\x79",
            GetInsertedString(kConvT12r, 2, "i"));

  // "あきぃきったっty"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\x8D\xE3\x81\x83\xE3\x81\x8D\xE3\x81\xA3"
            "\xE3\x81\x9F\xE3\x81\xA3\x74\x79",
            GetInsertedString(kConvT12r, 3, "i"));

  // "あkyきいったっty"
  EXPECT_EQ("\xE3\x81\x82\x6B\x79\xE3\x81\x8D\xE3\x81\x84\xE3\x81\xA3"
            "\xE3\x81\x9F\xE3\x81\xA3\x74\x79",
            GetInsertedString(kConvT12r, 4, "i"));

  // "あkyきっいたっty"
  EXPECT_EQ("\xE3\x81\x82\x6B\x79\xE3\x81\x8D\xE3\x81\xA3\xE3\x81\x84"
            "\xE3\x81\x9F\xE3\x81\xA3\x74\x79",
            GetInsertedString(kConvT12r, 5, "i"));

  // "あkyきったっちぃ"
  EXPECT_EQ("\xE3\x81\x82\x6B\x79\xE3\x81\x8D\xE3\x81\xA3\xE3\x81\x9F"
            "\xE3\x81\xA3\xE3\x81\xA1\xE3\x81\x83",
            GetInsertedString(kConvT12r, 9, "i"));

  // "yあkyきったっty"
  EXPECT_EQ("\x79\xE3\x81\x82\x6B\x79\xE3\x81\x8D\xE3\x81\xA3\xE3\x81\x9F"
            "\xE3\x81\xA3\x74\x79",
            GetInsertedString(kConvT12r, 0, "y"));

  // "あykyきったっty"
  EXPECT_EQ("\xE3\x81\x82\x79\x6B\x79\xE3\x81\x8D\xE3\x81\xA3\xE3\x81\x9F"
            "\xE3\x81\xA3\x74\x79",
            GetInsertedString(kConvT12r, 1, "y"));

  // "あkyyきったっty"
  EXPECT_EQ("\xE3\x81\x82\x6B\x79\x79\xE3\x81\x8D\xE3\x81\xA3\xE3\x81\x9F"
            "\xE3\x81\xA3\x74\x79",
            GetInsertedString(kConvT12r, 2, "y"));

  // "あkyyきったっty"
  EXPECT_EQ("\xE3\x81\x82\x6B\x79\x79\xE3\x81\x8D\xE3\x81\xA3\xE3\x81\x9F"
            "\xE3\x81\xA3\x74\x79",
            GetInsertedString(kConvT12r, 3, "y"));

  // "あkyきyったっty"
  EXPECT_EQ("\xE3\x81\x82\x6B\x79\xE3\x81\x8D\x79\xE3\x81\xA3\xE3\x81\x9F"
            "\xE3\x81\xA3\x74\x79",
            GetInsertedString(kConvT12r, 4, "y"));

  // "あkyきっyたっty"
  EXPECT_EQ("\xE3\x81\x82\x6B\x79\xE3\x81\x8D\xE3\x81\xA3\x79\xE3\x81\x9F"
            "\xE3\x81\xA3\x74\x79",
            GetInsertedString(kConvT12r, 5, "y"));

  // "あkyきったっちぃ"
  EXPECT_EQ("\xE3\x81\x82\x6B\x79\xE3\x81\x8D\xE3\x81\xA3\xE3\x81\x9F"
            "\xE3\x81\xA3\xE3\x81\xA1\xE3\x81\x83",
            GetInsertedString(kConvT12r, 9, "i"));  // end

  // "あkyきったっtyy"
  EXPECT_EQ("\xE3\x81\x82\x6B\x79\xE3\x81\x8D\xE3\x81\xA3\xE3\x81\x9F"
            "\xE3\x81\xA3\x74\x79\x79",
            GetInsertedString(kConvT12r, 9, "y"));  // end


  // "akykittatty" is the original string
  EXPECT_EQ("iakykittatty", GetInsertedString(kRawT12r, 0, "i"));

  EXPECT_EQ("aikykittatty", GetInsertedString(kRawT12r, 1, "i"));

  EXPECT_EQ("akiykittatty", GetInsertedString(kRawT12r, 2, "i"));

  EXPECT_EQ("akyikittatty", GetInsertedString(kRawT12r, 3, "i"));

  EXPECT_EQ("akykiittatty", GetInsertedString(kRawT12r, 4, "i"));

  EXPECT_EQ("akykiittatty", GetInsertedString(kRawT12r, 5, "i"));

  EXPECT_EQ("akykittattyi", GetInsertedString(kRawT12r, 11, "i"));  // end
}

TEST_F(CompositionTest, ConvertPosition) {
  // Test against http://b/1550597
  Composition comp;

  // Invalid positions.
  EXPECT_EQ(0, comp.ConvertPosition(static_cast<size_t>(-1),
                                    kConvT12r, kRawT12r));
  EXPECT_EQ(0, comp.ConvertPosition(0, kConvT12r, kRawT12r));
  EXPECT_EQ(0, comp.ConvertPosition(1, kConvT12r, kRawT12r));
  EXPECT_EQ(0, comp.ConvertPosition(0, kRawT12r, kConvT12r));
  EXPECT_EQ(0, comp.ConvertPosition(static_cast<size_t>(-1),
                                    kRawT12r, kConvT12r));
  EXPECT_EQ(0, comp.ConvertPosition(1, kRawT12r, kConvT12r));

  // "ね"
  AppendChunk("\xe3\x81\xad", "", "ne", &comp);
  // "っと"
  AppendChunk("\xe3\x81\xa3\xe3\x81\xa8", "", "tto", &comp);

  // "|ねっと" -> "|netto"
  EXPECT_EQ(0, comp.ConvertPosition(0, kConvT12r, kRawT12r));
  // "ね|っと" -> "ne|tto"
  EXPECT_EQ(2, comp.ConvertPosition(1, kConvT12r, kRawT12r));
  // "ねっ|と" -> "net|to"
  EXPECT_EQ(3, comp.ConvertPosition(2, kConvT12r, kRawT12r));
  // "ねっと|" -> "netto|"
  EXPECT_EQ(5, comp.ConvertPosition(3, kConvT12r, kRawT12r));

  // Invalid positions.
  EXPECT_EQ(5, comp.ConvertPosition(static_cast<size_t>(-1),
                                    kConvT12r, kRawT12r));
  EXPECT_EQ(5, comp.ConvertPosition(4, kConvT12r, kRawT12r));

  // "|netto" -> "|ねっと"
  EXPECT_EQ(0, comp.ConvertPosition(0, kRawT12r, kConvT12r));
  // "n|etto" -> "ね|っと"
  EXPECT_EQ(1, comp.ConvertPosition(1, kRawT12r, kConvT12r));
  // "ne|tto" -> "ね|っと"
  EXPECT_EQ(1, comp.ConvertPosition(2, kRawT12r, kConvT12r));
  // "net|to" -> "ねっ|と"
  EXPECT_EQ(2, comp.ConvertPosition(3, kRawT12r, kConvT12r));
  // "nett|o" -> "ねっと|"
  EXPECT_EQ(3, comp.ConvertPosition(4, kRawT12r, kConvT12r));
  // "netto|" -> "ねっと|"
  EXPECT_EQ(3, comp.ConvertPosition(5, kRawT12r, kConvT12r));
  // Invalid positions.
  EXPECT_EQ(3, comp.ConvertPosition(static_cast<size_t>(-1),
                                    kRawT12r, kConvT12r));
  EXPECT_EQ(3, comp.ConvertPosition(6, kRawT12r, kConvT12r));

  CharChunkList::iterator chunk_it;
  size_t inner_position;
  comp.GetChunkAt(5, kRawT12r, &chunk_it, &inner_position);

  EXPECT_EQ("tto", (*chunk_it)->raw());
  EXPECT_EQ(3, inner_position);
}

TEST_F(CompositionTest, SetDisplayMode) {
  Composition comp;
  // "も"
  AppendChunk("\xe3\x82\x82", "", "mo", &comp);
  // "ず"
  AppendChunk("\xe3\x81\x9a", "", "zu", &comp);
  // "く"
  AppendChunk("\xe3\x81\x8f", "", "ku", &comp);

  CharChunkList::iterator chunk_it;
  size_t inner_position;
  comp.GetChunkAt(0, kConvT12r, &chunk_it, &inner_position);
  EXPECT_EQ("mo", (*chunk_it)->raw());
  EXPECT_EQ(0, inner_position);
  comp.GetChunkAt(1, kConvT12r, &chunk_it, &inner_position);
  EXPECT_EQ("mo", (*chunk_it)->raw());
  EXPECT_EQ(1, inner_position);
  comp.GetChunkAt(2, kConvT12r, &chunk_it, &inner_position);
  EXPECT_EQ("zu", (*chunk_it)->raw());
  EXPECT_EQ(1, inner_position);
  comp.GetChunkAt(3, kConvT12r, &chunk_it, &inner_position);
  EXPECT_EQ("ku", (*chunk_it)->raw());
  EXPECT_EQ(1, inner_position);

  EXPECT_EQ(6, comp.SetDisplayMode(1, kRawT12r));
  EXPECT_EQ(3, comp.SetDisplayMode(2, kConvT12r));
  EXPECT_EQ(6, comp.SetDisplayMode(2, kRawT12r));
}

TEST_F(CompositionTest, GetStringWithTrimMode) {
  Composition comp;
  Table table;
  // "か"
  table.AddRule("ka", "\xe3\x81\x8b", "");
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // This makes the above rule ambiguous.
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  comp.SetTable(&table);

  string output_empty;
  comp.GetStringWithTrimMode(TRIM, &output_empty);
  EXPECT_TRUE(output_empty.empty());

  size_t pos = 0;
  pos = comp.InsertAt(pos, "k");
  pos = comp.InsertAt(pos, "a");
  pos = comp.InsertAt(pos, "n");

  string output_trim;
  comp.GetStringWithTrimMode(TRIM, &output_trim);
  // "か"
  EXPECT_EQ("\xe3\x81\x8b", output_trim);

  string output_asis;
  comp.GetStringWithTrimMode(ASIS, &output_asis);
  // "かn"
  EXPECT_EQ("\xe3\x81\x8b\x6e", output_asis);

  string output_fix;
  comp.GetStringWithTrimMode(FIX, &output_asis);
  // "かん"
  EXPECT_EQ("\xe3\x81\x8b\xe3\x82\x93", output_asis);
}

TEST_F(CompositionTest, InsertKeyAndPreeditAt) {
  Composition comp;
  Table table;
  // "す゛", "ず"
  table.AddRule("\xe3\x81\x99\xe3\x82\x9b", "\xe3\x81\x9a", "");
  // "く゛", "ぐ"
  table.AddRule("\xe3\x81\x8f\xe3\x82\x9b", "\xe3\x81\x90", "");
  comp.SetTable(&table);

  size_t pos = 0;
  // "も"
  pos = comp.InsertKeyAndPreeditAt(pos, "m", "\xe3\x82\x82");
  // "す"
  pos = comp.InsertKeyAndPreeditAt(pos, "r", "\xe3\x81\x99");
  // "゛"
  pos = comp.InsertKeyAndPreeditAt(pos, "@", "\xe3\x82\x9b");
  // "く"
  pos = comp.InsertKeyAndPreeditAt(pos, "h", "\xe3\x81\x8f");
  pos = comp.InsertKeyAndPreeditAt(pos, "!", "!");

  string comp_str;
  comp.GetString(&comp_str);
  // "もずく!"
  EXPECT_EQ("\xe3\x82\x82\xe3\x81\x9a\xe3\x81\x8f\x21", comp_str);

  string comp_ascii_str;
  comp.GetStringWithTransliterator(kRawT12r, &comp_ascii_str);
  EXPECT_EQ("mr@h!", comp_ascii_str);
}

TEST_F(CompositionTest, InsertKey_ForN) {
  Composition comp;
  Table table;
  table.AddRule("a", "[A]", "");
  table.AddRule("n", "[N]", "");
  table.AddRule("nn", "[N]", "");
  table.AddRule("na", "[NA]", "");
  table.AddRule("nya", "[NYA]", "");
  table.AddRule("ya", "[YA]", "");
  table.AddRule("ka", "[KA]", "");
  comp.SetTable(&table);

  size_t pos = 0;
  pos = comp.InsertAt(pos, "n");
  pos = comp.InsertAt(pos, "y");
  pos = comp.InsertAt(pos, "n");
  pos = comp.InsertAt(pos, "y");
  pos = comp.InsertAt(pos, "a");

  string comp_str;
  comp.GetString(&comp_str);
  EXPECT_EQ("ny[NYA]", comp_str);
}

TEST_F(CompositionTest, GetStringWithDisplayMode_ForKana) {
  Composition comp;
  Table table;
  // Empty table is OK.
  comp.SetTable(&table);

  size_t pos = 0;
  // "も"
  pos = comp.InsertKeyAndPreeditAt(pos, "m", "\xe3\x82\x82");

  string comp_str;
  comp.GetStringWithTransliterator(kRawT12r, &comp_str);
  EXPECT_EQ("m", comp_str);
}

TEST_F(CompositionTest, InputMode) {
  Composition comp;
  Table table;
  // "あ"
  table.AddRule("a", "\xE3\x81\x82", "");
  // "か"
  table.AddRule("ka", "\xE3\x81\x8B", "");
  comp.SetTable(&table);

  size_t pos = 0;
  pos = comp.InsertAt(pos, "k");

  string result;
  comp.GetString(&result);
  // "k"
  EXPECT_EQ("k", result);

  comp.SetInputMode(kFullKatakanaT12r);
  pos = comp.InsertAt(pos, "a");
  comp.GetString(&result);
  // If a vowel and a consonant were typed with different
  // transliterators, these characters should not be combined.
  // "kア"
  EXPECT_EQ("\x6B\xE3\x82\xA2", result);

  comp.SetInputMode(kHalfAsciiT12r);
  pos = comp.InsertAt(pos, "k");
  comp.GetString(&result);
  // "kアk"
  EXPECT_EQ("\x6B\xE3\x82\xA2\x6B", result);

  comp.SetInputMode(kHiraganaT12r);
  pos = comp.InsertAt(pos, "a");
  comp.GetString(&result);
  // "kアkあ"
  EXPECT_EQ("\x6B\xE3\x82\xA2\x6B\xE3\x81\x82", result);

  // "|kアkあ"
  EXPECT_EQ(kDefaultT12r, comp.GetTransliterator(0));
  // "k|アkあ"
  EXPECT_EQ(kDefaultT12r, comp.GetTransliterator(1));
  // "kア|kあ"
  EXPECT_EQ(kFullKatakanaT12r, comp.GetTransliterator(2));
  // "kアk|あ"
  EXPECT_EQ(kHalfAsciiT12r, comp.GetTransliterator(3));
  // "kアkあ|"
  EXPECT_EQ(kHiraganaT12r, comp.GetTransliterator(4));
  // "kアkあ...|"
  EXPECT_EQ(kHiraganaT12r, comp.GetTransliterator(5));
  EXPECT_EQ(kHiraganaT12r, comp.GetTransliterator(10));
}

TEST_F(CompositionTest, Transliterator) {
  Composition comp;
  Table table;
  // "あ"
  table.AddRule("a", "\xE3\x81\x82", "");
  comp.SetTable(&table);

  // Insert "a" which is converted to "あ".
  size_t pos = 0;
  pos = comp.InsertAt(pos, "a");
  EXPECT_EQ(1, pos);
  string result;
  comp.GetString(&result);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", result);

  // Set transliterator for Half Ascii.
  comp.SetTransliterator(0, pos, kHalfAsciiT12r);
  comp.GetString(&result);
  EXPECT_EQ("a", result);

  // Insert "a" again.
  pos = comp.InsertAt(pos, "a");
  EXPECT_EQ(2, pos);
  result.clear();
  comp.GetString(&result);
  // "aあ"
  EXPECT_EQ("a\xE3\x81\x82", result);

  // Set transliterator for Full Katakana.
  comp.SetTransliterator(0, pos, kFullKatakanaT12r);
  comp.GetString(&result);
  // "アア"
  EXPECT_EQ("\xE3\x82\xA2\xE3\x82\xA2", result);
}

TEST_F(CompositionTest, HalfAsciiTransliterator) {
  Composition comp;
  Table table;
  // "ー"
  table.AddRule("-", "\xE3\x83\xBC", "");
  comp.SetTable(&table);
  comp.SetInputMode(kHalfAsciiT12r);

  size_t pos = 0;
  pos = comp.InsertKeyAndPreeditAt(pos, "-", "-");
  EXPECT_EQ(1, pos);
  EXPECT_EQ("-", GetString(comp));

  pos = comp.InsertKeyAndPreeditAt(pos, "-", "-");
  EXPECT_EQ(2, pos);
  EXPECT_EQ("--", GetString(comp));
}

TEST_F(CompositionTest, ShouldCommit) {
  table_->AddRuleWithAttributes("ka", "[KA]", "", DIRECT_INPUT);
  table_->AddRuleWithAttributes("tt", "[X]", "t", DIRECT_INPUT);
  table_->AddRuleWithAttributes("ta", "[TA]", "", NO_TABLE_ATTRIBUTE);

  size_t pos = 0;

  // k
  pos = composition_->InsertAt(pos, "k");
  EXPECT_FALSE(composition_->ShouldCommit());

  // k + a
  pos = composition_->InsertAt(pos, "a");
  EXPECT_TRUE(composition_->ShouldCommit());

  // ka + t
  pos = composition_->InsertAt(pos, "t");
  EXPECT_FALSE(composition_->ShouldCommit());

  // kat + t
  pos = composition_->InsertAt(pos, "t");
  EXPECT_FALSE(composition_->ShouldCommit());

  // katt + a
  pos = composition_->InsertAt(pos, "a");
  EXPECT_TRUE(composition_->ShouldCommit());

  // katta + t
  pos = composition_->InsertAt(pos, "t");
  EXPECT_FALSE(composition_->ShouldCommit());

  // kattat + a
  pos = composition_->InsertAt(pos, "a");
  EXPECT_FALSE(composition_->ShouldCommit());
  EXPECT_EQ("[KA][X][TA][TA]", GetString(*composition_));
}

TEST_F(CompositionTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  Composition comp;
  Table table;
  comp.SetTable(&table);

  size_t pos = 0;

  comp.SetInputMode(kFullAsciiT12r);
  // "ち"
  pos = comp.InsertKeyAndPreeditAt(pos, "a", "\xE3\x81\xA1");
  // "ａ"
  EXPECT_EQ("\xEF\xBD\x81", GetString(comp));

  pos = comp.InsertAt(pos, " ");
  // "ａ　"
  EXPECT_EQ("\xEF\xBD\x81\xE3\x80\x80", GetString(comp));
}


TEST_F(CompositionTest, Issue1817410) {
  // This is a unittest against http://b/2190364
  Composition comp;
  Table table;
  // "っ"
  table.AddRule("ss", "\xE3\x81\xA3", "s");
  comp.SetTable(&table);

  size_t pos = 0;
  pos = comp.InsertAt(pos, "s");
  pos = comp.InsertAt(pos, "s");

  string preedit;
  comp.GetString(&preedit);
  // "っs"
  EXPECT_EQ("\xE3\x81\xA3\x73", preedit);

  EXPECT_EQ(0, comp.ConvertPosition(0, kNullT12r, kHalfAsciiT12r));
  EXPECT_EQ(1, comp.ConvertPosition(1, kNullT12r, kHalfAsciiT12r));
  EXPECT_EQ(2, comp.ConvertPosition(2, kNullT12r, kHalfAsciiT12r));

  {  // "s|s"
    CharChunkList::iterator chunk_it;
    size_t inner_position;
    comp.GetChunkAt(1, kNullT12r, &chunk_it, &inner_position);
    EXPECT_EQ(1, inner_position);
    EXPECT_EQ(2, (*chunk_it)->GetLength(kNullT12r));

    EXPECT_EQ(0, comp.GetPosition(kHalfAsciiT12r, chunk_it));
    EXPECT_EQ(2, (*chunk_it)->GetLength(kHalfAsciiT12r));
  }

  {  // "ss|"
    CharChunkList::iterator chunk_it;
    size_t inner_position;
    comp.GetChunkAt(2, kNullT12r, &chunk_it, &inner_position);
    EXPECT_EQ(2, inner_position);
    EXPECT_EQ(2, (*chunk_it)->GetLength(kNullT12r));

    EXPECT_EQ(0, comp.GetPosition(kHalfAsciiT12r, chunk_it));
    EXPECT_EQ(2, (*chunk_it)->GetLength(kHalfAsciiT12r));
  }
}

TEST_F(CompositionTest, Issue2209634) {
  // This is a unittest against http://b/2209634
  // "q@" becomes "qた@".
  Composition comp;
  Table table;
  // "た"
  table.AddRule("q", "", "\xE3\x81\x9F");
  // "た@", "だ"
  table.AddRule("\xE3\x81\x9F\x40", "\xE3\x81\xA0", "");
  comp.SetTable(&table);

  comp.SetInputMode(kHalfAsciiT12r);

  size_t pos = 0;
  pos = comp.InsertAt(pos, "q");
  pos = comp.InsertAt(pos, "@");

  string preedit;
  comp.GetString(&preedit);
  EXPECT_EQ("q@", preedit);
}

TEST_F(CompositionTest, Issue2330530) {
  // This is a unittest against http://b/2330530
  // "Win" + Numpad7 becomes "Win77" instead of "Win7".
  Composition comp;
  Table table;
  // "うぃ"
  table.AddRule("wi", "\xe3\x81\x86\xe3\x81\x83", "");
  // "い"
  table.AddRule("i", "\xe3\x81\x84", "");
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  comp.SetTable(&table);

  comp.SetInputMode(kHalfAsciiT12r);

  size_t pos = 0;
  pos = comp.InsertAt(pos, "W");
  pos = comp.InsertAt(pos, "i");
  pos = comp.InsertAt(pos, "n");

  string preedit;
  comp.GetString(&preedit);
  EXPECT_EQ("Win", preedit);

  pos = comp.InsertKeyAndPreeditAt(pos, "7", "7");
  comp.GetString(&preedit);
  EXPECT_EQ("Win7", preedit);
}

TEST_F(CompositionTest, Issue2819580) {
  // This is a unittest against http://b/2819580.
  // 'y' after 'n' disappears.
  Table table;
  // "ぽ"
  table.AddRule("po", "\xe3\x81\xbd", "");
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  // "や"
  table.AddRule("ya", "\xe3\x82\x84", "");
  // "にゃ"
  table.AddRule("nya", "\xe3\x81\xab\xe3\x82\x83", "");
  // "びょ"
  table.AddRule("byo", "\xe3\x81\xb3\xe3\x82\x87", "");

  Composition comp;
  comp.SetTable(&table);

  comp.SetInputMode(kHiraganaT12r);

  size_t pos = 0;
  pos = comp.InsertAt(pos, "n");
  pos = comp.InsertAt(pos, "y");
  {
    string output;
    comp.GetStringWithTrimMode(FIX, &output);
    // "んｙ"
    EXPECT_EQ("\xe3\x82\x93\xef\xbd\x99", output);

    comp.GetStringWithTrimMode(ASIS, &output);
    // "ｎｙ"
    EXPECT_EQ("\xef\xbd\x8e\xef\xbd\x99", output);

    comp.GetStringWithTrimMode(TRIM, &output);
    // ""
    EXPECT_EQ("", output);
  }
}

TEST_F(CompositionTest, Issue2990253) {
  // SplitChunk fails.
  // Ambiguous text is left in rhs CharChunk invalidly.
  Table table;
  // "ぽ"
  table.AddRule("po", "\xe3\x81\xbd", "");
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  // "や"
  table.AddRule("ya", "\xe3\x82\x84", "");
  // "にゃ"
  table.AddRule("nya", "\xe3\x81\xab\xe3\x82\x83", "");
  // "びょ"
  table.AddRule("byo", "\xe3\x81\xb3\xe3\x82\x87", "");

  Composition comp;
  comp.SetTable(&table);

  comp.SetInputMode(kHiraganaT12r);

  size_t pos = 0;
  pos = comp.InsertAt(pos, "n");
  pos = comp.InsertAt(pos, "y");
  pos = 1;
  pos = comp.InsertAt(pos, "b");
  {
    string output;
    comp.GetStringWithTrimMode(FIX, &output);
    // "んｂｙ"
    EXPECT_EQ("\xe3\x82\x93\xef\xbd\x82\xef\xbd\x99", output);

    comp.GetStringWithTrimMode(ASIS, &output);
    // "んｂｙ"
    EXPECT_EQ("\xe3\x82\x93\xef\xbd\x82\xef\xbd\x99", output);

    comp.GetStringWithTrimMode(TRIM, &output);
    // "んｂ"
    // doubtful result. should be "ん"
    // May relate to http://b/2990358
    EXPECT_EQ("\xe3\x82\x93\xef\xbd\x82", output);
  }
}

TEST_F(CompositionTest, InsertionIntoPreeditMakesInvalidText_1) {
  // http://b/2990358
  // Test for mainly Composition::InsertAt()

  Table table;
  // "ぽ"
  table.AddRule("po", "\xe3\x81\xbd", "");
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  // "や"
  table.AddRule("ya", "\xe3\x82\x84", "");
  // "にゃ"
  table.AddRule("nya", "\xe3\x81\xab\xe3\x82\x83", "");
  // "びょ"
  table.AddRule("byo", "\xe3\x81\xb3\xe3\x82\x87", "");

  Composition comp;
  comp.SetTable(&table);

  comp.SetInputMode(kHiraganaT12r);

  size_t pos = 0;
  pos = comp.InsertAt(pos, "n");
  pos = comp.InsertAt(pos, "y");
  pos = 1;
  pos = comp.InsertAt(pos, "b");
  pos = 3;
  pos = comp.InsertAt(pos, "o");
  {
    string output;
    comp.GetStringWithTrimMode(FIX, &output);
    // "んびょ"
    EXPECT_EQ("\xe3\x82\x93\xe3\x81\xb3\xe3\x82\x87", output);

    comp.GetStringWithTrimMode(ASIS, &output);
    // "んびょ"
    EXPECT_EQ("\xe3\x82\x93\xe3\x81\xb3\xe3\x82\x87", output);

    comp.GetStringWithTrimMode(TRIM, &output);
    // "んびょ"
    EXPECT_EQ("\xe3\x82\x93\xe3\x81\xb3\xe3\x82\x87", output);
  }
}

TEST_F(CompositionTest, InsertionIntoPreeditMakesInvalidText_2) {
  // http://b/2990358
  // Test for mainly Composition::InsertKeyAndPreeditAt()

  Composition comp;
  Table table;
  // "す゛", "ず"
  table.AddRule("\xe3\x81\x99\xe3\x82\x9b", "\xe3\x81\x9a", "");
  // "く゛", "ぐ"
  table.AddRule("\xe3\x81\x8f\xe3\x82\x9b", "\xe3\x81\x90", "");
  comp.SetTable(&table);

  size_t pos = 0;
  // "も"
  pos = comp.InsertKeyAndPreeditAt(pos, "m", "\xe3\x82\x82");
  // "す"
  pos = comp.InsertKeyAndPreeditAt(pos, "r", "\xe3\x81\x99");
  // "く"
  pos = comp.InsertKeyAndPreeditAt(pos, "h", "\xe3\x81\x8f");
  // "゛"
  pos = comp.InsertKeyAndPreeditAt(2, "@", "\xe3\x82\x9b");
  pos = comp.InsertKeyAndPreeditAt(5, "!", "!");

  string comp_str;
  comp.GetString(&comp_str);
  // "もずく!"
  EXPECT_EQ("\xe3\x82\x82\xe3\x81\x9a\xe3\x81\x8f\x21", comp_str);

  string comp_ascii_str;
  comp.GetStringWithTransliterator(kRawT12r, &comp_ascii_str);
  EXPECT_EQ("mr@h!", comp_ascii_str);
}

TEST_F(CompositionTest, CombinePendingChunks) {
  Table table;
  // "ぽ"
  table.AddRule("po", "\xe3\x81\xbd", "");
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  // "や"
  table.AddRule("ya", "\xe3\x82\x84", "");
  // "にゃ"
  table.AddRule("nya", "\xe3\x81\xab\xe3\x82\x83", "");
  // "びょ"
  table.AddRule("byo", "\xe3\x81\xb3\xe3\x82\x87", "");

  {
    // empty chunks + "n" -> empty chunks + "n"
    Composition comp;
    comp.SetTable(&table);
    comp.SetInputMode(kHiraganaT12r);

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
    Composition comp;
    comp.SetTable(&table);
    comp.SetInputMode(kHiraganaT12r);

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
    Composition comp;
    comp.SetTable(&table);
    comp.SetInputMode(kHiraganaT12r);

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
    // "んy"
    EXPECT_EQ("\xe3\x82\x93y", (*chunk_it)->ambiguous());
  }
  {
    // Append "a" to [x][n][y] -> [x][ny] + "a"
    // Combination performed.
    Composition comp;
    comp.SetTable(&table);
    comp.SetInputMode(kHiraganaT12r);

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
    // "んy"
    EXPECT_EQ("\xe3\x82\x93y", (*chunk_it)->ambiguous());
  }

  {
    // Append "a" of conversion value to [x][n][y] -> [x][ny] + "a"
    // Combination performed.  If composition input contains a
    // conversion, the conversion is used rather than a raw value.
    Composition comp;
    comp.SetTable(&table);
    comp.SetInputMode(kHiraganaT12r);

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





TEST_F(CompositionTest, AlphanumericOfSSH) {
  // This is a unittest against http://b/3199626
  // 'ssh' (っｓｈ) + F10 should be 'ssh'.
  Table table;
  // "っ"
  table.AddRule("ss", "\xE3\x81\xA3", "s");
  // "し"
  table.AddRule("shi", "\xE3\x81\x97", "");

  Composition comp;
  comp.SetTable(&table);
  comp.SetInputMode(kHiraganaT12r);
  size_t pos = 0;
  pos = comp.InsertAt(pos, "s");
  pos = comp.InsertAt(pos, "s");
  pos = comp.InsertAt(pos, "h");
  EXPECT_EQ(3, pos);

  string output;
  comp.GetStringWithTrimMode(FIX, &output);
  // "っｓｈ"
  EXPECT_EQ("\xE3\x81\xA3\xEF\xBD\x93\xEF\xBD\x88", output);
}

TEST_F(CompositionTest, GrassHack) {
  Table table;
  // "っ"
  table.AddRule("ww", "\xE3\x81\xA3", "w");
  // "うぇ"
  table.AddRule("we", "\xE3\x81\x86\xE3\x81\x87", "");
  table.AddRule("www", "w", "ww");

  Composition comp;
  comp.SetTable(&table);
  comp.SetInputMode(kHiraganaT12r);
  size_t pos = 0;
  pos = comp.InsertAt(pos, "w");
  pos = comp.InsertAt(pos, "w");
  pos = comp.InsertAt(pos, "w");

  // "ｗｗｗ"
  EXPECT_EQ("\xEF\xBD\x97" "\xEF\xBD\x97" "\xEF\xBD\x97",
            GetString(comp));

  pos = comp.InsertAt(pos, "e");
  // "ｗっうぇ"
  EXPECT_EQ("\xEF\xBD\x97" "\xE3\x81\xA3" "\xE3\x81\x86\xE3\x81\x87",
            GetString(comp));
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

    string raw_t13n;
    composition_->GetStringWithTransliterator(kRawT12r, &raw_t13n);
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

    string raw_t13n;
    composition_->GetStringWithTransliterator(kRawT12r, &raw_t13n);
    EXPECT_EQ("anni", raw_t13n);
  }
}

TEST_F(CompositionTest, NoTransliteration) {
  table_->AddRuleWithAttributes("0", "0", "", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("1", "1", "", NO_TRANSLITERATION);
  // "っ"
  table_->AddRuleWithAttributes("kk", "\xE3\x81\xA3", "k", NO_TABLE_ATTRIBUTE);
  // "か"
  table_->AddRuleWithAttributes("ka", "\xE3\x81\x8B", "", NO_TRANSLITERATION);
  // "っ"
  table_->AddRuleWithAttributes("ss", "\xE3\x81\xA3", "s", NO_TRANSLITERATION);
  // "さ"
  table_->AddRuleWithAttributes("sa", "\xE3\x81\x95", "", NO_TABLE_ATTRIBUTE);
  // "っ"
  table_->AddRuleWithAttributes("tt", "\xE3\x81\xA3", "t", NO_TRANSLITERATION);
  // "た"
  table_->AddRuleWithAttributes("ta", "\xE3\x81\x9F", "", NO_TRANSLITERATION);

  composition_->SetInputMode(kFullKatakanaT12r);

  InsertCharacters("01kkassatta", 0, composition_.get());
  // "０1ッカっさった"
  EXPECT_EQ("\xEF\xBC\x90\x31\xE3\x83\x83\xE3\x82\xAB\xE3\x81\xA3\xE3\x81\x95"
            "\xE3\x81\xA3\xE3\x81\x9F",
            GetString(*composition_));
}


TEST_F(CompositionTest, SetTransliteratorOnEmpty) {
  composition_->SetTransliterator(0, 0, kHiraganaT12r);
  CompositionInput input;
  SetInput("a", "", true, &input);
  composition_->InsertInput(0, input);
  EXPECT_EQ(1, composition_->GetLength());
}

}  // namespace composer
}  // namespace mozc

// Copyright 2010-2012, Google Inc.
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
#include <set>
#include <string>

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
    composition_.reset(new Composition(table_.get()));
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
  CharChunk *chunk = AppendChunk("", "", "", composition_.get());

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
  InitComposition(composition_.get());
  const TransliteratorInterface *transliterator;

  transliterator = Transliterators::GetConversionStringSelector();
  TestGetChunkAt(composition_.get(), transliterator, 0, 0, 0);
  TestGetChunkAt(composition_.get(), transliterator, 1, 0, 1);
  TestGetChunkAt(composition_.get(), transliterator, 2, 1, 1);
  TestGetChunkAt(composition_.get(), transliterator, 3, 1, 2);
  TestGetChunkAt(composition_.get(), transliterator, 4, 2, 1);
  TestGetChunkAt(composition_.get(), transliterator, 5, 3, 1);
  TestGetChunkAt(composition_.get(), transliterator, 6, 3, 2);
  TestGetChunkAt(composition_.get(), transliterator, 7, 4, 1);
  TestGetChunkAt(composition_.get(), transliterator, 8, 4, 2);
  TestGetChunkAt(composition_.get(), transliterator, 9, 4, 3);
  TestGetChunkAt(composition_.get(), transliterator, 10, 4, 3);  // end
  TestGetChunkAt(composition_.get(), transliterator, 11, 4, 3);  // end

  transliterator = Transliterators::GetRawStringSelector();
  TestGetChunkAt(composition_.get(), transliterator, 0, 0, 0);
  TestGetChunkAt(composition_.get(), transliterator, 1, 0, 1);
  TestGetChunkAt(composition_.get(), transliterator, 2, 1, 1);
  TestGetChunkAt(composition_.get(), transliterator, 3, 1, 2);
  TestGetChunkAt(composition_.get(), transliterator, 4, 2, 1);
  TestGetChunkAt(composition_.get(), transliterator, 5, 2, 2);
  TestGetChunkAt(composition_.get(), transliterator, 6, 3, 1);
  TestGetChunkAt(composition_.get(), transliterator, 7, 3, 2);
  TestGetChunkAt(composition_.get(), transliterator, 8, 3, 3);
  TestGetChunkAt(composition_.get(), transliterator, 9, 4, 1);
  TestGetChunkAt(composition_.get(), transliterator, 10, 4, 2);
  TestGetChunkAt(composition_.get(), transliterator, 11, 4, 3);
  TestGetChunkAt(composition_.get(), transliterator, 12, 4, 3);  // end
  TestGetChunkAt(composition_.get(), transliterator, 13, 4, 3);  // end
}

TEST_F(CompositionTest, GetString) {
  InitComposition(composition_.get());
  string composition;

  const size_t dummy_position = 0;

  // Test RAW mode
  composition_->SetDisplayMode(dummy_position,
                      Transliterators::GetRawStringSelector());
  composition_->GetString(&composition);
  EXPECT_EQ("akykittatty", composition);

  // Test CONVERSION mode
  composition_->SetDisplayMode(dummy_position,
                      Transliterators::GetConversionStringSelector());
  composition_->GetString(&composition);
  // "あkyきったっty"
  EXPECT_EQ("\xe3\x81\x82\x6b\x79\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\x9f\xe3\x81"
            "\xa3\x74\x79", composition);
}

TEST_F(CompositionTest, GetStringWithDisplayMode) {
  // "も"
  AppendChunk("\xe3\x82\x82", "", "mo", composition_.get());
  // "ず"
  AppendChunk("\xe3\x81\x9a", "", "z", composition_.get());
  // "く"
  AppendChunk("\xe3\x81\x8f", "", "c", composition_.get());

  string composition;
  composition_->GetStringWithTransliterator(
      Transliterators::GetConversionStringSelector(), &composition);
  // "もずく"
  EXPECT_EQ("\xe3\x82\x82\xe3\x81\x9a\xe3\x81\x8f", composition);

  composition_->GetStringWithTransliterator(
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
    CharChunk right_orig_chunk(NULL, NULL);
    right_orig_chunk.set_conversion(test.conversion);
    right_orig_chunk.set_pending(test.pending);
    right_orig_chunk.set_raw(test.raw);
    CharChunk *left_new_chunk_ptr = NULL;
    right_orig_chunk.SplitChunk(kRawT12r, test.position, &left_new_chunk_ptr);
    scoped_ptr<CharChunk> left_new_chunk(left_new_chunk_ptr);

    if (left_new_chunk.get() != NULL) {
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
    CharChunk right_orig_chunk(NULL, NULL);
    right_orig_chunk.set_conversion(test.conversion);
    right_orig_chunk.set_pending(test.pending);
    right_orig_chunk.set_raw(test.raw);
    CharChunk *left_new_chunk_ptr = NULL;
    right_orig_chunk.SplitChunk(kConvT12r, test.position, &left_new_chunk_ptr);
    scoped_ptr<CharChunk> left_new_chunk(left_new_chunk_ptr);

    if (left_new_chunk.get() != NULL) {
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

    {  // Test RAW mode
      Composition raw_comp(table_.get());
      InitComposition(&raw_comp);
      raw_comp.SetDisplayMode(dummy_position, kRawT12r);
      CharChunkList::iterator raw_it;

      raw_comp.MaybeSplitChunkAt(test.position, &raw_it);
      const size_t raw_chunks_size = raw_comp.GetCharChunkList().size();
      EXPECT_EQ(test.expected_raw_chunks_size, raw_chunks_size);
    }

    {  // Test CONVERSION mode
      Composition conv_comp(table_.get());
      InitComposition(&conv_comp);
      conv_comp.SetDisplayMode(dummy_position, kConvT12r);
      CharChunkList::iterator conv_it;

      conv_comp.MaybeSplitChunkAt(test.position, &conv_it);
      const size_t conv_chunks_size = conv_comp.GetCharChunkList().size();
      EXPECT_EQ(test.expected_conv_chunks_size, conv_chunks_size);
    }
  }
}

namespace {
string GetDeletedString(const TransliteratorInterface* t12r,
                        const int position) {
  scoped_ptr<Table> table(new Table);
  scoped_ptr<Composition> comp(new Composition(table.get()));

  InitComposition(comp.get());
  comp->SetDisplayMode(0, t12r);
  comp->DeleteAt(position);
  string composition;
  comp->GetString(&composition);

  comp.reset();
  table.reset();
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

TEST_F(CompositionTest, DeleteAt_InvisibleCharacter) {
  CharChunkList::iterator it;
  CharChunk *chunk;

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
  string composition;
  composition_->GetString(&composition);
  EXPECT_EQ("3", composition);
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
  // "ちゃ"
  table->AddRule("tya", "\xe3\x81\xa1\xe3\x82\x83", "");
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
  scoped_ptr<Table> table(new Table);
  InitTable(table.get());
  scoped_ptr<Composition> comp(new Composition(table.get()));
  InitComposition(comp.get());

  comp->SetTable(table.get());
  comp->SetDisplayMode(0, t12r);
  comp->InsertAt(position, input);

  string composition;
  comp->GetString(&composition);

  comp.reset();
  table.reset();

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

TEST_F(CompositionTest, GetExpandedStrings) {
  Table table;
  InitTable(&table);
  composition_->SetTable(&table);
  InitComposition(composition_.get());

  // a ky ki tta tty
  string base;
  set<string> expanded;
  composition_->GetExpandedStrings(&base, &expanded);
  // "あkyきったっ"
  EXPECT_EQ(
      "\xe3\x81\x82\x6b\x79\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\x9f\xe3\x81\xa3",
      base);
  EXPECT_EQ(2, expanded.size());
  // "ちぃ"
  // You cannot use EXPECT_NE here because it causes compile error in gtest
  // when the compiler is Visual C++. b/5655673
  EXPECT_TRUE(expanded.find("\xe3\x81\xa1\xe3\x81\x83") != expanded.end());
  // "ちゃ"
  // You cannot use EXPECT_NE here because it causes compile error in gtest
  // when the compiler is Visual C++. b/5655673
  EXPECT_TRUE(expanded.find("\xe3\x81\xa1\xe3\x82\x83") != expanded.end());
}

TEST_F(CompositionTest, ConvertPosition) {
  // Test against http://b/1550597

  // Invalid positions.
  EXPECT_EQ(0, composition_->ConvertPosition(static_cast<size_t>(-1),
                                    kConvT12r, kRawT12r));
  EXPECT_EQ(0, composition_->ConvertPosition(0, kConvT12r, kRawT12r));
  EXPECT_EQ(0, composition_->ConvertPosition(1, kConvT12r, kRawT12r));
  EXPECT_EQ(0, composition_->ConvertPosition(0, kRawT12r, kConvT12r));
  EXPECT_EQ(0, composition_->ConvertPosition(static_cast<size_t>(-1),
                                    kRawT12r, kConvT12r));
  EXPECT_EQ(0, composition_->ConvertPosition(1, kRawT12r, kConvT12r));

  // "ね"
  AppendChunk("\xe3\x81\xad", "", "ne", composition_.get());
  // "っと"
  AppendChunk("\xe3\x81\xa3\xe3\x81\xa8", "", "tto", composition_.get());

  // "|ねっと" -> "|netto"
  EXPECT_EQ(0, composition_->ConvertPosition(0, kConvT12r, kRawT12r));
  // "ね|っと" -> "ne|tto"
  EXPECT_EQ(2, composition_->ConvertPosition(1, kConvT12r, kRawT12r));
  // "ねっ|と" -> "net|to"
  EXPECT_EQ(3, composition_->ConvertPosition(2, kConvT12r, kRawT12r));
  // "ねっと|" -> "netto|"
  EXPECT_EQ(5, composition_->ConvertPosition(3, kConvT12r, kRawT12r));

  // Invalid positions.
  EXPECT_EQ(5, composition_->ConvertPosition(static_cast<size_t>(-1),
                                    kConvT12r, kRawT12r));
  EXPECT_EQ(5, composition_->ConvertPosition(4, kConvT12r, kRawT12r));

  // "|netto" -> "|ねっと"
  EXPECT_EQ(0, composition_->ConvertPosition(0, kRawT12r, kConvT12r));
  // "n|etto" -> "ね|っと"
  EXPECT_EQ(1, composition_->ConvertPosition(1, kRawT12r, kConvT12r));
  // "ne|tto" -> "ね|っと"
  EXPECT_EQ(1, composition_->ConvertPosition(2, kRawT12r, kConvT12r));
  // "net|to" -> "ねっ|と"
  EXPECT_EQ(2, composition_->ConvertPosition(3, kRawT12r, kConvT12r));
  // "nett|o" -> "ねっと|"
  EXPECT_EQ(3, composition_->ConvertPosition(4, kRawT12r, kConvT12r));
  // "netto|" -> "ねっと|"
  EXPECT_EQ(3, composition_->ConvertPosition(5, kRawT12r, kConvT12r));
  // Invalid positions.
  EXPECT_EQ(3, composition_->ConvertPosition(static_cast<size_t>(-1),
                                    kRawT12r, kConvT12r));
  EXPECT_EQ(3, composition_->ConvertPosition(6, kRawT12r, kConvT12r));

  CharChunkList::iterator chunk_it;
  size_t inner_position;
  composition_->GetChunkAt(5, kRawT12r, &chunk_it, &inner_position);

  EXPECT_EQ("tto", (*chunk_it)->raw());
  EXPECT_EQ(3, inner_position);
}

TEST_F(CompositionTest, SetDisplayMode) {
  // "も"
  AppendChunk("\xe3\x82\x82", "", "mo", composition_.get());
  // "ず"
  AppendChunk("\xe3\x81\x9a", "", "zu", composition_.get());
  // "く"
  AppendChunk("\xe3\x81\x8f", "", "ku", composition_.get());

  CharChunkList::iterator chunk_it;
  size_t inner_position;
  composition_->GetChunkAt(0, kConvT12r, &chunk_it, &inner_position);
  EXPECT_EQ("mo", (*chunk_it)->raw());
  EXPECT_EQ(0, inner_position);
  composition_->GetChunkAt(1, kConvT12r, &chunk_it, &inner_position);
  EXPECT_EQ("mo", (*chunk_it)->raw());
  EXPECT_EQ(1, inner_position);
  composition_->GetChunkAt(2, kConvT12r, &chunk_it, &inner_position);
  EXPECT_EQ("zu", (*chunk_it)->raw());
  EXPECT_EQ(1, inner_position);
  composition_->GetChunkAt(3, kConvT12r, &chunk_it, &inner_position);
  EXPECT_EQ("ku", (*chunk_it)->raw());
  EXPECT_EQ(1, inner_position);

  EXPECT_EQ(6, composition_->SetDisplayMode(1, kRawT12r));
  EXPECT_EQ(3, composition_->SetDisplayMode(2, kConvT12r));
  EXPECT_EQ(6, composition_->SetDisplayMode(2, kRawT12r));
}

TEST_F(CompositionTest, GetStringWithTrimMode) {
  Table table;
  // "か"
  table.AddRule("ka", "\xe3\x81\x8b", "");
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // This makes the above rule ambiguous.
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  composition_->SetTable(&table);

  string output_empty;
  composition_->GetStringWithTrimMode(TRIM, &output_empty);
  EXPECT_TRUE(output_empty.empty());

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "k");
  pos = composition_->InsertAt(pos, "a");
  pos = composition_->InsertAt(pos, "n");

  string output_trim;
  composition_->GetStringWithTrimMode(TRIM, &output_trim);
  // "か"
  EXPECT_EQ("\xe3\x81\x8b", output_trim);

  string output_asis;
  composition_->GetStringWithTrimMode(ASIS, &output_asis);
  // "かn"
  EXPECT_EQ("\xe3\x81\x8b\x6e", output_asis);

  string output_fix;
  composition_->GetStringWithTrimMode(FIX, &output_asis);
  // "かん"
  EXPECT_EQ("\xe3\x81\x8b\xe3\x82\x93", output_asis);
}

TEST_F(CompositionTest, InsertKeyAndPreeditAt) {
  Table table;
  // "す゛", "ず"
  table.AddRule("\xe3\x81\x99\xe3\x82\x9b", "\xe3\x81\x9a", "");
  // "く゛", "ぐ"
  table.AddRule("\xe3\x81\x8f\xe3\x82\x9b", "\xe3\x81\x90", "");
  composition_->SetTable(&table);

  size_t pos = 0;
  // "も"
  pos = composition_->InsertKeyAndPreeditAt(pos, "m", "\xe3\x82\x82");
  // "す"
  pos = composition_->InsertKeyAndPreeditAt(pos, "r", "\xe3\x81\x99");
  // "゛"
  pos = composition_->InsertKeyAndPreeditAt(pos, "@", "\xe3\x82\x9b");
  // "く"
  pos = composition_->InsertKeyAndPreeditAt(pos, "h", "\xe3\x81\x8f");
  pos = composition_->InsertKeyAndPreeditAt(pos, "!", "!");

  string comp_str;
  composition_->GetString(&comp_str);
  // "もずく!"
  EXPECT_EQ("\xe3\x82\x82\xe3\x81\x9a\xe3\x81\x8f\x21", comp_str);

  string comp_ascii_str;
  composition_->GetStringWithTransliterator(kRawT12r, &comp_ascii_str);
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

  string comp_str;
  composition_->GetString(&comp_str);
  EXPECT_EQ("ny[NYA]", comp_str);
}

TEST_F(CompositionTest, GetStringWithDisplayMode_ForKana) {
  Table table;
  // Empty table is OK.
  composition_->SetTable(&table);

  size_t pos = 0;
  // "も"
  pos = composition_->InsertKeyAndPreeditAt(pos, "m", "\xe3\x82\x82");

  string comp_str;
  composition_->GetStringWithTransliterator(kRawT12r, &comp_str);
  EXPECT_EQ("m", comp_str);
}

TEST_F(CompositionTest, InputMode) {
  Table table;
  // "あ"
  table.AddRule("a", "\xE3\x81\x82", "");
  // "か"
  table.AddRule("ka", "\xE3\x81\x8B", "");
  composition_->SetTable(&table);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "k");

  string result;
  composition_->GetString(&result);
  // "k"
  EXPECT_EQ("k", result);

  composition_->SetInputMode(kFullKatakanaT12r);
  pos = composition_->InsertAt(pos, "a");
  composition_->GetString(&result);
  // If a vowel and a consonant were typed with different
  // transliterators, these characters should not be combined.
  // "kア"
  EXPECT_EQ("\x6B\xE3\x82\xA2", result);

  composition_->SetInputMode(kHalfAsciiT12r);
  pos = composition_->InsertAt(pos, "k");
  composition_->GetString(&result);
  // "kアk"
  EXPECT_EQ("\x6B\xE3\x82\xA2\x6B", result);

  composition_->SetInputMode(kHiraganaT12r);
  pos = composition_->InsertAt(pos, "a");
  composition_->GetString(&result);
  // "kアkあ"
  EXPECT_EQ("\x6B\xE3\x82\xA2\x6B\xE3\x81\x82", result);

  // "|kアkあ"
  EXPECT_EQ(kDefaultT12r, composition_->GetTransliterator(0));
  // "k|アkあ"
  EXPECT_EQ(kDefaultT12r, composition_->GetTransliterator(1));
  // "kア|kあ"
  EXPECT_EQ(kFullKatakanaT12r, composition_->GetTransliterator(2));
  // "kアk|あ"
  EXPECT_EQ(kHalfAsciiT12r, composition_->GetTransliterator(3));
  // "kアkあ|"
  EXPECT_EQ(kHiraganaT12r, composition_->GetTransliterator(4));
  // "kアkあ...|"
  EXPECT_EQ(kHiraganaT12r, composition_->GetTransliterator(5));
  EXPECT_EQ(kHiraganaT12r, composition_->GetTransliterator(10));
}

TEST_F(CompositionTest, SetTable) {
  Table table;
  // "あ"
  table.AddRule("a", "\xE3\x81\x82", "");
  // "か"
  table.AddRule("ka", "\xE3\x81\x8B", "");

  Table table2;
  // "あ"
  table2.AddRule("a", "\xE3\x81\x82", "");
  // "か"
  table2.AddRule("ka", "\xE3\x81\x8B", "");

  composition_->SetTable(&table);
  composition_->SetInputMode(kHiraganaT12r);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "k");

  string result;
  composition_->GetString(&result);
  // "ｋ"
  EXPECT_EQ("\xEF\xBD\x8B", result);

  composition_->SetTable(&table2);

  pos = composition_->InsertAt(pos, "a");
  composition_->GetString(&result);
  // "ｋあ";
  EXPECT_EQ("\xEF\xBD\x8B\xE3\x81\x82", result);
}

TEST_F(CompositionTest, Transliterator) {
  Table table;
  // "あ"
  table.AddRule("a", "\xE3\x81\x82", "");
  composition_->SetTable(&table);

  // Insert "a" which is converted to "あ".
  size_t pos = 0;
  pos = composition_->InsertAt(pos, "a");
  EXPECT_EQ(1, pos);
  string result;
  composition_->GetString(&result);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", result);

  // Set transliterator for Half Ascii.
  composition_->SetTransliterator(0, pos, kHalfAsciiT12r);
  composition_->GetString(&result);
  EXPECT_EQ("a", result);

  // Insert "a" again.
  pos = composition_->InsertAt(pos, "a");
  EXPECT_EQ(2, pos);
  result.clear();
  composition_->GetString(&result);
  // "aあ"
  EXPECT_EQ("a\xE3\x81\x82", result);

  // Set transliterator for Full Katakana.
  composition_->SetTransliterator(0, pos, kFullKatakanaT12r);
  composition_->GetString(&result);
  // "アア"
  EXPECT_EQ("\xE3\x82\xA2\xE3\x82\xA2", result);
}

TEST_F(CompositionTest, HalfAsciiTransliterator) {
  Table table;
  // "ー"
  table.AddRule("-", "\xE3\x83\xBC", "");
  composition_->SetTable(&table);
  composition_->SetInputMode(kHalfAsciiT12r);

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
  Table table;
  composition_->SetTable(&table);

  size_t pos = 0;

  composition_->SetInputMode(kFullAsciiT12r);
  // "ち"
  pos = composition_->InsertKeyAndPreeditAt(pos, "a", "\xE3\x81\xA1");
  // "ａ"
  EXPECT_EQ("\xEF\xBD\x81", GetString(*composition_));

  pos = composition_->InsertAt(pos, " ");
  // "ａ　"
  EXPECT_EQ("\xEF\xBD\x81\xE3\x80\x80", GetString(*composition_));
}


TEST_F(CompositionTest, Issue1817410) {
  // This is a unittest against http://b/2190364
  Table table;
  // "っ"
  table.AddRule("ss", "\xE3\x81\xA3", "s");
  composition_->SetTable(&table);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "s");
  pos = composition_->InsertAt(pos, "s");

  string preedit;
  composition_->GetString(&preedit);
  // "っs"
  EXPECT_EQ("\xE3\x81\xA3\x73", preedit);

  EXPECT_EQ(0, composition_->ConvertPosition(0, kNullT12r, kHalfAsciiT12r));
  EXPECT_EQ(1, composition_->ConvertPosition(1, kNullT12r, kHalfAsciiT12r));
  EXPECT_EQ(2, composition_->ConvertPosition(2, kNullT12r, kHalfAsciiT12r));

  {  // "s|s"
    CharChunkList::iterator chunk_it;
    size_t inner_position;
    composition_->GetChunkAt(1, kNullT12r, &chunk_it, &inner_position);
    EXPECT_EQ(1, inner_position);
    EXPECT_EQ(2, (*chunk_it)->GetLength(kNullT12r));

    EXPECT_EQ(0, composition_->GetPosition(kHalfAsciiT12r, chunk_it));
    EXPECT_EQ(2, (*chunk_it)->GetLength(kHalfAsciiT12r));
  }

  {  // "ss|"
    CharChunkList::iterator chunk_it;
    size_t inner_position;
    composition_->GetChunkAt(2, kNullT12r, &chunk_it, &inner_position);
    EXPECT_EQ(2, inner_position);
    EXPECT_EQ(2, (*chunk_it)->GetLength(kNullT12r));

    EXPECT_EQ(0, composition_->GetPosition(kHalfAsciiT12r, chunk_it));
    EXPECT_EQ(2, (*chunk_it)->GetLength(kHalfAsciiT12r));
  }
}

TEST_F(CompositionTest, Issue2209634) {
  // This is a unittest against http://b/2209634
  // "q@" becomes "qた@".
  Table table;
  // "た"
  table.AddRule("q", "", "\xE3\x81\x9F");
  // "た@", "だ"
  table.AddRule("\xE3\x81\x9F\x40", "\xE3\x81\xA0", "");
  composition_->SetTable(&table);

  composition_->SetInputMode(kHalfAsciiT12r);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "q");
  pos = composition_->InsertAt(pos, "@");

  string preedit;
  composition_->GetString(&preedit);
  EXPECT_EQ("q@", preedit);
}

TEST_F(CompositionTest, Issue2330530) {
  // This is a unittest against http://b/2330530
  // "Win" + Numpad7 becomes "Win77" instead of "Win7".
  Table table;
  // "うぃ"
  table.AddRule("wi", "\xe3\x81\x86\xe3\x81\x83", "");
  // "い"
  table.AddRule("i", "\xe3\x81\x84", "");
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  composition_->SetTable(&table);

  composition_->SetInputMode(kHalfAsciiT12r);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "W");
  pos = composition_->InsertAt(pos, "i");
  pos = composition_->InsertAt(pos, "n");

  string preedit;
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

  composition_->SetTable(&table);

  composition_->SetInputMode(kHiraganaT12r);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "n");
  pos = composition_->InsertAt(pos, "y");
  {
    string output;
    composition_->GetStringWithTrimMode(FIX, &output);
    // "んｙ"
    EXPECT_EQ("\xe3\x82\x93\xef\xbd\x99", output);

    composition_->GetStringWithTrimMode(ASIS, &output);
    // "ｎｙ"
    EXPECT_EQ("\xef\xbd\x8e\xef\xbd\x99", output);

    composition_->GetStringWithTrimMode(TRIM, &output);
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

  composition_->SetTable(&table);

  composition_->SetInputMode(kHiraganaT12r);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "n");
  pos = composition_->InsertAt(pos, "y");
  pos = 1;
  pos = composition_->InsertAt(pos, "b");
  {
    string output;
    composition_->GetStringWithTrimMode(FIX, &output);
    // "んｂｙ"
    EXPECT_EQ("\xe3\x82\x93\xef\xbd\x82\xef\xbd\x99", output);

    composition_->GetStringWithTrimMode(ASIS, &output);
    // "んｂｙ"
    EXPECT_EQ("\xe3\x82\x93\xef\xbd\x82\xef\xbd\x99", output);

    composition_->GetStringWithTrimMode(TRIM, &output);
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

  composition_->SetTable(&table);

  composition_->SetInputMode(kHiraganaT12r);

  size_t pos = 0;
  pos = composition_->InsertAt(pos, "n");
  pos = composition_->InsertAt(pos, "y");
  pos = 1;
  pos = composition_->InsertAt(pos, "b");
  pos = 3;
  pos = composition_->InsertAt(pos, "o");
  {
    string output;
    composition_->GetStringWithTrimMode(FIX, &output);
    // "んびょ"
    EXPECT_EQ("\xe3\x82\x93\xe3\x81\xb3\xe3\x82\x87", output);

    composition_->GetStringWithTrimMode(ASIS, &output);
    // "んびょ"
    EXPECT_EQ("\xe3\x82\x93\xe3\x81\xb3\xe3\x82\x87", output);

    composition_->GetStringWithTrimMode(TRIM, &output);
    // "んびょ"
    EXPECT_EQ("\xe3\x82\x93\xe3\x81\xb3\xe3\x82\x87", output);
  }
}

TEST_F(CompositionTest, InsertionIntoPreeditMakesInvalidText_2) {
  // http://b/2990358
  // Test for mainly Composition::InsertKeyAndPreeditAt()

  Table table;
  // "す゛", "ず"
  table.AddRule("\xe3\x81\x99\xe3\x82\x9b", "\xe3\x81\x9a", "");
  // "く゛", "ぐ"
  table.AddRule("\xe3\x81\x8f\xe3\x82\x9b", "\xe3\x81\x90", "");
  composition_->SetTable(&table);

  size_t pos = 0;
  // "も"
  pos = composition_->InsertKeyAndPreeditAt(pos, "m", "\xe3\x82\x82");
  // "す"
  pos = composition_->InsertKeyAndPreeditAt(pos, "r", "\xe3\x81\x99");
  // "く"
  pos = composition_->InsertKeyAndPreeditAt(pos, "h", "\xe3\x81\x8f");
  // "゛"
  pos = composition_->InsertKeyAndPreeditAt(2, "@", "\xe3\x82\x9b");
  pos = composition_->InsertKeyAndPreeditAt(5, "!", "!");

  string comp_str;
  composition_->GetString(&comp_str);
  // "もずく!"
  EXPECT_EQ("\xe3\x82\x82\xe3\x81\x9a\xe3\x81\x8f\x21", comp_str);

  string comp_ascii_str;
  composition_->GetStringWithTransliterator(kRawT12r, &comp_ascii_str);
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
    Composition comp(&table);
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
    Composition comp(&table);
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
    Composition comp(&table);
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
    Composition comp(&table);
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
    Composition comp(&table);
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

TEST_F(CompositionTest, NewChunkBehaviors) {
  Table table;
  // "ん"
  table.AddRule("n", "", "\xe3\x82\x93");
  // "な"
  table.AddRule("na", "", "\xe3\x81\xaa");
  // "あ"
  table.AddRuleWithAttributes("a", "", "\xE3\x81\x82", NEW_CHUNK);
  // "ん*" -> "猫"
  table.AddRule("\xe3\x82\x93*", "", "\xE7\x8C\xAB");
  // "*" -> ""
  table.AddRule("*", "", "");
  // "ん#" -> "猫"
  table.AddRule("\xe3\x82\x93#", "", "\xE7\x8C\xAB");

  composition_->SetTable(&table);

  CompositionInput input;
  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("a", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    // "nあ"
    EXPECT_EQ("n\xe3\x81\x82", GetString(*composition_));
  }
  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("a", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    // "な"
    EXPECT_EQ("\xe3\x81\xaa", GetString(*composition_));
  }

  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("*", "", true, &input);
    pos = composition_->InsertInput(pos, input);
    // "猫"
    EXPECT_EQ("\xE7\x8C\xAB", GetString(*composition_));
  }
  {
    size_t pos = 0;
    composition_->Erase();
    SetInput("n", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    SetInput("*", "", false, &input);
    pos = composition_->InsertInput(pos, input);
    // "猫"
    EXPECT_EQ("\xE7\x8C\xAB", GetString(*composition_));
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
    // "猫"
    EXPECT_EQ("\xE7\x8C\xAB", GetString(*composition_));
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
    // "ん1"
    EXPECT_EQ("\xE3\x82\x93""1", GetString(*composition_));
  }
}

TEST_F(CompositionTest, TwelveKeysInput) {
  // Simulates flick + toggle input mode.

  Table table;
  // "ん"
  table.AddRule("n", "", "\xe3\x82\x93");
  // "な"
  table.AddRule("na", "", "\xe3\x81\xaa");
  // "あ"
  table.AddRule("a", "", "\xE3\x81\x82");
  // "*" -> ""
  table.AddRule("*", "", "");
  // "ほ" + "*" ->"ぼ"
  table.AddRule("\xE3\x81\xBB*", "", "\xE3\x81\xBC");
  // "は"
  table.AddRuleWithAttributes("7", "", "\xE3\x81\xAF", NEW_CHUNK);
  // "は7" -> "ひ"
  table.AddRule("\xE3\x81\xAF""7", "", "\xE3\x81\xB2");
  // "ひ" + "*" ->"び"
  table.AddRule("\xE3\x81\xB2*", "", "\xE3\x81\xB3");

  composition_->SetTable(&table);

  CompositionInput input;
  size_t pos = 0;

  SetInput("n", "", true, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("a", "", false, &input);
  pos = composition_->InsertInput(pos, input);

  // "ほ"
  SetInput("7", "\xE3\x81\xBB", false, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("*", "", true, &input);
  pos = composition_->InsertInput(pos, input);

  // "ひ"
  SetInput("7", "\xE3\x81\xB2", false, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("7", "", true, &input);
  pos = composition_->InsertInput(pos, input);

  // "は"
  SetInput("7", "\xE3\x81\xAF", false, &input);
  pos = composition_->InsertInput(pos, input);

  SetInput("7", "", true, &input);
  pos = composition_->InsertInput(pos, input);

  // "は"
  SetInput("7", "\xE3\x81\xAF", false, &input);
  pos = composition_->InsertInput(pos, input);

  // "なぼひはははは"
  EXPECT_EQ("\xE3\x81\xAA\xE3\x81\xBC\xE3\x81\xB2\xE3\x81\xAF"
            "\xE3\x81\xAF\xE3\x81\xAF\xE3\x81\xAF",
            GetString(*composition_));
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
  // "っ"
  table.AddRule("ss", "\xE3\x81\xA3", "s");
  // "し"
  table.AddRule("shi", "\xE3\x81\x97", "");

  composition_->SetTable(&table);
  composition_->SetInputMode(kHiraganaT12r);
  size_t pos = 0;
  pos = composition_->InsertAt(pos, "s");
  pos = composition_->InsertAt(pos, "s");
  pos = composition_->InsertAt(pos, "h");
  EXPECT_EQ(3, pos);

  string output;
  composition_->GetStringWithTrimMode(FIX, &output);
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

  composition_->SetTable(&table);
  composition_->SetInputMode(kHiraganaT12r);
  size_t pos = 0;
  pos = composition_->InsertAt(pos, "w");
  pos = composition_->InsertAt(pos, "w");
  pos = composition_->InsertAt(pos, "w");

  // "ｗｗｗ"
  EXPECT_EQ("\xEF\xBD\x97" "\xEF\xBD\x97" "\xEF\xBD\x97",
            GetString(*composition_));

  pos = composition_->InsertAt(pos, "e");
  // "ｗっうぇ"
  EXPECT_EQ("\xEF\xBD\x97" "\xE3\x81\xA3" "\xE3\x81\x86\xE3\x81\x87",
            GetString(*composition_));
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

TEST_F(CompositionTest, NoTransliteration_Issue3497962) {
  table_->AddRuleWithAttributes("2", "", "a", NEW_CHUNK | NO_TRANSLITERATION);
  table_->AddRuleWithAttributes("a2", "", "b", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("b2", "", "c", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("c2", "", "{2}2", NO_TABLE_ATTRIBUTE);
  table_->AddRuleWithAttributes("{2}22", "", "a", NO_TABLE_ATTRIBUTE);

  composition_->SetInputMode(kHiraganaT12r);

  int pos = 0;
  pos = composition_->InsertAt(pos, "2");
  EXPECT_EQ("a", GetString(*composition_));

  pos = composition_->InsertAt(pos, "2");
  EXPECT_EQ("b", GetString(*composition_));
}

TEST_F(CompositionTest, SetTransliteratorOnEmpty) {
  composition_->SetTransliterator(0, 0, kHiraganaT12r);
  CompositionInput input;
  SetInput("a", "", true, &input);
  composition_->InsertInput(0, input);
  EXPECT_EQ(1, composition_->GetLength());
}

TEST_F(CompositionTest, Clone) {
  Composition src(table_.get());
  src.SetInputMode(kFullKatakanaT12r);

  // "も"
  AppendChunk("\xe3\x82\x82", "", "mo", &src);
  // "ず"
  AppendChunk("\xe3\x81\x9a", "", "z", &src);
  // "く"
  AppendChunk("\xe3\x81\x8f", "", "c", &src);

  EXPECT_TRUE(table_.get() == src.table_);
  EXPECT_TRUE(kFullKatakanaT12r == src.input_t12r_);
  EXPECT_EQ(3, src.chunks_.size());

  scoped_ptr<Composition> dest(src.CloneImpl());
  ASSERT_TRUE(dest.get());
  EXPECT_TRUE(src.table_ == dest->table_);
  EXPECT_TRUE(src.input_t12r_ == dest->input_t12r_);
  ASSERT_EQ(src.chunks_.size(), dest->chunks_.size());

  CharChunkList::iterator src_it = src.chunks_.begin();
  CharChunkList::iterator dest_it = dest->chunks_.begin();
  for (; src_it != src.chunks_.end(); ++src_it, ++dest_it) {
    EXPECT_EQ((*src_it)->raw(), (*dest_it)->raw());
  }
}

}  // namespace composer
}  // namespace mozc

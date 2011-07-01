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

#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "converter/converter_interface.h"
#include "converter/node_allocator.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/suppression_dictionary.h"
#include "session/config.pb.h"
#include "session/config_handler.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

class ConverterTest : public testing::Test {
 public:
  virtual void SetUp() {
    // set default user profile directory
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    // just in case, reset the config in test_tmpdir
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    SuffixDictionaryFactory::SetSuffixDictionary(NULL);
  }
};

// test for issue:2209644
// just checking whether this causes segmentation fault or not.
// TODO(toshiyuki): make dictionary mock and test strictly.
TEST_F(ConverterTest, CanConvertTest) {
  ConverterInterface *converter = ConverterFactory::GetConverter();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, string("-")));
  }
  {
    Segments segments;
    // "おきておきて"
    EXPECT_TRUE(converter->StartConversion(
        &segments,
        string("\xe3\x81\x8a\xe3\x81\x8d\xe3\x81\xa6\xe3\x81\x8a"
               "\xe3\x81\x8d\xe3\x81\xa6")));
  }
}

namespace {
string ContextAwareConvert(const string &first_key,
                           const string &first_value,
                           const string &second_key) {
  ConverterInterface *converter = ConverterFactory::GetConverter();
  CHECK(converter);
  converter->ClearUserHistory();

  Segments segments;
  EXPECT_TRUE(converter->StartConversion(&segments, first_key));

  int position = -1;
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    if (first_value == segments.segment(0).candidate(i).value) {
      position = static_cast<int>(i);
      break;
    }
  }
  EXPECT_GE(position, 0) << first_value;

  EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, position))
      << first_value;
  EXPECT_TRUE(converter->FinishConversion(&segments));

  EXPECT_TRUE(converter->StartConversion(&segments, second_key));
  EXPECT_EQ(2, segments.segments_size());

  return segments.segment(1).candidate(0).value;
}
}  // anonymous namespace

TEST_F(ConverterTest, ContextAwareConversionTest) {
  // EXPECT_EQ("一髪", ContextAwareConvert("きき", "危機", "いっぱつ"));
  // EXPECT_EQ("大", ContextAwareConvert("きょうと", "京都", "だい"));
  // EXPECT_EQ("点", ContextAwareConvert("もんだい", "問題", "てん"));
  // EXPECT_EQ("來未", ContextAwareConvert("こうだ", "倖田", "くみ"));
  // EXPECT_EQ("々", ContextAwareConvert("ねん", "年", "ねん"));
  // EXPECT_EQ("々", ContextAwareConvert("ひと", "人", "びと"));

  EXPECT_EQ("\xE4\xB8\x80\xE9\xAB\xAA",
            ContextAwareConvert(
                "\xE3\x81\x8D\xE3\x81\x8D",
                "\xE5\x8D\xB1\xE6\xA9\x9F",
                "\xE3\x81\x84\xE3\x81\xA3\xE3\x81\xB1\xE3\x81\xA4"));
  EXPECT_EQ("\xE5\xA4\xA7",
            ContextAwareConvert(
                "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8",
                "\xE4\xBA\xAC\xE9\x83\xBD",
                "\xE3\x81\xA0\xE3\x81\x84"));
  EXPECT_EQ("\xE7\x82\xB9",
            ContextAwareConvert(
                "\xE3\x82\x82\xE3\x82\x93\xE3\x81\xA0\xE3\x81\x84",
                "\xE5\x95\x8F\xE9\xA1\x8C",
                "\xE3\x81\xA6\xE3\x82\x93"));
  EXPECT_EQ("\xE4\xBE\x86\xE6\x9C\xAA",
            ContextAwareConvert(
                "\xE3\x81\x93\xE3\x81\x86\xE3\x81\xA0",
                "\xE5\x80\x96\xE7\x94\xB0",
                "\xE3\x81\x8F\xE3\x81\xBF"));
  EXPECT_EQ("\xE3\x80\x85",
            ContextAwareConvert(
                "\xE3\x81\xAD\xE3\x82\x93",
                "\xE5\xB9\xB4",
                "\xE3\x81\xAD\xE3\x82\x93"));
  EXPECT_EQ("\xE3\x80\x85",
            ContextAwareConvert(
                "\xE3\x81\xB2\xE3\x81\xA8",
                "\xE4\xBA\xBA",
                "\xE3\x81\xB3\xE3\x81\xA8"));
}

TEST_F(ConverterTest, CandidateKeyTest) {
  ConverterInterface *converter = ConverterFactory::GetConverter();
  CHECK(converter);
  Segments segments;
  // "わたしは"
  EXPECT_TRUE(converter->StartConversion(
      &segments,
      string("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF")));
  EXPECT_EQ(1, segments.segments_size());
  // "わたしは"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF",
            segments.segment(0).candidate(0).key);
  // "わたし"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97",
            segments.segment(0).candidate(0).content_key);
}

TEST_F(ConverterTest, QueryOfDeathTest) {
  ConverterInterface *converter = ConverterFactory::GetConverter();
  CHECK(converter);
  Segments segments;
  // "りゅきゅけmぽ"
  EXPECT_TRUE(converter->StartConversion(
      &segments,
      "\xE3\x82\x8A\xE3\x82\x85"
      "\xE3\x81\x8D\xE3\x82\x85"
      "\xE3\x81\x91"
      "m"
      "\xE3\x81\xBD"));
}

TEST_F(ConverterTest, Regression3323108) {
  ConverterInterface *converter = ConverterFactory::GetConverter();
  Segments segments;

  // "ここではきものをぬぐ"
  EXPECT_TRUE(converter->StartConversion(
      &segments,
      "\xE3\x81\x93\xE3\x81\x93\xE3\x81\xA7"
      "\xE3\x81\xAF\xE3\x81\x8D\xE3\x82\x82"
      "\xE3\x81\xAE\xE3\x82\x92\xE3\x81\xAC"
      "\xE3\x81\x90"));
  EXPECT_EQ(3, segments.conversion_segments_size());
  EXPECT_TRUE(converter->ResizeSegment(
      &segments, 1, 2));
  EXPECT_EQ(2, segments.conversion_segments_size());

  // "きものをぬぐ"
  EXPECT_EQ("\xE3\x81\x8D\xE3\x82\x82"
            "\xE3\x81\xAE\xE3\x82\x92"
            "\xE3\x81\xAC\xE3\x81\x90",
            segments.conversion_segment(1).key());
}

TEST_F(ConverterTest, Regression3437022) {
  ConverterInterface *converter = ConverterFactory::GetConverter();
  Segments segments;

  // "しっこうゆうよ"
  const char kKey[] = "\xE3\x81\x97\xE3\x81\xA3\xE3\x81\x93"
      "\xE3\x81\x86\xE3\x82\x86\xE3\x81\x86\xE3\x82\x88";

  // "執行猶予"
  const char kValue[] = "\xE5\x9F\xB7\xE8\xA1\x8C"
      "\xE7\x8C\xB6\xE4\xBA\x88";

  EXPECT_TRUE(converter->StartConversion(
      &segments, kKey));
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ(kValue, segments.conversion_segment(0).candidate(0).value);

  SuppressionDictionary *dic
      = SuppressionDictionary::GetSuppressionDictionary();
  dic->Lock();
  dic->AddEntry(kKey, kValue);
  dic->UnLock();

  EXPECT_TRUE(converter->StartConversion(
      &segments, kKey));

  EXPECT_NE(1, segments.conversion_segments_size());
  EXPECT_TRUE(converter->ResizeSegment(
      &segments, 0, 3));

  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_NE(kValue, segments.conversion_segment(0).candidate(0).value);

  dic->Lock();
  dic->Clear();
  dic->UnLock();
}

namespace {
struct Token {
  const char *key;
  const char *value;
  uint16 rid;
  int32 cost;
};

const Token kTestTokens[] = {
  // { "です", "です", 1, 100 },
  // { "そうです", "そうです", 2, 100 },
  // { "す", "す", 3, 100 },
  // { "です", "です", 4, 50 }
  { "\xE3\x81\xA7\xE3\x81\x99", "\xE3\x81\xA7\xE3\x81\x99", 1, 100 },
  { "\xE3\x81\x9D\xE3\x81\x86\xE3\x81\xA7\xE3\x81\x99",
    "\xE3\x81\x9D\xE3\x81\x86\xE3\x81\xA7\xE3\x81\x99", 2, 100 },
  { "\xE3\x81\x99", "\xE3\x81\x99", 3, 100 },
  { "\xE3\x81\xA7\xE3\x81\x99", "\xE3\x81\xA7\xE3\x81\x99", 4, 50 }
};

class TestDictionary : public DictionaryInterface {
 public:
  TestDictionary() {}
  virtual ~TestDictionary() {}

  virtual Node *LookupPredictive(const char *str, int size,
                                 NodeAllocatorInterface *allocator) const {
    Node *result = NULL;
    for (int i = 0; i < arraysize(kTestTokens); ++i) {
      Node *n = allocator->NewNode();
      n->Init();
      const Token *token = &kTestTokens[i];
      n->value = token->value;
      n->key = token->key;
      n->lid = 0;
      n->rid = token->rid;
      n->cost = token->cost;
      n->bnext = result;
      result = n;
    }

    return result;
  }

  virtual Node *LookupPrefix(const char *str, int size,
                             NodeAllocatorInterface *allocator) const {
    return NULL;
  }

  virtual Node *LookupReverse(const char *str, int size,
                              NodeAllocatorInterface *allocator) const {
    return NULL;
  }
};
}   // namespace

TEST_F(ConverterTest, CompletePOSIds) {
  Segment::Candidate candidate;

  TestDictionary test_dictionary;
  SuffixDictionaryFactory::SetSuffixDictionary(&test_dictionary);

  // NO rewrite
  {
    candidate.lid = 1;
    candidate.rid = 2;
    candidate.value = "test";
    ConverterUtil::CompletePOSIds(&candidate);
    EXPECT_EQ(1, candidate.lid);
    EXPECT_EQ(2, candidate.rid);
  }

  {
    candidate.lid = 0;
    candidate.rid = 0;
    //    candidate.value = "たべそうです";
    candidate.value =
        "\xE3\x81\x9F\xE3\x81\xB9\xE3\x81\x9D\xE3\x81\x86"
        "\xE3\x81\xA7\xE3\x81\x99";
    ConverterUtil::CompletePOSIds(&candidate);
    EXPECT_EQ(POSMatcher::GetUnknownId(), candidate.lid);
    // "そうです" is the longest match.
    EXPECT_EQ(2, candidate.rid);
  }

  {
    candidate.lid = 0;
    candidate.rid = 0;
    //    candidate.value = "おす";
    candidate.value = "\xE3\x81\x8A\xE3\x81\x99";
    ConverterUtil::CompletePOSIds(&candidate);
    EXPECT_EQ(POSMatcher::GetUnknownId(), candidate.lid);
    EXPECT_EQ(3, candidate.rid);
  }

  {
    candidate.lid = 0;
    candidate.rid = 0;
    //    candidate.value = "です";
    candidate.value = "\xE3\x81\xA7\xE3\x81\x99";
    ConverterUtil::CompletePOSIds(&candidate);
    EXPECT_EQ(POSMatcher::GetUnknownId(), candidate.lid);
    // Have two "です". Select one who has the minimum cost.
    EXPECT_EQ(4, candidate.rid);
  }

  {
    candidate.lid = 0;
    candidate.rid = 0;
    //    candidate.value = "うごくです";
    candidate.value = "\xE3\x81\x86\xE3\x81\x94\xE3\x81\x8F"
        "\xE3\x81\xA7\xE3\x81\x99";
    ConverterUtil::CompletePOSIds(&candidate);
    EXPECT_EQ(POSMatcher::GetUnknownId(), candidate.lid);
    EXPECT_EQ(4, candidate.rid);
  }

  // completely unknown.
  {
    candidate.lid = 0;
    candidate.rid = 0;
    // candidate.value = "京都";
    candidate.value = "\xE4\xBA\xAC\xE9\x83\xBD";
    ConverterUtil::CompletePOSIds(&candidate);
    EXPECT_EQ(POSMatcher::GetUnknownId(), candidate.lid);
    EXPECT_EQ(POSMatcher::GetUnknownId(), candidate.rid);
  }
}
}  // namespace mozc

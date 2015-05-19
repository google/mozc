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

#include <string>
#include <vector>

#include "base/util.h"
#include "languages/pinyin/english_dictionary.h"
#include "testing/base/public/gunit.h"

// This test may be unstable because it depends on real english dictionary.

DECLARE_string(test_tmpdir);

namespace mozc {
namespace pinyin {
namespace english {

class EnglishDictionaryTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    UnlinkUserHistoryDatabase();
  }

  virtual void TearDown() {
    UnlinkUserHistoryDatabase();
  }

  void UnlinkUserHistoryDatabase() {
    Util::Unlink(EnglishDictionary::file_name());
  }
};

TEST_F(EnglishDictionaryTest, GetSuggestions) {
  EnglishDictionary dictionary;

  {  // Searches with an empty query.
    vector<string> output;
    EXPECT_FALSE(dictionary.GetSuggestions("", &output));
  }

  {  // Searches with normal queries.
    vector<string> output;
    ASSERT_TRUE(dictionary.GetSuggestions("th", &output));
    ASSERT_FALSE(output.empty());
    EXPECT_EQ("the", output.front());
    const size_t th_size = output.size();

    ASSERT_TRUE(dictionary.GetSuggestions("tHa", &output));
    ASSERT_FALSE(output.empty());
    EXPECT_EQ("that", output.front());
    const size_t tha_size = output.size();

    EXPECT_GT(th_size, tha_size);

    ASSERT_TRUE(dictionary.GetSuggestions("OF", &output));
    ASSERT_FALSE(output.empty());
    EXPECT_EQ("of", output.front());
  }

  {  // Searches with illegal queries.
    vector<string> output;
    EXPECT_FALSE(dictionary.GetSuggestions("-", &output));
  }
}

TEST_F(EnglishDictionaryTest, LearningFunction) {
  EnglishDictionary dictionary;

  const char *kQueryPrefix = "the";
  vector<string> output;

  const string word_a = Util::StringPrintf("%s%s", kQueryPrefix, "abcde");
  const string word_b = Util::StringPrintf("%s%s", kQueryPrefix, "fghij");

  ASSERT_TRUE(dictionary.GetSuggestions(kQueryPrefix, &output));
  ASSERT_FALSE(find(output.begin(), output.end(), word_a) != output.end());
  ASSERT_FALSE(find(output.begin(), output.end(), word_b) != output.end());
  const size_t original_size = output.size();

  {  // Learns word_a once.
    dictionary.LearnWord(word_a);
    ASSERT_TRUE(dictionary.GetSuggestions(kQueryPrefix, &output));
    EXPECT_EQ(original_size + 1, output.size());
    EXPECT_TRUE(find(output.begin(), output.end(), word_a) != output.end());
  }

  {  // Learns word_b twice.
    dictionary.LearnWord(word_b);
    dictionary.LearnWord(word_b);
    EXPECT_TRUE(dictionary.GetSuggestions(kQueryPrefix, &output));
    EXPECT_EQ(original_size + 2, output.size());
    vector<string>::iterator it_a = find(output.begin(), output.end(), word_a);
    vector<string>::iterator it_b = find(output.begin(), output.end(), word_b);
    EXPECT_TRUE(it_a != output.end());
    EXPECT_TRUE(it_b != output.end());
    EXPECT_TRUE(it_a > it_b);
  }

  {  // Learns word_a twice.
    dictionary.LearnWord(word_a);
    dictionary.LearnWord(word_a);
    EXPECT_TRUE(dictionary.GetSuggestions(kQueryPrefix, &output));
    EXPECT_EQ(original_size + 2, output.size());
    vector<string>::iterator it_a = find(output.begin(), output.end(), word_a);
    vector<string>::iterator it_b = find(output.begin(), output.end(), word_b);
    EXPECT_TRUE(it_a != output.end());
    EXPECT_TRUE(it_b != output.end());
    EXPECT_TRUE(it_a < it_b);
  }

  {  // Learns more 5 times and move word_a to top of candidates.
    ASSERT_TRUE(dictionary.GetSuggestions(kQueryPrefix, &output));
    ASSERT_EQ(original_size + 2, output.size());
    ASSERT_NE(word_a, output[0]);

    for (size_t i = 0; i < 5; ++i) {
      dictionary.LearnWord(word_a);
    }
    ASSERT_TRUE(dictionary.GetSuggestions(kQueryPrefix, &output));
    ASSERT_EQ(original_size + 2, output.size());
    EXPECT_EQ(word_a, output[0]);
  }

  {  // Learns more 100 times and move word_a to top of candidates.
    ASSERT_TRUE(dictionary.GetSuggestions(kQueryPrefix, &output));
    ASSERT_EQ(original_size + 2, output.size());
    ASSERT_NE(word_a, output[0]);

    for (size_t i = 0; i < 100; ++i) {
      dictionary.LearnWord(word_a);
    }
    ASSERT_TRUE(dictionary.GetSuggestions(kQueryPrefix, &output));
    ASSERT_EQ(original_size + 2, output.size());
    EXPECT_EQ(word_a, output[0]);
  }
}

TEST_F(EnglishDictionaryTest, LearnWordsContainsUpperAlphabet_Issue6136098) {
  EnglishDictionary dictionary;

  vector<string> output;

  const char *kWord = "abcDEFghi";
  const char *kLowerWord = "abcdefghi";

  ASSERT_TRUE(dictionary.GetSuggestions(kWord, &output));
  ASSERT_TRUE(output.empty());

  output.clear();
  dictionary.LearnWord(kWord);
  ASSERT_TRUE(dictionary.GetSuggestions(kWord, &output));
  EXPECT_EQ(1, output.size());
  EXPECT_EQ(kLowerWord, output.front());
}

TEST_F(EnglishDictionaryTest, StoreUserDictionaryToStorage) {
  const char *kUnknownWord = "thisisunknownword";
  vector<string> output;

  {  // Creates a dictionary and lean a new word.
    EnglishDictionary dictionary;
    ASSERT_FALSE(dictionary.GetSuggestions(kUnknownWord, &output));
    dictionary.LearnWord(kUnknownWord);
    ASSERT_TRUE(dictionary.GetSuggestions(kUnknownWord, &output));
    ASSERT_EQ(1, output.size());
    ASSERT_EQ(kUnknownWord, output[0]);
  }

  {  // Creates another dictionary and verifies that it has a new word.
    EnglishDictionary dictionary;
    EXPECT_TRUE(dictionary.GetSuggestions(kUnknownWord, &output));
    EXPECT_EQ(1, output.size());
    EXPECT_EQ(kUnknownWord, output[0]);
  }

  UnlinkUserHistoryDatabase();

  {  // Creates another dictionary and verifies that it doesn't have a new word.
    EnglishDictionary dictionary;
    EXPECT_FALSE(dictionary.GetSuggestions(kUnknownWord, &output));
  }
}

}  // namespace english
}  // namespace pinyin
}  // namespace mozc

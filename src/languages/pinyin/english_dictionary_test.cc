// Copyright 2010-2013, Google Inc.
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

#include "languages/pinyin/english_dictionary.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/system_util.h"
#include "base/util.h"
#include "storage/encrypted_string_storage.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

// This test requires following condition to system dictionary.
// - "the" has a highest priority in words "th*"
// - "that" has a highest priority in words "tha*"
// - "of" has a highest priority in words "of*"
// Actual dictionary entries are defined on data/pinyin/english_dictionary.txt
// It should be something wrong on generating dictionary data process if above
// condition is not satisfied.

DECLARE_string(test_tmpdir);

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::_;

namespace mozc {
namespace pinyin {
namespace english {

class MockStorage : public storage::StringStorageInterface {
 public:
  MockStorage() {}
  virtual ~MockStorage() {}

  MOCK_CONST_METHOD1(Load, bool(string *output));
  MOCK_CONST_METHOD1(Save, bool(const string &input));
};

class EnglishDictionaryTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    UnlinkUserHistoryDatabase();
  }

  virtual void TearDown() {
    UnlinkUserHistoryDatabase();
  }

  // Unlinks user history database file to reset.
  void UnlinkUserHistoryDatabase() {
    FileUtil::Unlink(EnglishDictionary::user_dictionary_file_path());
  }

  // Sets mock user dictionary storage for unit testing.
  // |*dictionary| takes a ownership of |*mock_user_dictionary_storage|.
  void SetMockUserDictionaryStorage(
      EnglishDictionary *dictionary,
      storage::StringStorageInterface *mock_user_dictionary_storage) {
    dictionary->storage_.reset(mock_user_dictionary_storage);
  }

  // Reloads user dictionary for unit testing.
  bool ReloadUserDictionary(EnglishDictionary *dictionary) {
    return dictionary->ReloadUserDictionary();
  }
};

// Checks GetSuggestions() with some famous English words.
TEST_F(EnglishDictionaryTest, GetSuggestions) {
  EnglishDictionary dictionary;

  {  // Searches with an empty query.
    vector<string> output;
    output.push_back("dummy_entry");
    dictionary.GetSuggestions("", &output);
    ASSERT_TRUE(output.empty());
  }

  {  // Searches with normal queries.
    vector<string> output;
    dictionary.GetSuggestions("th", &output);
    ASSERT_FALSE(output.empty());
    EXPECT_EQ("the", output.front());
    const size_t th_size = output.size();

    dictionary.GetSuggestions("tHa", &output);
    ASSERT_FALSE(output.empty());
    EXPECT_EQ("that", output.front());
    const size_t tha_size = output.size();

    EXPECT_GT(th_size, tha_size);

    dictionary.GetSuggestions("OF", &output);
    ASSERT_FALSE(output.empty());
    EXPECT_EQ("of", output.front());
  }

  {  // Searches with an illegal querie.
    vector<string> output;
    output.push_back("dummy_entry");
    dictionary.GetSuggestions("-", &output);
    ASSERT_TRUE(output.empty());
  }
}

// Checks LearnWord().
TEST_F(EnglishDictionaryTest, LearningFunction) {
  EnglishDictionary dictionary;

  const char *kQueryPrefix = "the";
  vector<string> output;

  const string word_a = Util::StringPrintf("%s%s", kQueryPrefix, "abcde");
  const string word_b = Util::StringPrintf("%s%s", kQueryPrefix, "fghij");

  dictionary.GetSuggestions(kQueryPrefix, &output);
  ASSERT_TRUE(find(output.begin(), output.end(), word_a) == output.end());
  ASSERT_TRUE(find(output.begin(), output.end(), word_b) == output.end());
  const size_t original_size = output.size();

  // Empty word.
  EXPECT_FALSE(dictionary.LearnWord(""));

  // Too long word.
  EXPECT_TRUE(dictionary.LearnWord(
      "0123456789" "0123456789" "0123456789" "0123456789"
      "0123456789" "0123456789" "0123456789" "0123456789"));
  EXPECT_FALSE(dictionary.LearnWord(
      "0123456789" "0123456789" "0123456789" "0123456789"
      "0123456789" "0123456789" "0123456789" "0123456789" "0"));

  {  // Learns word_a once. (a: 1, b: 0)
    EXPECT_TRUE(dictionary.LearnWord(word_a));
    dictionary.GetSuggestions(kQueryPrefix, &output);
    EXPECT_EQ(original_size + 1, output.size());
    EXPECT_NE(find(output.begin(), output.end(), word_a), output.end());
  }

  {  // Learns word_b twice. (a: 1, b: 2)
    EXPECT_TRUE(dictionary.LearnWord(word_b));
    EXPECT_TRUE(dictionary.LearnWord(word_b));
    dictionary.GetSuggestions(kQueryPrefix, &output);
    EXPECT_EQ(original_size + 2, output.size());
    vector<string>::iterator it_a = find(output.begin(), output.end(), word_a);
    vector<string>::iterator it_b = find(output.begin(), output.end(), word_b);
    EXPECT_NE(it_a, output.end());
    EXPECT_NE(it_b, output.end());
    EXPECT_GT(it_a, it_b);
  }

  {  // Learns word_a twice. (a: 3, b: 2)
    EXPECT_TRUE(dictionary.LearnWord(word_a));
    EXPECT_TRUE(dictionary.LearnWord(word_a));
    dictionary.GetSuggestions(kQueryPrefix, &output);
    EXPECT_EQ(original_size + 2, output.size());
    vector<string>::iterator it_a = find(output.begin(), output.end(), word_a);
    vector<string>::iterator it_b = find(output.begin(), output.end(), word_b);
    EXPECT_NE(it_a, output.end());
    EXPECT_NE(it_b, output.end());
    EXPECT_LT(it_a, it_b);
  }

  {  // Learns word_b once. (a: 3, b: 3)
    EXPECT_TRUE(dictionary.LearnWord(word_b));
    dictionary.GetSuggestions(kQueryPrefix, &output);
    EXPECT_EQ(original_size + 2, output.size());
    vector<string>::iterator it_a = find(output.begin(), output.end(), word_a);
    vector<string>::iterator it_b = find(output.begin(), output.end(), word_b);
    EXPECT_NE(it_a, output.end());
    EXPECT_NE(it_b, output.end());
    EXPECT_LT(it_a, it_b);
  }

  {  // Learns more 100 times and moves word_a to top of candidates.
    dictionary.GetSuggestions(kQueryPrefix, &output);
    ASSERT_EQ(original_size + 2, output.size());
    ASSERT_NE(word_a, output[0]);

    for (size_t i = 0; i < 100; ++i) {
      EXPECT_TRUE(dictionary.LearnWord(word_a));
    }
    dictionary.GetSuggestions(kQueryPrefix, &output);
    ASSERT_EQ(original_size + 2, output.size());
    EXPECT_EQ(word_a, output[0]);
  }
}

// Checks that LearnWord() handle upper case characters correctly.
// http://b/6136098
TEST_F(EnglishDictionaryTest, LearnWordsContainsUpperAlphabet_Issue6136098) {
  EnglishDictionary dictionary;

  vector<string> output;

  const char *kWord = "abcDEFghi";
  const char *kLowerWord = "abcdefghi";

  dictionary.GetSuggestions("", &output);
  ASSERT_TRUE(output.empty());

  output.clear();
  dictionary.GetSuggestions(kWord, &output);
  ASSERT_TRUE(output.empty());

  output.clear();
  EXPECT_TRUE(dictionary.LearnWord(kWord));
  dictionary.GetSuggestions(kWord, &output);
  EXPECT_EQ(1, output.size());
  EXPECT_EQ(kLowerWord, output.front());
}

// Checks that user dictionary is correctly stored to a storage.
TEST_F(EnglishDictionaryTest, StoreUserDictionaryToStorage) {
  const char *kUnknownWord = "thisisunknownword";
  vector<string> output;

  {  // Creates a dictionary and lean a new word.
    EnglishDictionary dictionary;
    dictionary.GetSuggestions(kUnknownWord, &output);
    ASSERT_TRUE(output.empty());
    EXPECT_TRUE(dictionary.LearnWord(kUnknownWord));
    dictionary.GetSuggestions(kUnknownWord, &output);
    ASSERT_EQ(1, output.size());
    ASSERT_EQ(kUnknownWord, output[0]);
  }

  {  // Creates another dictionary and verifies that it has a new word.
    EnglishDictionary dictionary;
    dictionary.GetSuggestions(kUnknownWord, &output);
    EXPECT_EQ(1, output.size());
    EXPECT_EQ(kUnknownWord, output[0]);
  }

  UnlinkUserHistoryDatabase();

  {  // Creates another dictionary and verifies that it doesn't have a new word.
    EnglishDictionary dictionary;
    dictionary.GetSuggestions(kUnknownWord, &output);
    EXPECT_TRUE(output.empty());
  }
}

// Checks that broken user dictionary is correctly handled.
TEST_F(EnglishDictionaryTest, InvalidUserDictionary) {
  EnglishDictionary dictionary;
  MockStorage *mock_storage = new MockStorage;
  SetMockUserDictionaryStorage(&dictionary, mock_storage);
  EXPECT_CALL(*mock_storage, Save(_)).WillRepeatedly(Return(true));

  // Cannot open storage.
  EXPECT_CALL(*mock_storage, Load(_)).WillOnce(Return(false));
  EXPECT_FALSE(ReloadUserDictionary(&dictionary));

  // Empty storage (success)
  EXPECT_CALL(*mock_storage, Load(_)).WillOnce(DoAll(
      SetArgPointee<0>(""), Return(true)));
  EXPECT_TRUE(ReloadUserDictionary(&dictionary));

  const char *kWrongUserDictionaryData[] = {
    "\x01",             // Wrong key length (key length: 1, key: "")
    "\x02" "a",         // Wrong key length (key length: 2, key: "a")
    "\x01" "aa",        // Wrong key length (key length: 1, key: "aa")
    "\x01" "a",         // Wrong used count length (length == 0)
    "\x01" "a" "\x00",  // Wrong used count length (length != 0 && length != 4)
  };

  for (size_t i = 0; i < arraysize(kWrongUserDictionaryData); ++i) {
    EXPECT_CALL(*mock_storage, Load(_)).WillOnce(DoAll(
        SetArgPointee<0>(kWrongUserDictionaryData[i]), Return(true)));
    EXPECT_FALSE(ReloadUserDictionary(&dictionary));
  }
}

}  // namespace english
}  // namespace pinyin
}  // namespace mozc

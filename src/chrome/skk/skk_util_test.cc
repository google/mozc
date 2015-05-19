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

#include "chrome/skk/skk_util.h"

#include <string>
#include <vector>

#include "dictionary/embedded_dictionary_data.h"
#include "dictionary/system/system_dictionary.h"
#include "testing/base/public/gunit.h"

namespace mozc {

TEST(SkkUtilTest, IsSupportedMethodTest) {
  // Currently "LOOKUP" is the only supported method name
  const string kSupportedMethod = "LOOKUP";
  const string kUnsupportedMethod1 = "NOT_SUPPORTED";
  // Case sensitivity test
  const string kUnsupportedMethod2 = "lookup";

  EXPECT_TRUE(SkkUtil::IsSupportedMethod(kSupportedMethod));
  EXPECT_FALSE(SkkUtil::IsSupportedMethod(kUnsupportedMethod1));
  EXPECT_FALSE(SkkUtil::IsSupportedMethod(kUnsupportedMethod2));
}

TEST(SkkUtilTest, RemoveDuplicateEntryTest) {
  {
    vector<string> entries(4);
    // "やまのて"
    entries[0] = "\xE3\x82\x84\xE3\x81\xBE\xE3\x81\xAE\xE3\x81\xA6";
    // "山手"
    entries[1] = "\xE5\xB1\xB1\xE6\x89\x8B";
    // "やまのて"
    entries[2] = "\xE3\x82\x84\xE3\x81\xBE\xE3\x81\xAE\xE3\x81\xA6";
    // "山の手"
    entries[3] = "\xE5\xB1\xB1\xE3\x81\xAE\xE6\x89\x8B";

    SkkUtil::RemoveDuplicateEntry(&entries);
    ASSERT_EQ(3, entries.size());

    // "やまのて"
    EXPECT_EQ("\xE3\x82\x84\xE3\x81\xBE\xE3\x81\xAE\xE3\x81\xA6", entries[0]);
    // "山手"
    EXPECT_EQ("\xE5\xB1\xB1\xE6\x89\x8B", entries[1]);
    // "山の手"
    EXPECT_EQ("\xE5\xB1\xB1\xE3\x81\xAE\xE6\x89\x8B", entries[2]);
  }

  {
    vector<string> entries(4);
    // "やまのて"
    entries[0] = "\xE3\x82\x84\xE3\x81\xBE\xE3\x81\xAE\xE3\x81\xA6";
    // "やまのて"
    entries[1] = "\xE3\x82\x84\xE3\x81\xBE\xE3\x81\xAE\xE3\x81\xA6";
    // "やまのて"
    entries[2] = "\xE3\x82\x84\xE3\x81\xBE\xE3\x81\xAE\xE3\x81\xA6";
    // "山の手"
    entries[3] = "\xE5\xB1\xB1\xE6\x89\x8B";

    SkkUtil::RemoveDuplicateEntry(&entries);
    ASSERT_EQ(2, entries.size());

    // "やまのて"
    EXPECT_EQ("\xE3\x82\x84\xE3\x81\xBE\xE3\x81\xAE\xE3\x81\xA6", entries[0]);
    // "山手"
    EXPECT_EQ("\xE5\xB1\xB1\xE6\x89\x8B", entries[1]);
  }
}

namespace {

void TestInvalidMessageValidation(const char *json) {
  string error_message;
  Json::Value parsed;
  Json::Reader().parse(json, parsed);
  EXPECT_FALSE(SkkUtil::ValidateMessage(parsed, &error_message));
  EXPECT_FALSE(error_message.empty());
}

}  // namespace

TEST(SkkUtilTest, ValidateMessageTest) {
  // Valid message
  {
    const char *kMessage = "{\n"
        "  \"id\": \"42\",\n"
        "  \"method\": \"LOOKUP\",\n"
        "  \"base\": \"\xE3\x81\xAF\xE3\x81\x97\",\n"  // 'はし'
        "  \"stem\": \"r\"\n"
        "}";
    string error_message;
    Json::Value parsed;
    Json::Reader().parse(kMessage, parsed);
    EXPECT_TRUE(SkkUtil::ValidateMessage(parsed, &error_message));
    EXPECT_TRUE(error_message.empty());
  }

  // Not a object
  {
    const char *kMessage = "42";
    TestInvalidMessageValidation(kMessage);
  }

  // Malformed JSON
  {
    const char *kMessage = "{ \"prop\": \"value\"";
    TestInvalidMessageValidation(kMessage);
  }

  // Missing ID
  {
    const char *kMessage = "{\n"
        "  \"method\": \"LOOKUP\",\n"
        "  \"base\": \"\xE3\x81\xAF\xE3\x81\x97\",\n"  // 'はし'
        "  \"stem\": \"r\"\n"
        "}";
    TestInvalidMessageValidation(kMessage);
  }

  // Missing method name
  {
    const char *kMessage = "{\n"
        "  \"id\": \"42\",\n"
        "  \"base\": \"\xE3\x81\xAF\xE3\x81\x97\",\n"  // 'はし'
        "  \"stem\": \"r\"\n"
        "}";
    TestInvalidMessageValidation(kMessage);
  }

  // Missing method parameters
  {
    const char *kMessage = "{\n"
        "  \"id\": \"42\",\n"
        "  \"method\": \"LOOKUP\",\n"
        "}";
    TestInvalidMessageValidation(kMessage);
  }
}

TEST(SkkUtilTest, LookupEntryTest) {
  mozc::SystemDictionary *dictionary
      = mozc::SystemDictionary::CreateSystemDictionaryFromImage(
          kDictionaryData_data, kDictionaryData_size);
  ASSERT_TRUE(dictionary != NULL);

  // 'ことの'
  const char *kQuery = "\xE3\x81\x93\xE3\x81\xA8\xE3\x81\xAE";
  vector<string> candidates, predictions;
  SkkUtil::LookupEntry(dictionary, kQuery, &candidates, &predictions);
  EXPECT_GT(candidates.size(), 0);
  EXPECT_GT(predictions.size(), 0);
}

}  // namespace mozc

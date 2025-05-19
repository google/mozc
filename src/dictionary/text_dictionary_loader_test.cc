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

#include "dictionary/text_dictionary_loader.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "testing/test_peer.h"

namespace mozc {
namespace dictionary {

class TextDictionaryLoaderTestPeer
    : public testing::TestPeer<TextDictionaryLoader> {
 public:
  explicit TextDictionaryLoaderTestPeer(TextDictionaryLoader &loader)
      : testing::TestPeer<TextDictionaryLoader>(loader) {}

  PEER_METHOD(RewriteSpecialToken);
};

namespace {

constexpr char kTextLines[] =
    "key_test1\t0\t0\t1\tvalue_test1\n"
    "foo\t1\t2\t3\tbar\n"
    "buz\t10\t20\t30\tfoobar\n";

constexpr char kReadingCorrectionLines[] =
    "bar\tfoo\tfoo_correct\n"
    "foobar\tfoobar_error\tfoobar_correct\n";

}  // namespace

class TextDictionaryLoaderTest : public ::testing::Test {
 protected:
  TextDictionaryLoaderTest()
      : pos_matcher_(mock_data_manager_.GetPosMatcherData()),
        temp_dir_(testing::MakeTempDirectoryOrDie()) {}

  std::unique_ptr<TextDictionaryLoader> CreateTextDictionaryLoader() {
    return std::make_unique<TextDictionaryLoader>(pos_matcher_);
  }

  testing::MockDataManager mock_data_manager_;
  PosMatcher pos_matcher_;
  TempDirectory temp_dir_;
};

TEST_F(TextDictionaryLoaderTest, BasicTest) {
  {
    std::unique_ptr<TextDictionaryLoader> loader = CreateTextDictionaryLoader();
    std::vector<Token *> tokens;
    loader->CollectTokens(&tokens);
    EXPECT_TRUE(tokens.empty());
  }

  const std::string filename = FileUtil::JoinPath(temp_dir_.path(), "test.tsv");
  ASSERT_OK(FileUtil::SetContents(filename, kTextLines));

  {
    std::unique_ptr<TextDictionaryLoader> loader = CreateTextDictionaryLoader();
    loader->Load(filename, "");
    absl::Span<const std::unique_ptr<Token>> tokens = loader->tokens();

    EXPECT_EQ(tokens.size(), 3);

    EXPECT_EQ(tokens[0]->key, "key_test1");
    EXPECT_EQ(tokens[0]->value, "value_test1");
    EXPECT_EQ(tokens[0]->lid, 0);
    EXPECT_EQ(tokens[0]->rid, 0);
    EXPECT_EQ(tokens[0]->cost, 1);

    EXPECT_EQ(tokens[1]->key, "foo");
    EXPECT_EQ(tokens[1]->value, "bar");
    EXPECT_EQ(tokens[1]->lid, 1);
    EXPECT_EQ(tokens[1]->rid, 2);
    EXPECT_EQ(tokens[1]->cost, 3);

    EXPECT_EQ(tokens[2]->key, "buz");
    EXPECT_EQ(tokens[2]->value, "foobar");
    EXPECT_EQ(tokens[2]->lid, 10);
    EXPECT_EQ(tokens[2]->rid, 20);
    EXPECT_EQ(tokens[2]->cost, 30);

    loader->Clear();
    EXPECT_TRUE(loader->tokens().empty());
  }

  {
    std::unique_ptr<TextDictionaryLoader> loader = CreateTextDictionaryLoader();
    loader->LoadWithLineLimit(filename, "", 2);
    absl::Span<const std::unique_ptr<Token>> tokens = loader->tokens();

    EXPECT_EQ(tokens.size(), 2);

    EXPECT_EQ(tokens[0]->key, "key_test1");
    EXPECT_EQ(tokens[0]->value, "value_test1");
    EXPECT_EQ(tokens[0]->lid, 0);
    EXPECT_EQ(tokens[0]->rid, 0);
    EXPECT_EQ(tokens[0]->cost, 1);

    EXPECT_EQ(tokens[1]->key, "foo");
    EXPECT_EQ(tokens[1]->value, "bar");
    EXPECT_EQ(tokens[1]->lid, 1);
    EXPECT_EQ(tokens[1]->rid, 2);
    EXPECT_EQ(tokens[1]->cost, 3);

    loader->Clear();
    EXPECT_TRUE(loader->tokens().empty());
  }

  {
    std::unique_ptr<TextDictionaryLoader> loader = CreateTextDictionaryLoader();
    // open twice -- tokens are cleared everytime
    loader->Load(filename, "");
    loader->Load(filename, "");
    absl::Span<const std::unique_ptr<Token>> tokens = loader->tokens();
    EXPECT_EQ(tokens.size(), 3);
  }

  EXPECT_OK(FileUtil::Unlink(filename));
}

TEST_F(TextDictionaryLoaderTest, RewriteSpecialTokenTest) {
  std::unique_ptr<TextDictionaryLoader> loader = CreateTextDictionaryLoader();
  TextDictionaryLoaderTestPeer loader_peer(*loader);
  {
    Token token;
    token.lid = 100;
    token.rid = 200;
    EXPECT_TRUE(loader_peer.RewriteSpecialToken(&token, ""));
    EXPECT_EQ(token.lid, 100);
    EXPECT_EQ(token.rid, 200);
    EXPECT_EQ(token.attributes, Token::NONE);
  }

  {
    Token token;
    token.lid = 100;
    token.rid = 200;
    EXPECT_TRUE(loader_peer.RewriteSpecialToken(&token, "SPELLING_CORRECTION"));
    EXPECT_EQ(token.lid, 100);
    EXPECT_EQ(token.rid, 200);
    EXPECT_EQ(token.attributes, Token::SPELLING_CORRECTION);
  }

  {
    Token token;
    token.lid = 100;
    token.rid = 200;
    EXPECT_TRUE(loader_peer.RewriteSpecialToken(&token, "ZIP_CODE"));
    EXPECT_EQ(token.lid, pos_matcher_.GetZipcodeId());
    EXPECT_EQ(token.rid, pos_matcher_.GetZipcodeId());
    EXPECT_EQ(token.attributes, Token::NONE);
  }

  {
    Token token;
    token.lid = 100;
    token.rid = 200;
    EXPECT_TRUE(loader_peer.RewriteSpecialToken(&token, "ENGLISH:RATED"));
    EXPECT_EQ(token.lid, pos_matcher_.GetIsolatedWordId());
    EXPECT_EQ(token.rid, pos_matcher_.GetIsolatedWordId());
    EXPECT_EQ(token.attributes, Token::NONE);
  }

  {
    Token token;
    token.lid = 100;
    token.rid = 200;
    EXPECT_FALSE(loader_peer.RewriteSpecialToken(&token, "foo"));
    EXPECT_EQ(token.lid, 100);
    EXPECT_EQ(token.rid, 200);
    EXPECT_EQ(token.attributes, Token::NONE);
  }
}

TEST_F(TextDictionaryLoaderTest, LoadMultipleFilesTest) {
  const std::string filename1 =
      FileUtil::JoinPath(temp_dir_.path(), "test1.tsv");
  const std::string filename2 =
      FileUtil::JoinPath(temp_dir_.path(), "test2.tsv");
  const std::string filename = filename1 + "," + filename2;

  ASSERT_OK(FileUtil::SetContents(filename1, kTextLines));
  FileUnlinker unlinker1(filename1);
  ASSERT_OK(FileUtil::SetContents(filename2, kTextLines));
  FileUnlinker unlinker2(filename2);

  {
    std::unique_ptr<TextDictionaryLoader> loader = CreateTextDictionaryLoader();
    loader->Load(filename, "");
    EXPECT_EQ(loader->tokens().size(), 6);
  }
}

TEST_F(TextDictionaryLoaderTest, ReadingCorrectionTest) {
  std::unique_ptr<TextDictionaryLoader> loader = CreateTextDictionaryLoader();

  const std::string dic_filename =
      FileUtil::JoinPath(temp_dir_.path(), "test.tsv");
  const std::string reading_correction_filename =
      FileUtil::JoinPath(temp_dir_.path(), "reading_correction.tsv");

  ASSERT_OK(FileUtil::SetContents(dic_filename, kTextLines));
  FileUnlinker dic_unlinker(dic_filename);
  ASSERT_OK(FileUtil::SetContents(reading_correction_filename,
                                  kReadingCorrectionLines));
  FileUnlinker reading_correction_unlinker(reading_correction_filename);

  loader->Load(dic_filename, reading_correction_filename);
  absl::Span<const std::unique_ptr<Token>> tokens = loader->tokens();
  ASSERT_EQ(tokens.size(), 4);
  EXPECT_EQ(tokens[3]->key, "foobar_error");
  EXPECT_EQ(tokens[3]->value, "foobar");
  EXPECT_EQ(tokens[3]->lid, 10);
  EXPECT_EQ(tokens[3]->rid, 20);
  EXPECT_EQ(tokens[3]->cost, 30 + 2302);
}

}  // namespace dictionary
}  // namespace mozc

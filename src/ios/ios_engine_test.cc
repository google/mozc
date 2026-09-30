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

#include "ios/ios_engine.h"

#include <ios>
#include <string>

#include "base/config_file_stream.h"
#include "base/file/temp_dir.h"
#include "base/file_stream.h"
#include "base/system_util.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace ios {
namespace {

TEST(IosEngineTest, ImportUserDictionary) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  SystemUtil::SetUserProfileDirectory(temp_dir.path());

  const std::string data_file_path = testing::GetSourceFileOrDie(
      {"data_manager", "testing", "mock_mozc.data"});
  IosEngine engine(data_file_path);

  commands::Command command;
  config::Config config;
  IosEngine::FillMobileConfig(&config);
  EXPECT_TRUE(engine.SetConfig(config, &command));
  EXPECT_TRUE(engine.SetMobileRequest("12KEYS", &command));
  ASSERT_TRUE(engine.CreateSession(&command));

  const std::string tsv =
      "かおもじ\t(^-^)\t顔文字\n"
      "てすと\tテスト単語\t品詞なし\n"
      "invalid_line\n"
      "\t空キー\t品詞なし\n"
      "てすと\t\t品詞なし\n";
  EXPECT_TRUE(engine.ImportUserDictionary(tsv, &command));
  EXPECT_EQ(command.input().type(), commands::Input::RELOAD);

  const std::string db_file =
      ConfigFileStream::GetFileName("user://user_dictionary.db");
  user_dictionary::UserDictionaryStorage storage;
  {
    InputFileStream ifs(db_file, std::ios::binary);
    ASSERT_TRUE(ifs.good());
    ASSERT_TRUE(storage.ParseFromIstream(&ifs));
  }
  ASSERT_EQ(storage.dictionaries_size(), 1);
  const auto& dic = storage.dictionaries(0);
  EXPECT_EQ(dic.name(), "iOS_system_dictionary");
  ASSERT_EQ(dic.entries_size(), 2);
  EXPECT_EQ(dic.entries(0).key(), "かおもじ");
  EXPECT_EQ(dic.entries(0).value(), "(^-^)");
  EXPECT_EQ(dic.entries(0).pos(), user_dictionary::UserDictionary::EMOTICON);
  EXPECT_EQ(dic.entries(1).key(), "てすと");
  EXPECT_EQ(dic.entries(1).value(), "テスト単語");
  EXPECT_EQ(dic.entries(1).pos(), user_dictionary::UserDictionary::NO_POS);

  // Importing an empty TSV clears the user dictionary.
  EXPECT_TRUE(engine.ImportUserDictionary("", &command));
  EXPECT_EQ(command.input().type(), commands::Input::RELOAD);

  user_dictionary::UserDictionaryStorage empty_storage;
  {
    InputFileStream ifs(db_file, std::ios::binary);
    ASSERT_TRUE(ifs.good());
    ASSERT_TRUE(empty_storage.ParseFromIstream(&ifs));
  }
  EXPECT_EQ(empty_storage.dictionaries_size(), 0);

  EXPECT_TRUE(engine.DeleteSession(&command));
}

}  // namespace
}  // namespace ios
}  // namespace mozc

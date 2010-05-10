// Copyright 2010, Google Inc.
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
#include "base/base.h"
#include "base/util.h"
#include "converter/character_form_manager.h"
#include "session/config.pb.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

DECLARE_string(test_tmpdir);

namespace mozc {

TEST(CharacterFormManagerTest, DefaultTest) {
  // set default user profile directory
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  string output;

  CharacterFormManager *manager =
      CharacterFormManager::GetCharacterFormManager();

  manager->ClearHistory();

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "カタカナ"
      manager->GetPreeditCharacterForm("\xe3\x82\xab\xe3\x82\xbf\xe3\x82\xab"
                                       "\xe3\x83\x8a"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("012"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("["));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("/"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "・"
      manager->GetPreeditCharacterForm("\xe3\x83\xbb"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "。"
      manager->GetPreeditCharacterForm("\xe3\x80\x82"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "、"
      manager->GetPreeditCharacterForm("\xe3\x80\x81"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("\\"));

  EXPECT_EQ(
      config::Config::NO_CONVERSION,
      // "ABC012ほげ"
      manager->GetConversionCharacterForm("\x41\x42\x43\x30\x31\x32\xe3\x81\xbb"
                                          "\xe3\x81\x92"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "カタカナ"
      manager->GetConversionCharacterForm("\xe3\x82\xab\xe3\x82\xbf\xe3\x82\xab"
                                          "\xe3\x83\x8a"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetConversionCharacterForm("012"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetConversionCharacterForm("["));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetConversionCharacterForm("/"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "・"
      manager->GetConversionCharacterForm("\xe3\x83\xbb"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "。"
      manager->GetConversionCharacterForm("\xe3\x80\x82"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "、"
      manager->GetConversionCharacterForm("\xe3\x80\x81"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetConversionCharacterForm("\\"));

  EXPECT_EQ(
      config::Config::NO_CONVERSION,
      // "ABC012ほげ"
      manager->GetConversionCharacterForm("\x41\x42\x43\x30\x31\x32\xe3\x81\xbb"
                                          "\xe3\x81\x92"));

  // "京都東京ABCインターネット"
  manager->ConvertPreeditString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba"
                                "\xac\x41\x42\x43\xe3\x82\xa4\xe3\x83\xb3\xe3"
                                "\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3\x83\x83"
                                "\xe3\x83\x88", &output);
  // "京都東京ＡＢＣインターネット"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // "ｲﾝﾀｰﾈｯﾄ"
  manager->ConvertPreeditString("\xef\xbd\xb2\xef\xbe\x9d\xef\xbe\x80\xef\xbd"
                                "\xb0\xef\xbe\x88\xef\xbd\xaf\xef\xbe\x84",
                                &output);
  // "インターネット"
  EXPECT_EQ("\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3"
            "\x83\x83\xe3\x83\x88", output);

  // "[]・。、"
  manager->ConvertPreeditString("\x5b\x5d\xe3\x83\xbb\xe3\x80\x82\xe3\x80\x81",
                                &output);
  // "［］・。、"
  EXPECT_EQ("\xef\xbc\xbb\xef\xbc\xbd\xe3\x83\xbb\xe3\x80\x82\xe3\x80\x81",
            output);

  manager->ConvertPreeditString(".!@#$%^&", &output);
  // "．！＠＃＄％＾＆"
  EXPECT_EQ("\xef\xbc\x8e\xef\xbc\x81\xef\xbc\xa0\xef\xbc\x83\xef\xbc\x84\xef"
            "\xbc\x85\xef\xbc\xbe\xef\xbc\x86", output);

  // "京都東京ABCｲﾝﾀｰﾈｯﾄ012"
  manager->ConvertPreeditString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba"
                                "\xac\x41\x42\x43\xef\xbd\xb2\xef\xbe\x9d\xef"
                                "\xbe\x80\xef\xbd\xb0\xef\xbe\x88\xef\xbd\xaf"
                                "\xef\xbe\x84\x30\x31\x32", &output);
  // "京都東京ＡＢＣインターネット０１２"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88\xef\xbc\x90\xef\xbc\x91"
            "\xef\xbc\x92", output);

  // "グーグルABCｲﾝﾀｰﾈｯﾄ012あいう"
  manager->ConvertPreeditString("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83"
                                "\xab\x41\x42\x43\xef\xbd\xb2\xef\xbe\x9d\xef"
                                "\xbe\x80\xef\xbd\xb0\xef\xbe\x88\xef\xbd\xaf"
                                "\xef\xbe\x84\x30\x31\x32\xe3\x81\x82\xe3\x81"
                                "\x84\xe3\x81\x86", &output);
  // "グーグルＡＢＣインターネット０１２あいう"
  EXPECT_EQ("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88\xef\xbc\x90\xef\xbc\x91"
            "\xef\xbc\x92\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86", output);

  // "京都東京ABCインターネット"
  manager->ConvertPreeditString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba"
                                "\xac\x41\x42\x43\xe3\x82\xa4\xe3\x83\xb3\xe3"
                                "\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3\x83\x83"
                                "\xe3\x83\x88", &output);
  // "京都東京ＡＢＣインターネット"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // "[京都]{東京}ABC!インターネット"
  manager->ConvertPreeditString("\x5b\xe4\xba\xac\xe9\x83\xbd\x5d\x7b\xe6\x9d"
                                "\xb1\xe4\xba\xac\x7d\x41\x42\x43\x21\xe3\x82"
                                "\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc\xe3"
                                "\x83\x8d\xe3\x83\x83\xe3\x83\x88", &output);
  // "［京都］｛東京｝ＡＢＣ！インターネット"
  EXPECT_EQ("\xef\xbc\xbb\xe4\xba\xac\xe9\x83\xbd\xef\xbc\xbd\xef\xbd\x9b\xe6"
            "\x9d\xb1\xe4\xba\xac\xef\xbd\x9d\xef\xbc\xa1\xef\xbc\xa2\xef\xbc"
            "\xa3\xef\xbc\x81\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc"
            "\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // "ｲﾝﾀｰﾈｯﾄ"
  manager->ConvertConversionString("\xef\xbd\xb2\xef\xbe\x9d\xef\xbe\x80\xef"
                                   "\xbd\xb0\xef\xbe\x88\xef\xbd\xaf\xef\xbe"
                                   "\x84", &output);
  // "インターネット"
  EXPECT_EQ("\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3"
            "\x83\x83\xe3\x83\x88", output);

  // "[]・。、"
  manager->ConvertConversionString("\x5b\x5d\xe3\x83\xbb\xe3\x80\x82\xe3\x80"
                                   "\x81", &output);
  // "［］・。、"
  EXPECT_EQ("\xef\xbc\xbb\xef\xbc\xbd\xe3\x83\xbb\xe3\x80\x82\xe3\x80\x81",
            output);

  manager->ConvertConversionString(".!@#$%^&", &output);
  // "．！＠＃＄％＾＆"
  EXPECT_EQ("\xef\xbc\x8e\xef\xbc\x81\xef\xbc\xa0\xef\xbc\x83\xef\xbc\x84\xef"
            "\xbc\x85\xef\xbc\xbe\xef\xbc\x86", output);

  // "京都東京ABCｲﾝﾀｰﾈｯﾄ012"
  manager->ConvertConversionString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4"
                                   "\xba\xac\x41\x42\x43\xef\xbd\xb2\xef\xbe"
                                   "\x9d\xef\xbe\x80\xef\xbd\xb0\xef\xbe\x88"
                                   "\xef\xbd\xaf\xef\xbe\x84\x30\x31\x32",
                                   &output);
  // "京都東京ＡＢＣインターネット０１２"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88\xef\xbc\x90\xef\xbc\x91"
            "\xef\xbc\x92", output);

  // "グーグルABCｲﾝﾀｰﾈｯﾄ012あいう"
  manager->ConvertConversionString("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3"
                                   "\x83\xab\x41\x42\x43\xef\xbd\xb2\xef\xbe"
                                   "\x9d\xef\xbe\x80\xef\xbd\xb0\xef\xbe\x88"
                                   "\xef\xbd\xaf\xef\xbe\x84\x30\x31\x32\xe3"
                                   "\x81\x82\xe3\x81\x84\xe3\x81\x86",
                                   &output);
  // "グーグルＡＢＣインターネット０１２あいう"
  EXPECT_EQ("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88\xef\xbc\x90\xef\xbc\x91"
            "\xef\xbc\x92\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86", output);

  // "[京都]{東京}ABC!インターネット"
  manager->ConvertConversionString("\x5b\xe4\xba\xac\xe9\x83\xbd\x5d\x7b\xe6"
                                   "\x9d\xb1\xe4\xba\xac\x7d\x41\x42\x43\x21"
                                   "\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3"
                                   "\x83\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83"
                                   "\x88", &output);
  // "［京都］｛東京｝ＡＢＣ！インターネット"
  EXPECT_EQ("\xef\xbc\xbb\xe4\xba\xac\xe9\x83\xbd\xef\xbc\xbd\xef\xbd\x9b\xe6"
            "\x9d\xb1\xe4\xba\xac\xef\xbd\x9d\xef\xbc\xa1\xef\xbc\xa2\xef\xbc"
            "\xa3\xef\xbc\x81\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc"
            "\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // Set
  // "カタカナ"
  manager->SetCharacterForm("\xe3\x82\xab\xe3\x82\xbf\xe3\x82\xab\xe3\x83\x8a",
                            config::Config::HALF_WIDTH);

  manager->SetCharacterForm("012",
                            config::Config::HALF_WIDTH);

  manager->SetCharacterForm("[",
                            config::Config::HALF_WIDTH);

  manager->SetCharacterForm("/",
                            config::Config::HALF_WIDTH);

  // "・"
  manager->SetCharacterForm("\xe3\x83\xbb",
                            config::Config::HALF_WIDTH);

  // "。"
  manager->SetCharacterForm("\xe3\x80\x82",
                            config::Config::HALF_WIDTH);

  // "、"
  manager->SetCharacterForm("\xe3\x80\x81",
                            config::Config::HALF_WIDTH);

  manager->SetCharacterForm("\\",
                            config::Config::HALF_WIDTH);

  // retry
  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "カタカナ"
      manager->GetPreeditCharacterForm("\xe3\x82\xab\xe3\x82\xbf\xe3\x82\xab"
                                       "\xe3\x83\x8a"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("012"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("["));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("/"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "・"
      manager->GetPreeditCharacterForm("\xe3\x83\xbb"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "。"
      manager->GetPreeditCharacterForm("\xe3\x80\x82"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "、"
      manager->GetPreeditCharacterForm("\xe3\x80\x81"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("\\"));

  EXPECT_EQ(
      config::Config::NO_CONVERSION,
      // "ABC012ほげ"
      manager->GetConversionCharacterForm("\x41\x42\x43\x30\x31\x32\xe3\x81\xbb"
                                          "\xe3\x81\x92"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "カタカナ"
      manager->GetConversionCharacterForm("\xe3\x82\xab\xe3\x82\xbf\xe3\x82\xab"
                                          "\xe3\x83\x8a"));

  EXPECT_EQ(
      config::Config::HALF_WIDTH,
      manager->GetConversionCharacterForm("012"));

  EXPECT_EQ(
      config::Config::HALF_WIDTH,
      manager->GetConversionCharacterForm("["));

  EXPECT_EQ(
      config::Config::HALF_WIDTH,
      manager->GetConversionCharacterForm("/"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "・"
      manager->GetConversionCharacterForm("\xe3\x83\xbb"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "。"
      manager->GetConversionCharacterForm("\xe3\x80\x82"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "、"
      manager->GetConversionCharacterForm("\xe3\x80\x81"));

  EXPECT_EQ(
      config::Config::HALF_WIDTH,
      manager->GetConversionCharacterForm("\\"));

  // "京都東京ABCインターネット"
  manager->ConvertPreeditString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba"
                                "\xac\x41\x42\x43\xe3\x82\xa4\xe3\x83\xb3\xe3"
                                "\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3\x83\x83"
                                "\xe3\x83\x88", &output);
  // "京都東京ＡＢＣインターネット"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // "ｲﾝﾀｰﾈｯﾄ"
  manager->ConvertPreeditString("\xef\xbd\xb2\xef\xbe\x9d\xef\xbe\x80\xef\xbd"
                                "\xb0\xef\xbe\x88\xef\xbd\xaf\xef\xbe\x84",
                                &output);
  // "インターネット"
  EXPECT_EQ("\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3"
            "\x83\x83\xe3\x83\x88", output);

  // "[]・。、"
  manager->ConvertPreeditString("\x5b\x5d\xe3\x83\xbb\xe3\x80\x82\xe3\x80\x81",
                                &output);
  // "［］・。、"
  EXPECT_EQ("\xef\xbc\xbb\xef\xbc\xbd\xe3\x83\xbb\xe3\x80\x82\xe3\x80\x81",
            output);

  manager->ConvertPreeditString(".!@#$%^&", &output);
  // "．！＠＃＄％＾＆"
  EXPECT_EQ("\xef\xbc\x8e\xef\xbc\x81\xef\xbc\xa0\xef\xbc\x83\xef\xbc\x84\xef"
            "\xbc\x85\xef\xbc\xbe\xef\xbc\x86", output);

  // "京都東京ABCｲﾝﾀｰﾈｯﾄ"
  manager->ConvertPreeditString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba"
                                "\xac\x41\x42\x43\xef\xbd\xb2\xef\xbe\x9d\xef"
                                "\xbe\x80\xef\xbd\xb0\xef\xbe\x88\xef\xbd\xaf"
                                "\xef\xbe\x84", &output);
  // "京都東京ＡＢＣインターネット"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // "グーグルABCｲﾝﾀｰﾈｯﾄあいう"
  manager->ConvertPreeditString("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83"
                                "\xab\x41\x42\x43\xef\xbd\xb2\xef\xbe\x9d\xef"
                                "\xbe\x80\xef\xbd\xb0\xef\xbe\x88\xef\xbd\xaf"
                                "\xef\xbe\x84\xe3\x81\x82\xe3\x81\x84\xe3\x81"
                                "\x86", &output);
  // "グーグルＡＢＣインターネットあいう"
  EXPECT_EQ("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88\xe3\x81\x82\xe3\x81\x84"
            "\xe3\x81\x86", output);

  // "京都東京ABCインターネット"
  manager->ConvertPreeditString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba"
                                "\xac\x41\x42\x43\xe3\x82\xa4\xe3\x83\xb3\xe3"
                                "\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3\x83\x83"
                                "\xe3\x83\x88", &output);
  // "京都東京ＡＢＣインターネット"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // "[京都]{東京}ABC!インターネット"
  manager->ConvertPreeditString("\x5b\xe4\xba\xac\xe9\x83\xbd\x5d\x7b\xe6\x9d"
                                "\xb1\xe4\xba\xac\x7d\x41\x42\x43\x21\xe3\x82"
                                "\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc\xe3"
                                "\x83\x8d\xe3\x83\x83\xe3\x83\x88", &output);
  // "［京都］｛東京｝ＡＢＣ！インターネット"
  EXPECT_EQ("\xef\xbc\xbb\xe4\xba\xac\xe9\x83\xbd\xef\xbc\xbd\xef\xbd\x9b\xe6"
            "\x9d\xb1\xe4\xba\xac\xef\xbd\x9d\xef\xbc\xa1\xef\xbc\xa2\xef\xbc"
            "\xa3\xef\xbc\x81\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc"
            "\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // "ｲﾝﾀｰﾈｯﾄ"
  manager->ConvertConversionString("\xef\xbd\xb2\xef\xbe\x9d\xef\xbe\x80\xef"
                                   "\xbd\xb0\xef\xbe\x88\xef\xbd\xaf\xef\xbe"
                                   "\x84", &output);
  // "インターネット"
  EXPECT_EQ("\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3"
            "\x83\x83\xe3\x83\x88", output);

  // "[]・。、"
  manager->ConvertConversionString("\x5b\x5d\xe3\x83\xbb\xe3\x80\x82\xe3\x80"
                                   "\x81", &output);
  // "[]・。、"
  EXPECT_EQ("\x5b\x5d\xe3\x83\xbb\xe3\x80\x82\xe3\x80\x81", output);

  manager->ConvertConversionString(".!@#$%^&", &output);
  // ".!@#$%^&" will be "．！@#$%^&" by preference, but this is not
  // consistent form. so we do not convert this.
  EXPECT_EQ(".!@#$%^&", output);

  // However we can convert separately.
  manager->ConvertConversionString(".!", &output);
  // "．！"
  EXPECT_EQ("\xef\xbc\x8e\xef\xbc\x81", output);
  manager->ConvertConversionString("@#$%^&", &output);
  EXPECT_EQ("@#$%^&", output);

  // "京都東京ABCｲﾝﾀｰﾈｯﾄ"
  manager->ConvertConversionString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4"
                                   "\xba\xac\x41\x42\x43\xef\xbd\xb2\xef\xbe"
                                   "\x9d\xef\xbe\x80\xef\xbd\xb0\xef\xbe\x88"
                                   "\xef\xbd\xaf\xef\xbe\x84", &output);
  // "京都東京ＡＢＣインターネット"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // "グーグルABCｲﾝﾀｰﾈｯﾄあいう"
  manager->ConvertConversionString("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3"
                                   "\x83\xab\x41\x42\x43\xef\xbd\xb2\xef\xbe"
                                   "\x9d\xef\xbe\x80\xef\xbd\xb0\xef\xbe\x88"
                                   "\xef\xbd\xaf\xef\xbe\x84\xe3\x81\x82\xe3"
                                   "\x81\x84\xe3\x81\x86", &output);
  // "グーグルＡＢＣインターネットあいう"
  EXPECT_EQ("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88\xe3\x81\x82\xe3\x81\x84"
            "\xe3\x81\x86", output);

  // "[京都]{東京}ABC!インターネット"
  manager->ConvertConversionString("\x5b\xe4\xba\xac\xe9\x83\xbd\x5d\x7b\xe6"
                                   "\x9d\xb1\xe4\xba\xac\x7d\x41\x42\x43\x21"
                                   "\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3"
                                   "\x83\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83"
                                   "\x88", &output);
  // "[京都]{東京}ABC!インターネット" will be
  // "[京都]{東京}ＡＢＣ！インターネット" by preference and this is
  // not consistent
  // "[京都]{東京}ABC!インターネット"
  EXPECT_EQ("\x5b\xe4\xba\xac\xe9\x83\xbd\x5d\x7b\xe6\x9d\xb1\xe4"
            "\xba\xac\x7d\x41\x42\x43\x21\xe3\x82\xa4\xe3\x83\xb3"
            "\xe3\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3\x83\x83\xe3"
            "\x83\x88", output);

  // we can convert separately
  // "[京都]{東京}ＡＢＣ！インターネット"
  // "[京都]{東京}"
  manager->ConvertConversionString(
      "\x5b\xe4\xba\xac\xe9\x83\xbd\x5d\x7b\xe6\x9d\xb1\xe4\xba\xac\x7d",
      &output);
  // "[京都]{東京}"
  EXPECT_EQ("\x5b\xe4\xba\xac\xe9\x83\xbd\x5d\x7b\xe6\x9d\xb1\xe4\xba\xac\x7d",
            output);

  // "ABC!インターネット"
  manager->ConvertConversionString(
      "\x41\x42\x43\x21\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf"
      "\xe3\x83\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", &output);
  // "ＡＢＣ！インターネット"
  EXPECT_EQ("\xef\xbc\xa1\xef\xbc\xa2\xef\xbc\xa3\xef\xbc\x81\xe3\x82\xa4"
            "\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3\x83\x83"
            "\xe3\x83\x88", output);

  // reset
  // "カタカナ"
  manager->SetCharacterForm("\xe3\x82\xab\xe3\x82\xbf\xe3\x82\xab\xe3\x83\x8a",
                            config::Config::FULL_WIDTH);

  manager->SetCharacterForm("012",
                            config::Config::FULL_WIDTH);

  manager->SetCharacterForm("[",
                            config::Config::FULL_WIDTH);

  manager->SetCharacterForm("/",
                            config::Config::FULL_WIDTH);

  // "・"
  manager->SetCharacterForm("\xe3\x83\xbb",
                            config::Config::FULL_WIDTH);

  // "。"
  manager->SetCharacterForm("\xe3\x80\x82",
                            config::Config::FULL_WIDTH);

  // "、"
  manager->SetCharacterForm("\xe3\x80\x81",
                            config::Config::FULL_WIDTH);

  manager->SetCharacterForm("\\",
                            config::Config::FULL_WIDTH);

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "カタカナ"
      manager->GetPreeditCharacterForm("\xe3\x82\xab\xe3\x82\xbf\xe3\x82\xab"
                                       "\xe3\x83\x8a"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("012"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("["));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("/"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "・"
      manager->GetPreeditCharacterForm("\xe3\x83\xbb"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "。"
      manager->GetPreeditCharacterForm("\xe3\x80\x82"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "、"
      manager->GetPreeditCharacterForm("\xe3\x80\x81"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetPreeditCharacterForm("\\"));

  EXPECT_EQ(
      config::Config::NO_CONVERSION,
      // "ABC012ほげ"
      manager->GetConversionCharacterForm("\x41\x42\x43\x30\x31\x32\xe3\x81"
                                          "\xbb\xe3\x81\x92"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "カタカナ"
      manager->GetConversionCharacterForm("\xe3\x82\xab\xe3\x82\xbf\xe3\x82\xab"
                                          "\xe3\x83\x8a"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetConversionCharacterForm("012"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetConversionCharacterForm("["));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      manager->GetConversionCharacterForm("/"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "・"
      manager->GetConversionCharacterForm("\xe3\x83\xbb"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "。"
      manager->GetConversionCharacterForm("\xe3\x80\x82"));

  EXPECT_EQ(
      config::Config::FULL_WIDTH,
      // "、"
      manager->GetConversionCharacterForm("\xe3\x80\x81"));

  EXPECT_EQ(
      config::Config::NO_CONVERSION,
      // "ABC012ほげ"
      manager->GetConversionCharacterForm("\x41\x42\x43\x30\x31\x32\xe3\x81\xbb"
                                          "\xe3\x81\x92"));

  // "京都東京ABCインターネット"
  manager->ConvertPreeditString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba"
                                "\xac\x41\x42\x43\xe3\x82\xa4\xe3\x83\xb3\xe3"
                                "\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3\x83\x83"
                                "\xe3\x83\x88", &output);
  // "京都東京ＡＢＣインターネット"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // "ｲﾝﾀｰﾈｯﾄ"
  manager->ConvertPreeditString("\xef\xbd\xb2\xef\xbe\x9d\xef\xbe\x80\xef\xbd"
                                "\xb0\xef\xbe\x88\xef\xbd\xaf\xef\xbe\x84",
                                &output);
  // "インターネット"
  EXPECT_EQ("\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3"
            "\x83\x83\xe3\x83\x88", output);

  // "[]・。、"
  manager->ConvertPreeditString("\x5b\x5d\xe3\x83\xbb\xe3\x80\x82\xe3\x80\x81",
                                &output);
  // "［］・。、"
  EXPECT_EQ("\xef\xbc\xbb\xef\xbc\xbd\xe3\x83\xbb\xe3\x80\x82\xe3\x80\x81",
            output);

  manager->ConvertPreeditString(".!@#$%^&", &output);
  // "．！＠＃＄％＾＆"
  EXPECT_EQ("\xef\xbc\x8e\xef\xbc\x81\xef\xbc\xa0\xef\xbc\x83\xef\xbc\x84\xef"
            "\xbc\x85\xef\xbc\xbe\xef\xbc\x86", output);

  // "京都東京ABCｲﾝﾀｰﾈｯﾄ012"
  manager->ConvertPreeditString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba"
                                "\xac\x41\x42\x43\xef\xbd\xb2\xef\xbe\x9d\xef"
                                "\xbe\x80\xef\xbd\xb0\xef\xbe\x88\xef\xbd\xaf"
                                "\xef\xbe\x84\x30\x31\x32", &output);
  // "京都東京ＡＢＣインターネット０１２"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88\xef\xbc\x90\xef\xbc\x91"
            "\xef\xbc\x92", output);

  // "グーグルABCｲﾝﾀｰﾈｯﾄ012あいう"
  manager->ConvertPreeditString("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83"
                                "\xab\x41\x42\x43\xef\xbd\xb2\xef\xbe\x9d\xef"
                                "\xbe\x80\xef\xbd\xb0\xef\xbe\x88\xef\xbd\xaf"
                                "\xef\xbe\x84\x30\x31\x32\xe3\x81\x82\xe3\x81"
                                "\x84\xe3\x81\x86", &output);
  // "グーグルＡＢＣインターネット０１２あいう"
  EXPECT_EQ("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88\xef\xbc\x90\xef\xbc\x91"
            "\xef\xbc\x92\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86", output);

  // "京都東京ABCインターネット"
  manager->ConvertPreeditString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba"
                                "\xac\x41\x42\x43\xe3\x82\xa4\xe3\x83\xb3\xe3"
                                "\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3\x83\x83"
                                "\xe3\x83\x88", &output);
  // "京都東京ＡＢＣインターネット"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // "[京都]{東京}ABC!インターネット"
  manager->ConvertPreeditString("\x5b\xe4\xba\xac\xe9\x83\xbd\x5d\x7b\xe6\x9d"
                                "\xb1\xe4\xba\xac\x7d\x41\x42\x43\x21\xe3\x82"
                                "\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc\xe3"
                                "\x83\x8d\xe3\x83\x83\xe3\x83\x88", &output);
  // "［京都］｛東京｝ＡＢＣ！インターネット"
  EXPECT_EQ("\xef\xbc\xbb\xe4\xba\xac\xe9\x83\xbd\xef\xbc\xbd\xef\xbd\x9b\xe6"
            "\x9d\xb1\xe4\xba\xac\xef\xbd\x9d\xef\xbc\xa1\xef\xbc\xa2\xef\xbc"
            "\xa3\xef\xbc\x81\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc"
            "\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);

  // "ｲﾝﾀｰﾈｯﾄ"
  manager->ConvertConversionString("\xef\xbd\xb2\xef\xbe\x9d\xef\xbe\x80\xef"
                                   "\xbd\xb0\xef\xbe\x88\xef\xbd\xaf\xef\xbe"
                                   "\x84", &output);
  // "インターネット"
  EXPECT_EQ("\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc\xe3\x83\x8d\xe3"
            "\x83\x83\xe3\x83\x88", output);

  // "[]・。、"
  manager->ConvertConversionString("\x5b\x5d\xe3\x83\xbb\xe3\x80\x82\xe3\x80"
                                   "\x81", &output);
  // "［］・。、"
  EXPECT_EQ("\xef\xbc\xbb\xef\xbc\xbd\xe3\x83\xbb\xe3\x80\x82\xe3\x80\x81",
            output);

  manager->ConvertConversionString(".!@#$%^&", &output);
  // "．！＠＃＄％＾＆"
  EXPECT_EQ("\xef\xbc\x8e\xef\xbc\x81\xef\xbc\xa0\xef\xbc\x83\xef\xbc\x84\xef"
            "\xbc\x85\xef\xbc\xbe\xef\xbc\x86", output);

  // "京都東京ABCｲﾝﾀｰﾈｯﾄ012"
  manager->ConvertConversionString("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4"
                                   "\xba\xac\x41\x42\x43\xef\xbd\xb2\xef\xbe"
                                   "\x9d\xef\xbe\x80\xef\xbd\xb0\xef\xbe\x88"
                                   "\xef\xbd\xaf\xef\xbe\x84\x30\x31\x32",
                                   &output);
  // "京都東京ＡＢＣインターネット０１２"
  EXPECT_EQ("\xe4\xba\xac\xe9\x83\xbd\xe6\x9d\xb1\xe4\xba\xac\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88\xef\xbc\x90\xef\xbc\x91"
            "\xef\xbc\x92", output);

  // "グーグルABCｲﾝﾀｰﾈｯﾄ012あいう"
  manager->ConvertConversionString("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3"
                                   "\x83\xab\x41\x42\x43\xef\xbd\xb2\xef\xbe"
                                   "\x9d\xef\xbe\x80\xef\xbd\xb0\xef\xbe\x88"
                                   "\xef\xbd\xaf\xef\xbe\x84\x30\x31\x32\xe3"
                                   "\x81\x82\xe3\x81\x84\xe3\x81\x86", &output);
  // "グーグルＡＢＣインターネット０１２あいう"
  EXPECT_EQ("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab\xef\xbc\xa1\xef"
            "\xbc\xa2\xef\xbc\xa3\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83"
            "\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88\xef\xbc\x90\xef\xbc\x91"
            "\xef\xbc\x92\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86", output);

  // "[京都]{東京}ABC!インターネット"
  manager->ConvertConversionString("\x5b\xe4\xba\xac\xe9\x83\xbd\x5d\x7b\xe6"
                                   "\x9d\xb1\xe4\xba\xac\x7d\x41\x42\x43\x21"
                                   "\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3"
                                   "\x83\xbc\xe3\x83\x8d\xe3\x83\x83\xe3\x83"
                                   "\x88", &output);
  // "［京都］｛東京｝ＡＢＣ！インターネット"
  EXPECT_EQ("\xef\xbc\xbb\xe4\xba\xac\xe9\x83\xbd\xef\xbc\xbd\xef\xbd\x9b\xe6"
            "\x9d\xb1\xe4\xba\xac\xef\xbd\x9d\xef\xbc\xa1\xef\xbc\xa2\xef\xbc"
            "\xa3\xef\xbc\x81\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xbf\xe3\x83\xbc"
            "\xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88", output);
}

TEST(CharacterFormManagerTest, MixedFormTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  CharacterFormManager *manager =
      CharacterFormManager::GetCharacterFormManager();

  manager->AddConversionRule("0", config::Config::FULL_WIDTH);
  manager->AddConversionRule(".,", config::Config::HALF_WIDTH);
  manager->AddPreeditRule("0", config::Config::FULL_WIDTH);
  manager->AddPreeditRule(".,", config::Config::HALF_WIDTH);

  string output;
  manager->ConvertConversionString("1.23", &output);
  EXPECT_EQ("1.23", output);

  manager->ConvertPreeditString("1.23", &output);
  // The period is half width here
  // because require_consistent_conversion_ is false.
  // "１.２３"
  EXPECT_EQ("\xef\xbc\x91\x2e\xef\xbc\x92\xef\xbc\x93", output);
}

TEST(CharacterFormManagerTest, GroupTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  CharacterFormManager *manager =
      CharacterFormManager::GetCharacterFormManager();

  {
    manager->ClearHistory();
    manager->Clear();
    // "ア"
    manager->AddConversionRule("\xe3\x82\xa2", config::Config::FULL_WIDTH);
    // "ア"
    manager->AddPreeditRule("\xe3\x82\xa2", config::Config::HALF_WIDTH);
    manager->AddConversionRule("[]", config::Config::HALF_WIDTH);
    manager->AddPreeditRule("[]", config::Config::FULL_WIDTH);
    manager->AddConversionRule("!@#$%^&*()-=",
                               config::Config::FULL_WIDTH);
    manager->AddConversionRule("!@#$%^&*()-=",
                               config::Config::HALF_WIDTH);

    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        manager->GetConversionCharacterForm("["));

    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetPreeditCharacterForm("["));

    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        // "ア"
        manager->GetPreeditCharacterForm("\xe3\x82\xa2"));


    manager->SetCharacterForm("[",
                              config::Config::FULL_WIDTH);
    // "ア"
    manager->SetCharacterForm("\xe3\x82\xa2",
                              config::Config::FULL_WIDTH);
    manager->SetCharacterForm("@",
                              config::Config::FULL_WIDTH);

    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        manager->GetConversionCharacterForm("["));

    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetPreeditCharacterForm("["));

    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        // "ア"
        manager->GetPreeditCharacterForm("\xe3\x82\xa2"));
  }

  {
    manager->ClearHistory();
    manager->Clear();
    // "ア"
    manager->AddConversionRule("\xe3\x82\xa2", config::Config::FULL_WIDTH);
    manager->AddConversionRule("[]", config::Config::LAST_FORM);
    manager->AddConversionRule("!@#$%^&*()-=",
                               config::Config::FULL_WIDTH);

    EXPECT_EQ(
        config::Config::FULL_WIDTH,  // default
        manager->GetConversionCharacterForm("["));

    // same group
    manager->SetCharacterForm("]",
                              config::Config::HALF_WIDTH);

    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        manager->GetConversionCharacterForm("["));
  }

  {
    manager->ClearHistory();
    manager->Clear();
    // "ア"
    manager->AddConversionRule("\xe3\x82\xa2", config::Config::FULL_WIDTH);
    manager->AddConversionRule("[](){}", config::Config::LAST_FORM);
    manager->AddConversionRule("!@#$%^&*-=",
                               config::Config::FULL_WIDTH);

    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetConversionCharacterForm("{"));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetConversionCharacterForm("}"));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetConversionCharacterForm("("));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetConversionCharacterForm(")"));

    // same group
    manager->SetCharacterForm(")",
                              config::Config::HALF_WIDTH);

    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        manager->GetConversionCharacterForm("{"));
    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        manager->GetConversionCharacterForm("}"));
    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        manager->GetConversionCharacterForm("("));
    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        manager->GetConversionCharacterForm(")"));
  }

  {
    manager->ClearHistory();
    manager->Clear();
    // "ア"
    manager->AddConversionRule("\xe3\x82\xa2", config::Config::FULL_WIDTH);
    manager->AddConversionRule("[](){}", config::Config::LAST_FORM);
    manager->AddPreeditRule("[](){}", config::Config::FULL_WIDTH);
    manager->AddConversionRule("!@#$%^&*-=",
                               config::Config::FULL_WIDTH);

    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetConversionCharacterForm("{"));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetConversionCharacterForm("}"));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetConversionCharacterForm("("));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetConversionCharacterForm(")"));

    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetPreeditCharacterForm("{"));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetPreeditCharacterForm("}"));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetPreeditCharacterForm("("));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetPreeditCharacterForm(")"));

    // same group
    manager->SetCharacterForm(")",
                              config::Config::HALF_WIDTH);

    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        manager->GetConversionCharacterForm("{"));
    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        manager->GetConversionCharacterForm("}"));
    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        manager->GetConversionCharacterForm("("));
    EXPECT_EQ(
        config::Config::HALF_WIDTH,
        manager->GetConversionCharacterForm(")"));

    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetPreeditCharacterForm("{"));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetPreeditCharacterForm("}"));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetPreeditCharacterForm("("));
    EXPECT_EQ(
        config::Config::FULL_WIDTH,
        manager->GetPreeditCharacterForm(")"));
  }
}
}   // mozc

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
#include "session/commands.pb.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/base/string_util.h"

namespace mozc {
namespace win32 {

TEST(StringUtilTest, InvalidCases) {
  EXPECT_EQ("", StringUtil::KeyToReadingA(""));
  // KeyToReadingA fails if the resultant string is longer than 512 characters.
  string longa(10000, 'a');
  EXPECT_EQ("", StringUtil::KeyToReadingA(longa.c_str()));
}

TEST(StringUtilTest, Hiragana) {
  // "ｱ", "あ"
  EXPECT_EQ("\xef\xbd\xb1", StringUtil::KeyToReadingA("\xe3\x81\x82"));
  // "ｲ", "い"
  EXPECT_EQ("\xef\xbd\xb2", StringUtil::KeyToReadingA("\xe3\x81\x84"));
  // "ｳ", "う"
  EXPECT_EQ("\xef\xbd\xb3", StringUtil::KeyToReadingA("\xe3\x81\x86"));
  // "ｴ", "え"
  EXPECT_EQ("\xef\xbd\xb4", StringUtil::KeyToReadingA("\xe3\x81\x88"));
  // "ｵ", "お"
  EXPECT_EQ("\xef\xbd\xb5", StringUtil::KeyToReadingA("\xe3\x81\x8a"));
  // "ｧ", "ぁ"
  EXPECT_EQ("\xef\xbd\xa7", StringUtil::KeyToReadingA("\xe3\x81\x81"));
  // "ｨ", "ぃ"
  EXPECT_EQ("\xef\xbd\xa8", StringUtil::KeyToReadingA("\xe3\x81\x83"));
  // "ｩ", "ぅ"
  EXPECT_EQ("\xef\xbd\xa9", StringUtil::KeyToReadingA("\xe3\x81\x85"));
  // "ｪ", "ぇ"
  EXPECT_EQ("\xef\xbd\xaa", StringUtil::KeyToReadingA("\xe3\x81\x87"));
  // "ｫ", "ぉ"
  EXPECT_EQ("\xef\xbd\xab", StringUtil::KeyToReadingA("\xe3\x81\x89"));
  // "ｶ", "か"
  EXPECT_EQ("\xef\xbd\xb6", StringUtil::KeyToReadingA("\xe3\x81\x8b"));
  // "ｷ", "き"
  EXPECT_EQ("\xef\xbd\xb7", StringUtil::KeyToReadingA("\xe3\x81\x8d"));
  // "ｸ", "く"
  EXPECT_EQ("\xef\xbd\xb8", StringUtil::KeyToReadingA("\xe3\x81\x8f"));
  // "ｹ", "け"
  EXPECT_EQ("\xef\xbd\xb9", StringUtil::KeyToReadingA("\xe3\x81\x91"));
  // "ｺ", "こ"
  EXPECT_EQ("\xef\xbd\xba", StringUtil::KeyToReadingA("\xe3\x81\x93"));
  // "ｶﾞ", "が"
  EXPECT_EQ("\xef\xbd\xb6\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\x8c"));
  // "ｷﾞ", "ぎ"
  EXPECT_EQ("\xef\xbd\xb7\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\x8e"));
  // "ｸﾞ", "ぐ"
  EXPECT_EQ("\xef\xbd\xb8\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\x90"));
  // "ｹﾞ", "げ"
  EXPECT_EQ("\xef\xbd\xb9\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\x92"));
  // "ｺﾞ", "ご"
  EXPECT_EQ("\xef\xbd\xba\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\x94"));
  // "ｻ", "さ"
  EXPECT_EQ("\xef\xbd\xbb", StringUtil::KeyToReadingA("\xe3\x81\x95"));
  // "ｼ", "し"
  EXPECT_EQ("\xef\xbd\xbc", StringUtil::KeyToReadingA("\xe3\x81\x97"));
  // "ｽ", "す"
  EXPECT_EQ("\xef\xbd\xbd", StringUtil::KeyToReadingA("\xe3\x81\x99"));
  // "ｾ", "せ"
  EXPECT_EQ("\xef\xbd\xbe", StringUtil::KeyToReadingA("\xe3\x81\x9b"));
  // "ｿ", "そ"
  EXPECT_EQ("\xef\xbd\xbf", StringUtil::KeyToReadingA("\xe3\x81\x9d"));
  // "ｻﾞ", "ざ"
  EXPECT_EQ("\xef\xbd\xbb\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\x96"));
  // "ｼﾞ", "じ"
  EXPECT_EQ("\xef\xbd\xbc\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\x98"));
  // "ｽﾞ", "ず"
  EXPECT_EQ("\xef\xbd\xbd\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\x9a"));
  // "ｾﾞ", "ぜ"
  EXPECT_EQ("\xef\xbd\xbe\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\x9c"));
  // "ｿﾞ", "ぞ"
  EXPECT_EQ("\xef\xbd\xbf\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\x9e"));
  // "ﾀ", "た"
  EXPECT_EQ("\xef\xbe\x80", StringUtil::KeyToReadingA("\xe3\x81\x9f"));
  // "ﾁ", "ち"
  EXPECT_EQ("\xef\xbe\x81", StringUtil::KeyToReadingA("\xe3\x81\xa1"));
  // "ﾂ", "つ"
  EXPECT_EQ("\xef\xbe\x82", StringUtil::KeyToReadingA("\xe3\x81\xa4"));
  // "ﾃ", "て"
  EXPECT_EQ("\xef\xbe\x83", StringUtil::KeyToReadingA("\xe3\x81\xa6"));
  // "ﾄ", "と"
  EXPECT_EQ("\xef\xbe\x84", StringUtil::KeyToReadingA("\xe3\x81\xa8"));
  // "ﾀﾞ", "だ"
  EXPECT_EQ("\xef\xbe\x80\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\xa0"));
  // "ﾁﾞ", "ぢ"
  EXPECT_EQ("\xef\xbe\x81\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\xa2"));
  // "ﾂﾞ", "づ"
  EXPECT_EQ("\xef\xbe\x82\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\xa5"));
  // "ﾃﾞ", "で"
  EXPECT_EQ("\xef\xbe\x83\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\xa7"));
  // "ﾄﾞ", "ど"
  EXPECT_EQ("\xef\xbe\x84\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\xa9"));
  // "ｯ", "っ"
  EXPECT_EQ("\xef\xbd\xaf", StringUtil::KeyToReadingA("\xe3\x81\xa3"));
  // "ﾅ", "な"
  EXPECT_EQ("\xef\xbe\x85", StringUtil::KeyToReadingA("\xe3\x81\xaa"));
  // "ﾆ", "に"
  EXPECT_EQ("\xef\xbe\x86", StringUtil::KeyToReadingA("\xe3\x81\xab"));
  // "ﾇ", "ぬ"
  EXPECT_EQ("\xef\xbe\x87", StringUtil::KeyToReadingA("\xe3\x81\xac"));
  // "ﾈ", "ね"
  EXPECT_EQ("\xef\xbe\x88", StringUtil::KeyToReadingA("\xe3\x81\xad"));
  // "ﾉ", "の"
  EXPECT_EQ("\xef\xbe\x89", StringUtil::KeyToReadingA("\xe3\x81\xae"));
  // "ﾊ", "は"
  EXPECT_EQ("\xef\xbe\x8a", StringUtil::KeyToReadingA("\xe3\x81\xaf"));
  // "ﾋ", "ひ"
  EXPECT_EQ("\xef\xbe\x8b", StringUtil::KeyToReadingA("\xe3\x81\xb2"));
  // "ﾌ", "ふ"
  EXPECT_EQ("\xef\xbe\x8c", StringUtil::KeyToReadingA("\xe3\x81\xb5"));
  // "ﾍ", "へ"
  EXPECT_EQ("\xef\xbe\x8d", StringUtil::KeyToReadingA("\xe3\x81\xb8"));
  // "ﾎ", "ほ"
  EXPECT_EQ("\xef\xbe\x8e", StringUtil::KeyToReadingA("\xe3\x81\xbb"));
  // "ﾊﾞ", "ば"
  EXPECT_EQ("\xef\xbe\x8a\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\xb0"));
  // "ﾋﾞ", "び"
  EXPECT_EQ("\xef\xbe\x8b\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\xb3"));
  // "ﾌﾞ", "ぶ"
  EXPECT_EQ("\xef\xbe\x8c\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\xb6"));
  // "ﾍﾞ", "べ"
  EXPECT_EQ("\xef\xbe\x8d\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\xb9"));
  // "ﾎﾞ", "ぼ"
  EXPECT_EQ("\xef\xbe\x8e\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x81\xbc"));
  // "ﾊﾟ", "ぱ"
  EXPECT_EQ("\xef\xbe\x8a\xef\xbe\x9f",
            StringUtil::KeyToReadingA("\xe3\x81\xb1"));
  // "ﾋﾟ", "ぴ"
  EXPECT_EQ("\xef\xbe\x8b\xef\xbe\x9f",
            StringUtil::KeyToReadingA("\xe3\x81\xb4"));
  // "ﾌﾟ", "ぷ"
  EXPECT_EQ("\xef\xbe\x8c\xef\xbe\x9f",
            StringUtil::KeyToReadingA("\xe3\x81\xb7"));
  // "ﾍﾟ", "ぺ"
  EXPECT_EQ("\xef\xbe\x8d\xef\xbe\x9f",
            StringUtil::KeyToReadingA("\xe3\x81\xba"));
  // "ﾎﾟ", "ぽ"
  EXPECT_EQ("\xef\xbe\x8e\xef\xbe\x9f",
            StringUtil::KeyToReadingA("\xe3\x81\xbd"));
  // "ﾏ", "ま"
  EXPECT_EQ("\xef\xbe\x8f", StringUtil::KeyToReadingA("\xe3\x81\xbe"));
  // "ﾐ", "み"
  EXPECT_EQ("\xef\xbe\x90", StringUtil::KeyToReadingA("\xe3\x81\xbf"));
  // "ﾑ", "む"
  EXPECT_EQ("\xef\xbe\x91", StringUtil::KeyToReadingA("\xe3\x82\x80"));
  // "ﾒ", "め"
  EXPECT_EQ("\xef\xbe\x92", StringUtil::KeyToReadingA("\xe3\x82\x81"));
  // "ﾓ", "も"
  EXPECT_EQ("\xef\xbe\x93", StringUtil::KeyToReadingA("\xe3\x82\x82"));
  // "ﾔ", "や"
  EXPECT_EQ("\xef\xbe\x94", StringUtil::KeyToReadingA("\xe3\x82\x84"));
  // "ﾕ", "ゆ"
  EXPECT_EQ("\xef\xbe\x95", StringUtil::KeyToReadingA("\xe3\x82\x86"));
  // "ﾖ", "よ"
  EXPECT_EQ("\xef\xbe\x96", StringUtil::KeyToReadingA("\xe3\x82\x88"));
  // "ｬ", "ゃ"
  EXPECT_EQ("\xef\xbd\xac", StringUtil::KeyToReadingA("\xe3\x82\x83"));
  // "ｭ", "ゅ"
  EXPECT_EQ("\xef\xbd\xad", StringUtil::KeyToReadingA("\xe3\x82\x85"));
  // "ｮ", "ょ"
  EXPECT_EQ("\xef\xbd\xae", StringUtil::KeyToReadingA("\xe3\x82\x87"));
  // "ﾜ", "わ"
  EXPECT_EQ("\xef\xbe\x9c", StringUtil::KeyToReadingA("\xe3\x82\x8f"));
  // "ｦ", "を"
  EXPECT_EQ("\xef\xbd\xa6", StringUtil::KeyToReadingA("\xe3\x82\x92"));
  // "ﾝ", "ん"
  EXPECT_EQ("\xef\xbe\x9d", StringUtil::KeyToReadingA("\xe3\x82\x93"));
}

TEST(StringUtilTest, Katakana) {
    // "ｱ", "ア"
  EXPECT_EQ("\xef\xbd\xb1", StringUtil::KeyToReadingA("\xe3\x82\xa2"));
  // "ｲ", "イ"
  EXPECT_EQ("\xef\xbd\xb2", StringUtil::KeyToReadingA("\xe3\x82\xa4"));
  // "ｳ", "ウ"
  EXPECT_EQ("\xef\xbd\xb3", StringUtil::KeyToReadingA("\xe3\x82\xa6"));
  // "ｴ", "エ"
  EXPECT_EQ("\xef\xbd\xb4", StringUtil::KeyToReadingA("\xe3\x82\xa8"));
  // "ｵ", "オ"
  EXPECT_EQ("\xef\xbd\xb5", StringUtil::KeyToReadingA("\xe3\x82\xaa"));
  // "ｶ", "カ"
  EXPECT_EQ("\xef\xbd\xb6", StringUtil::KeyToReadingA("\xe3\x82\xab"));
  // "ｷ", "キ"
  EXPECT_EQ("\xef\xbd\xb7", StringUtil::KeyToReadingA("\xe3\x82\xad"));
  // "ｸ", "ク"
  EXPECT_EQ("\xef\xbd\xb8", StringUtil::KeyToReadingA("\xe3\x82\xaf"));
  // "ｹ", "ケ"
  EXPECT_EQ("\xef\xbd\xb9", StringUtil::KeyToReadingA("\xe3\x82\xb1"));
  // "ｺ", "コ"
  EXPECT_EQ("\xef\xbd\xba", StringUtil::KeyToReadingA("\xe3\x82\xb3"));
  // "ｶﾞ", "ガ"
  EXPECT_EQ("\xef\xbd\xb6\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x82\xac"));
  // "ｷﾞ", "ギ"
  EXPECT_EQ("\xef\xbd\xb7\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x82\xae"));
  // "ｸﾞ", "グ"
  EXPECT_EQ("\xef\xbd\xb8\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x82\xb0"));
  // "ｹﾞ", "ゲ"
  EXPECT_EQ("\xef\xbd\xb9\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x82\xb2"));
  // "ｺﾞ", "ゴ"
  EXPECT_EQ("\xef\xbd\xba\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x82\xb4"));
  // "ｻ", "サ"
  EXPECT_EQ("\xef\xbd\xbb", StringUtil::KeyToReadingA("\xe3\x82\xb5"));
  // "ｼ", "シ"
  EXPECT_EQ("\xef\xbd\xbc", StringUtil::KeyToReadingA("\xe3\x82\xb7"));
  // "ｽ", "ス"
  EXPECT_EQ("\xef\xbd\xbd", StringUtil::KeyToReadingA("\xe3\x82\xb9"));
  // "ｾ", "セ"
  EXPECT_EQ("\xef\xbd\xbe", StringUtil::KeyToReadingA("\xe3\x82\xbb"));
  // "ｿ", "ソ"
  EXPECT_EQ("\xef\xbd\xbf", StringUtil::KeyToReadingA("\xe3\x82\xbd"));
  // "ｻﾞ", "ザ"
  EXPECT_EQ("\xef\xbd\xbb\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x82\xb6"));
  // "ｼﾞ", "ジ"
  EXPECT_EQ("\xef\xbd\xbc\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x82\xb8"));
  // "ｽﾞ", "ズ"
  EXPECT_EQ("\xef\xbd\xbd\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x82\xba"));
  // "ｾﾞ", "ゼ"
  EXPECT_EQ("\xef\xbd\xbe\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x82\xbc"));
  // "ｿﾞ", "ゾ"
  EXPECT_EQ("\xef\xbd\xbf\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x82\xbe"));
  // "ﾀ", "タ"
  EXPECT_EQ("\xef\xbe\x80", StringUtil::KeyToReadingA("\xe3\x82\xbf"));
  // "ﾁ", "チ"
  EXPECT_EQ("\xef\xbe\x81", StringUtil::KeyToReadingA("\xe3\x83\x81"));
  // "ﾂ", "ツ"
  EXPECT_EQ("\xef\xbe\x82", StringUtil::KeyToReadingA("\xe3\x83\x84"));
  // "ﾃ", "テ"
  EXPECT_EQ("\xef\xbe\x83", StringUtil::KeyToReadingA("\xe3\x83\x86"));
  // "ﾄ", "ト"
  EXPECT_EQ("\xef\xbe\x84", StringUtil::KeyToReadingA("\xe3\x83\x88"));
  // "ﾀﾞ", "ダ"
  EXPECT_EQ("\xef\xbe\x80\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x83\x80"));
  // "ﾁﾞ", "ヂ"
  EXPECT_EQ("\xef\xbe\x81\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x83\x82"));
  // "ﾂﾞ", "ヅ"
  EXPECT_EQ("\xef\xbe\x82\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x83\x85"));
  // "ﾃﾞ", "デ"
  EXPECT_EQ("\xef\xbe\x83\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x83\x87"));
  // "ﾄﾞ", "ド"
  EXPECT_EQ("\xef\xbe\x84\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x83\x89"));
  // "ﾅ", "ナ"
  EXPECT_EQ("\xef\xbe\x85", StringUtil::KeyToReadingA("\xe3\x83\x8a"));
  // "ﾆ", "ニ"
  EXPECT_EQ("\xef\xbe\x86", StringUtil::KeyToReadingA("\xe3\x83\x8b"));
  // "ﾇ", "ヌ"
  EXPECT_EQ("\xef\xbe\x87", StringUtil::KeyToReadingA("\xe3\x83\x8c"));
  // "ﾈ", "ネ"
  EXPECT_EQ("\xef\xbe\x88", StringUtil::KeyToReadingA("\xe3\x83\x8d"));
  // "ﾉ", "ノ"
  EXPECT_EQ("\xef\xbe\x89", StringUtil::KeyToReadingA("\xe3\x83\x8e"));
  // "ﾊ", "ハ"
  EXPECT_EQ("\xef\xbe\x8a", StringUtil::KeyToReadingA("\xe3\x83\x8f"));
  // "ﾋ", "ヒ"
  EXPECT_EQ("\xef\xbe\x8b", StringUtil::KeyToReadingA("\xe3\x83\x92"));
  // "ﾌ", "フ"
  EXPECT_EQ("\xef\xbe\x8c", StringUtil::KeyToReadingA("\xe3\x83\x95"));
  // "ﾍ", "ヘ"
  EXPECT_EQ("\xef\xbe\x8d", StringUtil::KeyToReadingA("\xe3\x83\x98"));
  // "ﾎ", "ホ"
  EXPECT_EQ("\xef\xbe\x8e", StringUtil::KeyToReadingA("\xe3\x83\x9b"));
  // "ﾊﾞ", "バ"
  EXPECT_EQ("\xef\xbe\x8a\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x83\x90"));
  // "ﾋﾞ", "ビ"
  EXPECT_EQ("\xef\xbe\x8b\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x83\x93"));
  // "ﾌﾞ", "ブ"
  EXPECT_EQ("\xef\xbe\x8c\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x83\x96"));
  // "ﾍﾞ", "ベ"
  EXPECT_EQ("\xef\xbe\x8d\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x83\x99"));
  // "ﾎﾞ", "ボ"
  EXPECT_EQ("\xef\xbe\x8e\xef\xbe\x9e",
            StringUtil::KeyToReadingA("\xe3\x83\x9c"));
  // "ﾊﾟ", "パ"
  EXPECT_EQ("\xef\xbe\x8a\xef\xbe\x9f",
            StringUtil::KeyToReadingA("\xe3\x83\x91"));
  // "ﾋﾟ", "ピ"
  EXPECT_EQ("\xef\xbe\x8b\xef\xbe\x9f",
            StringUtil::KeyToReadingA("\xe3\x83\x94"));
  // "ﾌﾟ", "プ"
  EXPECT_EQ("\xef\xbe\x8c\xef\xbe\x9f",
            StringUtil::KeyToReadingA("\xe3\x83\x97"));
  // "ﾍﾟ", "ペ"
  EXPECT_EQ("\xef\xbe\x8d\xef\xbe\x9f",
            StringUtil::KeyToReadingA("\xe3\x83\x9a"));
  // "ﾎﾟ", "ポ"
  EXPECT_EQ("\xef\xbe\x8e\xef\xbe\x9f",
            StringUtil::KeyToReadingA("\xe3\x83\x9d"));
  // "ﾏ", "マ"
  EXPECT_EQ("\xef\xbe\x8f", StringUtil::KeyToReadingA("\xe3\x83\x9e"));
  // "ﾐ", "ミ"
  EXPECT_EQ("\xef\xbe\x90", StringUtil::KeyToReadingA("\xe3\x83\x9f"));
  // "ﾑ", "ム"
  EXPECT_EQ("\xef\xbe\x91", StringUtil::KeyToReadingA("\xe3\x83\xa0"));
  // "ﾒ", "メ"
  EXPECT_EQ("\xef\xbe\x92", StringUtil::KeyToReadingA("\xe3\x83\xa1"));
  // "ﾓ", "モ"
  EXPECT_EQ("\xef\xbe\x93", StringUtil::KeyToReadingA("\xe3\x83\xa2"));
  // "ﾔ", "ヤ"
  EXPECT_EQ("\xef\xbe\x94", StringUtil::KeyToReadingA("\xe3\x83\xa4"));
  // "ﾕ", "ユ"
  EXPECT_EQ("\xef\xbe\x95", StringUtil::KeyToReadingA("\xe3\x83\xa6"));
  // "ﾖ", "ヨ"
  EXPECT_EQ("\xef\xbe\x96", StringUtil::KeyToReadingA("\xe3\x83\xa8"));
  // "ﾜ", "ワ"
  EXPECT_EQ("\xef\xbe\x9c", StringUtil::KeyToReadingA("\xe3\x83\xaf"));
  // "ｦ", "ヲ"
  EXPECT_EQ("\xef\xbd\xa6", StringUtil::KeyToReadingA("\xe3\x83\xb2"));
  // "ﾝ", "ン"
  EXPECT_EQ("\xef\xbe\x9d", StringUtil::KeyToReadingA("\xe3\x83\xb3"));
}

TEST(StringUtilTest, AlphaNumeric) {
    // "！"
  EXPECT_EQ("!", StringUtil::KeyToReadingA("\xef\xbc\x81"));
  // "￥"
  EXPECT_EQ("\"", StringUtil::KeyToReadingA("\xe2\x80\x9d"));
  // "＃"
  EXPECT_EQ("#", StringUtil::KeyToReadingA("\xef\xbc\x83"));
  // "＄"
  EXPECT_EQ("$", StringUtil::KeyToReadingA("\xef\xbc\x84"));
  // "％"
  EXPECT_EQ("%", StringUtil::KeyToReadingA("\xef\xbc\x85"));
  // "＆"
  EXPECT_EQ("&", StringUtil::KeyToReadingA("\xef\xbc\x86"));
  // "’"
  EXPECT_EQ("'", StringUtil::KeyToReadingA("\xe2\x80\x99"));
  // "（"
  EXPECT_EQ("(", StringUtil::KeyToReadingA("\xef\xbc\x88"));
  // "）"
  EXPECT_EQ(")", StringUtil::KeyToReadingA("\xef\xbc\x89"));
  // "＝"
  EXPECT_EQ("=", StringUtil::KeyToReadingA("\xef\xbc\x9d"));
  // "－"
  EXPECT_EQ("-", StringUtil::KeyToReadingA("\xef\xbc\x8d"));
  // "～"
  EXPECT_EQ("~", StringUtil::KeyToReadingA("\xef\xbd\x9e"));
  // "＾"
  EXPECT_EQ("^", StringUtil::KeyToReadingA("\xef\xbc\xbe"));
  // "｜"
  EXPECT_EQ("|", StringUtil::KeyToReadingA("\xef\xbd\x9c"));
  // "￥"
  EXPECT_EQ("\\", StringUtil::KeyToReadingA("\xef\xbf\xa5"));
  // "‘"
  EXPECT_EQ("`", StringUtil::KeyToReadingA("\xe2\x80\x98"));
  // "ﾞ", "゛"
  EXPECT_EQ("\xef\xbe\x9e", StringUtil::KeyToReadingA("\xe3\x82\x9b"));
  // "＠"
  EXPECT_EQ("@", StringUtil::KeyToReadingA("\xef\xbc\xa0"));
  // "｛"
  EXPECT_EQ("{", StringUtil::KeyToReadingA("\xef\xbd\x9b"));
  // "｢", "「"
  EXPECT_EQ("\xef\xbd\xa2", StringUtil::KeyToReadingA("\xe3\x80\x8c"));
  // "＋"
  EXPECT_EQ("+", StringUtil::KeyToReadingA("\xef\xbc\x8b"));
  // "；"
  EXPECT_EQ(";", StringUtil::KeyToReadingA("\xef\xbc\x9b"));
  // "＊"
  EXPECT_EQ("*", StringUtil::KeyToReadingA("\xef\xbc\x8a"));
  // "："
  EXPECT_EQ(":", StringUtil::KeyToReadingA("\xef\xbc\x9a"));
  // "｝"
  EXPECT_EQ("}", StringUtil::KeyToReadingA("\xef\xbd\x9d"));
  // "｣", "」"
  EXPECT_EQ("\xef\xbd\xa3", StringUtil::KeyToReadingA("\xe3\x80\x8d"));
  // "＜"
  EXPECT_EQ("<", StringUtil::KeyToReadingA("\xef\xbc\x9c"));
  // "､", "、"
  EXPECT_EQ("\xef\xbd\xa4", StringUtil::KeyToReadingA("\xe3\x80\x81"));
  // "＞"
  EXPECT_EQ(">", StringUtil::KeyToReadingA("\xef\xbc\x9e"));
  // "｡", "。"
  EXPECT_EQ("\xef\xbd\xa1", StringUtil::KeyToReadingA("\xe3\x80\x82"));
  // "？"
  EXPECT_EQ("?", StringUtil::KeyToReadingA("\xef\xbc\x9f"));
  // "･", "・"
  EXPECT_EQ("\xef\xbd\xa5", StringUtil::KeyToReadingA("\xe3\x83\xbb"));
  // "＿"
  EXPECT_EQ("_", StringUtil::KeyToReadingA("\xef\xbc\xbf"));
  // "１"
  EXPECT_EQ("1", StringUtil::KeyToReadingA("\xef\xbc\x91"));
  // "２"
  EXPECT_EQ("2", StringUtil::KeyToReadingA("\xef\xbc\x92"));
  // "３"
  EXPECT_EQ("3", StringUtil::KeyToReadingA("\xef\xbc\x93"));
  // "４"
  EXPECT_EQ("4", StringUtil::KeyToReadingA("\xef\xbc\x94"));
  // "５"
  EXPECT_EQ("5", StringUtil::KeyToReadingA("\xef\xbc\x95"));
  // "６"
  EXPECT_EQ("6", StringUtil::KeyToReadingA("\xef\xbc\x96"));
  // "７"
  EXPECT_EQ("7", StringUtil::KeyToReadingA("\xef\xbc\x97"));
  // "８"
  EXPECT_EQ("8", StringUtil::KeyToReadingA("\xef\xbc\x98"));
  // "９"
  EXPECT_EQ("9", StringUtil::KeyToReadingA("\xef\xbc\x99"));
  // "０"
  EXPECT_EQ("0", StringUtil::KeyToReadingA("\xef\xbc\x90"));
  // "ａ"
  EXPECT_EQ("a", StringUtil::KeyToReadingA("\xef\xbd\x81"));
  // "ｂ"
  EXPECT_EQ("b", StringUtil::KeyToReadingA("\xef\xbd\x82"));
  // "ｃ"
  EXPECT_EQ("c", StringUtil::KeyToReadingA("\xef\xbd\x83"));
  // "ｄ"
  EXPECT_EQ("d", StringUtil::KeyToReadingA("\xef\xbd\x84"));
  // "ｅ"
  EXPECT_EQ("e", StringUtil::KeyToReadingA("\xef\xbd\x85"));
  // "ｆ"
  EXPECT_EQ("f", StringUtil::KeyToReadingA("\xef\xbd\x86"));
  // "ｇ"
  EXPECT_EQ("g", StringUtil::KeyToReadingA("\xef\xbd\x87"));
  // "ｈ"
  EXPECT_EQ("h", StringUtil::KeyToReadingA("\xef\xbd\x88"));
  // "ｉ"
  EXPECT_EQ("i", StringUtil::KeyToReadingA("\xef\xbd\x89"));
  // "ｊ"
  EXPECT_EQ("j", StringUtil::KeyToReadingA("\xef\xbd\x8a"));
  // "ｋ"
  EXPECT_EQ("k", StringUtil::KeyToReadingA("\xef\xbd\x8b"));
  // "ｌ"
  EXPECT_EQ("l", StringUtil::KeyToReadingA("\xef\xbd\x8c"));
  // "ｍ"
  EXPECT_EQ("m", StringUtil::KeyToReadingA("\xef\xbd\x8d"));
  // "ｎ"
  EXPECT_EQ("n", StringUtil::KeyToReadingA("\xef\xbd\x8e"));
  // "ｏ"
  EXPECT_EQ("o", StringUtil::KeyToReadingA("\xef\xbd\x8f"));
  // "ｐ"
  EXPECT_EQ("p", StringUtil::KeyToReadingA("\xef\xbd\x90"));
  // "ｑ"
  EXPECT_EQ("q", StringUtil::KeyToReadingA("\xef\xbd\x91"));
  // "ｒ"
  EXPECT_EQ("r", StringUtil::KeyToReadingA("\xef\xbd\x92"));
  // "ｓ"
  EXPECT_EQ("s", StringUtil::KeyToReadingA("\xef\xbd\x93"));
  // "ｔ"
  EXPECT_EQ("t", StringUtil::KeyToReadingA("\xef\xbd\x94"));
  // "ｕ"
  EXPECT_EQ("u", StringUtil::KeyToReadingA("\xef\xbd\x95"));
  // "ｖ"
  EXPECT_EQ("v", StringUtil::KeyToReadingA("\xef\xbd\x96"));
  // "ｗ"
  EXPECT_EQ("w", StringUtil::KeyToReadingA("\xef\xbd\x97"));
  // "ｘ"
  EXPECT_EQ("x", StringUtil::KeyToReadingA("\xef\xbd\x98"));
  // "ｙ"
  EXPECT_EQ("y", StringUtil::KeyToReadingA("\xef\xbd\x99"));
  // "ｚ"
  EXPECT_EQ("z", StringUtil::KeyToReadingA("\xef\xbd\x9a"));
  // "Ａ"
  EXPECT_EQ("A", StringUtil::KeyToReadingA("\xef\xbc\xa1"));
  // "Ｂ"
  EXPECT_EQ("B", StringUtil::KeyToReadingA("\xef\xbc\xa2"));
  // "Ｃ"
  EXPECT_EQ("C", StringUtil::KeyToReadingA("\xef\xbc\xa3"));
  // "Ｄ"
  EXPECT_EQ("D", StringUtil::KeyToReadingA("\xef\xbc\xa4"));
  // "Ｅ"
  EXPECT_EQ("E", StringUtil::KeyToReadingA("\xef\xbc\xa5"));
  // "Ｆ"
  EXPECT_EQ("F", StringUtil::KeyToReadingA("\xef\xbc\xa6"));
  // "Ｇ"
  EXPECT_EQ("G", StringUtil::KeyToReadingA("\xef\xbc\xa7"));
  // "Ｈ"
  EXPECT_EQ("H", StringUtil::KeyToReadingA("\xef\xbc\xa8"));
  // "Ｉ"
  EXPECT_EQ("I", StringUtil::KeyToReadingA("\xef\xbc\xa9"));
  // "Ｊ"
  EXPECT_EQ("J", StringUtil::KeyToReadingA("\xef\xbc\xaa"));
  // "Ｋ"
  EXPECT_EQ("K", StringUtil::KeyToReadingA("\xef\xbc\xab"));
  // "Ｌ"
  EXPECT_EQ("L", StringUtil::KeyToReadingA("\xef\xbc\xac"));
  // "Ｍ"
  EXPECT_EQ("M", StringUtil::KeyToReadingA("\xef\xbc\xad"));
  // "Ｎ"
  EXPECT_EQ("N", StringUtil::KeyToReadingA("\xef\xbc\xae"));
  // "Ｏ"
  EXPECT_EQ("O", StringUtil::KeyToReadingA("\xef\xbc\xaf"));
  // "Ｐ"
  EXPECT_EQ("P", StringUtil::KeyToReadingA("\xef\xbc\xb0"));
  // "Ｑ"
  EXPECT_EQ("Q", StringUtil::KeyToReadingA("\xef\xbc\xb1"));
  // "Ｒ"
  EXPECT_EQ("R", StringUtil::KeyToReadingA("\xef\xbc\xb2"));
  // "Ｓ"
  EXPECT_EQ("S", StringUtil::KeyToReadingA("\xef\xbc\xb3"));
  // "Ｔ"
  EXPECT_EQ("T", StringUtil::KeyToReadingA("\xef\xbc\xb4"));
  // "Ｕ"
  EXPECT_EQ("U", StringUtil::KeyToReadingA("\xef\xbc\xb5"));
  // "Ｖ"
  EXPECT_EQ("V", StringUtil::KeyToReadingA("\xef\xbc\xb6"));
  // "Ｗ"
  EXPECT_EQ("W", StringUtil::KeyToReadingA("\xef\xbc\xb7"));
  // "Ｘ"
  EXPECT_EQ("X", StringUtil::KeyToReadingA("\xef\xbc\xb8"));
  // "Ｙ"
  EXPECT_EQ("Y", StringUtil::KeyToReadingA("\xef\xbc\xb9"));
  // "Ｚ"
  EXPECT_EQ("Z", StringUtil::KeyToReadingA("\xef\xbc\xba"));
}

TEST(StringUtilTest, LCMapStringATest) {
  DWORD lcid = MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT),
                        SORT_JAPANESE_XJIS);

  char buf[512];
  // backquote
  size_t len = LCMapStringA(lcid, LCMAP_HALFWIDTH, "\x81\x65", -1, buf,
                            sizeof(buf));
  EXPECT_EQ(2, len);
  // LCMapStringA converts "\x81\x65" (backquote) to ' for some reason.
  // EXPECT_EQ('`', buf[0]);
  EXPECT_EQ('\'', buf[0]);

  // quote
  len = LCMapStringA(lcid, LCMAP_HALFWIDTH, "\x81\x66", -1, buf,
                     sizeof(buf));
  EXPECT_EQ(2, len);
  EXPECT_EQ('\'', buf[0]);
}

TEST(StringUtilTest, ComposePreeditText) {
  commands::Preedit preedit;
  preedit.add_segment()->set_value("a");
  preedit.add_segment()->set_value("b");
  preedit.add_segment()->set_value("c");
  EXPECT_EQ(L"abc", StringUtil::ComposePreeditText(preedit));
}

}  // namespace win32
}  // namespace mozc

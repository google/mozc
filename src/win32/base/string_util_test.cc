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

#include "win32/base/string_util.h"

#include <string>

#include "protocol/commands.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace win32 {

TEST(StringUtilTest, InvalidCases) {
  EXPECT_EQ(StringUtil::KeyToReadingA(""), "");
  // KeyToReadingA fails if the resultant string is longer than 512 characters.
  std::string longa(10000, 'a');
  EXPECT_EQ(StringUtil::KeyToReadingA(longa), "");
}

TEST(StringUtilTest, Hiragana) {
  EXPECT_EQ(StringUtil::KeyToReadingA("あ"), "ｱ");
  EXPECT_EQ(StringUtil::KeyToReadingA("い"), "ｲ");
  EXPECT_EQ(StringUtil::KeyToReadingA("う"), "ｳ");
  EXPECT_EQ(StringUtil::KeyToReadingA("え"), "ｴ");
  EXPECT_EQ(StringUtil::KeyToReadingA("お"), "ｵ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぁ"), "ｧ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぃ"), "ｨ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぅ"), "ｩ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぇ"), "ｪ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぉ"), "ｫ");
  EXPECT_EQ(StringUtil::KeyToReadingA("か"), "ｶ");
  EXPECT_EQ(StringUtil::KeyToReadingA("き"), "ｷ");
  EXPECT_EQ(StringUtil::KeyToReadingA("く"), "ｸ");
  EXPECT_EQ(StringUtil::KeyToReadingA("け"), "ｹ");
  EXPECT_EQ(StringUtil::KeyToReadingA("こ"), "ｺ");
  EXPECT_EQ(StringUtil::KeyToReadingA("が"), "ｶﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぎ"), "ｷﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぐ"), "ｸﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("げ"), "ｹﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ご"), "ｺﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("さ"), "ｻ");
  EXPECT_EQ(StringUtil::KeyToReadingA("し"), "ｼ");
  EXPECT_EQ(StringUtil::KeyToReadingA("す"), "ｽ");
  EXPECT_EQ(StringUtil::KeyToReadingA("せ"), "ｾ");
  EXPECT_EQ(StringUtil::KeyToReadingA("そ"), "ｿ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ざ"), "ｻﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("じ"), "ｼﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ず"), "ｽﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぜ"), "ｾﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぞ"), "ｿﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("た"), "ﾀ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ち"), "ﾁ");
  EXPECT_EQ(StringUtil::KeyToReadingA("つ"), "ﾂ");
  EXPECT_EQ(StringUtil::KeyToReadingA("て"), "ﾃ");
  EXPECT_EQ(StringUtil::KeyToReadingA("と"), "ﾄ");
  EXPECT_EQ(StringUtil::KeyToReadingA("だ"), "ﾀﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぢ"), "ﾁﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("づ"), "ﾂﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("で"), "ﾃﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ど"), "ﾄﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("っ"), "ｯ");
  EXPECT_EQ(StringUtil::KeyToReadingA("な"), "ﾅ");
  EXPECT_EQ(StringUtil::KeyToReadingA("に"), "ﾆ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぬ"), "ﾇ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ね"), "ﾈ");
  EXPECT_EQ(StringUtil::KeyToReadingA("の"), "ﾉ");
  EXPECT_EQ(StringUtil::KeyToReadingA("は"), "ﾊ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ひ"), "ﾋ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ふ"), "ﾌ");
  EXPECT_EQ(StringUtil::KeyToReadingA("へ"), "ﾍ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ほ"), "ﾎ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ば"), "ﾊﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("び"), "ﾋﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぶ"), "ﾌﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("べ"), "ﾍﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぼ"), "ﾎﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぱ"), "ﾊﾟ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぴ"), "ﾋﾟ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぷ"), "ﾌﾟ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぺ"), "ﾍﾟ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ぽ"), "ﾎﾟ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ま"), "ﾏ");
  EXPECT_EQ(StringUtil::KeyToReadingA("み"), "ﾐ");
  EXPECT_EQ(StringUtil::KeyToReadingA("む"), "ﾑ");
  EXPECT_EQ(StringUtil::KeyToReadingA("め"), "ﾒ");
  EXPECT_EQ(StringUtil::KeyToReadingA("も"), "ﾓ");
  EXPECT_EQ(StringUtil::KeyToReadingA("や"), "ﾔ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ゆ"), "ﾕ");
  EXPECT_EQ(StringUtil::KeyToReadingA("よ"), "ﾖ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ゃ"), "ｬ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ゅ"), "ｭ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ょ"), "ｮ");
  EXPECT_EQ(StringUtil::KeyToReadingA("わ"), "ﾜ");
  EXPECT_EQ(StringUtil::KeyToReadingA("を"), "ｦ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ん"), "ﾝ");
}

TEST(StringUtilTest, Katakana) {
  EXPECT_EQ(StringUtil::KeyToReadingA("ア"), "ｱ");
  EXPECT_EQ(StringUtil::KeyToReadingA("イ"), "ｲ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ウ"), "ｳ");
  EXPECT_EQ(StringUtil::KeyToReadingA("エ"), "ｴ");
  EXPECT_EQ(StringUtil::KeyToReadingA("オ"), "ｵ");
  EXPECT_EQ(StringUtil::KeyToReadingA("カ"), "ｶ");
  EXPECT_EQ(StringUtil::KeyToReadingA("キ"), "ｷ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ク"), "ｸ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ケ"), "ｹ");
  EXPECT_EQ(StringUtil::KeyToReadingA("コ"), "ｺ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ガ"), "ｶﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ギ"), "ｷﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("グ"), "ｸﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ゲ"), "ｹﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ゴ"), "ｺﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("サ"), "ｻ");
  EXPECT_EQ(StringUtil::KeyToReadingA("シ"), "ｼ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ス"), "ｽ");
  EXPECT_EQ(StringUtil::KeyToReadingA("セ"), "ｾ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ソ"), "ｿ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ザ"), "ｻﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ジ"), "ｼﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ズ"), "ｽﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ゼ"), "ｾﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ゾ"), "ｿﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("タ"), "ﾀ");
  EXPECT_EQ(StringUtil::KeyToReadingA("チ"), "ﾁ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ツ"), "ﾂ");
  EXPECT_EQ(StringUtil::KeyToReadingA("テ"), "ﾃ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ト"), "ﾄ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ダ"), "ﾀﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ヂ"), "ﾁﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ヅ"), "ﾂﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("デ"), "ﾃﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ド"), "ﾄﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ナ"), "ﾅ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ニ"), "ﾆ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ヌ"), "ﾇ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ネ"), "ﾈ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ノ"), "ﾉ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ハ"), "ﾊ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ヒ"), "ﾋ");
  EXPECT_EQ(StringUtil::KeyToReadingA("フ"), "ﾌ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ヘ"), "ﾍ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ホ"), "ﾎ");
  EXPECT_EQ(StringUtil::KeyToReadingA("バ"), "ﾊﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ビ"), "ﾋﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ブ"), "ﾌﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ベ"), "ﾍﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ボ"), "ﾎﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("パ"), "ﾊﾟ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ピ"), "ﾋﾟ");
  EXPECT_EQ(StringUtil::KeyToReadingA("プ"), "ﾌﾟ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ペ"), "ﾍﾟ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ポ"), "ﾎﾟ");
  EXPECT_EQ(StringUtil::KeyToReadingA("マ"), "ﾏ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ミ"), "ﾐ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ム"), "ﾑ");
  EXPECT_EQ(StringUtil::KeyToReadingA("メ"), "ﾒ");
  EXPECT_EQ(StringUtil::KeyToReadingA("モ"), "ﾓ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ヤ"), "ﾔ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ユ"), "ﾕ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ヨ"), "ﾖ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ワ"), "ﾜ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ヲ"), "ｦ");
  EXPECT_EQ(StringUtil::KeyToReadingA("ン"), "ﾝ");
}

TEST(StringUtilTest, AlphaNumeric) {
  EXPECT_EQ(StringUtil::KeyToReadingA("！"), "!");
  EXPECT_EQ(StringUtil::KeyToReadingA("”"), "\"");
  EXPECT_EQ(StringUtil::KeyToReadingA("＃"), "#");
  EXPECT_EQ(StringUtil::KeyToReadingA("＄"), "$");
  EXPECT_EQ(StringUtil::KeyToReadingA("％"), "%");
  EXPECT_EQ(StringUtil::KeyToReadingA("＆"), "&");
  EXPECT_EQ(StringUtil::KeyToReadingA("’"), "'");
  EXPECT_EQ(StringUtil::KeyToReadingA("（"), "(");
  EXPECT_EQ(StringUtil::KeyToReadingA("）"), ")");
  EXPECT_EQ(StringUtil::KeyToReadingA("＝"), "=");
  EXPECT_EQ(StringUtil::KeyToReadingA("－"), "-");
  EXPECT_EQ(StringUtil::KeyToReadingA("～"), "~");
  EXPECT_EQ(StringUtil::KeyToReadingA("＾"), "^");
  EXPECT_EQ(StringUtil::KeyToReadingA("｜"), "|");
  EXPECT_EQ(StringUtil::KeyToReadingA("￥"), "\\");
  EXPECT_EQ(StringUtil::KeyToReadingA("‘"), "`");
  EXPECT_EQ(StringUtil::KeyToReadingA("゛"), "ﾞ");
  EXPECT_EQ(StringUtil::KeyToReadingA("＠"), "@");
  EXPECT_EQ(StringUtil::KeyToReadingA("｛"), "{");
  EXPECT_EQ(StringUtil::KeyToReadingA("「"), "｢");
  EXPECT_EQ(StringUtil::KeyToReadingA("＋"), "+");
  EXPECT_EQ(StringUtil::KeyToReadingA("；"), ";");
  EXPECT_EQ(StringUtil::KeyToReadingA("＊"), "*");
  EXPECT_EQ(StringUtil::KeyToReadingA("："), ":");
  EXPECT_EQ(StringUtil::KeyToReadingA("｝"), "}");
  EXPECT_EQ(StringUtil::KeyToReadingA("」"), "｣");
  EXPECT_EQ(StringUtil::KeyToReadingA("＜"), "<");
  EXPECT_EQ(StringUtil::KeyToReadingA("、"), "､");
  EXPECT_EQ(StringUtil::KeyToReadingA("＞"), ">");
  EXPECT_EQ(StringUtil::KeyToReadingA("。"), "｡");
  EXPECT_EQ(StringUtil::KeyToReadingA("？"), "?");
  EXPECT_EQ(StringUtil::KeyToReadingA("・"), "･");
  EXPECT_EQ(StringUtil::KeyToReadingA("＿"), "_");
  EXPECT_EQ(StringUtil::KeyToReadingA("１"), "1");
  EXPECT_EQ(StringUtil::KeyToReadingA("２"), "2");
  EXPECT_EQ(StringUtil::KeyToReadingA("３"), "3");
  EXPECT_EQ(StringUtil::KeyToReadingA("４"), "4");
  EXPECT_EQ(StringUtil::KeyToReadingA("５"), "5");
  EXPECT_EQ(StringUtil::KeyToReadingA("６"), "6");
  EXPECT_EQ(StringUtil::KeyToReadingA("７"), "7");
  EXPECT_EQ(StringUtil::KeyToReadingA("８"), "8");
  EXPECT_EQ(StringUtil::KeyToReadingA("９"), "9");
  EXPECT_EQ(StringUtil::KeyToReadingA("０"), "0");
  EXPECT_EQ(StringUtil::KeyToReadingA("ａ"), "a");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｂ"), "b");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｃ"), "c");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｄ"), "d");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｅ"), "e");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｆ"), "f");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｇ"), "g");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｈ"), "h");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｉ"), "i");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｊ"), "j");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｋ"), "k");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｌ"), "l");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｍ"), "m");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｎ"), "n");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｏ"), "o");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｐ"), "p");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｑ"), "q");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｒ"), "r");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｓ"), "s");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｔ"), "t");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｕ"), "u");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｖ"), "v");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｗ"), "w");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｘ"), "x");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｙ"), "y");
  EXPECT_EQ(StringUtil::KeyToReadingA("ｚ"), "z");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ａ"), "A");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｂ"), "B");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｃ"), "C");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｄ"), "D");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｅ"), "E");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｆ"), "F");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｇ"), "G");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｈ"), "H");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｉ"), "I");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｊ"), "J");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｋ"), "K");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｌ"), "L");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｍ"), "M");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｎ"), "N");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｏ"), "O");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｐ"), "P");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｑ"), "Q");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｒ"), "R");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｓ"), "S");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｔ"), "T");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｕ"), "U");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｖ"), "V");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｗ"), "W");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｘ"), "X");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｙ"), "Y");
  EXPECT_EQ(StringUtil::KeyToReadingA("Ｚ"), "Z");
}

TEST(StringUtilTest, LCMapStringATest) {
  DWORD lcid =
      MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), SORT_JAPANESE_XJIS);

  char buf[512];
  // backquote
  size_t len =
      LCMapStringA(lcid, LCMAP_HALFWIDTH, "\x81\x65", -1, buf, sizeof(buf));
  EXPECT_EQ(len, 2);
  // LCMapStringA converts "\x81\x65" (backquote) to ' for some reason.
  // EXPECT_EQ(buf[0], '`');
  EXPECT_EQ(buf[0], '\'');

  // quote
  len = LCMapStringA(lcid, LCMAP_HALFWIDTH, "\x81\x66", -1, buf, sizeof(buf));
  EXPECT_EQ(len, 2);
  EXPECT_EQ(buf[0], '\'');
}

TEST(StringUtilTest, ComposePreeditText) {
  commands::Preedit preedit;
  preedit.add_segment()->set_value("a");
  preedit.add_segment()->set_value("b");
  preedit.add_segment()->set_value("c");
  EXPECT_EQ(StringUtil::ComposePreeditText(preedit), L"abc");
}

}  // namespace win32
}  // namespace mozc

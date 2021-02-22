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
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace win32 {

TEST(StringUtilTest, InvalidCases) {
  EXPECT_EQ("", StringUtil::KeyToReadingA(""));
  // KeyToReadingA fails if the resultant string is longer than 512 characters.
  std::string longa(10000, 'a');
  EXPECT_EQ("", StringUtil::KeyToReadingA(longa));
}

TEST(StringUtilTest, Hiragana) {
  EXPECT_EQ("ｱ", StringUtil::KeyToReadingA("あ"));
  EXPECT_EQ("ｲ", StringUtil::KeyToReadingA("い"));
  EXPECT_EQ("ｳ", StringUtil::KeyToReadingA("う"));
  EXPECT_EQ("ｴ", StringUtil::KeyToReadingA("え"));
  EXPECT_EQ("ｵ", StringUtil::KeyToReadingA("お"));
  EXPECT_EQ("ｧ", StringUtil::KeyToReadingA("ぁ"));
  EXPECT_EQ("ｨ", StringUtil::KeyToReadingA("ぃ"));
  EXPECT_EQ("ｩ", StringUtil::KeyToReadingA("ぅ"));
  EXPECT_EQ("ｪ", StringUtil::KeyToReadingA("ぇ"));
  EXPECT_EQ("ｫ", StringUtil::KeyToReadingA("ぉ"));
  EXPECT_EQ("ｶ", StringUtil::KeyToReadingA("か"));
  EXPECT_EQ("ｷ", StringUtil::KeyToReadingA("き"));
  EXPECT_EQ("ｸ", StringUtil::KeyToReadingA("く"));
  EXPECT_EQ("ｹ", StringUtil::KeyToReadingA("け"));
  EXPECT_EQ("ｺ", StringUtil::KeyToReadingA("こ"));
  EXPECT_EQ("ｶﾞ", StringUtil::KeyToReadingA("が"));
  EXPECT_EQ("ｷﾞ", StringUtil::KeyToReadingA("ぎ"));
  EXPECT_EQ("ｸﾞ", StringUtil::KeyToReadingA("ぐ"));
  EXPECT_EQ("ｹﾞ", StringUtil::KeyToReadingA("げ"));
  EXPECT_EQ("ｺﾞ", StringUtil::KeyToReadingA("ご"));
  EXPECT_EQ("ｻ", StringUtil::KeyToReadingA("さ"));
  EXPECT_EQ("ｼ", StringUtil::KeyToReadingA("し"));
  EXPECT_EQ("ｽ", StringUtil::KeyToReadingA("す"));
  EXPECT_EQ("ｾ", StringUtil::KeyToReadingA("せ"));
  EXPECT_EQ("ｿ", StringUtil::KeyToReadingA("そ"));
  EXPECT_EQ("ｻﾞ", StringUtil::KeyToReadingA("ざ"));
  EXPECT_EQ("ｼﾞ", StringUtil::KeyToReadingA("じ"));
  EXPECT_EQ("ｽﾞ", StringUtil::KeyToReadingA("ず"));
  EXPECT_EQ("ｾﾞ", StringUtil::KeyToReadingA("ぜ"));
  EXPECT_EQ("ｿﾞ", StringUtil::KeyToReadingA("ぞ"));
  EXPECT_EQ("ﾀ", StringUtil::KeyToReadingA("た"));
  EXPECT_EQ("ﾁ", StringUtil::KeyToReadingA("ち"));
  EXPECT_EQ("ﾂ", StringUtil::KeyToReadingA("つ"));
  EXPECT_EQ("ﾃ", StringUtil::KeyToReadingA("て"));
  EXPECT_EQ("ﾄ", StringUtil::KeyToReadingA("と"));
  EXPECT_EQ("ﾀﾞ", StringUtil::KeyToReadingA("だ"));
  EXPECT_EQ("ﾁﾞ", StringUtil::KeyToReadingA("ぢ"));
  EXPECT_EQ("ﾂﾞ", StringUtil::KeyToReadingA("づ"));
  EXPECT_EQ("ﾃﾞ", StringUtil::KeyToReadingA("で"));
  EXPECT_EQ("ﾄﾞ", StringUtil::KeyToReadingA("ど"));
  EXPECT_EQ("ｯ", StringUtil::KeyToReadingA("っ"));
  EXPECT_EQ("ﾅ", StringUtil::KeyToReadingA("な"));
  EXPECT_EQ("ﾆ", StringUtil::KeyToReadingA("に"));
  EXPECT_EQ("ﾇ", StringUtil::KeyToReadingA("ぬ"));
  EXPECT_EQ("ﾈ", StringUtil::KeyToReadingA("ね"));
  EXPECT_EQ("ﾉ", StringUtil::KeyToReadingA("の"));
  EXPECT_EQ("ﾊ", StringUtil::KeyToReadingA("は"));
  EXPECT_EQ("ﾋ", StringUtil::KeyToReadingA("ひ"));
  EXPECT_EQ("ﾌ", StringUtil::KeyToReadingA("ふ"));
  EXPECT_EQ("ﾍ", StringUtil::KeyToReadingA("へ"));
  EXPECT_EQ("ﾎ", StringUtil::KeyToReadingA("ほ"));
  EXPECT_EQ("ﾊﾞ", StringUtil::KeyToReadingA("ば"));
  EXPECT_EQ("ﾋﾞ", StringUtil::KeyToReadingA("び"));
  EXPECT_EQ("ﾌﾞ", StringUtil::KeyToReadingA("ぶ"));
  EXPECT_EQ("ﾍﾞ", StringUtil::KeyToReadingA("べ"));
  EXPECT_EQ("ﾎﾞ", StringUtil::KeyToReadingA("ぼ"));
  EXPECT_EQ("ﾊﾟ", StringUtil::KeyToReadingA("ぱ"));
  EXPECT_EQ("ﾋﾟ", StringUtil::KeyToReadingA("ぴ"));
  EXPECT_EQ("ﾌﾟ", StringUtil::KeyToReadingA("ぷ"));
  EXPECT_EQ("ﾍﾟ", StringUtil::KeyToReadingA("ぺ"));
  EXPECT_EQ("ﾎﾟ", StringUtil::KeyToReadingA("ぽ"));
  EXPECT_EQ("ﾏ", StringUtil::KeyToReadingA("ま"));
  EXPECT_EQ("ﾐ", StringUtil::KeyToReadingA("み"));
  EXPECT_EQ("ﾑ", StringUtil::KeyToReadingA("む"));
  EXPECT_EQ("ﾒ", StringUtil::KeyToReadingA("め"));
  EXPECT_EQ("ﾓ", StringUtil::KeyToReadingA("も"));
  EXPECT_EQ("ﾔ", StringUtil::KeyToReadingA("や"));
  EXPECT_EQ("ﾕ", StringUtil::KeyToReadingA("ゆ"));
  EXPECT_EQ("ﾖ", StringUtil::KeyToReadingA("よ"));
  EXPECT_EQ("ｬ", StringUtil::KeyToReadingA("ゃ"));
  EXPECT_EQ("ｭ", StringUtil::KeyToReadingA("ゅ"));
  EXPECT_EQ("ｮ", StringUtil::KeyToReadingA("ょ"));
  EXPECT_EQ("ﾜ", StringUtil::KeyToReadingA("わ"));
  EXPECT_EQ("ｦ", StringUtil::KeyToReadingA("を"));
  EXPECT_EQ("ﾝ", StringUtil::KeyToReadingA("ん"));
}

TEST(StringUtilTest, Katakana) {
  EXPECT_EQ("ｱ", StringUtil::KeyToReadingA("ア"));
  EXPECT_EQ("ｲ", StringUtil::KeyToReadingA("イ"));
  EXPECT_EQ("ｳ", StringUtil::KeyToReadingA("ウ"));
  EXPECT_EQ("ｴ", StringUtil::KeyToReadingA("エ"));
  EXPECT_EQ("ｵ", StringUtil::KeyToReadingA("オ"));
  EXPECT_EQ("ｶ", StringUtil::KeyToReadingA("カ"));
  EXPECT_EQ("ｷ", StringUtil::KeyToReadingA("キ"));
  EXPECT_EQ("ｸ", StringUtil::KeyToReadingA("ク"));
  EXPECT_EQ("ｹ", StringUtil::KeyToReadingA("ケ"));
  EXPECT_EQ("ｺ", StringUtil::KeyToReadingA("コ"));
  EXPECT_EQ("ｶﾞ", StringUtil::KeyToReadingA("ガ"));
  EXPECT_EQ("ｷﾞ", StringUtil::KeyToReadingA("ギ"));
  EXPECT_EQ("ｸﾞ", StringUtil::KeyToReadingA("グ"));
  EXPECT_EQ("ｹﾞ", StringUtil::KeyToReadingA("ゲ"));
  EXPECT_EQ("ｺﾞ", StringUtil::KeyToReadingA("ゴ"));
  EXPECT_EQ("ｻ", StringUtil::KeyToReadingA("サ"));
  EXPECT_EQ("ｼ", StringUtil::KeyToReadingA("シ"));
  EXPECT_EQ("ｽ", StringUtil::KeyToReadingA("ス"));
  EXPECT_EQ("ｾ", StringUtil::KeyToReadingA("セ"));
  EXPECT_EQ("ｿ", StringUtil::KeyToReadingA("ソ"));
  EXPECT_EQ("ｻﾞ", StringUtil::KeyToReadingA("ザ"));
  EXPECT_EQ("ｼﾞ", StringUtil::KeyToReadingA("ジ"));
  EXPECT_EQ("ｽﾞ", StringUtil::KeyToReadingA("ズ"));
  EXPECT_EQ("ｾﾞ", StringUtil::KeyToReadingA("ゼ"));
  EXPECT_EQ("ｿﾞ", StringUtil::KeyToReadingA("ゾ"));
  EXPECT_EQ("ﾀ", StringUtil::KeyToReadingA("タ"));
  EXPECT_EQ("ﾁ", StringUtil::KeyToReadingA("チ"));
  EXPECT_EQ("ﾂ", StringUtil::KeyToReadingA("ツ"));
  EXPECT_EQ("ﾃ", StringUtil::KeyToReadingA("テ"));
  EXPECT_EQ("ﾄ", StringUtil::KeyToReadingA("ト"));
  EXPECT_EQ("ﾀﾞ", StringUtil::KeyToReadingA("ダ"));
  EXPECT_EQ("ﾁﾞ", StringUtil::KeyToReadingA("ヂ"));
  EXPECT_EQ("ﾂﾞ", StringUtil::KeyToReadingA("ヅ"));
  EXPECT_EQ("ﾃﾞ", StringUtil::KeyToReadingA("デ"));
  EXPECT_EQ("ﾄﾞ", StringUtil::KeyToReadingA("ド"));
  EXPECT_EQ("ﾅ", StringUtil::KeyToReadingA("ナ"));
  EXPECT_EQ("ﾆ", StringUtil::KeyToReadingA("ニ"));
  EXPECT_EQ("ﾇ", StringUtil::KeyToReadingA("ヌ"));
  EXPECT_EQ("ﾈ", StringUtil::KeyToReadingA("ネ"));
  EXPECT_EQ("ﾉ", StringUtil::KeyToReadingA("ノ"));
  EXPECT_EQ("ﾊ", StringUtil::KeyToReadingA("ハ"));
  EXPECT_EQ("ﾋ", StringUtil::KeyToReadingA("ヒ"));
  EXPECT_EQ("ﾌ", StringUtil::KeyToReadingA("フ"));
  EXPECT_EQ("ﾍ", StringUtil::KeyToReadingA("ヘ"));
  EXPECT_EQ("ﾎ", StringUtil::KeyToReadingA("ホ"));
  EXPECT_EQ("ﾊﾞ", StringUtil::KeyToReadingA("バ"));
  EXPECT_EQ("ﾋﾞ", StringUtil::KeyToReadingA("ビ"));
  EXPECT_EQ("ﾌﾞ", StringUtil::KeyToReadingA("ブ"));
  EXPECT_EQ("ﾍﾞ", StringUtil::KeyToReadingA("ベ"));
  EXPECT_EQ("ﾎﾞ", StringUtil::KeyToReadingA("ボ"));
  EXPECT_EQ("ﾊﾟ", StringUtil::KeyToReadingA("パ"));
  EXPECT_EQ("ﾋﾟ", StringUtil::KeyToReadingA("ピ"));
  EXPECT_EQ("ﾌﾟ", StringUtil::KeyToReadingA("プ"));
  EXPECT_EQ("ﾍﾟ", StringUtil::KeyToReadingA("ペ"));
  EXPECT_EQ("ﾎﾟ", StringUtil::KeyToReadingA("ポ"));
  EXPECT_EQ("ﾏ", StringUtil::KeyToReadingA("マ"));
  EXPECT_EQ("ﾐ", StringUtil::KeyToReadingA("ミ"));
  EXPECT_EQ("ﾑ", StringUtil::KeyToReadingA("ム"));
  EXPECT_EQ("ﾒ", StringUtil::KeyToReadingA("メ"));
  EXPECT_EQ("ﾓ", StringUtil::KeyToReadingA("モ"));
  EXPECT_EQ("ﾔ", StringUtil::KeyToReadingA("ヤ"));
  EXPECT_EQ("ﾕ", StringUtil::KeyToReadingA("ユ"));
  EXPECT_EQ("ﾖ", StringUtil::KeyToReadingA("ヨ"));
  EXPECT_EQ("ﾜ", StringUtil::KeyToReadingA("ワ"));
  EXPECT_EQ("ｦ", StringUtil::KeyToReadingA("ヲ"));
  EXPECT_EQ("ﾝ", StringUtil::KeyToReadingA("ン"));
}

TEST(StringUtilTest, AlphaNumeric) {
  EXPECT_EQ("!", StringUtil::KeyToReadingA("！"));
  EXPECT_EQ("\"", StringUtil::KeyToReadingA("”"));
  EXPECT_EQ("#", StringUtil::KeyToReadingA("＃"));
  EXPECT_EQ("$", StringUtil::KeyToReadingA("＄"));
  EXPECT_EQ("%", StringUtil::KeyToReadingA("％"));
  EXPECT_EQ("&", StringUtil::KeyToReadingA("＆"));
  EXPECT_EQ("'", StringUtil::KeyToReadingA("’"));
  EXPECT_EQ("(", StringUtil::KeyToReadingA("（"));
  EXPECT_EQ(")", StringUtil::KeyToReadingA("）"));
  EXPECT_EQ("=", StringUtil::KeyToReadingA("＝"));
  EXPECT_EQ("-", StringUtil::KeyToReadingA("－"));
  EXPECT_EQ("~", StringUtil::KeyToReadingA("～"));
  EXPECT_EQ("^", StringUtil::KeyToReadingA("＾"));
  EXPECT_EQ("|", StringUtil::KeyToReadingA("｜"));
  EXPECT_EQ("\\", StringUtil::KeyToReadingA("￥"));
  EXPECT_EQ("`", StringUtil::KeyToReadingA("‘"));
  EXPECT_EQ("ﾞ", StringUtil::KeyToReadingA("゛"));
  EXPECT_EQ("@", StringUtil::KeyToReadingA("＠"));
  EXPECT_EQ("{", StringUtil::KeyToReadingA("｛"));
  EXPECT_EQ("｢", StringUtil::KeyToReadingA("「"));
  EXPECT_EQ("+", StringUtil::KeyToReadingA("＋"));
  EXPECT_EQ(";", StringUtil::KeyToReadingA("；"));
  EXPECT_EQ("*", StringUtil::KeyToReadingA("＊"));
  EXPECT_EQ(":", StringUtil::KeyToReadingA("："));
  EXPECT_EQ("}", StringUtil::KeyToReadingA("｝"));
  EXPECT_EQ("｣", StringUtil::KeyToReadingA("」"));
  EXPECT_EQ("<", StringUtil::KeyToReadingA("＜"));
  EXPECT_EQ("､", StringUtil::KeyToReadingA("、"));
  EXPECT_EQ(">", StringUtil::KeyToReadingA("＞"));
  EXPECT_EQ("｡", StringUtil::KeyToReadingA("。"));
  EXPECT_EQ("?", StringUtil::KeyToReadingA("？"));
  EXPECT_EQ("･", StringUtil::KeyToReadingA("・"));
  EXPECT_EQ("_", StringUtil::KeyToReadingA("＿"));
  EXPECT_EQ("1", StringUtil::KeyToReadingA("１"));
  EXPECT_EQ("2", StringUtil::KeyToReadingA("２"));
  EXPECT_EQ("3", StringUtil::KeyToReadingA("３"));
  EXPECT_EQ("4", StringUtil::KeyToReadingA("４"));
  EXPECT_EQ("5", StringUtil::KeyToReadingA("５"));
  EXPECT_EQ("6", StringUtil::KeyToReadingA("６"));
  EXPECT_EQ("7", StringUtil::KeyToReadingA("７"));
  EXPECT_EQ("8", StringUtil::KeyToReadingA("８"));
  EXPECT_EQ("9", StringUtil::KeyToReadingA("９"));
  EXPECT_EQ("0", StringUtil::KeyToReadingA("０"));
  EXPECT_EQ("a", StringUtil::KeyToReadingA("ａ"));
  EXPECT_EQ("b", StringUtil::KeyToReadingA("ｂ"));
  EXPECT_EQ("c", StringUtil::KeyToReadingA("ｃ"));
  EXPECT_EQ("d", StringUtil::KeyToReadingA("ｄ"));
  EXPECT_EQ("e", StringUtil::KeyToReadingA("ｅ"));
  EXPECT_EQ("f", StringUtil::KeyToReadingA("ｆ"));
  EXPECT_EQ("g", StringUtil::KeyToReadingA("ｇ"));
  EXPECT_EQ("h", StringUtil::KeyToReadingA("ｈ"));
  EXPECT_EQ("i", StringUtil::KeyToReadingA("ｉ"));
  EXPECT_EQ("j", StringUtil::KeyToReadingA("ｊ"));
  EXPECT_EQ("k", StringUtil::KeyToReadingA("ｋ"));
  EXPECT_EQ("l", StringUtil::KeyToReadingA("ｌ"));
  EXPECT_EQ("m", StringUtil::KeyToReadingA("ｍ"));
  EXPECT_EQ("n", StringUtil::KeyToReadingA("ｎ"));
  EXPECT_EQ("o", StringUtil::KeyToReadingA("ｏ"));
  EXPECT_EQ("p", StringUtil::KeyToReadingA("ｐ"));
  EXPECT_EQ("q", StringUtil::KeyToReadingA("ｑ"));
  EXPECT_EQ("r", StringUtil::KeyToReadingA("ｒ"));
  EXPECT_EQ("s", StringUtil::KeyToReadingA("ｓ"));
  EXPECT_EQ("t", StringUtil::KeyToReadingA("ｔ"));
  EXPECT_EQ("u", StringUtil::KeyToReadingA("ｕ"));
  EXPECT_EQ("v", StringUtil::KeyToReadingA("ｖ"));
  EXPECT_EQ("w", StringUtil::KeyToReadingA("ｗ"));
  EXPECT_EQ("x", StringUtil::KeyToReadingA("ｘ"));
  EXPECT_EQ("y", StringUtil::KeyToReadingA("ｙ"));
  EXPECT_EQ("z", StringUtil::KeyToReadingA("ｚ"));
  EXPECT_EQ("A", StringUtil::KeyToReadingA("Ａ"));
  EXPECT_EQ("B", StringUtil::KeyToReadingA("Ｂ"));
  EXPECT_EQ("C", StringUtil::KeyToReadingA("Ｃ"));
  EXPECT_EQ("D", StringUtil::KeyToReadingA("Ｄ"));
  EXPECT_EQ("E", StringUtil::KeyToReadingA("Ｅ"));
  EXPECT_EQ("F", StringUtil::KeyToReadingA("Ｆ"));
  EXPECT_EQ("G", StringUtil::KeyToReadingA("Ｇ"));
  EXPECT_EQ("H", StringUtil::KeyToReadingA("Ｈ"));
  EXPECT_EQ("I", StringUtil::KeyToReadingA("Ｉ"));
  EXPECT_EQ("J", StringUtil::KeyToReadingA("Ｊ"));
  EXPECT_EQ("K", StringUtil::KeyToReadingA("Ｋ"));
  EXPECT_EQ("L", StringUtil::KeyToReadingA("Ｌ"));
  EXPECT_EQ("M", StringUtil::KeyToReadingA("Ｍ"));
  EXPECT_EQ("N", StringUtil::KeyToReadingA("Ｎ"));
  EXPECT_EQ("O", StringUtil::KeyToReadingA("Ｏ"));
  EXPECT_EQ("P", StringUtil::KeyToReadingA("Ｐ"));
  EXPECT_EQ("Q", StringUtil::KeyToReadingA("Ｑ"));
  EXPECT_EQ("R", StringUtil::KeyToReadingA("Ｒ"));
  EXPECT_EQ("S", StringUtil::KeyToReadingA("Ｓ"));
  EXPECT_EQ("T", StringUtil::KeyToReadingA("Ｔ"));
  EXPECT_EQ("U", StringUtil::KeyToReadingA("Ｕ"));
  EXPECT_EQ("V", StringUtil::KeyToReadingA("Ｖ"));
  EXPECT_EQ("W", StringUtil::KeyToReadingA("Ｗ"));
  EXPECT_EQ("X", StringUtil::KeyToReadingA("Ｘ"));
  EXPECT_EQ("Y", StringUtil::KeyToReadingA("Ｙ"));
  EXPECT_EQ("Z", StringUtil::KeyToReadingA("Ｚ"));
}

TEST(StringUtilTest, LCMapStringATest) {
  DWORD lcid =
      MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), SORT_JAPANESE_XJIS);

  char buf[512];
  // backquote
  size_t len =
      LCMapStringA(lcid, LCMAP_HALFWIDTH, "\x81\x65", -1, buf, sizeof(buf));
  EXPECT_EQ(2, len);
  // LCMapStringA converts "\x81\x65" (backquote) to ' for some reason.
  // EXPECT_EQ('`', buf[0]);
  EXPECT_EQ('\'', buf[0]);

  // quote
  len = LCMapStringA(lcid, LCMAP_HALFWIDTH, "\x81\x66", -1, buf, sizeof(buf));
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

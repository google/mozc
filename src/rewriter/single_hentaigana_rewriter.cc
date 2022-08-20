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

#include "rewriter/single_hentaigana_rewriter.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "composer/composer.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "request/conversion_request.h"

namespace mozc {
struct Pair {
  // Hentaigana Glyph.
  std::string glyph;
  // The origin Kanji of Hentaigana, from which the glyph came. For example,
  // '𛀇'(U+1B007) comes from Kanji '伊'. Because people sometimes explain it
  // using the origin like '伊の変体仮名', this information is used to generate
  // description of the candidates. Note that this origin can be empty string.
  std::string origin;
};
namespace {
// This mapping is based on latest NamesList.txt available at
// https://www.unicode.org/Public/UCD/latest/ucd/ (version: 2021-09-07 12:23).
// For Hentaigana of ゐ/ゑ, readings い/え and うぃ/うぇ are manually added, for
// input-ability.
const auto *kHentaiganaTable = new std::map<std::string, std::vector<Pair>>({
    {"え",
     {
         {"\U0001B000", ""},    // 𛀀
         {"\U0001B001", "江"},  // 𛀁
         {"\U0001B00F", "盈"},  // 𛀏
         {"\U0001B010", "縁"},  // 𛀐
         {"\U0001B011", "衣"},  // 𛀑
         {"\U0001B012", "衣"},  // 𛀒
         {"\U0001B013", "要"},  // 𛀓
         {"\U0001B112", "惠"},  // 𛄒
         {"\U0001B113", "衞"},  // 𛄓
         {"\U0001B114", "衞"},  // 𛄔
         {"\U0001B115", "衞"},  // 𛄕
         {"\U0001B121", ""},    // 𛄡
     }},
    {"いぇ",
     {
         {"\U0001B001", "江"},  // 𛀁
         {"\U0001B121", ""},    // 𛄡
     }},
    {"あ",
     {
         {"\U0001B002", "安"},  // 𛀂
         {"\U0001B003", "愛"},  // 𛀃
         {"\U0001B004", "阿"},  // 𛀄
         {"\U0001B005", "惡"},  // 𛀅
     }},
    {"を",
     {
         {"\U0001B005", "惡"},  // 𛀅
         {"\U0001B116", "乎"},  // 𛄖
         {"\U0001B117", "乎"},  // 𛄗
         {"\U0001B118", "尾"},  // 𛄘
         {"\U0001B119", "緒"},  // 𛄙
         {"\U0001B11A", "越"},  // 𛄚
         {"\U0001B11B", "遠"},  // 𛄛
         {"\U0001B11C", "遠"},  // 𛄜
     }},
    {"お",
     {
         {"\U0001B005", "惡"},  // 𛀅
         {"\U0001B014", "於"},  // 𛀔
         {"\U0001B015", "於"},  // 𛀕
         {"\U0001B016", "隱"},  // 𛀖
         {"\U0001B116", "乎"},  // 𛄖
         {"\U0001B117", "乎"},  // 𛄗
         {"\U0001B118", "尾"},  // 𛄘
         {"\U0001B119", "緒"},  // 𛄙
         {"\U0001B11A", "越"},  // 𛄚
         {"\U0001B11B", "遠"},  // 𛄛
         {"\U0001B11C", "遠"},  // 𛄜
     }},
    {"うぉ",
     {
         {"\U0001B005", "惡"},  // 𛀅
         {"\U0001B116", "乎"},  // 𛄖
         {"\U0001B117", "乎"},  // 𛄗
         {"\U0001B118", "尾"},  // 𛄘
         {"\U0001B119", "緒"},  // 𛄙
         {"\U0001B11A", "越"},  // 𛄚
         {"\U0001B11B", "遠"},  // 𛄛
         {"\U0001B11C", "遠"},  // 𛄜
     }},
    {"い",
     {
         {"\U0001B006", "以"},  // 𛀆
         {"\U0001B007", "伊"},  // 𛀇
         {"\U0001B008", "意"},  // 𛀈
         {"\U0001B009", "移"},  // 𛀉
         {"\U0001B10D", "井"},  // 𛄍
         {"\U0001B10E", "井"},  // 𛄎
         {"\U0001B10F", "居"},  // 𛄏
         {"\U0001B110", "爲"},  // 𛄐
         {"\U0001B111", "遺"},  // 𛄑
         {"\U0001B120", ""},    // 𛄠
     }},
    {"う",
     {
         {"\U0001B00A", "宇"},  // 𛀊
         {"\U0001B00B", "宇"},  // 𛀋
         {"\U0001B00C", "憂"},  // 𛀌
         {"\U0001B00D", "有"},  // 𛀍
         {"\U0001B00E", "雲"},  // 𛀎
         {"\U0001B11F", "汙"},  // 𛄟
         {"\U0001B122", "汙"},  // 𛄢
     }},
    {"か",
     {
         {"\U0001B017", "佳"},  // 𛀗
         {"\U0001B018", "加"},  // 𛀘
         {"\U0001B019", "可"},  // 𛀙
         {"\U0001B01A", "可"},  // 𛀚
         {"\U0001B01B", "嘉"},  // 𛀛
         {"\U0001B01C", "我"},  // 𛀜
         {"\U0001B01D", "歟"},  // 𛀝
         {"\U0001B01E", "賀"},  // 𛀞
         {"\U0001B01F", "閑"},  // 𛀟
         {"\U0001B020", "香"},  // 𛀠
         {"\U0001B021", "駕"},  // 𛀡
         {"\U0001B022", "家"},  // 𛀢
     }},
    {"け",
     {
         {"\U0001B022", "家"},  // 𛀢
         {"\U0001B032", "介"},  // 𛀲
         {"\U0001B033", "介"},  // 𛀳
         {"\U0001B034", "希"},  // 𛀴
         {"\U0001B035", "氣"},  // 𛀵
         {"\U0001B036", "計"},  // 𛀶
         {"\U0001B037", "遣"},  // 𛀷
     }},
    {"き",
     {
         {"\U0001B023", "喜"},  // 𛀣
         {"\U0001B024", "幾"},  // 𛀤
         {"\U0001B025", "幾"},  // 𛀥
         {"\U0001B026", "支"},  // 𛀦
         {"\U0001B027", "木"},  // 𛀧
         {"\U0001B028", "祈"},  // 𛀨
         {"\U0001B029", "貴"},  // 𛀩
         {"\U0001B02A", "起"},  // 𛀪
         {"\U0001B03B", "期"},  // 𛀻
     }},
    {"く",
     {
         {"\U0001B02B", "久"},  // 𛀫
         {"\U0001B02C", "久"},  // 𛀬
         {"\U0001B02D", "九"},  // 𛀭
         {"\U0001B02E", "供"},  // 𛀮
         {"\U0001B02F", "倶"},  // 𛀯
         {"\U0001B030", "具"},  // 𛀰
         {"\U0001B031", "求"},  // 𛀱
     }},
    {"こ",
     {
         {"\U0001B038", "古"},  // 𛀸
         {"\U0001B039", "故"},  // 𛀹
         {"\U0001B03A", "許"},  // 𛀺
         {"\U0001B03B", "期"},  // 𛀻
         {"\U0001B098", "子"},  // 𛂘
     }},
    {"さ",
     {
         {"\U0001B03C", "乍"},  // 𛀼
         {"\U0001B03D", "佐"},  // 𛀽
         {"\U0001B03E", "佐"},  // 𛀾
         {"\U0001B03F", "左"},  // 𛀿
         {"\U0001B040", "差"},  // 𛁀
         {"\U0001B041", "散"},  // 𛁁
         {"\U0001B042", "斜"},  // 𛁂
         {"\U0001B043", "沙"},  // 𛁃
     }},
    {"し",
     {
         {"\U0001B044", "之"},  // 𛁄
         {"\U0001B045", "之"},  // 𛁅
         {"\U0001B046", "事"},  // 𛁆
         {"\U0001B047", "四"},  // 𛁇
         {"\U0001B048", "志"},  // 𛁈
         {"\U0001B049", "新"},  // 𛁉
     }},
    {"す",
     {
         {"\U0001B04A", "受"},  // 𛁊
         {"\U0001B04B", "壽"},  // 𛁋
         {"\U0001B04C", "數"},  // 𛁌
         {"\U0001B04D", "數"},  // 𛁍
         {"\U0001B04E", "春"},  // 𛁎
         {"\U0001B04F", "春"},  // 𛁏
         {"\U0001B050", "須"},  // 𛁐
         {"\U0001B051", "須"},  // 𛁑
     }},
    {"せ",
     {
         {"\U0001B052", "世"},  // 𛁒
         {"\U0001B053", "世"},  // 𛁓
         {"\U0001B054", "世"},  // 𛁔
         {"\U0001B055", "勢"},  // 𛁕
         {"\U0001B056", "聲"},  // 𛁖
     }},
    {"そ",
     {
         {"\U0001B057", "所"},  // 𛁗
         {"\U0001B058", "所"},  // 𛁘
         {"\U0001B059", "曾"},  // 𛁙
         {"\U0001B05A", "曾"},  // 𛁚
         {"\U0001B05B", "楚"},  // 𛁛
         {"\U0001B05C", "蘇"},  // 𛁜
         {"\U0001B05D", "處"},  // 𛁝
     }},
    {"た",
     {
         {"\U0001B05E", "堂"},  // 𛁞
         {"\U0001B05F", "多"},  // 𛁟
         {"\U0001B060", "多"},  // 𛁠
         {"\U0001B061", "當"},  // 𛁡
     }},
    {"ち",
     {
         {"\U0001B062", "千"},  // 𛁢
         {"\U0001B063", "地"},  // 𛁣
         {"\U0001B064", "智"},  // 𛁤
         {"\U0001B065", "知"},  // 𛁥
         {"\U0001B066", "知"},  // 𛁦
         {"\U0001B067", "致"},  // 𛁧
         {"\U0001B068", "遲"},  // 𛁨
     }},
    {"つ",
     {
         {"\U0001B069", "川"},  // 𛁩
         {"\U0001B06A", "川"},  // 𛁪
         {"\U0001B06B", "津"},  // 𛁫
         {"\U0001B06C", "都"},  // 𛁬
         {"\U0001B06D", "徒"},  // 𛁭
     }},
    {"と",
     {
         {"\U0001B06D", "徒"},  // 𛁭
         {"\U0001B077", "土"},  // 𛁷
         {"\U0001B078", "度"},  // 𛁸
         {"\U0001B079", "東"},  // 𛁹
         {"\U0001B07A", "登"},  // 𛁺
         {"\U0001B07B", "登"},  // 𛁻
         {"\U0001B07C", "砥"},  // 𛁼
         {"\U0001B07D", "等"},  // 𛁽
     }},
    {"て",
     {
         {"\U0001B06E", "亭"},  // 𛁮
         {"\U0001B06F", "低"},  // 𛁯
         {"\U0001B070", "傳"},  // 𛁰
         {"\U0001B071", "天"},  // 𛁱
         {"\U0001B072", "天"},  // 𛁲
         {"\U0001B073", "天"},  // 𛁳
         {"\U0001B074", "帝"},  // 𛁴
         {"\U0001B075", "弖"},  // 𛁵
         {"\U0001B076", "轉"},  // 𛁶
         {"\U0001B08E", "而"},  // 𛂎
     }},
    {"ら",
     {
         {"\U0001B07D", "等"},  // 𛁽
         {"\U0001B0ED", "羅"},  // 𛃭
         {"\U0001B0EE", "良"},  // 𛃮
         {"\U0001B0EF", "良"},  // 𛃯
         {"\U0001B0F0", "良"},  // 𛃰
     }},
    {"な",
     {
         {"\U0001B07E", "南"},  // 𛁾
         {"\U0001B07F", "名"},  // 𛁿
         {"\U0001B080", "奈"},  // 𛂀
         {"\U0001B081", "奈"},  // 𛂁
         {"\U0001B082", "奈"},  // 𛂂
         {"\U0001B083", "菜"},  // 𛂃
         {"\U0001B084", "那"},  // 𛂄
         {"\U0001B085", "那"},  // 𛂅
         {"\U0001B086", "難"},  // 𛂆
     }},
    {"に",
     {
         {"\U0001B087", "丹"},  // 𛂇
         {"\U0001B088", "二"},  // 𛂈
         {"\U0001B089", "仁"},  // 𛂉
         {"\U0001B08A", "兒"},  // 𛂊
         {"\U0001B08B", "爾"},  // 𛂋
         {"\U0001B08C", "爾"},  // 𛂌
         {"\U0001B08D", "耳"},  // 𛂍
         {"\U0001B08E", "而"},  // 𛂎
     }},
    {"ぬ",
     {
         {"\U0001B08F", "努"},  // 𛂏
         {"\U0001B090", "奴"},  // 𛂐
         {"\U0001B091", "怒"},  // 𛂑
     }},
    {"ね",
     {
         {"\U0001B092", "年"},  // 𛂒
         {"\U0001B093", "年"},  // 𛂓
         {"\U0001B094", "年"},  // 𛂔
         {"\U0001B095", "根"},  // 𛂕
         {"\U0001B096", "熱"},  // 𛂖
         {"\U0001B097", "禰"},  // 𛂗
         {"\U0001B098", "子"},  // 𛂘
     }},
    {"の",
     {
         {"\U0001B099", "乃"},  // 𛂙
         {"\U0001B09A", "濃"},  // 𛂚
         {"\U0001B09B", "能"},  // 𛂛
         {"\U0001B09C", "能"},  // 𛂜
         {"\U0001B09D", "農"},  // 𛂝
     }},
    {"は",
     {
         {"\U0001B09E", "八"},  // 𛂞
         {"\U0001B09F", "半"},  // 𛂟
         {"\U0001B0A0", "婆"},  // 𛂠
         {"\U0001B0A1", "波"},  // 𛂡
         {"\U0001B0A2", "盤"},  // 𛂢
         {"\U0001B0A3", "盤"},  // 𛂣
         {"\U0001B0A4", "破"},  // 𛂤
         {"\U0001B0A5", "者"},  // 𛂥
         {"\U0001B0A6", "者"},  // 𛂦
         {"\U0001B0A7", "葉"},  // 𛂧
         {"\U0001B0A8", "頗"},  // 𛂨
     }},
    {"ひ",
     {
         {"\U0001B0A9", "悲"},  // 𛂩
         {"\U0001B0AA", "日"},  // 𛂪
         {"\U0001B0AB", "比"},  // 𛂫
         {"\U0001B0AC", "避"},  // 𛂬
         {"\U0001B0AD", "非"},  // 𛂭
         {"\U0001B0AE", "飛"},  // 𛂮
         {"\U0001B0AF", "飛"},  // 𛂯
     }},
    {"ふ",
     {
         {"\U0001B0B0", "不"},  // 𛂰
         {"\U0001B0B1", "婦"},  // 𛂱
         {"\U0001B0B2", "布"},  // 𛂲
     }},
    {"へ",
     {
         {"\U0001B0B3", "倍"},  // 𛂳
         {"\U0001B0B4", "弊"},  // 𛂴
         {"\U0001B0B5", "弊"},  // 𛂵
         {"\U0001B0B6", "遍"},  // 𛂶
         {"\U0001B0B7", "邊"},  // 𛂷
         {"\U0001B0B8", "邊"},  // 𛂸
         {"\U0001B0B9", "部"},  // 𛂹
     }},
    {"ほ",
     {
         {"\U0001B0BA", "保"},  // 𛂺
         {"\U0001B0BB", "保"},  // 𛂻
         {"\U0001B0BC", "報"},  // 𛂼
         {"\U0001B0BD", "奉"},  // 𛂽
         {"\U0001B0BE", "寶"},  // 𛂾
         {"\U0001B0BF", "本"},  // 𛂿
         {"\U0001B0C0", "本"},  // 𛃀
         {"\U0001B0C1", "豐"},  // 𛃁
     }},
    {"ま",
     {
         {"\U0001B0C2", "万"},  // 𛃂
         {"\U0001B0C3", "末"},  // 𛃃
         {"\U0001B0C4", "末"},  // 𛃄
         {"\U0001B0C5", "滿"},  // 𛃅
         {"\U0001B0C6", "滿"},  // 𛃆
         {"\U0001B0C7", "萬"},  // 𛃇
         {"\U0001B0C8", "麻"},  // 𛃈
         {"\U0001B0D6", "馬"},  // 𛃖
     }},
    {"み",
     {
         {"\U0001B0C9", "三"},  // 𛃉
         {"\U0001B0CA", "微"},  // 𛃊
         {"\U0001B0CB", "美"},  // 𛃋
         {"\U0001B0CC", "美"},  // 𛃌
         {"\U0001B0CD", "美"},  // 𛃍
         {"\U0001B0CE", "見"},  // 𛃎
         {"\U0001B0CF", "身"},  // 𛃏
     }},
    {"む",
     {
         {"\U0001B0D0", "武"},  // 𛃐
         {"\U0001B0D1", "無"},  // 𛃑
         {"\U0001B0D2", "牟"},  // 𛃒
         {"\U0001B0D3", "舞"},  // 𛃓
         {"\U0001B11D", "无"},  // 𛄝
         {"\U0001B11E", "无"},  // 𛄞
     }},
    {"め",
     {
         {"\U0001B0D4", "免"},  // 𛃔
         {"\U0001B0D5", "面"},  // 𛃕
         {"\U0001B0D6", "馬"},  // 𛃖
     }},
    {"も",
     {
         {"\U0001B0D7", "母"},  // 𛃗
         {"\U0001B0D8", "毛"},  // 𛃘
         {"\U0001B0D9", "毛"},  // 𛃙
         {"\U0001B0DA", "毛"},  // 𛃚
         {"\U0001B0DB", "茂"},  // 𛃛
         {"\U0001B0DC", "裳"},  // 𛃜
         {"\U0001B11D", "无"},  // 𛄝
         {"\U0001B11E", "无"},  // 𛄞
     }},
    {"や",
     {
         {"\U0001B0DD", "也"},  // 𛃝
         {"\U0001B0DE", "也"},  // 𛃞
         {"\U0001B0DF", "屋"},  // 𛃟
         {"\U0001B0E0", "耶"},  // 𛃠
         {"\U0001B0E1", "耶"},  // 𛃡
         {"\U0001B0E2", "夜"},  // 𛃢
     }},
    {"よ",
     {
         {"\U0001B0E2", "夜"},  // 𛃢
         {"\U0001B0E7", "代"},  // 𛃧
         {"\U0001B0E8", "余"},  // 𛃨
         {"\U0001B0E9", "與"},  // 𛃩
         {"\U0001B0EA", "與"},  // 𛃪
         {"\U0001B0EB", "與"},  // 𛃫
         {"\U0001B0EC", "餘"},  // 𛃬
     }},
    {"ゆ",
     {
         {"\U0001B0E3", "游"},  // 𛃣
         {"\U0001B0E4", "由"},  // 𛃤
         {"\U0001B0E5", "由"},  // 𛃥
         {"\U0001B0E6", "遊"},  // 𛃦
     }},
    {"り",
     {
         {"\U0001B0F1", "利"},  // 𛃱
         {"\U0001B0F2", "利"},  // 𛃲
         {"\U0001B0F3", "李"},  // 𛃳
         {"\U0001B0F4", "梨"},  // 𛃴
         {"\U0001B0F5", "理"},  // 𛃵
         {"\U0001B0F6", "里"},  // 𛃶
         {"\U0001B0F7", "離"},  // 𛃷
     }},
    {"る",
     {
         {"\U0001B0F8", "流"},  // 𛃸
         {"\U0001B0F9", "留"},  // 𛃹
         {"\U0001B0FA", "留"},  // 𛃺
         {"\U0001B0FB", "留"},  // 𛃻
         {"\U0001B0FC", "累"},  // 𛃼
         {"\U0001B0FD", "類"},  // 𛃽
     }},
    {"れ",
     {
         {"\U0001B0FE", "禮"},  // 𛃾
         {"\U0001B0FF", "禮"},  // 𛃿
         {"\U0001B100", "連"},  // 𛄀
         {"\U0001B101", "麗"},  // 𛄁
     }},
    {"ろ",
     {
         {"\U0001B102", "呂"},  // 𛄂
         {"\U0001B103", "呂"},  // 𛄃
         {"\U0001B104", "婁"},  // 𛄄
         {"\U0001B105", "樓"},  // 𛄅
         {"\U0001B106", "路"},  // 𛄆
         {"\U0001B107", "露"},  // 𛄇
     }},
    {"わ",
     {
         {"\U0001B108", "倭"},  // 𛄈
         {"\U0001B109", "和"},  // 𛄉
         {"\U0001B10A", "和"},  // 𛄊
         {"\U0001B10B", "王"},  // 𛄋
         {"\U0001B10C", "王"},  // 𛄌
     }},
    {"ゐ",
     {
         {"\U0001B10D", "井"},  // 𛄍
         {"\U0001B10E", "井"},  // 𛄎
         {"\U0001B10F", "居"},  // 𛄏
         {"\U0001B110", "爲"},  // 𛄐
         {"\U0001B111", "遺"},  // 𛄑
     }},
    {"うぃ",
     {
         {"\U0001B10D", "井"},  // 𛄍
         {"\U0001B10E", "井"},  // 𛄎
         {"\U0001B10F", "居"},  // 𛄏
         {"\U0001B110", "爲"},  // 𛄐
         {"\U0001B111", "遺"},  // 𛄑
     }},
    {"ゑ",
     {
         {"\U0001B112", "惠"},  // 𛄒
         {"\U0001B113", "衞"},  // 𛄓
         {"\U0001B114", "衞"},  // 𛄔
         {"\U0001B115", "衞"},  // 𛄕
     }},
    {"うぇ",
     {
         {"\U0001B112", "惠"},  // 𛄒
         {"\U0001B113", "衞"},  // 𛄓
         {"\U0001B114", "衞"},  // 𛄔
         {"\U0001B115", "衞"},  // 𛄕
     }},
    {"ん",
     {
         {"\U0001B11D", "无"},  // 𛄝
         {"\U0001B11E", "无"},  // 𛄞
     }},
});

bool EnsureSingleSegment(const ConversionRequest &request, Segments *segments,
                         const ConverterInterface *parent_converter,
                         const std::string &key) {
  if (segments->conversion_segments_size() == 1) {
    return true;
  }

  if (segments->resized()) {
    // The given segments are resized by user so don't modify anymore.
    return false;
  }

  const uint32_t resize_len =
      Util::CharsLen(key) -
      Util::CharsLen(segments->conversion_segment(0).key());
  if (!parent_converter->ResizeSegment(segments, request, 0, resize_len)) {
    return false;
  }
  DCHECK_EQ(1, segments->conversion_segments_size());
  return true;
}

void AddCandidate(const std::string &key, const std::string &description,
                  const std::string &value, Segment *segment) {
  DCHECK(segment);
  Segment::Candidate *candidate =
      segment->insert_candidate(segment->candidates_size());
  DCHECK(candidate);

  candidate->Init();
  segment->set_key(key);
  candidate->key = key;
  candidate->value = value;
  candidate->content_value = value;
  candidate->description = description;
  candidate->attributes |= (Segment::Candidate::NO_VARIANTS_EXPANSION);
}
}  // namespace

SingleHentaiganaRewriter::SingleHentaiganaRewriter(
    const ConverterInterface *parent_converter)
    : parent_converter_(parent_converter) {}

SingleHentaiganaRewriter::~SingleHentaiganaRewriter() {}

int SingleHentaiganaRewriter::capability(
    const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

void SingleHentaiganaRewriter::SetEnabled(const bool enabled) {
  // TODO(b/242276533): Replace this with better mechanism later. Right now this
  // rewriter is always disabled intentionally except for tests.
  enabled_ = enabled;
}

bool SingleHentaiganaRewriter::Rewrite(const ConversionRequest &request,
                                       Segments *segments) const {
  // If hentaigana rewriter is not requested to use, do nothing.
  if (!enabled_) {
    return false;
  }

  std::string key;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    key += segments->conversion_segment(i).key();
  }

  if (!EnsureSingleSegment(request, segments, parent_converter_, key)) {
    return false;
  }

  // Ensure table has entry for key.
  if (kHentaiganaTable->find(key) == kHentaiganaTable->end()) {
    return false;
  }
  const std::vector<Pair> &pairs = kHentaiganaTable->at(key);
  // Ensure pairs is not empty
  if (pairs.empty()) {
    return false;
  }

  // Generate candidate for each value in pairs.
  for (const Pair &pair : pairs) {
    const std::string &value = pair.glyph;
    Segment *segment = segments->mutable_conversion_segment(0);

    // If origin is not available, ignore it and set "変体仮名" for description.
    if (pair.origin.empty()) {
      AddCandidate(key, "変体仮名", value, segment);
    } else {
      const auto description = pair.origin + "の変体仮名";
      AddCandidate(key, description, value, segment);
    }
  }
  return true;
}
}  // namespace mozc

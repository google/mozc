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

// Counter suffix euphony (音便: 促音便・連濁・半濁音化) rules, Wago (和語)
// numeral rules, and stop-decoding suffix entries for NumberDecoder.
//
// This file contains data tables only (plus the enum and struct types that
// describe them) and is included inside the anonymous namespace of
// number_decoder.cc.
//
// Sources and references used to construct these rules:
// - UniDic counter euphony categories (助数詞音便分類: ハ半濁, フ混合, ハ混合,
//   ヒ混合, ヘ混合, ホ半濁, ホ混合, ワ混合; see Table 10 of unidic2.pdf).
// - data/dictionary_oss/dictionary0*.txt: system dictionary counter suffix
//   (助数詞 POS) entries and common-word homophone collision checks.
// - https://en.wikipedia.org/wiki/Japanese_counter_word#Euphonic_changes
//
// Relationship to other counter data in Mozc:
// - data/zero_query/zero_query_number.def lists counter *surfaces* suggested
//   after a number (zero query). It has no readings or euphony information.
// - The system dictionary has counter suffix entries (reading -> surface with
//   the counter suffix POS), but not the number-tail-dependent readings
//   ("いっぱい"/"さんばい"/"ろっぴゃく") or the Wago forms ("ふつか",
//   "ひとはこ").
// Neither source can generate the euphony masks below, so this table is
// maintained by hand. Keep the counter surfaces consistent with those files
// when adding entries.

// =============================================================================
// 1. Tail Token Bitflags
// =============================================================================
//
// Japanese counter suffixes undergo euphonic changes (促音便・連濁・半濁音化)
// depending on the *last* consumed number token before the suffix (e.g., for
// "さんびゃくばい" [300杯], the last number token is "びゃく" -> kHyaku; for
// "にじゅういっかい" [21回], the last number token is "いっ" -> kIt).
//
// Each number token variant is assigned a distinct bit in TailType so that
// valid (number_tail, counter_reading) combinations can be checked in O(1)
// using bitwise AND. See GetTailType() in number_decoder.cc for the mapping
// from number tokens to TailType.
enum TailType : uint32_t {
  kTailNone = 0,
  kZero = 1u << 0,     // "ぜろ" (0)
  kIchi = 1u << 1,     // "いち" (1: 非促音)
  kIt = 1u << 2,       // "いっ" (1: カ・サ・タ・パ行前の促音変化)
  kNi = 1u << 3,       // "に" (2)
  kSan = 1u << 4,      // "さん" (3: 撥音の後で連濁・半濁音化)
  kYon = 1u << 5,      // "よん" (4: 標準読み)
  kShi = 1u << 6,      // "し" (4: 漢語系, e.g., 4月 "しがつ")
  kYo = 1u << 7,       // "よ" (4: 4人 "よにん", 4時 "よじ", 4日 "よっか")
  kGo = 1u << 8,       // "ご" (5)
  kRoku = 1u << 9,     // "ろく" (6: 非促音)
  kRok = 1u << 10,     // "ろっ" (6: カ・パ行前の促音変化)
  kNana = 1u << 11,    // "なな" (7: 標準読み)
  kShichi = 1u << 12,  // "しち" (7: 漢語系, e.g., 7月 "しちがつ", 7時 "しちじ")
  kHachi = 1u << 13,   // "はち" (8: 非促音)
  kHat = 1u << 14,     // "はっ" (8: カ・サ・タ・パ行前の促音変化)
  kKyu = 1u << 15,     // "きゅう", "きゅー" (9: 標準読み)
  kKu = 1u << 16,      // "く" (9: 漢語系, e.g., 9月 "くがつ", 9時 "くじ")
  kJu = 1u << 17,      // "じゅう", "じゅー" (10: 非促音)
  kJut = 1u << 18,     // "じゅっ", "じっ" (10: カ・サ・タ・パ行前の促音変化)
  kHyaku = 1u << 19,   // "ひゃく", "びゃく", "ぴゃく" (100: 非促音)
  kHyak = 1u << 20,  // "ひゃっ", "びゃっ", "ぴゃっ" (100: カ・パ行前の促音変化)
  kSen = 1u << 21,   // "せん", "ぜん" (1000: 撥音の後で連濁・半濁音化)
  kMan = 1u << 22,   // "まん" (万: 撥音の後で連濁・半濁音化)
  kOku = 1u << 23,   // "おく" (億: 非促音)
  kOk = 1u << 24,    // "おっ" (億: カ・パ行前の促音変化, 1億回 "いちおっかい")
  kCho = 1u << 25,   // "ちょう", "けい", "がい" (兆, 京, 垓)
};

// =============================================================================
// 2. Phonological / Euphony Group Bitmasks
// =============================================================================
//
// The bitmasks below group number-tail forms according to Japanese counter
// phonological sandhi categories (UniDic Table 10 助数詞音便分類 and
// https://en.wikipedia.org/wiki/Japanese_counter_word#Euphonic_changes).

// Standard number tails that attach without euphonic change (無変化助数詞:
// 枚, 台, 番, 秒, 名, メートル, etc.). Excludes 促音 ("いっ", "ろっ", etc.) and
// special readings ("し", "よ", "く").
constexpr uint32_t kStandardTails = kIchi | kNi | kSan | kYon | kGo | kRoku |
                                    kNana | kShichi | kHachi | kKyu | kJu |
                                    kHyaku | kSen | kMan | kOku | kCho;

// Number tails that undergo 促音変化 before カ行・パ行頭音 counters (1="いっ",
// 6="ろっ", 8="はっ", 10="じゅっ"/"じっ", 100="ひゃっ"/"びゃっ"/"ぴゃっ",
// 億="おっ").
constexpr uint32_t kKaPaSokuon = kIt | kRok | kHat | kJut | kHyak | kOk;

// Loanword counters starting with カ行・パ行 (外来語系助数詞: ページ,
// パーセント, キロ, ポイント, etc.). Allows both 促音形 ("いっぱーせんと",
// "ろっぺーじ") and 非促音形 ("ごじゅうぱーせんと", "ひゃくぺーじ").
constexpr uint32_t kForeignKaPaTails = kKaPaSokuon | kStandardTails;

// Native/Sino-Japanese counters starting with カ行 (和漢助数詞: 回, 個, 巻, 件,
// ヶ月, 箇所, か国, etc.). Requires 促音形 for 1, 6, 10 ("いっ", "ろっ",
// "じゅっ"), allows both 促音形 and 非促音形 for 8, 100, 億, and uses 非促音形
// for 2, 3, 4, 5, 7, 9, 千, 万, 兆.
constexpr uint32_t kKaTails =
    (kStandardTails & ~kIchi & ~kRoku & ~kJu) | kKaPaSokuon;

// Counters whose initial カ行 consonant undergoes 連濁 (ガ行化) after 撥音
// tails (3="さん", 千="せん"/"ぜん", 万="まん"), e.g., 3階 "さんがい", 3軒
// "さんげん".
// kHyaku is excluded ("さんびゃくがい" is not generated; standalone
// "ひゃくがい" collides with "百害").
constexpr uint32_t kKaVoicedTails = kSan | kSen | kMan;

// Number tails that undergo 促音変化 before サ行・タ行・シャ行頭音 counters
// (1="いっ", 8="はっ", 10="じゅっ"/"じっ", 100="ひゃっ", 億="おっ"). Note that
// 6 remains 非促音 ("ろくさい", "ろくさつ").
constexpr uint32_t kSaTaSokuon = kIt | kHat | kJut | kHyak | kOk;

// Counters starting with サ行・タ行・シャ行 (歳, 冊, 体, 通, 点, 着, 週間, 勝,
// 社, etc.). Requires 促音形 for 1 and 10 ("いっ", "じゅっ"), 非促音形 for 6
// ("ろく"), and allows both for 8, 100, 億.
// 非促音 "いち" is additionally allowed only for 試合, 種類, 種, 世代, 世帯,
// シート, cc, and セント (see the per-rule masks below).
constexpr uint32_t kSaTaTails = (kStandardTails & ~kIchi & ~kJu) | kSaTaSokuon;

// Three-way alternation (ハ行・バ行・パ行三段変化) for standard ハ行頭音
// counters (UniDic ハ混合/ヒ混合/ホ混合: 杯, 本, 匹, 票, 敗, 箱, 針, 版, 品,
// 報, etc.).
// - ハ行 (清音): 2, 4, 5, 7, 8 ("はち"), 9, 100 ("ひゃく"), 億 ("おく"),
//   兆 ("ちょう")
// - バ行 (連濁): 3 ("さん"), 千 ("せん"/"ぜん"), 万 ("まん")
// - パ行 (半濁音): 1 ("いっ"), 6 ("ろっ"), 8 ("はっ"), 10 ("じゅっ"/"じっ"),
//   100 ("ひゃっ"), 千, 万, 億 ("おっ")
// 半濁音 for 4 ("よんぽん"), 清音 for 6 ("ろくほん"), and 連濁 after 100
// ("ひゃくぼん") are not generated.
constexpr uint32_t kHaTails =
    kNi | kYon | kGo | kNana | kShichi | kHachi | kKyu | kHyaku | kOku | kCho;
constexpr uint32_t kBaTails = kSan | kSen | kMan;
constexpr uint32_t kPaTails = kKaPaSokuon | kSen | kMan;

// "分" / "分間" ("ふん" / "ぷん"; UniDic フ混合).
// Takes 半濁音 "ぷん" after 3 ("さん") and 4 ("よん"/"よ") as well ("さんぷん",
// "よんぷん"), and never undergoes 連濁 ("ぶん").
// 清音 "ふん" is allowed for 8 ("はちふん", via kHaTails) but not for 6
// ("ろくふん"); 6 takes "ろっぷん" only.
constexpr uint32_t kFunHaTails = kHaTails & ~kHyaku;
constexpr uint32_t kFunPaTails = kPaTails | kSan | kYon | kYo;

// "泊", "拍", "発", "編", "辺", "返", "遍", "歩" (UniDic ハ半濁/ヘ混合/ホ半濁).
// Takes 半濁音 after 3 and 4 ("さんぱく", "さんぽ", "よんぽ"), and also allows
// 連濁 (バ行) after 3, 千, 万 ("さんばつ", "せんぼ").
constexpr uint32_t kHakuHaTails = kFunHaTails;
constexpr uint32_t kHakuPaTails = kPaTails | kSan | kYon;
constexpr uint32_t kHakuBaTails = kSan | kSen | kMan;

// "羽", "把" ("わ" / "ば" / "ぱ"; UniDic ワ混合).
// Allows バ行 after 3, 千, 万 ("さんば", "せんば") and パ行 after 促音, 千,
// 万 ("ろっぱ", "じゅっぱ"). 6 and 100 take 清音 or 促音 ("ろくわ"/"ろっぱ",
// "ひゃくわ"/"ひゃっぱ"), not バ行.
constexpr uint32_t kWaTails = kStandardTails | kYo;
constexpr uint32_t kWaBaTails = kSan | kSen | kMan;
constexpr uint32_t kWaPaTails = kPaTails;

// =============================================================================
// 3. Counter Suffix Rules
// =============================================================================

struct CounterSuffixRule {
  // Reading of the counter suffix (e.g., "まい", "かい", "ぱーせんと").
  absl::string_view reading;
  // Comma-separated surface forms of the counter suffix (e.g., "回,階",
  // "パーセント,%").
  absl::string_view suffixes;
  // Bitmask of TailType values allowed immediately before this suffix.
  uint32_t allowed_tail_mask;
  // Subset of allowed_tail_mask that is only permitted in multi-token compound
  // numbers (2 or more number tokens, e.g., "にじゅうにほん" -> "22本") to
  // avoid homophone collisions on single-token inputs (e.g., suppressing "2本"
  // for standalone "にほん" [日本] when kNi is set).
  uint32_t multi_digit_only_tail_mask = 0;
};

constexpr CounterSuffixRule kCounterSuffixRules[] = {
    // -------------------------------------------------------------------------
    // Unchanged counters and loanword units
    // -------------------------------------------------------------------------
    // For 枚, 台, 番, 割, 列, and 段, the reading "よ" (4) is restricted to
    // compound numbers (e.g., 14枚 "じゅうよまい") to avoid homophone
    // collisions
    // on standalone inputs (四大, 夜番, 弱り, 余列, 余談, etc.).
    {"まい", "枚", kStandardTails | kYo, /*multi_digit_only_tail_mask=*/kYo},
    {"だい", "台,代,題", kStandardTails | kYo,
     /*multi_digit_only_tail_mask=*/kYo},
    {"ばん", "番", kStandardTails | kYo, /*multi_digit_only_tail_mask=*/kYo},
    {"ごう", "号,合", kStandardTails},
    // Suppress standalone "いちど" (一度) and "にど" (二度), which exist as
    // low-cost words in the system dictionary; allow in compound numbers.
    {"ど", "度,℃", kStandardTails, /*multi_digit_only_tail_mask=*/kIchi | kNi},
    {"びょうかん", "秒間", kZero | kStandardTails},
    {"びょう", "秒", kZero | kStandardTails},
    {"めい", "名", kStandardTails},
    {"わり", "割", kStandardTails | kYo, /*multi_digit_only_tail_mask=*/kYo},
    {"れつ", "列", kStandardTails | kYo, /*multi_digit_only_tail_mask=*/kYo},
    // Avoid collision with standalone "せんぎょう" (専業).
    {"ぎょう", "行", kStandardTails, /*multi_digit_only_tail_mask=*/kSen},
    {"ちょうめ", "丁目", kStandardTails},
    {"ばんち", "番地", kStandardTails},
    // Avoid collisions with standalone "いちぶ" (一部), "にぶ" (二部), and
    // "ごぶ" (五分).
    {"ぶ", "部", kStandardTails,
     /*multi_digit_only_tail_mask=*/kIchi | kNi | kGo},
    // Avoid collision with standalone "せんじょう" (戦場, 洗浄).
    {"じょう", "条,畳,乗", kStandardTails | kYo,
     /*multi_digit_only_tail_mask=*/kSen},
    // 章 is a シャ行 counter and takes 促音 like 勝 ("いっしょう" [1章]).
    {"しょう", "章", kSaTaTails},
    {"わ", "話", kStandardTails},
    // Avoid collision with standalone "せんもん" (専門).
    {"もん", "問", kStandardTails, /*multi_digit_only_tail_mask=*/kSen},
    {"くみ", "組", kStandardTails},
    {"しゅるい", "種類", kSaTaTails | kIchi | kJu},
    // Avoid collision with standalone "せんしゅ" (選手).
    {"しゅ", "種,首", kSaTaTails | kIchi | kJu,
     /*multi_digit_only_tail_mask=*/kSen},
    {"せだい", "世代", kSaTaTails | kIchi | kJu},
    {"せたい", "世帯", kSaTaTails | kIchi | kJu},
    {"だんかい", "段階", kStandardTails},
    {"だん", "段,弾", kStandardTails | kYo, /*multi_digit_only_tail_mask=*/kYo},
    {"へや", "部屋", kStandardTails | kJut},
    {"ぺや", "部屋", kJut},
    // Avoid collisions with standalone "ごい" (語彙) and "せんい" (繊維).
    {"い", "位", kStandardTails, /*multi_digit_only_tail_mask=*/kGo | kSen},
    {"みり", "ミリ,mm", kZero | kStandardTails},
    {"せんち", "センチ,cm", kZero | kStandardTails},
    {"めーとる", "メートル", kZero | kStandardTails},
    {"ぐらむ", "グラム", kZero | kStandardTails},
    {"りっとる", "リットル", kZero | kStandardTails},
    // トン takes 促音 for 1/8/10 ("いっとん") as well as 非促音 ("いちとん").
    {"とん", "トン", kZero | kSaTaTails | kIchi | kJu},
    {"どる", "ドル", kZero | kStandardTails},
    {"せんと", "セント", kZero | kSaTaTails | kIchi | kJu},
    {"ゆーろ", "ユーロ", kZero | kStandardTails},
    {"びっと", "ビット", kZero | kStandardTails},
    {"ばいと", "バイト", kZero | kStandardTails},
    {"おくたーぶ", "オクターブ", kStandardTails},

    // -------------------------------------------------------------------------
    // Counters taking special readings ("よ", "し", "しち", "く") for 4/7/9
    // -------------------------------------------------------------------------
    // 人: 4人 takes "よにん" or "よんにん"; 1人 and 2人 use Wago "ひとり" and
    // "ふたり" (standalone "いちにん" [一任] and "ににん" are suppressed, while
    // compound "じゅういちにん" [11人] and "にじゅうににん" [22人] are
    // allowed).
    {"にん", "人", kStandardTails | kYo | kKu,
     /*multi_digit_only_tail_mask=*/kIchi | kNi},
    // 年 / 年間: 4年 takes "よねん" or "よんねん".
    {"ねんかん", "年間", kZero | kStandardTails | kYo | kKu},
    {"ねん", "年", kZero | kStandardTails | kYo | kKu},
    // 円: 4円 takes "よえん" or "よんえん"; 7円 takes "ななえん".
    // "しちえん" (7円) and "くえん" (9円) are not generated.
    {"えん", "円", kZero | (kStandardTails & ~kShichi) | kYo},
    // 月: 4月="しがつ", 7月="しちがつ", 9月="くがつ" ("よんがつ", "なながつ",
    // and "きゅうがつ" are disallowed).
    // Colloquial/misread inputs "よんがつ", "なながつ", and "きゅうがつ" are
    // not generated; only "しがつ", "しちがつ", and "くがつ" are.
    {"がつ", "月",
     kIchi | kNi | kSan | kShi | kGo | kRoku | kShichi | kHachi | kKu | kJu},
    // 日 / 日間: 4日, 14日, 24日 take "よっか" / "よっかかん".
    {"っかかん", "日間", kYo},
    {"っか", "日", kYo},
    // 日 / 日間 (Sino-Japanese): Standalone 2, 3, 4, 5, 6, 8, 10日 use Wago
    // ("ふつか".."とおか") and are therefore restricted to compound numbers
    // ("よんにちかん" [4日間] is allowed even as a single digit).
    // Standalone "ににち" (2日), "さんにち" (3日), "よんにち" (4日), etc. are
    // not generated (use the Wago forms "ふつか", "みっか", "よっか");
    // "じゅうなのか" (17日) and "じゅうようか" (18日) are not supported.
    {"にちかん", "日間",
     kIchi | kNi | kSan | kYon | kGo | kRoku | kNana | kShichi | kHachi | kKyu |
         kKu | kJu | kHyaku | kSen | kMan,
     /*multi_digit_only_tail_mask=*/kNi | kSan | kGo | kRoku | kHachi | kJu |
         kKu},
    {"にち", "日",
     kIchi | kNi | kSan | kYon | kGo | kRoku | kNana | kShichi | kHachi | kKyu |
         kKu | kJu | kHyaku | kSen | kMan,
     /*multi_digit_only_tail_mask=*/kNi | kSan | kYon | kGo | kRoku | kHachi |
         kJu | kKu},
    // 時 / 時間 / 次: 4時="よじ", 7時="しちじ"/"ななじ", 9時="くじ"/"きゅうじ".
    // "よんじかん" (4時間) is not generated; 4時間 takes "よじかん".
    {"じかん", "時間",
     kZero | (kStandardTails & ~kYon & ~kOku & ~kCho) | kYo | kKu},
    {"じ", "時", kZero | (kStandardTails & ~kYon & ~kOku & ~kCho) | kYo | kKu},
    {"じ", "次", (kStandardTails & ~kYon & ~kOku & ~kCho) | kYo},

    // -------------------------------------------------------------------------
    // カ行・パ行頭音 counters
    // -------------------------------------------------------------------------
    {"かい", "回,階", kKaTails},
    {"がい", "階", kKaVoicedTails},
    {"こ", "個,戸", kKaTails},
    // Avoid collision with standalone "せんかん" (戦艦).
    {"かん", "巻,缶,冠", kKaTails, /*multi_digit_only_tail_mask=*/kSen},
    {"けん", "件,軒", kKaTails},
    {"き", "期,機,基", kKaTails},
    // Avoid collision with standalone "せんげん" (宣言).
    {"げん", "軒", kKaVoicedTails, /*multi_digit_only_tail_mask=*/kSen},
    {"かげつ", "ヶ月,か月,カ月,ヵ月", kKaTails},
    {"かしょ", "箇所,か所,ヶ所,カ所", kKaTails},
    {"かこく", "か国,カ国,ヶ国", kKaTails},
    // Avoid collision with standalone "せんきょく" (選挙区).
    {"きょく", "曲,局", kKaTails, /*multi_digit_only_tail_mask=*/kSen},
    // Avoid collisions with standalone "ごこう" (後光) and "せんこう"
    // (選考/専攻).
    {"こう", "校,項", kKaTails, /*multi_digit_only_tail_mask=*/kGo | kSen},
    {"ぺーじ", "ページ,頁", kForeignKaPaTails},
    {"ぱーせんと", "%,パーセント", kZero | kForeignKaPaTails},
    {"きろ", "キロ,km,kg", kZero | kForeignKaPaTails},
    {"かっぷ", "カップ", kForeignKaPaTails},
    {"けーす", "ケース", kForeignKaPaTails},
    {"こーす", "コース", kForeignKaPaTails},
    {"くらす", "クラス", kForeignKaPaTails},
    {"ぱっく", "パック", kForeignKaPaTails},
    {"ぽいんと", "ポイント", kZero | kForeignKaPaTails},
    {"ぽんど", "ポンド", kForeignKaPaTails},

    // -------------------------------------------------------------------------
    // サ行・タ行・シャ行頭音 counters
    // -------------------------------------------------------------------------
    {"さい", "歳,才", kZero | kSaTaTails},
    {"さつ", "冊", kSaTaTails},
    // "はったい" is only valid for "8体" ("8対" requires "はちたい").
    {"たい", "対", kSaTaTails & ~kHat},
    {"たい", "体", kSaTaTails},
    {"つう", "通", kSaTaTails},
    {"てんぽ", "店舗", kSaTaTails},
    {"てん", "点,店", kZero | kSaTaTails},
    {"ちゃく", "着", kSaTaTails},
    // Avoid collisions with standalone "ごとう" (後藤, 五島) and "せんとう"
    // (戦闘, 銭湯, 先頭).
    {"とう", "頭,等,棟", kSaTaTails,
     /*multi_digit_only_tail_mask=*/kGo | kSen},
    {"しつ", "室", kSaTaTails},
    {"しょう", "勝", kZero | kSaTaTails},
    {"しあい", "試合", kSaTaTails | kIchi | kJu},
    // Avoid collision with standalone "にしき" (錦).
    {"しき", "式", kSaTaTails, /*multi_digit_only_tail_mask=*/kNi},
    {"しゃ", "社,車", kSaTaTails},
    {"しゅうかん", "週間", kSaTaTails},
    // Avoid collisions with standalone "せんしゅう" (先週) and "きゅうしゅう"
    // (九州).
    {"しゅう", "週,周", kSaTaTails, /*multi_digit_only_tail_mask=*/kSen | kKyu},
    {"しょく", "色,食", kSaTaTails},
    {"せき", "席,隻", kSaTaTails},
    {"そく", "足,束", kSaTaTails},
    {"しーしー", "cc", kSaTaTails | kIchi | kJu},
    {"しーと", "シート", kSaTaTails | kIchi | kJu},

    // -------------------------------------------------------------------------
    // ハ行頭音 counters: 清音 は / 連濁 ば / 半濁音 ぱ
    // (UniDic ハ混合 / ヒ混合 / ホ混合)
    // -------------------------------------------------------------------------
    // Avoid collision with standalone "せんぱい" (先輩).
    // 倍 precedes 杯 so that "ひゃくばい" ranks 100倍 above 100杯.
    {"ばい", "倍", kStandardTails & ~kShichi},
    {"はい", "杯", kHaTails},
    {"ばい", "杯", kBaTails},
    {"ぱい", "杯", kPaTails, /*multi_digit_only_tail_mask=*/kSen},
    {"はい", "敗", kHaTails},
    {"ばい", "敗", kBaTails},
    {"ぱい", "敗", kPaTails, /*multi_digit_only_tail_mask=*/kSen},
    // Avoid collision with standalone "にほん" (日本); compound
    // "にじゅうにほん"
    // -> "22本" is allowed.
    {"ほん", "本", kHaTails, /*multi_digit_only_tail_mask=*/kNi},
    {"ぼん", "本", kBaTails},
    {"ぽん", "本", kPaTails},
    {"ひき", "匹", kHaTails},
    {"びき", "匹", kBaTails},
    {"ぴき", "匹", kPaTails},
    {"ひょう", "票", kHaTails},
    {"びょう", "票", kBaTails},
    {"ぴょう", "票", kPaTails},
    {"はこ", "箱", kHaTails | kYo, /*multi_digit_only_tail_mask=*/kYo},
    {"ばこ", "箱", kBaTails},
    {"ぱこ", "箱", kPaTails},
    {"はり", "針", kHaTails | kYo, /*multi_digit_only_tail_mask=*/kYo},
    {"ばり", "針", kBaTails},
    {"ぱり", "針", kPaTails},
    // Avoid collisions with standalone "ごはん" (ご飯) and "いっぱん" (一般).
    {"はん", "版,班", kHaTails, /*multi_digit_only_tail_mask=*/kGo},
    {"ばん", "版,班", kBaTails},
    // Avoid collision with standalone "せんぱん" (戦犯) as well.
    {"ぱん", "版,班", kPaTails, /*multi_digit_only_tail_mask=*/kIt | kSen},
    {"ひん", "品", kHaTails},
    {"びん", "品", kBaTails},
    {"ぴん", "品", kPaTails},
    // Avoid collision with standalone "ごほう" (誤報).
    {"ほう", "報,包,砲", kHaTails, /*multi_digit_only_tail_mask=*/kGo},
    {"ぼう", "報,包,砲", kBaTails},
    {"ぽう", "報,包,砲", kPaTails},
    {"ふくろ", "袋", kStandardTails | kYo, /*multi_digit_only_tail_mask=*/kYo},
    {"ぶくろ", "袋", kBaTails},
    {"ぷくろ", "袋", kPaTails},
    {"ふさ", "房", kStandardTails | kYo, /*multi_digit_only_tail_mask=*/kYo},
    {"ぶさ", "房", kBaTails},
    {"ぷさ", "房", kPaTails},

    // -------------------------------------------------------------------------
    // 分 / 分間 (UniDic フ混合: 3 and 4 also take 半濁音 "ぷん")
    // -------------------------------------------------------------------------
    {"ふんかん", "分間", kFunHaTails},
    {"ぷんかん", "分間", kFunPaTails},
    {"ふん", "分", kFunHaTails},
    {"ぷん", "分", kFunPaTails},

    // -------------------------------------------------------------------------
    // 泊, 拍, 発, 編, 辺, 返, 遍, 歩 (UniDic ハ半濁 / ヘ混合 / ホ半濁)
    // -------------------------------------------------------------------------
    {"はく", "泊,拍", kHakuHaTails},
    {"ぱく", "泊,拍", kHakuPaTails},
    {"はつ", "発", kHakuHaTails},
    {"ぱつ", "発", kHakuPaTails},
    {"ばつ", "発", kHakuBaTails},
    {"へん", "編,辺,返,遍,篇", kHakuHaTails},
    {"ぺん", "編,辺,返,遍,篇", kHakuPaTails},
    {"べん", "編,辺,返,遍,篇", kHakuBaTails},
    {"ほ", "歩", kHakuHaTails},
    {"ぽ", "歩", kHakuPaTails},
    {"ぼ", "歩", kHakuBaTails},

    // -------------------------------------------------------------------------
    // 羽, 把 ("わ" / "ば" / "ぱ"; UniDic ワ混合)
    // -------------------------------------------------------------------------
    // 4羽 is "よんわ"; "よわ" (prefix of 弱い) is allowed only in compounds.
    // Avoid collision with standalone "せんば" (船場, 仙波).
    {"わ", "羽,把", kWaTails, /*multi_digit_only_tail_mask=*/kYo},
    {"ば", "羽,把", kWaBaTails, /*multi_digit_only_tail_mask=*/kSen},
    {"ぱ", "羽,把", kWaPaTails},

    // -------------------------------------------------------------------------
    // Additional counters taken from the system dictionary (助数詞 POS entries
    // with low cost) that are not covered above. Grouped by euphony class.
    // Homophone guards were derived by checking "<digit><counter>" readings
    // against common dictionary words.
    // -------------------------------------------------------------------------
    // Derived forms of 回 / 階 / 期 (カ行).
    {"かいめ", "回目", kKaTails},
    {"かいせん", "回戦", kKaTails},
    {"かいぶん", "回分", kKaTails},
    {"かいだて", "階建て", kKaTails},
    {"きせい", "期生", kKaTails},
    // Avoid collision with standalone "きゅうきゅう" (救急).
    {"きゅう", "級,球", kKaTails, /*multi_digit_only_tail_mask=*/kKyu},
    {"きろめーとる", "キロメートル,km", kZero | kForeignKaPaTails},
    {"きろぐらむ", "キログラム,kg", kZero | kForeignKaPaTails},
    // Derived forms of 番 / 代 / 枚 / 度.
    {"ばんめ", "番目", kStandardTails | kYo,
     /*multi_digit_only_tail_mask=*/kYo},
    {"ばんて", "番手", kStandardTails | kYo,
     /*multi_digit_only_tail_mask=*/kYo},
    {"ばんせん", "番線", kStandardTails | kYo,
     /*multi_digit_only_tail_mask=*/kYo},
    {"だいめ", "代目", kStandardTails | kYo,
     /*multi_digit_only_tail_mask=*/kYo},
    {"まいめ", "枚目", kStandardTails | kYo,
     /*multi_digit_only_tail_mask=*/kYo},
    {"どめ", "度目", kStandardTails},
    // Derived forms of 年 ("よねん", "くねん").
    {"ねんせい", "年生", kZero | kStandardTails | kYo | kKu},
    {"ねんど", "年度", kZero | kStandardTails | kYo | kKu},
    {"ねんだい", "年代", kZero | kStandardTails | kYo | kKu},
    {"ねんめ", "年目", kZero | kStandardTails | kYo | kKu},
    {"ねんぶり", "年ぶり", kZero | kStandardTails | kYo | kKu},
    {"ねんご", "年後", kZero | kStandardTails | kYo | kKu},
    {"ねんばん", "年版", kZero | kStandardTails | kYo | kKu},
    // Derived forms of 時 / 次 ("よじ", "くじ").
    {"じはん", "時半",
     kZero | (kStandardTails & ~kYon & ~kOku & ~kCho) | kYo | kKu},
    {"じすぎ", "時過ぎ,時すぎ",
     kZero | (kStandardTails & ~kYon & ~kOku & ~kCho) | kYo | kKu},
    {"じごろ", "時頃,時ごろ",
     kZero | (kStandardTails & ~kYon & ~kOku & ~kCho) | kYo | kKu},
    {"じだい", "時台",
     kZero | (kStandardTails & ~kYon & ~kOku & ~kCho) | kYo | kKu},
    {"じげん", "次元", (kStandardTails & ~kYon & ~kOku & ~kCho) | kYo},
    // Derived forms of 人 ("よにん", "くにん"). Unlike 人, 1人前 and
    // 2人前 take "いちにんまえ" / "ににんまえ".
    {"にんまえ", "人前", kStandardTails | kYo | kKu},
    {"にんぐみ", "人組", kStandardTails | kYo | kKu,
     /*multi_digit_only_tail_mask=*/kIchi},
    // Derived form of 日; standalone 2日目..10日目 use the Wago forms below.
    {"にちめ", "日目",
     kIchi | kNi | kSan | kYon | kGo | kRoku | kNana | kShichi | kHachi | kKyu |
         kKu | kJu | kHyaku | kSen | kMan,
     /*multi_digit_only_tail_mask=*/kNi | kSan | kYon | kGo | kRoku | kHachi |
         kJu | kKu},
    // Derived forms of 号.
    {"ごうせん", "号線", kStandardTails},
    {"ごうき", "号機", kStandardTails},
    // サ行・タ行 counters.
    {"しゅうめ", "周目", kSaTaTails},
    {"しゅうねん", "周年", kSaTaTails},
    {"せいき", "世紀", kSaTaTails},
    {"さいじ", "歳児", kZero | kSaTaTails},
    {"とうぶん", "等分", kSaTaTails},
    // Avoid collisions with standalone "いっせい" (一斉), "さんせい" (賛成),
    // "はっせい" (発生), "きゅうせい" (急性), and "せんせい" (先生).
    {"せい", "世", kSaTaTails,
     /*multi_digit_only_tail_mask=*/kIt | kSan | kHat | kKyu | kSen},
    // Avoid collisions with standalone "さんさく" (散策) and "きゅうさく"
    // (久作).
    {"さく", "作", kSaTaTails, /*multi_digit_only_tail_mask=*/kSan | kKyu},
    {"せつ", "節", kSaTaTails},
    // Avoid collisions with standalone "いっそう" (一層), "はっそう" (発送),
    // and
    // "せんそう" (戦争).
    {"そう", "層", kSaTaTails,
     /*multi_digit_only_tail_mask=*/kIt | kHat | kSen},
    // Avoid collision with standalone "せんたく" (選択).
    {"たく", "択", kSaTaTails, /*multi_digit_only_tail_mask=*/kSen},
    {"せっと", "セット", kZero | kSaTaTails | kIchi | kJu},
    // Other unchanged counters.
    {"いんち", "インチ", kZero | kStandardTails},
    // Avoid collisions with standalone "ごめん" and "せんめん" (洗面).
    {"めん", "面", kStandardTails, /*multi_digit_only_tail_mask=*/kGo | kSen},
    // Avoid collisions with standalone "ごれい" (御礼) and "せんれい" (洗礼).
    {"れい", "例", kStandardTails, /*multi_digit_only_tail_mask=*/kGo | kSen},
    // Avoid collisions with standalone "ごりょう" (御料), "きゅうりょう"
    // (給料),
    // "じゅうりょう" (重量), and "せんりょう" (占領).
    {"りょう", "両", kStandardTails,
     /*multi_digit_only_tail_mask=*/kGo | kKyu | kJu | kSen},
    {"りん", "輪", kStandardTails},
    // Avoid collision with standalone "せんれん" (洗練).
    {"れん", "連", kStandardTails, /*multi_digit_only_tail_mask=*/kSen},
    {"わく", "枠", kStandardTails},
    {"まく", "幕", kStandardTails},
    {"とおり", "通り", kStandardTails},
    {"らうんど", "ラウンド", kStandardTails},
    {"ふぃーと", "フィート", kZero | kStandardTails},
    {"まいる", "マイル", kZero | kStandardTails},
    {"うぉん", "ウォン", kZero | kStandardTails},
    {"へいほうめーとる", "平方メートル", kZero | kStandardTails},
    {"せんちめーとる", "センチメートル,cm", kZero | kStandardTails},
    {"みりめーとる", "ミリメートル,mm", kZero | kStandardTails},
    {"みりりっとる", "ミリリットル,ml", kZero | kStandardTails},
    {"みりぐらむ", "ミリグラム,mg", kZero | kStandardTails},
    {"ぎが", "ギガ,GB", kZero | kStandardTails},
    {"めが", "メガ,MB", kZero | kStandardTails},
};

// =============================================================================
// 4. Standalone Wago / Irregular Number + Counter Rules
// =============================================================================

struct WagoNumberSuffixRule {
  absl::string_view reading;
  absl::string_view candidate;
  int digit_num;
};

constexpr WagoNumberSuffixRule kWagoNumberSuffixRules[] = {
    // 人 (1人, 2人)
    {"ひとり", "1人", 1},
    {"ふたり", "2人", 1},

    // 日 / 日間 (1日..10日, 20日)
    {"ついたち", "1日", 1},
    {"いっぴ", "1日", 1},
    {"ふつかかん", "2日間", 1},
    {"ふつか", "2日", 1},
    {"みっかかん", "3日間", 1},
    {"みっか", "3日", 1},
    {"いつかかん", "5日間", 1},
    {"いつか", "5日", 1},
    {"むいかかん", "6日間", 1},
    {"むいか", "6日", 1},
    {"なのかかん", "7日間", 1},
    {"なのか", "7日", 1},
    {"ようかかん", "8日間", 1},
    {"ようか", "8日", 1},
    {"よおかかん", "8日間", 1},
    {"よおか", "8日", 1},
    {"ここのかかん", "9日間", 1},
    {"ここのか", "9日", 1},
    {"とおかかん", "10日間", 2},
    {"とおか", "10日", 2},
    {"とうかかん", "10日間", 2},
    {"とうか", "10日", 2},
    {"はつかかん", "20日間", 2},
    {"はつか", "20日", 2},

    // つ (1つ..9つ)
    {"ひとつ", "1つ", 1},
    {"ふたつ", "2つ", 1},
    {"みっつ", "3つ", 1},
    {"よっつ", "4つ", 1},
    {"いつつ", "5つ", 1},
    {"むっつ", "6つ", 1},
    {"ななつ", "7つ", 1},
    {"やっつ", "8つ", 1},
    {"ここのつ", "9つ", 1},

    // つ目 (1つ目..9つ目)
    {"ひとつめ", "1つ目", 1},
    {"ふたつめ", "2つ目", 1},
    {"みっつめ", "3つ目", 1},
    {"よっつめ", "4つ目", 1},
    {"いつつめ", "5つ目", 1},
    {"むっつめ", "6つ目", 1},
    {"ななつめ", "7つ目", 1},
    {"やっつめ", "8つ目", 1},
    {"ここのつめ", "9つ目", 1},

    // 日目 (2日目..10日目, 20日目)
    {"ふつかめ", "2日目", 1},
    {"みっかめ", "3日目", 1},
    {"よっかめ", "4日目", 1},
    {"いつかめ", "5日目", 1},
    {"むいかめ", "6日目", 1},
    {"なのかめ", "7日目", 1},
    {"ようかめ", "8日目", 1},
    {"ここのかめ", "9日目", 1},
    {"とおかめ", "10日目", 2},
    {"はつかめ", "20日目", 2},

    // 0時 / 0時間 / 0点 / 0度
    {"れいじかん", "0時間", 1},
    {"れいじ", "0時", 1},
    {"れいてん", "0点", 1},
    {"れいど", "0度", 1},
};

// =============================================================================
// 5. Wago Numeral Prefix ("ひと" / "ふた" / "み" / "よ") + Counter Rules
// =============================================================================

enum WagoPrefix : uint8_t {
  kWagoHito = 1u << 0,  // "ひと" (1)
  kWagoFuta = 1u << 1,  // "ふた" (2)
  kWagoMi = 1u << 2,    // "み" (3)
  kWagoYo = 1u << 3,    // "よ" (4)
};

struct WagoPrefixEntry {
  absl::string_view reading;
  absl::string_view number;
  uint8_t flag;
};

// Wago numeral prefixes that combine with the suffixes in
// kWagoPrefixSuffixRules below (e.g., "ひと" + "はこ" -> "1箱").
constexpr WagoPrefixEntry kWagoPrefixes[] = {
    {"ひと", "1", kWagoHito},
    {"ふた", "2", kWagoFuta},
    {"み", "3", kWagoMi},
    {"よ", "4", kWagoYo},
};

// Wago counters used with 1 and 2 ("ひと", "ふた").
// 3 ("み") and 4 ("よ") are not allowed for these counters because of
// homophone collisions with common words such as "みかん" (蜜柑), "みかけ"
// (見掛け), "みはり" (見張り), "みふり" (身振り), and "みさお" (操).
constexpr uint8_t kHitoFuta = kWagoHito | kWagoFuta;
// Wago counters used with 1, 2, and 4 ("ひと", "ふた", "よ"), e.g., 晩
// ("みばん" is excluded because it collides with 三番).
constexpr uint8_t kHitoFutaYo = kWagoHito | kWagoFuta | kWagoYo;
// Wago counters used with 1 through 4 ("ひと", "ふた", "み", "よ"), e.g., 部屋.
constexpr uint8_t kHitoToYo = kWagoHito | kWagoFuta | kWagoMi | kWagoYo;

struct WagoPrefixSuffixRule {
  absl::string_view reading;
  absl::string_view suffixes;
  uint8_t allowed_prefix_mask;
};

constexpr WagoPrefixSuffixRule kWagoPrefixSuffixRules[] = {
    {"はこ", "箱", kHitoFuta},       {"ばん", "晩", kHitoFutaYo},
    {"へや", "部屋", kHitoToYo},     {"ふくろ", "袋", kHitoFuta},
    {"つぶ", "粒", kHitoFuta},       {"さら", "皿", kHitoFuta},
    {"やま", "山", kHitoFuta},       {"きれ", "切れ,切", kHitoFuta},
    {"くち", "口", kHitoFuta},       {"つぼ", "坪,壺", kHitoFuta},
    {"かたまり", "塊", kHitoFuta},   {"まき", "巻", kHitoFuta},
    {"ふさ", "房", kHitoFuta},       {"かけ", "掛け", kHitoFuta},
    {"にぎり", "握り", kHitoFuta},   {"たば", "束", kHitoFuta},
    {"えだ", "枝", kHitoFuta},       {"かぶ", "株", kHitoFuta},
    {"けた", "桁", kHitoFuta},       {"おけ", "桶", kHitoFuta},
    {"むね", "棟", kHitoFuta},       {"たる", "樽", kHitoFuta},
    {"すじ", "筋", kHitoFuta},       {"かん", "缶", kHitoFuta},
    {"びん", "瓶", kHitoFuta},       {"ぱっく", "パック", kHitoFuta},
    {"つまみ", "つまみ", kHitoFuta}, {"さじ", "匙", kHitoFuta},
    {"ふり", "振り,振", kHitoFuta},  {"くし", "串", kHitoFuta},
    {"しずく", "雫", kHitoFuta},     {"かご", "籠", kHitoFuta},
    {"はち", "鉢", kHitoFuta},       {"はり", "針", kHitoFuta},
    {"さし", "差し", kHitoFuta},     {"さお", "竿,棹", kHitoFuta},
    {"わん", "椀", kHitoFuta},       {"とおり", "通り", kHitoFuta},
    {"かま", "釜,窯", kHitoFuta},
};

// =============================================================================
// 6. Stop-Decoding Suffixes (Longest-Match Guards Against False Number Splits)
// =============================================================================
//
// Counter suffixes that start with the same kana as a number token ("に", "し",
// "よ", "ご", "く", "せん", "おく", "ちょう") are registered in the Trie as
// Type::STOP_DECODING so that longest-match lookup stops before the suffix
// instead of greedily consuming its leading kana as part of the number.
// Examples:
// - Prevents "じゅうにん" (10人) from splitting into "じゅう" + "に" (12) +
// "ん"
// - Prevents "じゅうしあい" (10試合) from splitting into "じゅう" + "し" (14) +
// "あい"
// - Prevents "にせんち" (2センチ) from splitting into "に" + "せん" (2000) +
// "ち"
// - Prevents "さんちょうめ" (3丁目) from splitting into "さん" + "ちょう" (3兆)
// + "め"
//
// |emits_bare_number_prefix| controls whether the bare number preceding the
// suffix is also emitted as a partial-key candidate when the suffix ends the
// input (e.g., "にせんち" -> "2" (partial) + "2センチ" (full)). When false,
// only the full-key counter candidate is emitted (e.g., "じゅうにん" ->
// "10人" only).
//
// Only "ごう", "せんち", and "ちょうめ" emit the bare number prefix, keeping
// the behavior that existed before counter suffix support was added.
struct StopSuffixEntry {
  absl::string_view reading;
  bool emits_bare_number_prefix;
};

constexpr StopSuffixEntry kStopSuffixEntries[] = {
    // Counters starting with "に" (2): 握り, 日, 人
    {"にぎり", false},
    {"にち", false},
    {"にん", false},
    // Counters starting with "し" (4): cc, シート, シーベルト, 試合, 式, 室,
    // 品, 社, 種, 勝, 色, シリング, 進
    {"しーしー", false},
    {"しーと", false},
    {"しーべると", false},
    {"しあい", false},
    {"しき", false},
    {"しつ", false},
    {"しな", false},
    {"しゃ", false},
    {"しゅ", false},
    {"しょう", false},
    {"しょく", false},
    {"しりんぐ", false},
    {"しん", false},
    // Counters starting with "よ" (4): 葉
    {"よう", false},
    // Counters starting with "ご" (5): 号
    {"ごう", true},
    // Counters starting with "く" (9): 下り, 口, 組, クラス, クローナ
    {"くだり", false},
    {"くち", false},
    {"くみ", false},
    {"くらす", false},
    {"くろーな", false},
    // Counters starting with "せん" (1000): センチ, セント
    {"せんち", true},
    {"せんと", false},
    // Counters starting with "おく" (億): オクターブ
    {"おくたーぶ", false},
    // Counters starting with "ちょう" (兆): 丁目
    {"ちょうめ", true},
};

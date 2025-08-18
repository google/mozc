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

#include "rewriter/ivs_variants_rewriter.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

struct ExpansionValue {
  absl::string_view ivs_surface;
  absl::string_view additional_description;
};

// {"reading", "base surface"}, {"IVS surface", "additional description"}},
const auto* kIvsExpansionTable = new absl::flat_hash_map<
    std::pair<absl::string_view, absl::string_view>, ExpansionValue>{
    {{"かつらぎし", "葛城市"}, {"葛\U000E0100城市", "正式字体"}},  // 葛󠄀城市
    {{"ぎおん", "祇園"}, {"祇\U000E0100園", "礻"}},                // 祇󠄀園
    {{"つじのぞみ", "辻希美"}, {"辻\U000E0100希美", "「辻󠄀」"}},    // 辻󠄀希美
    {{"つじもときよみ", "辻元清美"},
     {"辻\U000E0100元清美", "「辻󠄀」"}},  // 辻󠄀元清美
    {{"つじよしなり", "辻よしなり"},
     {"辻\U000E0100よしなり", "「辻󠄀」"}},                          // 辻󠄀よしなり
    {{"つじしんぱち", "辻親八"}, {"辻\U000E0100新八", "「辻󠄀」"}},  // 辻󠄀新八
    {{"つじもとけんと", "辻本賢人"},
     {"辻\U000E0100本賢人", "「辻󠄀」"}},                              // 辻󠄀本賢人
    {{"つじあゆみ", "辻あゆみ"}, {"辻\U000E0100あゆみ", "「辻󠄀」"}},  // 辻󠄀あゆみ
    {{"つじかおり", "辻香緒里"}, {"辻\U000E0100香緒里", "「辻󠄀」"}},  // 辻󠄀香緒里
    {{"つじかおり", "辻香織"}, {"辻\U000E0100香織", "「辻󠄀」"}},      // 辻󠄀香織
    {{"つじもとたつのり", "辻本達規"},
     {"辻\U000E0100本達規", "「辻󠄀」"}},                              // 辻󠄀本達規
    {{"つじもととおり", "辻本通"}, {"辻\U000E0100本通", "「辻󠄀」"}},  // 辻󠄀本通
    {{"つじしんご", "辻慎吾"}, {"辻\U000E0100慎吾", "「辻󠄀」"}},      // 辻󠄀慎吾
    {{"つじせいめい", "辻清明"}, {"辻\U000E0100清明", "「辻󠄀」"}},    // 辻󠄀清明
    {{"つじなかゆたか", "辻中豊"}, {"辻\U000E0100中豊", "「辻󠄀」"}},  // 辻󠄀中豊
    {{"つじもとともひこ", "辻本知彦"},
     {"辻\U000E0100本知彦", "「辻󠄀」"}},  // 辻󠄀本知彦
    {{"つじいのぶゆき", "辻井伸行"},
     {"辻\U000E0100井伸行", "「辻󠄀」"}},                              // 辻󠄀井伸行
    {{"さかきいちろう", "榊一郎"}, {"榊\U000E0100一郎", "「榊󠄀」"}},  // 榊󠄀一郎
    {{"さかきばらいくえ", "榊原郁恵"},
     {"榊\U000E0100原郁恵", "「榊󠄀」"}},                              // 榊󠄀原郁恵
    {{"さかきひでお", "榊英雄"}, {"榊\U000E0100英雄", "「榊󠄀」"}},    // 榊󠄀英雄
    {{"さかきひろゆき", "榊裕之"}, {"榊\U000E0100裕之", "「榊󠄀」"}},  // 榊󠄀裕之
    {{"さかきよしゆき", "榊佳之"}, {"榊\U000E0100佳之", "「榊󠄀」"}},  // 榊󠄀佳之
    {{"さかきばらまさくに", "榊原政邦"},
     {"榊\U000E0100原政邦", "「榊󠄀」"}},  // 榊󠄀原政邦
    {{"さかきいずみ", "榊いずみ"},
     {"榊\U000E0100いずみ", "「榊󠄀」"}},  // 榊󠄀いずみ
    {{"さかきりょうざぶろう", "榊亮三郎"},
     {"榊\U000E0100亮三郎", "「榊󠄀」"}},  // 榊󠄀亮三郎
    {{"さかきばらなおこ", "榊原菜緒子"},
     {"榊\U000E0100原菜緒子", "「榊󠄀」"}},  // 榊󠄀原菜緒子
    {{"きりもとたくや", "桐本琢也"},
     {"桐本琢\U000E0100也", "「琢󠄀」"}},  // 桐本琢󠄀也
    {{"ふるたちいちろう", "古舘伊知郎"},
     {"古舘\U000E0101伊知郎", "正式字体"}},  // 古舘󠄁伊知郎
    {{"ひろたこうき", "廣田弘毅"},
     {"廣\U000E0101田弘毅", "「廣󠄁」"}},                            // 廣󠄁田弘毅
    {{"こばやしけん", "小林劍"}, {"小林劍\U000E0101", "「劍」"}},  // 小林劍󠄁
    {{"きりやまれん", "桐山漣"}, {"桐山漣\U000E0101", "「漣󠄁」"}},  // 桐山漣󠄁
    {{"しばりょうたろう", "司馬遼太郎"},
     {"司馬遼\U000E0101太郎", "正式字体"}},  // 司馬遼󠄁太郎
    {{"ほうらいだいすけ", "蓬莱大輔"},
     {"蓬\U000E0100莱大輔", "「蓬󠄀」"}},  // 蓬󠄀莱大輔
    {{"かまどねずこ", "竈門禰豆子"},
     {"竈門禰\U000E0100豆子", "正式字体"}},  // 竈門禰󠄀豆子
    {{"きぶつじむざん", "鬼舞辻無惨"},
     {"鬼舞辻\U000E0100無惨", "正式字体"}},  // 鬼舞辻󠄀無惨
    {{"れんごくきょうじゅろう", "煉獄杏寿郎"},
     {"煉\U000E0101獄杏寿郎", "正式字体"}},  // 煉󠄁獄杏寿郎
    {{"れんごくるか", "煉獄瑠火"},
     {"煉\U000E0101獄瑠火", "正式字体"}},  // 煉󠄁獄瑠火
    {{"れんごくしんじゅろう", "煉獄槇寿郎"},
     {"煉\U000E0101獄槇寿郎", "正式字体"}},  // 煉󠄁獄槇寿郎
    {{"れんごくせんじゅろう", "煉獄千寿郎"},
     {"煉\U000E0101獄千寿郎", "正式字体"}},                        // 煉󠄁獄千寿郎
    {{"れんごく", "煉獄"}, {"煉\U000E0101獄", "「煉󠄁」"}},          // 煉󠄁獄
    {{"ねずこ", "禰豆子"}, {"禰\U000E0100豆子", "正式字体"}},      // 禰󠄀豆子
    {{"みそ", "味噌"}, {"味噌\U000E0100", "「噌󠄀」"}},              // 味噌󠄀
    {{"つじ", "辻"}, {"辻\U000E0100", "一点しんにょう"}},          // 辻󠄀
    {{"つじもと", "辻本"}, {"辻\U000E0100本", "一点しんにょう"}},  // 辻󠄀本
    {{"つじもと", "辻元"}, {"辻\U000E0100元", "一点しんにょう"}},  // 辻󠄀元
    {{"つじなか", "辻中"}, {"辻\U000E0100中", "一点しんにょう"}},  // 辻󠄀中
    {{"つじい", "辻井"}, {"辻\U000E0100井", "一点しんにょう"}},    // 辻󠄀井
    {{"さかき", "榊"}, {"榊\U000E0100", ""}},                      // 榊󠄀
    {{"さかきばら", "榊原"}, {"榊\U000E0100原", "「榊」"}}         // 榊󠄀原
};

constexpr char kIvsVariantDescription[] = "環境依存(IVS)";

bool ExpandIvsVariantsWithSegment(Segment* seg) {
  CHECK(seg);

  bool modified = false;
  for (int i = seg->candidates_size() - 1; i >= 0; --i) {
    const converter::Candidate& original_candidate = seg->candidate(i);
    auto it = kIvsExpansionTable->find(
        {original_candidate.content_key, original_candidate.content_value});
    if (it == kIvsExpansionTable->end()) {
      continue;
    }
    modified = true;

    const auto& [key, value] = *it;

    auto new_candidate =
        std::make_unique<converter::Candidate>(original_candidate);
    // "は" for "葛城市は"
    const absl::string_view non_content_value =
        absl::string_view(original_candidate.value)
            .substr(original_candidate.content_value.size());
    new_candidate->value = absl::StrCat(value.ivs_surface, non_content_value);
    new_candidate->content_value.assign(value.ivs_surface.data(),
                                        value.ivs_surface.size());
    new_candidate->description =
        value.additional_description.empty()
            ? kIvsVariantDescription
            : absl::StrCat(kIvsVariantDescription, " ",
                           value.additional_description);
    seg->insert_candidate(i + 1, std::move(new_candidate));
  }
  return modified;
}
}  // namespace

int IvsVariantsRewriter::capability(const ConversionRequest& request) const {
  return RewriterInterface::ALL;
}

bool IvsVariantsRewriter::Rewrite(const ConversionRequest& request,
                                  Segments* segments) const {
  bool modified = false;
  for (Segment& segment : segments->conversion_segments()) {
    modified |= ExpandIvsVariantsWithSegment(&segment);
  }

  return modified;
}

}  // namespace mozc

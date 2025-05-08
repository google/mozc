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

#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_dictionary.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_pos.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

using dictionary::UserDictionary;
using dictionary::UserPos;
using ::testing::_;
using ::testing::SetArgPointee;

void AddCandidate(const absl::string_view key, const absl::string_view value,
                  const absl::string_view content_key,
                  const absl::string_view content_value, Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->key = std::string(key);
  candidate->value = std::string(value);
  candidate->content_key = std::string(content_key);
  candidate->content_value = std::string(content_value);
}

}  // namespace

class UsageRewriterPeer {
 public:
  explicit UsageRewriterPeer(std::unique_ptr<UsageRewriter> rewriter)
      : rewriter_(std::move(rewriter)) {}

  SerializedStringArray get_string_array() { return rewriter_->string_array_; }

 private:
  std::unique_ptr<UsageRewriter> rewriter_;
};

class TestDataManager : public testing::MockDataManager {
 public:
  MOCK_METHOD(void, GetUsageRewriterData,
              (absl::string_view * base_conjugation_suffix_data,
               absl::string_view *conjugation_suffix_data,
               absl::string_view *conjugation_index_data,
               absl::string_view *usage_items_data,
               absl::string_view *string_array_data),
              (const, override));
};

class UsageRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  UsageRewriterTest()
      : pos_matcher_(data_manager_.GetPosMatcherData()),
        user_dictionary_(UserPos::CreateFromDataManager(data_manager_),
                         pos_matcher_) {}

  void SetUp() override { config::ConfigHandler::GetDefaultConfig(&config_); }

  void TearDown() override {
    // just in case, reset the config
    config::ConfigHandler::GetDefaultConfig(&config_);
  }

  UsageRewriter *CreateUsageRewriter() const {
    return new UsageRewriter(data_manager_, user_dictionary_);
  }

  UsageRewriter *CreateUsageRewriterWithTestDataManager() const {
    return new UsageRewriter(test_data_manager_, user_dictionary_);
  }

  static ConversionRequest ConvReq(const config::Config &config,
                                   const commands::Request &request) {
    return ConversionRequestBuilder()
        .SetConfig(config)
        .SetRequest(request)
        .Build();
  }

 private:
  const testing::MockDataManager data_manager_;

 protected:
  commands::Request request_;
  config::Config config_;

  TestDataManager test_data_manager_;
  dictionary::PosMatcher pos_matcher_;
  UserDictionary user_dictionary_;
};

TEST_F(UsageRewriterTest, ConstructorTest) {
  EXPECT_CALL(test_data_manager_, GetUsageRewriterData(_, _, _, _, _))
      .WillOnce(SetArgPointee<4>(""));

  std::unique_ptr<UsageRewriter> rewriter(
      CreateUsageRewriterWithTestDataManager());

  UsageRewriterPeer rewriter_peer(std::move(rewriter));
  EXPECT_TRUE(rewriter_peer.get_string_array().empty());
  EXPECT_TRUE(SerializedStringArray::VerifyData(
      rewriter_peer.get_string_array().data()));
}

TEST_F(UsageRewriterTest, CapabilityTest) {
  std::unique_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  const ConversionRequest convreq = ConvReq(config_, request_);
  EXPECT_EQ(rewriter->capability(convreq),
            RewriterInterface::CONVERSION | RewriterInterface::PREDICTION);
}

TEST_F(UsageRewriterTest, ConjugationTest) {
  Segments segments;
  std::unique_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  Segment *seg;

  segments.Clear();
  seg = segments.push_back_segment();
  seg->set_key("うたえば");
  AddCandidate("うたえば", "歌えば", "うたえ", "歌え", seg);
  AddCandidate("うたえば", "唱えば", "うたえ", "唄え", seg);
  const ConversionRequest convreq = ConvReq(config_, request_);
  EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_title, "歌う");
  EXPECT_NE(segments.conversion_segment(0).candidate(0).usage_description, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).usage_title, "唄う");
  EXPECT_NE(segments.conversion_segment(0).candidate(1).usage_description, "");
}

TEST_F(UsageRewriterTest, SingleSegmentSingleCandidateTest) {
  Segments segments;
  std::unique_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  Segment *seg;
  const ConversionRequest convreq = ConvReq(config_, request_);

  segments.Clear();
  seg = segments.push_back_segment();
  seg->set_key("あおい");
  AddCandidate("あおい", "青い", "あおい", "青い", seg);
  EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_title, "青い");
  EXPECT_NE(segments.conversion_segment(0).candidate(0).usage_description, "");

  segments.Clear();
  seg = segments.push_back_segment();
  seg->set_key("あおい");
  AddCandidate("あおい", "あああ", "あおい", "あああ", seg);
  EXPECT_FALSE(rewriter->Rewrite(convreq, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_title, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_description, "");
}

TEST_F(UsageRewriterTest, ConfigTest) {
  Segments segments;
  std::unique_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  Segment *seg;

  // Default setting
  {
    segments.Clear();
    seg = segments.push_back_segment();
    seg->set_key("あおい");
    AddCandidate("あおい", "青い", "あおい", "青い", seg);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
  }

  {
    config_.mutable_information_list_config()->set_use_local_usage_dictionary(
        false);

    segments.Clear();
    seg = segments.push_back_segment();
    seg->set_key("あおい");
    AddCandidate("あおい", "青い", "あおい", "青い", seg);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_FALSE(rewriter->Rewrite(convreq, &segments));
  }

  {
    config_.mutable_information_list_config()->set_use_local_usage_dictionary(
        true);

    segments.Clear();
    seg = segments.push_back_segment();
    seg->set_key("あおい");
    AddCandidate("あおい", "青い", "あおい", "青い", seg);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
  }
}

TEST_F(UsageRewriterTest, SingleSegmentMultiCandidatesTest) {
  Segments segments;
  std::unique_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  Segment *seg;
  const ConversionRequest convreq = ConvReq(config_, request_);

  segments.Clear();
  seg = segments.push_back_segment();
  seg->set_key("あおい");
  AddCandidate("あおい", "青い", "あおい", "青い", seg);
  AddCandidate("あおい", "蒼い", "あおい", "蒼い", seg);
  EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_title, "青い");
  EXPECT_NE(segments.conversion_segment(0).candidate(0).usage_description, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).usage_title, "蒼い");
  EXPECT_NE(segments.conversion_segment(0).candidate(1).usage_description, "");

  segments.Clear();
  seg = segments.push_back_segment();
  seg->set_key("あおい");
  AddCandidate("あおい", "青い", "あおい", "青い", seg);
  AddCandidate("あおい", "あああ", "あおい", "あああ", seg);
  EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_title, "青い");
  EXPECT_NE(segments.conversion_segment(0).candidate(0).usage_description, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).usage_title, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).usage_description, "");

  segments.Clear();
  seg = segments.push_back_segment();
  seg->set_key("あおい");
  AddCandidate("あおい", "あああ", "あおい", "あああ", seg);
  AddCandidate("あおい", "青い", "あおい", "青い", seg);
  EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_title, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_description, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).usage_title, "青い");
  EXPECT_NE(segments.conversion_segment(0).candidate(1).usage_description, "");

  segments.Clear();
  seg = segments.push_back_segment();
  seg->set_key("あおい");
  AddCandidate("あおい", "あああ", "あおい", "あああ", seg);
  AddCandidate("あおい", "いいい", "あおい", "いいい", seg);
  EXPECT_FALSE(rewriter->Rewrite(convreq, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_title, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_description, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).usage_title, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).usage_description, "");
}

TEST_F(UsageRewriterTest, MultiSegmentsTest) {
  Segments segments;
  std::unique_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  Segment *seg;
  const ConversionRequest convreq = ConvReq(config_, request_);

  segments.Clear();
  seg = segments.push_back_segment();
  seg->set_key("あおく");
  AddCandidate("あおく", "青く", "あおく", "青く", seg);
  AddCandidate("あおく", "蒼く", "あおく", "蒼く", seg);
  AddCandidate("あおく", "アオク", "あおく", "アオク", seg);
  seg = segments.push_back_segment();
  seg->set_key("うたえば");
  AddCandidate("うたえば", "歌えば", "うたえ", "歌え", seg);
  AddCandidate("うたえば", "唱えば", "うたえ", "唄え", seg);
  EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_title, "青い");
  EXPECT_NE(segments.conversion_segment(0).candidate(0).usage_description, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).usage_title, "蒼い");
  EXPECT_NE(segments.conversion_segment(0).candidate(1).usage_description, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(2).usage_title, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(2).usage_description, "");
  EXPECT_EQ(segments.conversion_segment(1).candidate(0).usage_title, "歌う");
  EXPECT_NE(segments.conversion_segment(1).candidate(0).usage_description, "");
  EXPECT_EQ(segments.conversion_segment(1).candidate(1).usage_title, "唄う");
  EXPECT_NE(segments.conversion_segment(1).candidate(1).usage_description, "");
}

TEST_F(UsageRewriterTest, SameUsageTest) {
  Segments segments;
  std::unique_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  Segment *seg;
  const ConversionRequest convreq = ConvReq(config_, request_);

  seg = segments.push_back_segment();
  seg->set_key("うたえば");
  AddCandidate("うたえば", "歌えば", "うたえ", "歌え", seg);
  AddCandidate("うたえば", "唱えば", "うたえ", "唄え", seg);
  AddCandidate("うたえば", "唱エバ", "うたえ", "唄え", seg);
  EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).usage_title, "歌う");
  EXPECT_NE(segments.conversion_segment(0).candidate(0).usage_description, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).usage_title, "唄う");
  EXPECT_NE(segments.conversion_segment(0).candidate(1).usage_description, "");
  EXPECT_EQ(segments.conversion_segment(0).candidate(2).usage_title, "唄う");
  EXPECT_NE(segments.conversion_segment(0).candidate(2).usage_description, "");
  EXPECT_NE(segments.conversion_segment(0).candidate(0).usage_id,
            segments.conversion_segment(0).candidate(1).usage_id);
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).usage_id,
            segments.conversion_segment(0).candidate(2).usage_id);
}

TEST_F(UsageRewriterTest, GetKanjiPrefixAndOneHiragana) {
  EXPECT_EQ(UsageRewriter::GetKanjiPrefixAndOneHiragana("合わせる"), "合わ");
  EXPECT_EQ(UsageRewriter::GetKanjiPrefixAndOneHiragana("合う"), "合う");
  EXPECT_EQ(UsageRewriter::GetKanjiPrefixAndOneHiragana("合合わせる"),
            "合合わ");
  EXPECT_EQ(UsageRewriter::GetKanjiPrefixAndOneHiragana("合"), "");
  EXPECT_EQ(UsageRewriter::GetKanjiPrefixAndOneHiragana("京都"), "");
  EXPECT_EQ(UsageRewriter::GetKanjiPrefixAndOneHiragana("合合合わせる"), "");
  EXPECT_EQ(UsageRewriter::GetKanjiPrefixAndOneHiragana("カタカナ"), "");
  EXPECT_EQ(UsageRewriter::GetKanjiPrefixAndOneHiragana("abc"), "");
  EXPECT_EQ(UsageRewriter::GetKanjiPrefixAndOneHiragana("あ合わせる"), "");
}

TEST_F(UsageRewriterTest, CommentFromUserDictionary) {
  // Load mock data
  {
    UserDictionaryStorage storage("");
    UserDictionaryStorage::UserDictionary *dic =
        storage.GetProto().add_dictionaries();

    UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
    entry->set_key("うま");
    entry->set_value("アルパカ");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
    entry->set_comment("アルパカコメント");

    user_dictionary_.Load(storage.GetProto());
  }

  // Emulates the conversion of key="うま".
  Segments segments;
  segments.Clear();
  Segment *seg = segments.push_back_segment();
  seg->set_key("うま");
  AddCandidate("うま", "Horse", "うま", "Horse", seg);
  AddCandidate("うま", "アルパカ", "うま", "アルパカ", seg);

  std::unique_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  const ConversionRequest convreq = ConvReq(config_, request_);
  EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));

  // Result of ("うま", "Horse"). No comment is expected.
  const Segment::Candidate &cand0 = segments.conversion_segment(0).candidate(0);
  EXPECT_TRUE(cand0.usage_title.empty());
  EXPECT_TRUE(cand0.usage_description.empty());

  // Result of ("うま", "アルパカ"). Comment from user dictionary is expected.
  const Segment::Candidate &cand1 = segments.conversion_segment(0).candidate(1);
  EXPECT_EQ(cand1.usage_title, "アルパカ");
  EXPECT_EQ(cand1.usage_description, "アルパカコメント");
}

}  // namespace mozc

#endif  // NO_USAGE_REWRITER

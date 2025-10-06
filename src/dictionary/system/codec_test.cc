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

#include "dictionary/system/codec.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/random.h"
#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
#include "testing/gunit.h"

namespace mozc {
namespace dictionary {
namespace {

::testing::AssertionResult MakeAssertResult(bool success, char32_t c,
                                            const char* message) {
  if (success) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
         << message << " c = " << absl::StreamFormat("U+%05X", c);
}

::testing::AssertionResult IsExpectedEncodedSize(
    char32_t c, const absl::string_view encoded) {
  const std::string::size_type size = encoded.size();
  if (c == 0x00) {
    return ::testing::AssertionFailure() << "NUL is not supported.";
  }
  if (c <= 0xff) {
    return MakeAssertResult(size == 2, c,
                            "U+00?? (ASCII) should be encoded into 2 bytes.");
  }
  if (0x10000 <= c && c <= 0x10ffff) {
    if ((c & 0xffff) == 0) {
      return MakeAssertResult(size == 2, c,
                              "U+?0000 and U+100000 are encoded into 2 bytes.");
    }
    if ((c & 0xff) == 0) {
      return MakeAssertResult(size == 3, c,
                              "U+???00 and U+10??00 are encoded into 3 bytes.");
    }
    if (((c & 0xff00) >> 8) == 0) {
      return MakeAssertResult(size == 3, c,
                              "U+?00?? and U+1000?? are encoded into 3 bytes.");
    }
    return MakeAssertResult(
        size == 4, c,
        "[U+10000, U+10FFFF] except for U+???00, U+?00??, U+10??00 and "
        "U+1000?? should be encoded into 4 bytes.");
  }
  if (0x10ffff < c) {
    return MakeAssertResult(false, c,
                            "U+110000 and greater are not supported.");
  }
  if (0xffff < c) {
    return MakeAssertResult(false, c, "Should not reach here.");
  }

  // Hereafter, |c| should be represented as 0x????
  const uint16_t s = static_cast<uint16_t>(c);
  if ((s & 0xff) == 0) {
    return MakeAssertResult(size == 2, c, "U+??00 are encoded into 2 bytes.");
  }
  if (0x3041 <= s && s < 0x3095) {
    return MakeAssertResult(size == 1, c,
                            "Hiragana(85 characters) are encoded into 1 byte.");
  }
  if (0x30a1 <= s && s < 0x30fd) {
    return MakeAssertResult(
        size == 1, c, "Katakana (92 characters) are encoded into 1 byte.");
  }
  if (0x4e00 <= s && s < 0x9800) {
    return MakeAssertResult(size == 2, c,
                            "Frequent Kanji and others (74*256 characters) "
                            "are encoded into 2 bytes.");
  }
  return MakeAssertResult(size == 3, c,
                          "Other characters should be encoded into 3bytes.");
}

}  // namespace

class SystemDictionaryCodecTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SystemDictionaryCodecFactory::SetCodec(nullptr);
    ResetAllTokens();
  }

  void TearDown() override {
    SystemDictionaryCodecFactory::SetCodec(nullptr);
    ResetAllTokens();
  }

  void ResetAllTokens() {
    ClearTokens(&source_tokens_);
    ClearTokens(&decoded_tokens_);
  }

  void ClearTokens(std::vector<TokenInfo>* tokens) const {
    for (size_t i = 0; i < tokens->size(); ++i) {
      delete tokens->at(i).token;
    }
    tokens->clear();
  }

  void InitTokens(int size) {
    for (size_t i = 0; i < size; ++i) {
      Token* t = new Token();
      TokenInfo token_info(t);
      token_info.id_in_value_trie = 0;
      source_tokens_.push_back(token_info);
    }
  }

  void SetDefaultPos(TokenInfo* token_info) const {
    CHECK(token_info);
    token_info->pos_type = TokenInfo::DEFAULT_POS;
    // set id randomly
    const int id = absl::Uniform(random_, 0, 50);
    token_info->token->lid = id;
    token_info->token->rid = absl::Bernoulli(random_, 0.5) ? id : id + 1;
  }

  void SetFrequentPos(TokenInfo* token_info) const {
    CHECK(token_info);
    token_info->pos_type = TokenInfo::FREQUENT_POS;
    // set id randomly
    const int id = absl::Uniform(random_, 0, 256);
    token_info->id_in_frequent_pos_map = id;
  }

  void SetSamePos(TokenInfo* token_info) const {
    CHECK(token_info);
    token_info->pos_type = TokenInfo::SAME_AS_PREV_POS;
  }

  void SetRandPos() {
    for (size_t i = 0; i < source_tokens_.size(); ++i) {
      CHECK(source_tokens_[i].token);
      int n = absl::Uniform(random_, 0, TokenInfo::POS_TYPE_SIZE);
      CHECK_GE(n, 0);
      CHECK_LT(n, TokenInfo::POS_TYPE_SIZE);
      if (i == 0 && n == 2) {
        // First token cannot be the same pos.
        n = 0;
      }

      if (n == 0) {
        SetDefaultPos(&source_tokens_[i]);
      } else if (n == 1) {
        SetFrequentPos(&source_tokens_[i]);
      } else if (n == 2) {
        SetSamePos(&source_tokens_[i]);
      } else {
        FAIL();
      }
    }
  }

  void SetDefaultCost(TokenInfo* token_info) const {
    CHECK(token_info);
    token_info->cost_type = TokenInfo::DEFAULT_COST;
    // set cost randomly
    const int cost = absl::Uniform(random_, 0, 8000);
    token_info->token->cost = cost;
  }

  void SetSmallCost(TokenInfo* token_info) const {
    CHECK(token_info);
    token_info->cost_type = TokenInfo::CAN_USE_SMALL_ENCODING;
    // set cost randomly
    const int cost = absl::Uniform(random_, 0, 8000);
    token_info->token->cost = cost;
  }

  void SetRandCost() {
    for (size_t i = 0; i < source_tokens_.size(); ++i) {
      CHECK(source_tokens_[i].token);
      int n = absl::Uniform(random_, 0, TokenInfo::COST_TYPE_SIZE);
      CHECK_GE(n, 0);
      CHECK_LT(n, TokenInfo::POS_TYPE_SIZE);
      if (n == 0) {
        SetDefaultCost(&source_tokens_[i]);
      } else if (n == 1) {
        SetSmallCost(&source_tokens_[i]);
      }
    }
  }

  void SetDefaultValue(TokenInfo* token_info) const {
    CHECK(token_info);
    token_info->value_type = TokenInfo::DEFAULT_VALUE;
    // set id randomly
    const int id = absl::Uniform(random_, 0, 50000);
    token_info->id_in_value_trie = id;
  }

  void SetSameValue(TokenInfo* token_info) const {
    CHECK(token_info);
    token_info->value_type = TokenInfo::SAME_AS_PREV_VALUE;
  }

  void SetRandValue() {
    for (size_t i = 0; i < source_tokens_.size(); ++i) {
      CHECK(source_tokens_[i].token);
      int n = absl::Uniform(random_, 0, TokenInfo::VALUE_TYPE_SIZE);
      CHECK_GE(n, 0);
      CHECK_LT(n, TokenInfo::VALUE_TYPE_SIZE);
      if (i == 0 && n == 1) {
        // first token cannot be the same as before.
        n = 0;
      }
      if (n == 0) {
        SetDefaultValue(&source_tokens_[i]);
      } else if (n == 1) {
        SetSameValue(&source_tokens_[i]);
      } else if (n == 2) {
        source_tokens_[i].value_type = TokenInfo::AS_IS_HIRAGANA;
      } else if (n == 3) {
        source_tokens_[i].value_type = TokenInfo::AS_IS_KATAKANA;
      }
    }
  }

  void SetRandLabel() {
    for (size_t i = 0; i < source_tokens_.size(); ++i) {
      CHECK(source_tokens_[i].token);
      int n = absl::Uniform(random_, 0, Token::LABEL_SIZE);
      CHECK_GE(n, 0);
      CHECK_LT(n, Token::LABEL_SIZE);
      if (n == 0) {
        source_tokens_[i].token->attributes = Token::NONE;
      } else if (n == 1) {
        source_tokens_[i].token->attributes = Token::SPELLING_CORRECTION;
      }
    }
  }

  void CheckDecoded() const {
    EXPECT_EQ(decoded_tokens_.size(), source_tokens_.size());
    for (size_t i = 0; i < source_tokens_.size(); ++i) {
      ASSERT_NE(source_tokens_[i].token, nullptr);
      ASSERT_NE(decoded_tokens_[i].token, nullptr);

      EXPECT_EQ(source_tokens_[i].token->attributes,
                decoded_tokens_[i].token->attributes);

      EXPECT_EQ(decoded_tokens_[i].pos_type, source_tokens_[i].pos_type);
      if (source_tokens_[i].pos_type == TokenInfo::DEFAULT_POS) {
        EXPECT_EQ(decoded_tokens_[i].token->lid, source_tokens_[i].token->lid);
        EXPECT_EQ(decoded_tokens_[i].token->rid, source_tokens_[i].token->rid);
      } else if (source_tokens_[i].pos_type == TokenInfo::FREQUENT_POS) {
        EXPECT_EQ(decoded_tokens_[i].id_in_frequent_pos_map,
                  source_tokens_[i].id_in_frequent_pos_map);
      }

      if (source_tokens_[i].cost_type == TokenInfo::DEFAULT_COST) {
        EXPECT_EQ(decoded_tokens_[i].token->cost,
                  source_tokens_[i].token->cost);
      } else {  // small cost
        EXPECT_NEAR(source_tokens_[i].token->cost,
                    decoded_tokens_[i].token->cost, 256);
      }

      EXPECT_EQ(decoded_tokens_[i].value_type, source_tokens_[i].value_type);
      if (source_tokens_[i].value_type == TokenInfo::DEFAULT_VALUE) {
        EXPECT_EQ(decoded_tokens_[i].id_in_value_trie,
                  source_tokens_[i].id_in_value_trie);
      }
    }
  }

  std::vector<TokenInfo> source_tokens_;
  std::vector<TokenInfo> decoded_tokens_;
  mutable mozc::Random random_;
};

class SystemDictionaryCodecMock : public SystemDictionaryCodecInterface {
 public:
  std::string GetSectionNameForKey() const override { return "Mock"; }
  std::string GetSectionNameForValue() const override { return "Mock"; }
  std::string GetSectionNameForTokens() const override { return "Mock"; }
  std::string GetSectionNameForPos() const override { return "Mock"; }
  void EncodeKey(const absl::string_view src, std::string* dst) const override {
  }
  void DecodeKey(const absl::string_view src, std::string* dst) const override {
  }
  size_t GetEncodedKeyLength(const absl::string_view src) const override {
    return 0;
  }
  size_t GetDecodedKeyLength(const absl::string_view src) const override {
    return 0;
  }
  void EncodeValue(const absl::string_view src,
                   std::string* dst) const override {}
  void DecodeValue(const absl::string_view src,
                   std::string* dst) const override {}
  void EncodeTokens(absl::Span<const TokenInfo> tokens,
                    std::string* output) const override {}
  void DecodeTokens(const uint8_t* ptr,
                    std::vector<TokenInfo>* tokens) const override {}
  bool DecodeToken(const uint8_t* ptr, TokenInfo* token_info,
                   int* read_bytes) const override {
    *read_bytes = 0;
    return false;
  }
  bool ReadTokenForReverseLookup(const uint8_t* ptr, int* value_id,
                                 int* read_bytes) const override {
    return false;
  }
  uint8_t GetTokensTerminationFlag() const override { return 0xff; }
};

TEST_F(SystemDictionaryCodecTest, FactoryTest) {
  std::unique_ptr<SystemDictionaryCodecMock> mock =
      std::make_unique<SystemDictionaryCodecMock>();
  SystemDictionaryCodecFactory::SetCodec(mock.get());
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  EXPECT_EQ(codec->GetSectionNameForKey(), "Mock");
}

TEST_F(SystemDictionaryCodecTest, KeyCodecKanaTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  const std::string original = "よみ";
  std::string encoded;
  codec->EncodeKey(original, &encoded);
  // hiragana should be encoded in 1 byte
  EXPECT_EQ(encoded.size(), 2);
  EXPECT_EQ(codec->GetEncodedKeyLength(original), encoded.size());
  std::string decoded;
  codec->DecodeKey(encoded, &decoded);
  EXPECT_EQ(decoded, original);
  EXPECT_EQ(codec->GetDecodedKeyLength(encoded), decoded.size());
}

TEST_F(SystemDictionaryCodecTest, KeyCodecSymbolTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  const std::string original = "・ー";
  std::string encoded;
  codec->EncodeKey(original, &encoded);
  // middle dot and prolonged sound should be encoded in 1 byte
  EXPECT_EQ(encoded.size(), 2);
  EXPECT_EQ(codec->GetEncodedKeyLength(original), encoded.size());
  std::string decoded;
  codec->DecodeKey(encoded, &decoded);
  EXPECT_EQ(decoded, original);
  EXPECT_EQ(codec->GetDecodedKeyLength(encoded), decoded.size());
}

TEST_F(SystemDictionaryCodecTest, ValueCodecTest) {
  std::unique_ptr<SystemDictionaryCodec> codec =
      std::make_unique<SystemDictionaryCodec>();
  // TODO(toshiyuki): Use 0x10ffff instead when UCS4 is supported.
  constexpr char32_t kMaxUniChar = 0x10ffff;
  for (char32_t c = 0x01; c <= kMaxUniChar; ++c) {
    const std::string original = Util::CodepointToUtf8(c);
    std::string encoded;
    codec->EncodeValue(original, &encoded);
    EXPECT_TRUE(IsExpectedEncodedSize(c, encoded));
    std::string decoded;
    codec->DecodeValue(encoded, &decoded);
    EXPECT_EQ(decoded, original) << "failed at: " << static_cast<uint32_t>(c);
  }
}

TEST_F(SystemDictionaryCodecTest, ValueCodecKanaTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  const std::string original = "もジ";
  std::string encoded;
  codec->EncodeValue(original, &encoded);
  // kana should be encoded in 1 byte
  EXPECT_EQ(encoded.size(), 2);
  std::string decoded;
  codec->DecodeValue(encoded, &decoded);
  EXPECT_EQ(decoded, original);
}

TEST_F(SystemDictionaryCodecTest, ValueCodecAsciiTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  const std::string original = "word";
  std::string encoded;
  codec->EncodeValue(original, &encoded);
  // ascii should be encoded in 2 bytes
  EXPECT_EQ(encoded.size(), 8);
  std::string decoded;
  codec->DecodeValue(encoded, &decoded);
  EXPECT_EQ(decoded, original);
}

TEST_F(SystemDictionaryCodecTest, TokenDefaultPosTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(1);
  SetDefaultPos(&source_tokens_[0]);
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenFrequentPosTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(1);
  SetFrequentPos(&source_tokens_[0]);
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenSamePosTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  {
    InitTokens(2);
    SetDefaultPos(&source_tokens_[0]);
    SetSamePos(&source_tokens_[1]);
    std::string encoded;
    codec->EncodeTokens(source_tokens_, &encoded);
    EXPECT_GT(encoded.size(), 0);
    codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                        &decoded_tokens_);
    CheckDecoded();
  }
  ResetAllTokens();
  {
    InitTokens(2);
    SetFrequentPos(&source_tokens_[0]);
    SetSamePos(&source_tokens_[1]);
    std::string encoded;
    codec->EncodeTokens(source_tokens_, &encoded);
    EXPECT_GT(encoded.size(), 0);
    codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                        &decoded_tokens_);
    CheckDecoded();
  }
}

TEST_F(SystemDictionaryCodecTest, TokenRandomPosTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  SetRandPos();
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenDefaultCostTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(1);
  SetDefaultCost(&source_tokens_[0]);
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenSmallCostTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(1);
  SetSmallCost(&source_tokens_[0]);
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenRandomCostTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  SetRandCost();
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenDefaultValueTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(1);
  SetDefaultValue(&source_tokens_[0]);
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, UCS4CharactersTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  const std::string codepoint_including =
      // "𠀋𡈽𡌛𡑮𡢽𠮟𡚴𡸴𣇄𣗄𣜿𣝣𣳾𤟱𥒎𥔎𥝱𥧄𥶡𦫿𦹀𧃴𧚄𨉷𨏍𪆐𠂉"
      "\xf0\xa0\x80\x8b\xf0\xa1\x88\xbd\xf0\xa1\x8c\x9b\xf0\xa1\x91\xae\xf0"
      "\xa1\xa2\xbd\xf0\xa0\xae\x9f\xf0\xa1\x9a\xb4\xf0\xa1\xb8\xb4\xf0\xa3"
      "\x87\x84\xf0\xa3\x97\x84\xf0\xa3\x9c\xbf\xf0\xa3\x9d\xa3\xf0\xa3\xb3"
      "\xbe\xf0\xa4\x9f\xb1\xf0\xa5\x92\x8e\xf0\xa5\x94\x8e\xf0\xa5\x9d\xb1"
      "\xf0\xa5\xa7\x84\xf0\xa5\xb6\xa1\xf0\xa6\xab\xbf\xf0\xa6\xb9\x80\xf0"
      "\xa7\x83\xb4\xf0\xa7\x9a\x84\xf0\xa8\x89\xb7\xf0\xa8\x8f\x8d\xf0\xaa"
      "\x86\x90\xf0\xa0\x82\x89"
      // "𠂢𠂤𠆢𠈓𠌫𠎁𠍱𠏹𠑊𠔉𠗖𠘨𠝏𠠇𠠺𠢹𠥼𠦝𠫓𠬝𠵅𠷡𠺕𠹭𠹤𠽟𡈁"
      "\xf0\xa0\x82\xa2\xf0\xa0\x82\xa4\xf0\xa0\x86\xa2\xf0\xa0\x88\x93\xf0"
      "\xa0\x8c\xab\xf0\xa0\x8e\x81\xf0\xa0\x8d\xb1\xf0\xa0\x8f\xb9\xf0\xa0"
      "\x91\x8a\xf0\xa0\x94\x89\xf0\xa0\x97\x96\xf0\xa0\x98\xa8\xf0\xa0\x9d"
      "\x8f\xf0\xa0\xa0\x87\xf0\xa0\xa0\xba\xf0\xa0\xa2\xb9\xf0\xa0\xa5\xbc"
      "\xf0\xa0\xa6\x9d\xf0\xa0\xab\x93\xf0\xa0\xac\x9d\xf0\xa0\xb5\x85\xf0"
      "\xa0\xb7\xa1\xf0\xa0\xba\x95\xf0\xa0\xb9\xad\xf0\xa0\xb9\xa4\xf0\xa0"
      "\xbd\x9f\xf0\xa1\x88\x81"
      // "𡉕𡉻𡉴𡋤𡋗𡋽𡌶𡍄𡏄𡑭𡗗𦰩𡙇𡜆𡝂𡧃𡱖𡴭𡵅𡵸𡵢𡶡𡶜𡶒𡶷𡷠𡸳"
      "\xf0\xa1\x89\x95\xf0\xa1\x89\xbb\xf0\xa1\x89\xb4\xf0\xa1\x8b\xa4\xf0"
      "\xa1\x8b\x97\xf0\xa1\x8b\xbd\xf0\xa1\x8c\xb6\xf0\xa1\x8d\x84\xf0\xa1"
      "\x8f\x84\xf0\xa1\x91\xad\xf0\xa1\x97\x97\xf0\xa6\xb0\xa9\xf0\xa1\x99"
      "\x87\xf0\xa1\x9c\x86\xf0\xa1\x9d\x82\xf0\xa1\xa7\x83\xf0\xa1\xb1\x96"
      "\xf0\xa1\xb4\xad\xf0\xa1\xb5\x85\xf0\xa1\xb5\xb8\xf0\xa1\xb5\xa2\xf0"
      "\xa1\xb6\xa1\xf0\xa1\xb6\x9c\xf0\xa1\xb6\x92\xf0\xa1\xb6\xb7\xf0\xa1"
      "\xb7\xa0\xf0\xa1\xb8\xb3"
      // "𡼞𡽶𡿺𢅻𢌞𢎭𢛳𢡛𢢫𢦏𢪸𢭏𢭐𢭆𢰝𢮦𢰤𢷡𣇃𣇵𣆶𣍲𣏓𣏒𣏐𣏤𣏕"
      "\xf0\xa1\xbc\x9e\xf0\xa1\xbd\xb6\xf0\xa1\xbf\xba\xf0\xa2\x85\xbb\xf0"
      "\xa2\x8c\x9e\xf0\xa2\x8e\xad\xf0\xa2\x9b\xb3\xf0\xa2\xa1\x9b\xf0\xa2"
      "\xa2\xab\xf0\xa2\xa6\x8f\xf0\xa2\xaa\xb8\xf0\xa2\xad\x8f\xf0\xa2\xad"
      "\x90\xf0\xa2\xad\x86\xf0\xa2\xb0\x9d\xf0\xa2\xae\xa6\xf0\xa2\xb0\xa4"
      "\xf0\xa2\xb7\xa1\xf0\xa3\x87\x83\xf0\xa3\x87\xb5\xf0\xa3\x86\xb6\xf0"
      "\xa3\x8d\xb2\xf0\xa3\x8f\x93\xf0\xa3\x8f\x92\xf0\xa3\x8f\x90\xf0\xa3\x8f"
      "\xa4\xf0\xa3\x8f\x95"
      // "𣏚𣏟𣑊𣑑𣑋𣑥𣓤𣕚𣖔𣘹𣙇𣘸𣘺𣜜𣜌𣝤𣟿𣟧𣠤𣠽𣪘𣱿𣴀𣵀𣷺𣷹𣷓"
      "\xf0\xa3\x8f\x9a\xf0\xa3\x8f\x9f\xf0\xa3\x91\x8a\xf0\xa3\x91\x91\xf0"
      "\xa3\x91\x8b\xf0\xa3\x91\xa5\xf0\xa3\x93\xa4\xf0\xa3\x95\x9a\xf0\xa3"
      "\x96\x94\xf0\xa3\x98\xb9\xf0\xa3\x99\x87\xf0\xa3\x98\xb8\xf0\xa3\x98"
      "\xba\xf0\xa3\x9c\x9c\xf0\xa3\x9c\x8c\xf0\xa3\x9d\xa4\xf0\xa3\x9f\xbf"
      "\xf0\xa3\x9f\xa7\xf0\xa3\xa0\xa4\xf0\xa3\xa0\xbd\xf0\xa3\xaa\x98\xf0"
      "\xa3\xb1\xbf\xf0\xa3\xb4\x80\xf0\xa3\xb5\x80\xf0\xa3\xb7\xba\xf0\xa3"
      "\xb7\xb9\xf0\xa3\xb7\x93"
      // "𣽾𤂖𤄃𤇆𤇾𤎼𤘩𤚥𤢖𤩍𤭖𤭯𤰖𤴔𤸎𤸷𤹪𤺋𥁊𥁕𥄢𥆩𥇥𥇍𥈞𥉌𥐮"
      "\xf0\xa3\xbd\xbe\xf0\xa4\x82\x96\xf0\xa4\x84\x83\xf0\xa4\x87\x86\xf0"
      "\xa4\x87\xbe\xf0\xa4\x8e\xbc\xf0\xa4\x98\xa9\xf0\xa4\x9a\xa5\xf0\xa4"
      "\xa2\x96\xf0\xa4\xa9\x8d\xf0\xa4\xad\x96\xf0\xa4\xad\xaf\xf0\xa4\xb0"
      "\x96\xf0\xa4\xb4\x94\xf0\xa4\xb8\x8e\xf0\xa4\xb8\xb7\xf0\xa4\xb9\xaa"
      "\xf0\xa4\xba\x8b\xf0\xa5\x81\x8a\xf0\xa5\x81\x95\xf0\xa5\x84\xa2\xf0"
      "\xa5\x86\xa9\xf0\xa5\x87\xa5\xf0\xa5\x87\x8d\xf0\xa5\x88\x9e\xf0\xa5"
      "\x89\x8c\xf0\xa5\x90\xae"
      // "𥓙𥖧𥞩𥞴𥧔𥫤𥫣𥫱𥮲𥱋𥱤𥸮𥹖𥹥𥹢𥻘𥻂𥻨𥼣𥽜𥿠𥿔𦀌𥿻𦀗𦁠𦃭"
      "\xf0\xa5\x93\x99\xf0\xa5\x96\xa7\xf0\xa5\x9e\xa9\xf0\xa5\x9e\xb4\xf0"
      "\xa5\xa7\x94\xf0\xa5\xab\xa4\xf0\xa5\xab\xa3\xf0\xa5\xab\xb1\xf0\xa5"
      "\xae\xb2\xf0\xa5\xb1\x8b\xf0\xa5\xb1\xa4\xf0\xa5\xb8\xae\xf0\xa5\xb9"
      "\x96\xf0\xa5\xb9\xa5\xf0\xa5\xb9\xa2\xf0\xa5\xbb\x98\xf0\xa5\xbb\x82"
      "\xf0\xa5\xbb\xa8\xf0\xa5\xbc\xa3\xf0\xa5\xbd\x9c\xf0\xa5\xbf\xa0\xf0"
      "\xa5\xbf\x94\xf0\xa6\x80\x8c\xf0\xa5\xbf\xbb\xf0\xa6\x80\x97\xf0\xa6"
      "\x81\xa0\xf0\xa6\x83\xad"
      // "𦉰𦊆𦍌𣴎𦐂𦙾𦚰𦜝𦣝𦣪𦥑𦥯𦧝𦨞𦩘𦪌𦪷𦱳𦳝𦹥𦾔𦿸𦿶𦿷𧄍𧄹𧏛"
      "\xf0\xa6\x89\xb0\xf0\xa6\x8a\x86\xf0\xa6\x8d\x8c\xf0\xa3\xb4\x8e\xf0"
      "\xa6\x90\x82\xf0\xa6\x99\xbe\xf0\xa6\x9a\xb0\xf0\xa6\x9c\x9d\xf0\xa6"
      "\xa3\x9d\xf0\xa6\xa3\xaa\xf0\xa6\xa5\x91\xf0\xa6\xa5\xaf\xf0\xa6\xa7"
      "\x9d\xf0\xa6\xa8\x9e\xf0\xa6\xa9\x98\xf0\xa6\xaa\x8c\xf0\xa6\xaa\xb7"
      "\xf0\xa6\xb1\xb3\xf0\xa6\xb3\x9d\xf0\xa6\xb9\xa5\xf0\xa6\xbe\x94\xf0"
      "\xa6\xbf\xb8\xf0\xa6\xbf\xb6\xf0\xa6\xbf\xb7\xf0\xa7\x84\x8d\xf0\xa7"
      "\x84\xb9\xf0\xa7\x8f\x9b"
      // "𧏚𧏾𧐐𧑉𧘕𧘔𧘱𧚓𧜎𧜣𧝒𧦅𧪄𧮳𧮾𧯇𧲸𧶠𧸐𧾷𨂊𨂻𨊂𨋳𨐌𨑕𨕫"
      "\xf0\xa7\x8f\x9a\xf0\xa7\x8f\xbe\xf0\xa7\x90\x90\xf0\xa7\x91\x89\xf0"
      "\xa7\x98\x95\xf0\xa7\x98\x94\xf0\xa7\x98\xb1\xf0\xa7\x9a\x93\xf0\xa7"
      "\x9c\x8e\xf0\xa7\x9c\xa3\xf0\xa7\x9d\x92\xf0\xa7\xa6\x85\xf0\xa7\xaa"
      "\x84\xf0\xa7\xae\xb3\xf0\xa7\xae\xbe\xf0\xa7\xaf\x87\xf0\xa7\xb2\xb8"
      "\xf0\xa7\xb6\xa0\xf0\xa7\xb8\x90\xf0\xa7\xbe\xb7\xf0\xa8\x82\x8a\xf0"
      "\xa8\x82\xbb\xf0\xa8\x8a\x82\xf0\xa8\x8b\xb3\xf0\xa8\x90\x8c\xf0\xa8"
      "\x91\x95\xf0\xa8\x95\xab"
      // "𨗈𨗉𨛗𨛺𨥉𨥆𨥫𨦇𨦈𨦺𨦻𨨞𨨩𨩱𨩃𨪙𨫍𨫤𨫝𨯁𨯯𨴐𨵱𨷻𨸟𨸶𨺉"
      "\xf0\xa8\x97\x88\xf0\xa8\x97\x89\xf0\xa8\x9b\x97\xf0\xa8\x9b\xba\xf0"
      "\xa8\xa5\x89\xf0\xa8\xa5\x86\xf0\xa8\xa5\xab\xf0\xa8\xa6\x87\xf0\xa8"
      "\xa6\x88\xf0\xa8\xa6\xba\xf0\xa8\xa6\xbb\xf0\xa8\xa8\x9e\xf0\xa8\xa8"
      "\xa9\xf0\xa8\xa9\xb1\xf0\xa8\xa9\x83\xf0\xa8\xaa\x99\xf0\xa8\xab\x8d"
      "\xf0\xa8\xab\xa4\xf0\xa8\xab\x9d\xf0\xa8\xaf\x81\xf0\xa8\xaf\xaf\xf0\xa8"
      "\xb4\x90\xf0\xa8\xb5\xb1\xf0\xa8\xb7\xbb\xf0\xa8\xb8\x9f\xf0\xa8\xb8"
      "\xb6\xf0\xa8\xba\x89"
      // "𨻫𨼲𨿸𩊠𩊱𩒐𩗏𩙿𩛰𩜙𩝐𩣆𩩲𩷛𩸽𩸕𩺊𩹉𩻄𩻩𩻛𩿎𪀯𪀚𪃹𪂂𢈘"
      "\xf0\xa8\xbb\xab\xf0\xa8\xbc\xb2\xf0\xa8\xbf\xb8\xf0\xa9\x8a\xa0\xf0"
      "\xa9\x8a\xb1\xf0\xa9\x92\x90\xf0\xa9\x97\x8f\xf0\xa9\x99\xbf\xf0\xa9"
      "\x9b\xb0\xf0\xa9\x9c\x99\xf0\xa9\x9d\x90\xf0\xa9\xa3\x86\xf0\xa9\xa9"
      "\xb2\xf0\xa9\xb7\x9b\xf0\xa9\xb8\xbd\xf0\xa9\xb8\x95\xf0\xa9\xba\x8a"
      "\xf0\xa9\xb9\x89\xf0\xa9\xbb\x84\xf0\xa9\xbb\xa9\xf0\xa9\xbb\x9b\xf0"
      "\xa9\xbf\x8e\xf0\xaa\x80\xaf\xf0\xaa\x80\x9a\xf0\xaa\x83\xb9\xf0\xaa"
      "\x82\x82\xf0\xa2\x88\x98"
      // "𪎌𪐷𪗱𪘂𪘚𪚲"
      "\xf0\xaa\x8e\x8c\xf0\xaa\x90\xb7\xf0\xaa\x97\xb1\xf0\xaa\x98\x82\xf0"
      "\xaa\x98\x9a\xf0\xaa\x9a\xb2";
  std::string encoded;
  codec->EncodeValue(codepoint_including, &encoded);
  EXPECT_GT(encoded.size(), 0);
  std::string decoded;
  codec->DecodeValue(encoded, &decoded);
  EXPECT_EQ(decoded, codepoint_including);
}

TEST_F(SystemDictionaryCodecTest, TokenSameValueTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(2);
  SetDefaultValue(&source_tokens_[0]);
  SetSameValue(&source_tokens_[1]);
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenRandomValueTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  SetRandValue();
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenRandomLabelTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  SetRandLabel();
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenRandomTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  SetRandPos();
  SetRandCost();
  SetRandValue();
  SetRandLabel();
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, ReadTokenRandomTest) {
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  SetRandPos();
  SetRandCost();
  SetRandValue();
  SetRandLabel();
  std::string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  int read_num = 0;
  int offset = 0;
  while (true) {
    int read_byte = 0;
    int value_id = -1;
    const bool is_last_token = !(codec->ReadTokenForReverseLookup(
        reinterpret_cast<const unsigned char*>(encoded.data()) + offset,
        &value_id, &read_byte));
    if (source_tokens_[read_num].value_type == TokenInfo::DEFAULT_VALUE) {
      EXPECT_EQ(value_id, source_tokens_[read_num].id_in_value_trie);
    } else {
      EXPECT_EQ(value_id, -1);
    }
    offset += read_byte;
    ++read_num;
    if (is_last_token) {
      break;
    }
  }
  EXPECT_EQ(read_num, source_tokens_.size());
}

TEST_F(SystemDictionaryCodecTest, CodecTest) {
  std::unique_ptr<SystemDictionaryCodec> impl =
      std::make_unique<SystemDictionaryCodec>();
  SystemDictionaryCodecFactory::SetCodec(impl.get());
  SystemDictionaryCodecInterface* codec =
      SystemDictionaryCodecFactory::GetCodec();
  {  // Token
    InitTokens(50);
    SetRandPos();
    SetRandCost();
    SetRandValue();
    SetRandLabel();
    std::string encoded;
    codec->EncodeTokens(source_tokens_, &encoded);
    EXPECT_GT(encoded.size(), 0);
    codec->DecodeTokens(reinterpret_cast<const unsigned char*>(encoded.data()),
                        &decoded_tokens_);
    CheckDecoded();

    // ReadTokens
    int read_num = 0;
    int offset = 0;
    while (true) {
      int read_byte = 0;
      int value_id = -1;
      const bool is_last_token = !(codec->ReadTokenForReverseLookup(
          reinterpret_cast<const unsigned char*>(encoded.data()) + offset,
          &value_id, &read_byte));
      if (source_tokens_[read_num].value_type == TokenInfo::DEFAULT_VALUE) {
        EXPECT_EQ(value_id, source_tokens_[read_num].id_in_value_trie);
      } else {
        EXPECT_EQ(value_id, -1);
      }
      offset += read_byte;
      ++read_num;
      if (is_last_token) {
        break;
      }
    }
    EXPECT_EQ(read_num, source_tokens_.size());
  }
  {
    // Value
    // U+4E00-9FFF CJK Unified Ideographs
    constexpr char32_t a_codepoint = '!';
    const std::string original =
        random_.Utf8String(10000, a_codepoint, a_codepoint + 0x9f00);
    std::string encoded;
    codec->EncodeValue(original, &encoded);
    std::string decoded;
    codec->DecodeValue(encoded, &decoded);
    EXPECT_EQ(decoded, original);
  }
  {
    // Key
    constexpr char32_t a_codepoint = 0x3041;  // "ぁ"
    const std::string original =
        random_.Utf8String(1000, a_codepoint, a_codepoint + 1000);
    std::string encoded;
    codec->EncodeKey(original, &encoded);
    EXPECT_EQ(codec->GetEncodedKeyLength(original), encoded.size());
    std::string decoded;
    codec->DecodeKey(encoded, &decoded);
    EXPECT_EQ(decoded, original);
    EXPECT_EQ(codec->GetDecodedKeyLength(encoded), decoded.size());
  }
}

}  // namespace dictionary
}  // namespace mozc

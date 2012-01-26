// Copyright 2010-2012, Google Inc.
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

#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/system/codec.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace dictionary {
class SystemDictionaryCodecTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SystemDictionaryCodecFactory::SetCodec(NULL);
    ResetAllTokens();
  }

  virtual void TearDown() {
    SystemDictionaryCodecFactory::SetCodec(NULL);
    ResetAllTokens();
  }

  void ResetAllTokens() {
    ClearTokens(&source_tokens_);
    ClearTokens(&decoded_tokens_);
  }

  void ClearTokens(vector<TokenInfo> *tokens) const {
    for (size_t i = 0; i < tokens->size(); ++i) {
      delete tokens->at(i).token;
    }
    tokens->clear();
  }

  void InitTokens(int size) {
    for (size_t i = 0; i < size; ++i) {
      Token *t = new Token();
      TokenInfo token_info(t);
      token_info.id_in_value_trie = 0;
      source_tokens_.push_back(token_info);
    }
  }

  void SetDefaultPos(TokenInfo *token_info) const {
    CHECK(token_info);
    token_info->pos_type = TokenInfo::DEFAULT_POS;
    // set id randomly
    const int id = static_cast<int>(1.0 * 50 * rand() /
                                    (RAND_MAX + 1.0));
    token_info->token->lid = id;
    token_info->token->rid = ((static_cast<int>(rand()) % 2 == 0) ?
                              id : id + 1);
  }

  void SetFrequentPos(TokenInfo *token_info) const {
    CHECK(token_info);
    token_info->pos_type = TokenInfo::FREQUENT_POS;
    // set id randomly
    const int id = static_cast<int>(1.0 * 256 * rand() /
                                    (RAND_MAX + 1.0));
    token_info->id_in_frequent_pos_map = id;
  }

  void SetSamePos(TokenInfo *token_info) const {
    CHECK(token_info);
    token_info->pos_type = TokenInfo::SAME_AS_PREV_POS;
  }

  void SetRandPos() {
    for (size_t i = 0; i < source_tokens_.size(); ++i) {
      CHECK(source_tokens_[i].token);
      int n = static_cast<int>(1.0 * TokenInfo::POS_TYPE_SIZE * rand() /
                               (RAND_MAX + 1.0));
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

  void SetDefaultCost(TokenInfo *token_info) const {
    CHECK(token_info);
    token_info->cost_type = TokenInfo::DEFAULT_COST;
    // set cost randomly
    const int cost = static_cast<int>(1.0 * 8000 * rand() /
                                      (RAND_MAX + 1.0));
    token_info->token->cost = cost;
  }

  void SetSmallCost(TokenInfo *token_info) const {
    CHECK(token_info);
    token_info->cost_type = TokenInfo::CAN_USE_SMALL_ENCODING;
    // set cost randomly
    const int cost = static_cast<int>(1.0 * 8000 * rand() /
                                      (RAND_MAX + 1.0));
    token_info->token->cost = cost;
  }

  void SetRandCost() {
    for (size_t i = 0; i < source_tokens_.size(); ++i) {
      CHECK(source_tokens_[i].token);
      int n = static_cast<int>(1.0 * TokenInfo::COST_TYPE_SIZE * rand() /
                               (RAND_MAX + 1.0));
      CHECK_GE(n, 0);
      CHECK_LT(n, TokenInfo::POS_TYPE_SIZE);
      if (n == 0) {
        SetDefaultCost(&source_tokens_[i]);
      } else if (n == 1) {
        SetSmallCost(&source_tokens_[i]);
      }
    }
  }

  void SetDefaultValue(TokenInfo *token_info) const {
    CHECK(token_info);
    token_info->value_type = TokenInfo::DEFAULT_VALUE;
    // set id randomly
    const int id = static_cast<int>(1.0 * 50000 * rand() /
                                    (RAND_MAX + 1.0));
    token_info->id_in_value_trie = id;
  }

  void SetSameValue(TokenInfo *token_info) const {
    CHECK(token_info);
    token_info->value_type = TokenInfo::SAME_AS_PREV_VALUE;
  }

  void SetRandValue() {
    for (size_t i = 0; i < source_tokens_.size(); ++i) {
      CHECK(source_tokens_[i].token);
      int n = static_cast<int>(1.0 * TokenInfo::VALUE_TYPE_SIZE * rand() /
                               (RAND_MAX + 1.0));
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
      int n = static_cast<int>(1.0 * Token::LABEL_SIZE * rand() /
                               (RAND_MAX + 1.0));
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
    EXPECT_EQ(source_tokens_.size(), decoded_tokens_.size());
    for (size_t i = 0; i < source_tokens_.size(); ++i) {
      EXPECT_TRUE(source_tokens_[i].token != NULL);
      EXPECT_TRUE(decoded_tokens_[i].token != NULL);

      EXPECT_EQ(source_tokens_[i].token->attributes,
                decoded_tokens_[i].token->attributes);

      EXPECT_EQ(source_tokens_[i].pos_type, decoded_tokens_[i].pos_type);
      if (source_tokens_[i].pos_type == TokenInfo::DEFAULT_POS) {
        EXPECT_EQ(source_tokens_[i].token->lid, decoded_tokens_[i].token->lid);
        EXPECT_EQ(source_tokens_[i].token->rid, decoded_tokens_[i].token->rid);
      } else if (source_tokens_[i].pos_type == TokenInfo::FREQUENT_POS) {
        EXPECT_EQ(source_tokens_[i].id_in_frequent_pos_map,
                  decoded_tokens_[i].id_in_frequent_pos_map);
      }

      if (source_tokens_[i].cost_type == TokenInfo::DEFAULT_COST) {
        EXPECT_EQ(source_tokens_[i].token->cost,
                  decoded_tokens_[i].token->cost);
      } else {  // small cost
        EXPECT_NEAR(source_tokens_[i].token->cost,
                    decoded_tokens_[i].token->cost,
                    256);
      }

      EXPECT_EQ(source_tokens_[i].value_type, decoded_tokens_[i].value_type);
      if (source_tokens_[i].value_type == TokenInfo::DEFAULT_VALUE) {
        EXPECT_EQ(source_tokens_[i].id_in_value_trie,
                  decoded_tokens_[i].id_in_value_trie);
      }
    }
  }

  vector<TokenInfo> source_tokens_;
  vector<TokenInfo> decoded_tokens_;
};

class SystemDictionaryCodecMock : public SystemDictionaryCodecInterface {
 public:
  const string GetSectionNameForKey() const { return "Mock"; }
  const string GetSectionNameForValue() const { return "Mock"; }
  const string GetSectionNameForTokens() const { return "Mock"; }
  const string GetSectionNameForPos() const { return "Mock"; }
  void EncodeKey(const string &src, string *dst) const {}
  void DecodeKey(const string &src, string *dst) const {}
  void EncodeValue(const string &src, string *dst) const {}
  void DecodeValue(const string &src, string *dst) const {}
  void EncodeTokens(
      const vector<TokenInfo> &tokens, string *output) const {}
  void DecodeTokens(
      const uint8 *ptr, vector<TokenInfo> *tokens) const {}
  bool ReadTokenForReverseLookup(
      const uint8 *ptr, int *value_id, int *read_bytes) const { return false; }
  uint8 GetTokensTerminationFlag() const { return 0xff; }
};

TEST_F(SystemDictionaryCodecTest, FactoryTest) {
  scoped_ptr<SystemDictionaryCodecMock> mock(new SystemDictionaryCodecMock);
  SystemDictionaryCodecFactory::SetCodec(mock.get());
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  EXPECT_EQ("Mock", codec->GetSectionNameForKey());
}

TEST_F(SystemDictionaryCodecTest, KeyCodecKanaTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  const string original = "よみ";
  string encoded;
  codec->EncodeKey(original, &encoded);
  // hiragana should be encoded in 1 byte
  EXPECT_EQ(2, encoded.size());
  string decoded;
  codec->DecodeKey(encoded, &decoded);
  EXPECT_EQ(original, decoded);
}

TEST_F(SystemDictionaryCodecTest, KeyCodecKanaLongTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  string original;
  {
    const uint16 start_ucs2 = 0x3041;  // "ぁ"
    const uint16 end_ucs2 = 0x30fe;  // "ヾ"
    for (uint16 c = start_ucs2; c <= end_ucs2; ++c) {
      Util::UCS2ToUTF8Append(c, &original);
    }
  }
  string encoded;
  codec->EncodeKey(original, &encoded);
  string decoded;
  codec->DecodeKey(encoded, &decoded);
  EXPECT_EQ(original, decoded);
}

TEST_F(SystemDictionaryCodecTest, KeyCodecSymbolTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  const string original = "・ー";
  string encoded;
  codec->EncodeKey(original, &encoded);
  // middle dot and prolonged sound should be encoded in 1 byte
  EXPECT_EQ(2, encoded.size());
  string decoded;
  codec->DecodeKey(encoded, &decoded);
  EXPECT_EQ(original, decoded);
}

TEST_F(SystemDictionaryCodecTest, KeyCodecRandomTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  string original;
  {
    const uint16 a_ucs2 = 0x3041;  // "ぁ"
    srand(0);
    for (size_t i = 0; i < 1000; ++i) {
      const uint16 c = a_ucs2 + static_cast<uint16>(
          1.0 * 1000 * rand() / (RAND_MAX + 1.0));
      Util::UCS2ToUTF8Append(c, &original);
    }
  }
  string encoded;
  codec->EncodeKey(original, &encoded);
  string decoded;
  codec->DecodeKey(encoded, &decoded);
  EXPECT_EQ(original, decoded);
}

TEST_F(SystemDictionaryCodecTest, ValueCodecKanaTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  const string original = "もジ";
  string encoded;
  codec->EncodeValue(original, &encoded);
  // kana should be encoded in 1 byte
  EXPECT_EQ(2, encoded.size());
  string decoded;
  codec->DecodeValue(encoded, &decoded);
  EXPECT_EQ(original, decoded);
}

TEST_F(SystemDictionaryCodecTest, ValueCodecKanaLongTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  string original;
  {
    const uint16 start_ucs2 = 0x3041;  // "ぁ"
    const uint16 end_ucs2 = 0x30fe;  // "ヾ"
    for (uint16 c = start_ucs2; c <= end_ucs2; ++c) {
      Util::UCS2ToUTF8Append(c, &original);
    }
  }
  string encoded;
  codec->EncodeValue(original, &encoded);
  string decoded;
  codec->DecodeValue(encoded, &decoded);
  EXPECT_EQ(original, decoded);
}

TEST_F(SystemDictionaryCodecTest, ValueCodecAsciiTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  const string original = "word";
  string encoded;
  codec->EncodeValue(original, &encoded);
  // ascii should be encoded in 2 bytes
  EXPECT_EQ(8, encoded.size());
  string decoded;
  codec->DecodeValue(encoded, &decoded);
  EXPECT_EQ(original, decoded);
}

TEST_F(SystemDictionaryCodecTest, ValueCodecAsciiLongTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  string original;
  {
    const uint16 start_ucs2 = '!';
    const uint16 end_ucs2 = '~';
    for (uint16 c = start_ucs2; c <= end_ucs2; ++c) {
      Util::UCS2ToUTF8Append(c, &original);
    }
  }
  string encoded;
  codec->EncodeValue(original, &encoded);
  string decoded;
  codec->DecodeValue(encoded, &decoded);
  EXPECT_EQ(original, decoded);
}

TEST_F(SystemDictionaryCodecTest, ValueCodecRandomTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  string original;
  {
    const uint16 a_ucs2 = '!';
    srand(0);
    for (size_t i = 0; i < 10000; ++i) {
      // U+4E00-9FFF CJK Unified Ideographs
      // 0x52: 0xa0 - 0x4e
      const uint16 c = a_ucs2 + static_cast<uint16>(
          (0x4e + rand() % 0x52) * 0x100 + rand() % 0x100);
      Util::UCS2ToUTF8Append(c, &original);
    }
  }
  string encoded;
  codec->EncodeValue(original, &encoded);
  string decoded;
  codec->DecodeValue(encoded, &decoded);
  EXPECT_EQ(original, decoded);
}

TEST_F(SystemDictionaryCodecTest, TokenDefaultPosTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(1);
  SetDefaultPos(&source_tokens_[0]);
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenFrequentPosTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(1);
  SetFrequentPos(&source_tokens_[0]);
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenSamePosTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  {
    InitTokens(2);
    SetDefaultPos(&source_tokens_[0]);
    SetSamePos(&source_tokens_[1]);
    string encoded;
    codec->EncodeTokens(source_tokens_, &encoded);
    EXPECT_GT(encoded.size(), 0);
    codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                        &decoded_tokens_);
    CheckDecoded();
  }
  ResetAllTokens();
  {
    InitTokens(2);
    SetFrequentPos(&source_tokens_[0]);
    SetSamePos(&source_tokens_[1]);
    string encoded;
    codec->EncodeTokens(source_tokens_, &encoded);
    EXPECT_GT(encoded.size(), 0);
    codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                        &decoded_tokens_);
    CheckDecoded();
  }
}

TEST_F(SystemDictionaryCodecTest, TokenRandomPosTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  srand(0);
  SetRandPos();
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenDefaultCostTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(1);
  SetDefaultCost(&source_tokens_[0]);
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenSmallCostTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(1);
  SetSmallCost(&source_tokens_[0]);
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenRandomCostTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  srand(0);
  SetRandCost();
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenDefaultValueTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(1);
  SetDefaultValue(&source_tokens_[0]);
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenSameValueTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(2);
  SetDefaultValue(&source_tokens_[0]);
  SetSameValue(&source_tokens_[1]);
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenRandomValueTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  srand(0);
  SetRandValue();
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenRandomLabelTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  srand(0);
  SetRandLabel();
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, TokenRandomTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  srand(0);
  SetRandPos();
  SetRandCost();
  SetRandValue();
  SetRandLabel();
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                      &decoded_tokens_);
  CheckDecoded();
}

TEST_F(SystemDictionaryCodecTest, ReadTokenRandomTest) {
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  InitTokens(50);
  srand(0);
  SetRandPos();
  SetRandCost();
  SetRandValue();
  SetRandLabel();
  string encoded;
  codec->EncodeTokens(source_tokens_, &encoded);
  EXPECT_GT(encoded.size(), 0);
  int read_num = 0;
  int offset = 0;
  while (true) {
    int read_byte = 0;
    int value_id = -1;
    const bool is_last_token = !(codec->ReadTokenForReverseLookup(
        reinterpret_cast<const unsigned char *>(encoded.data()) + offset,
        &value_id,
        &read_byte));
    if (source_tokens_[read_num].value_type == TokenInfo::DEFAULT_VALUE) {
      EXPECT_EQ(source_tokens_[read_num].id_in_value_trie, value_id);
    } else {
      EXPECT_EQ(-1, value_id);
    }
    offset += read_byte;
    ++read_num;
    if (is_last_token) {
      break;
    }
  }
  EXPECT_EQ(source_tokens_.size(), read_num);
}

TEST_F(SystemDictionaryCodecTest, CodecTest) {
  scoped_ptr<SystemDictionaryCodec> impl(new SystemDictionaryCodec);
  SystemDictionaryCodecFactory::SetCodec(impl.get());
  SystemDictionaryCodecInterface *codec =
      SystemDictionaryCodecFactory::GetCodec();
  {  // Token
    InitTokens(50);
    srand(0);
    SetRandPos();
    SetRandCost();
    SetRandValue();
    SetRandLabel();
    string encoded;
    codec->EncodeTokens(source_tokens_, &encoded);
    EXPECT_GT(encoded.size(), 0);
    codec->DecodeTokens(reinterpret_cast<const unsigned char *>(encoded.data()),
                        &decoded_tokens_);
    CheckDecoded();

    // ReadTokens
    int read_num = 0;
    int offset = 0;
    while (true) {
      int read_byte = 0;
      int value_id = -1;
      const bool is_last_token = !(codec->ReadTokenForReverseLookup(
          reinterpret_cast<const unsigned char *>(encoded.data()) + offset,
          &value_id,
          &read_byte));
      if (source_tokens_[read_num].value_type == TokenInfo::DEFAULT_VALUE) {
        EXPECT_EQ(source_tokens_[read_num].id_in_value_trie, value_id);
      } else {
        EXPECT_EQ(-1, value_id);
      }
      offset += read_byte;
      ++read_num;
      if (is_last_token) {
        break;
      }
    }
    EXPECT_EQ(source_tokens_.size(), read_num);
  }
  {  // Value
    string original;
    {
      uint16 a_ucs2 = '!';
      srand(0);
      for (size_t i = 0; i < 10000; ++i) {
        // U+4E00-9FFF CJK Unified Ideographs
        uint16 c = a_ucs2 + static_cast<uint16>(
            1.0 * 0x9f00 * rand() / (RAND_MAX + 1.0));
        Util::UCS2ToUTF8Append(c, &original);
      }
    }
    string encoded;
    codec->EncodeValue(original, &encoded);
    string decoded;
    codec->DecodeValue(encoded, &decoded);
    EXPECT_EQ(original, decoded);
  }
  {  // Key
    string original;
    {
      uint16 a_ucs2 = 0x3041;  // "ぁ"
      srand(0);
      for (size_t i = 0; i < 1000; ++i) {
        uint16 c = a_ucs2 + static_cast<uint16>(
            1.0 * 1000 * rand() / (RAND_MAX + 1.0));
        Util::UCS2ToUTF8Append(c, &original);
      }
    }
    string encoded;
    codec->EncodeKey(original, &encoded);
    string decoded;
    codec->DecodeKey(encoded, &decoded);
    EXPECT_EQ(original, decoded);
  }
}

}  // namespace dictionary
}  // namespace mozc

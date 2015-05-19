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

#include "dictionary/system/codec.h"

#include <sstream>

#include "base/singleton.h"
#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/system/words_info.h"

namespace mozc {
namespace dictionary {
namespace {
void EncodeKeyImpl(const string &src, string *dst);

void DecodeKeyImpl(const string &src, string *dst);

uint8 GetFlagsForToken(const vector<TokenInfo> &tokens, int index);

uint8 GetFlagForPos(const TokenInfo &token_info, const Token *token);

uint8 GetFlagForValue(const TokenInfo &token_info, const Token *token);

uint8 GetKeyCharCode(const char *str);

void EncodeCost(const TokenInfo &token_info, uint8 *dst, int *offset);

void EncodePos(
    const TokenInfo &token_info, uint8 flags, uint8 *dst, int *offset);

void EncodeValueInfo(
    const TokenInfo &token_info, uint8 flags, uint8 *dst, int *offset);

uint8 ReadFlags(uint8 val);

void DecodeCost(const uint8 *ptr, TokenInfo *token, int *offset);

void ReadCost(const uint8 *ptr, int *offset);

void DecodePos(const uint8 *ptr, uint8 flags, TokenInfo *token, int *offset);

void DecodeValueInfo(
    const uint8 *ptr, uint8 flags, TokenInfo *token_info, int *offset);

void ReadValueInfo(
    const uint8 *ptr, uint8 flags, int *value_id, int *offset);

//// Constants for section name ////
const char kKeySectionName[] = "k";
const char kValueSectionName[] = "v";
const char kTokensSectionName[] = "t";
const char kPosSectionName[] = "p";

//// Constants for validation ////
// 12 bits
const int kPosMax = 0x0fff;
// 15 bits
const int kCostMax = 0x7fff;
// 22 bits
const int kValueTrieIdMax = 0x3fffff;

//// Constants for key ////
// This symbol in encoded index string escapes following 1 byte.
const uint8 kKeyCharMarkEscape = 0xff;
// following 2 characters in index string is encoded into 1 byte,
// since they are frequent.
const uint8 kKeyCharMiddleDot = 0xfd;
const uint8 kKeyCharProlongedSound = 0xfe;

//// Constants for value ////
// Unused for now.
// We are using from 0x00~0xfa for the Kanji, Hiragana and Katakana.
// Please see the comments for EncodeValue for details.
// const uint8 kValueCharMarkReserved = 0xfb;
// ASCII character.
const uint8 kValueCharMarkAscii = 0xfc;
// UCS4 character 0x??00.
const uint8 kValueCharMarkXX00 = 0xfd;
// This UCS4 character is neither Hiragana nor above 2 patterns 0x????
const uint8 kValueCharMarkOtherUCS2 = 0xfe;

// UCS4 character 0x00?????? (beyond UCS2 range)
// UCS4 characters never exceed 10FFFF. (three 8bits, A-B-C).
// For left most 8bits A, we will use upper 2bits for the flag
// that indicating whether B and C is 0 or not.
const uint8 kValueCharMarkUCS4 = 0xff;
const uint8 kValueCharMarkUCS4Middle0 = 0x80;
const uint8 kValueCharMarkUCS4Right0 = 0x40;
const uint8 kValueCharMarkUCS4LeftMask = 0x1f;

// character code related constants
const int kValueKanjiOffset = 0x01;
const int kValueHiraganaOffset = 0x4b;
const int kValueKatakanaOffset = 0x9f;

//// Cost encoding flag ////
const uint8 kSmallCostFlag = 0x80;
const uint8 kSmallCostMask = 0x7f;

//// Flags for token ////
const uint8 kTokenTerminationFlag = 0xff;
// Note that the flag for the first token for a certain key cannot be 0xff.
// First token cannot be kSameAsPrevValueFlag(0x33) nor kSameAsPrevPosFlag(0x0c)

// 7 kLastTokenFlag
// 6  <id encoding>
// below bits will be used for upper 6 bits of token value
// when CRAM_VALUE_FLAG is set.
// 5    <reserved(unused)>
// 4     kSpellingCorrectionFlag
// 3      <pos encoding(high)>
// 2       <pos encoding(low)>
// 1        <value encoding(high)>
// 0         <value encoding(low)>

//// Value encoding flag ////
// There are 4 mutually exclusive cases
//  1) Same as index hiragana key
//  2) Value is katakana
//  3) Same as previous token
//  4) Others. We have to store the value
const uint8 kValueTypeFlagMask = 0x03;
// Same as index hiragana word
const uint8 kAsIsHiraganaValueFlag = 0x01;
// Same as index katakana word
const uint8 kAsIsKatakanaValueFlag = 0x2;
// has same word
const uint8 kSameAsPrevValueFlag = 0x03;
// other cases
const uint8 kNormalValueFlag = 0x00;

//// Pos encoding flag ////
// There are 4 mutually exclusive cases
//  1) Same pos with previous token
//  2) Not same, frequent 1 byte pos
//  3) Not same, full_pos but lid==rid, 2 byte
//  4) Not same, full_pos 4 byte (no flag for this)
const uint8 kPosTypeFlagMask = 0x0c;
// Pos(left/right ID) is coded into 3 bytes
// Note that lid/rid is less than 12 bits
// We need 24 bits (= 3 bytes) to store full pos.
const uint8 kFullPosFlag = 0x04;
// lid == rid 8 bits
const uint8 kMonoPosFlag = 0x08;
// has same left/right id as previous token
const uint8 kSameAsPrevPosFlag = 0x0c;
// frequent
const uint8 kFrequentPosFlag = 0x00;

//// Spelling Correction flag ////
const uint8 kSpellingCorrectionFlag = 0x10;

//// Reverved ////
// You can use one more flag!
// const int kReservedFlag = 0x20;

//// Id encoding flag ////
// According to lower 6 bits of flags there are 2 patterns.
//  1) lower 6 bits are used.
//   - Store rx idx use 3 bytes
//  2) lower 6 bits are not used.
//   - Set CRAM_VALUE_FLAGS and use lower 6 bits.
//     We need another 2 bytes to store rx idx.
//     Note that we are assuming rx idx is less than 22 bits.
// Lower 6 bits of flags field are used to store upper part of id
// in value trie.
const uint8 kCrammedIDFlag = 0x40;
// Mask to cover upper valid 2bits when kCrammedIDFlag is used
const uint8 kUpperFlagsMask = 0xc0;
// Mask to get upper 6bits from flags value
const uint8 kUpperCrammedIDMask = 0x3f;

//// Last token flag ////
// This token is last token for a index word
const uint8 kLastTokenFlag = 0x80;
}  // namespace

SystemDictionaryCodec::SystemDictionaryCodec() {}

SystemDictionaryCodec::~SystemDictionaryCodec() {}

const string SystemDictionaryCodec::GetSectionNameForKey() const {
  return kKeySectionName;
}

const string SystemDictionaryCodec::GetSectionNameForValue() const {
  return kValueSectionName;
}

const string SystemDictionaryCodec::GetSectionNameForTokens() const {
  return kTokensSectionName;
}

const string SystemDictionaryCodec::GetSectionNameForPos() const {
  return kPosSectionName;
}

void SystemDictionaryCodec::EncodeKey(const string &src, string *dst) const {
  EncodeKeyImpl(src, dst);
}

void SystemDictionaryCodec::DecodeKey(const string &src, string *dst) const {
  DecodeKeyImpl(src, dst);
}

// This encodes each UCS4 character into following areas
// The trickier part in this encoding is handling of \0 byte in UCS4
// character. To avoid \0 in converted string, this function uses
// VALUE_CHAR_MARK_* markers.
//  Kanji in 0x4e00~0x97ff -> 0x01 0x00 ~ 0x4a 0xff (74*256 characters)
//  Hiragana 0x3041~0x3095 -> 0x4b~0x9f (84 characters)
//  Katakana 0x30a1~0x30fc -> 0x9f~0xfa (91 characters)
//  0x?? (ASCII) -> VALUE_CHAR_MARK_ASCII ??
//  0x??00 -> VALUE_CHAR_MARK_XX00 ??
//  Other 0x?? ?? -> VALUE_CHAR_MARK_OTHER ?? ??
//  0x?????? -> VALUE_CHAR_MARK_BIG ?? ?? ??

void SystemDictionaryCodec::EncodeValue(
    const string &src, string *dst) const {
  DCHECK(dst);
  const char *cstr = src.c_str();
  const char *end = src.c_str() + src.size();
  int pos = 0;
  while (pos < src.length()) {
    size_t mblen;
    COMPILE_ASSERT(sizeof(uint32) == sizeof(char32), check_sizeof_char32);
    const uint32 c = Util::UTF8ToUCS4(&cstr[pos], end, &mblen);
    pos += mblen;
    if (c >= 0x3041 && c < 0x3095) {
      // Hiragana(85 characters) are encoded into 1 byte.
      dst->push_back(c - 0x3041 + kValueHiraganaOffset);
    } else if (c >= 0x30a1 && c < 0x30fd) {
      // Katakana (92 characters) are encoded into 1 byte.
      dst->push_back(c - 0x30a1 + kValueKatakanaOffset);
    } else if (c < 0x10000 && ((c >> 8) & 255) == 0) {
      // 0x00?? (ASCII) are encoded into 2 bytes.
      dst->push_back(kValueCharMarkAscii);
      dst->push_back(c & 255);
    } else if (c < 0x10000 && (c & 255) == 0) {
      // 0x??00 are encoded into 2 bytes.
      dst->push_back(kValueCharMarkXX00);
      dst->push_back((c >> 8) & 255);
    } else if (c >= 0x4e00 && c < 0x9800) {
      // Frequent Kanji and others (74*256 characters) are encoded
      // into 2 bytes.
      // (Kanji in 0x9800 to 0x9fff are encoded in 3 bytes)
      const int h = ((c - 0x4e00) >> 8) + kValueKanjiOffset;
      dst->push_back(h);
      dst->push_back(c & 255);
    } else if (0x10000 <= c && c <= 0x10ffff) {
      // charaters encoded into 2-4bytes.
      int left = ((c >> 16) & 255);
      const int middle = ((c >> 8) & 255);
      const int right = (c & 255);
      if (middle == 0) {
        left |= kValueCharMarkUCS4Middle0;
      }
      if (right == 0) {
        left |= kValueCharMarkUCS4Right0;
      }
      dst->push_back(kValueCharMarkUCS4);
      dst->push_back(left);
      if (middle != 0) {
        dst->push_back(middle);
      }
      if (right != 0) {
        dst->push_back(right);
      }
    } else {
      DCHECK_LE(c, 0x10ffff);
      // Other charaters encoded into 3bytes.
      dst->push_back(kValueCharMarkOtherUCS2);
      dst->push_back((c >> 8) & 255);
      dst->push_back(c & 255);
    }
  }
}

void SystemDictionaryCodec::DecodeValue(
    const string &src, string *dst) const {
  DCHECK(dst);
  const uint8 *p = reinterpret_cast<const uint8 *>(src.c_str());
  while (*p != '\0') {
    int cc = p[0];
    int c = 0;
    if (kValueHiraganaOffset <= cc && cc < kValueKatakanaOffset) {
      // Hiragana
      c = 0x3041 + p[0] - kValueHiraganaOffset;
      p += 1;
    } else if (kValueKatakanaOffset <= cc && cc < kValueCharMarkAscii) {
      // Katakana
      c = 0x30a1 + p[0] - kValueKatakanaOffset;
      p += 1;
    } else if (cc == kValueCharMarkAscii) {
      // Ascii
      c = p[1];
      p += 2;
    } else if (cc == kValueCharMarkXX00) {
      // xx00
      c = (p[1] << 8);
      p += 2;
    } else if (cc == kValueCharMarkUCS4) {
      // UCS4
      c = ((p[1] & kValueCharMarkUCS4LeftMask) << 16);
      int pos = 2;
      if (!(p[1] & kValueCharMarkUCS4Middle0)) {
        c += (p[pos++] << 8);
      }
      if (!(p[1] & kValueCharMarkUCS4Right0)) {
        c += p[pos++];
      }
      p += pos;
    } else if (cc == kValueCharMarkOtherUCS2) {
      // other
      c = (p[1] << 8);
      c += p[2];
      p += 3;
    } else if (cc < kValueHiraganaOffset) {
      // Frequent kanji
      c = (((p[0] - kValueKanjiOffset) << 8) + 0x4e00);
      c += p[1];
      p += 2;
    } else {
      VLOG(1) << "should never come here";
    }
    Util::UCS4ToUTF8Append(c, dst);
  }
}

uint8 SystemDictionaryCodec::GetTokensTerminationFlag() const {
  return kTokenTerminationFlag;
}

void SystemDictionaryCodec::EncodeTokens(
    const vector<TokenInfo> &tokens, string *output) const {
  DCHECK(output);
  ostringstream oss;

  for (size_t i = 0; i < tokens.size(); ++i) {
    EncodeToken(tokens, i, &oss);
  }
  *output = oss.str();
  CHECK((*output)[0] != GetTokensTerminationFlag());
}

// Each token is encoded as following.
//
// Flags: 1 byte
// Cost:
//  For words without homonyms, 1 byte
//  Other words, 2 bytes
// Pos:
//  For pos same as the previous token, 0 byte
//  For frequent pos, 1 byte
//  For pos of left id == right id, 2 bytes
//  For other pos-es left id + right id 3 bytes
// Index: (less than 2^22)
//  When kCrammedIDFlag is set, 2 bytes
//  Othewise, 3 bytes
void SystemDictionaryCodec::EncodeToken(
    const vector<TokenInfo> &tokens, int index, ostringstream *oss) const {
  CHECK_LT(index, tokens.size());

  // Determines the flags for this token.
  const uint8 flags = GetFlagsForToken(tokens, index);

  // Encodes token into bytes.
  uint8 buff[9];
  buff[0] = flags;
  int offset = 1;

  const TokenInfo &token_info = tokens[index];
  EncodePos(token_info, flags, buff, &offset);  // <= 3 bytes
  EncodeCost(token_info, buff, &offset);  // <= 2 bytes
  EncodeValueInfo(token_info, flags, buff, &offset);  // <= 3 bytes

  CHECK_LE(offset, 9);
  oss->write(reinterpret_cast<char *>(buff), offset);
}

void SystemDictionaryCodec::DecodeTokens(
    const uint8 *ptr, vector<TokenInfo> *tokens) const {
  DCHECK(tokens);
  int offset = 0;
  while (true) {
    int read_bytes = 0;
    Token *token = new Token();
    tokens->push_back(TokenInfo(token));
    if (!DecodeToken(ptr + offset, &(tokens->back()), &read_bytes)) {
      break;
    }
    DCHECK_GT(read_bytes, 0);
    offset += read_bytes;
  }
}

bool SystemDictionaryCodec::DecodeToken(
    const uint8 *ptr, TokenInfo *token_info, int *read_bytes) const {
  DCHECK(ptr);
  DCHECK(token_info);
  DCHECK(read_bytes);

  const uint8 flags = ReadFlags(ptr[0]);
  if (flags & kSpellingCorrectionFlag) {
    token_info->token->attributes = Token::SPELLING_CORRECTION;
  }

  int offset = 1;
  DecodePos(ptr, flags, token_info, &offset);  // <= 3bytes
  DecodeCost(ptr, token_info, &offset);  // <= 2bytes
  DecodeValueInfo(ptr, flags, token_info, &offset);  // <= 3bytes
  DCHECK_LE(offset, 9);
  *read_bytes = offset;
  if (flags & kLastTokenFlag) {
    return false;
  } else {
    return true;
  }
}

bool SystemDictionaryCodec::ReadTokenForReverseLookup(
    const uint8 *ptr, int *value_id, int *read_bytes) const {
  DCHECK(ptr);
  DCHECK(value_id);
  DCHECK(read_bytes);

  const uint8 flags = ReadFlags(ptr[0]);
  int offset = 1;
  // Read pos
  const uint8 pos_flag = (flags & kPosTypeFlagMask);
  if (pos_flag == kFrequentPosFlag) {
    offset += 1;
  } else if (pos_flag == kMonoPosFlag) {
    offset += 2;
  } else if (pos_flag == kFullPosFlag) {
    offset += 3;
  }
  // Read cost
  if ((ptr[offset] & kSmallCostFlag)) {
    offset += 1;
  } else {
    offset += 2;
  }
  ReadValueInfo(ptr, flags, value_id, &offset);
  *read_bytes = offset;
  return !(flags & kLastTokenFlag);
}


namespace {
void EncodeKeyImpl(const string &src, string *dst) {
  DCHECK(dst);
  const char *p = src.c_str();
  while (*p != '\0') {
    const uint8 code = GetKeyCharCode(p);
    if (code != 0) {
      dst->push_back(code);
      p += 3;
    } else {
      dst->push_back(kKeyCharMarkEscape);
      dst->push_back(p[0]);
      ++p;
    }
  }
}

// Returns encoded code for a charactor from utf-8 key string
// "ぁ" = 0xe3,0x81,0x81 -> 0x81 & 0x7f = 0b10000001 & 0b01111111 = 1 = 0x01
// ...
// "み" = 0xe3,0x81,0xbf -> 0xbf & 0x7f = 0b10111111 & 0b01111111 = 63 = 0x3f
// "む" = 0xe3,0x82,0x80 -> 0x80 = 0b10000000 = 128 = 0x80
// ...
// "タ" = 0xe3,0x82,0xbf -> 0xbf = 0b10111111 = 191 (128+63) = 0xbf
// "・" = 0xe3,0x83,0xbb -> 0xfd
// "ー" = 0xe3,0x83,0xbc -> 0xfe
// (others) -> 0
//
// TODO(toshiyuki): Prepare code for numbers and hyphen, they are used
// for the key of zip code entries.
uint8 GetKeyCharCode(const char *str) {
  const uint8 *ustr =
      reinterpret_cast<const uint8 *>(str);
  if (ustr[0] == 0xe3 && ustr[1] == 0x81) {
    // Hiragana 0xe3,0x81,0x80|Z -> Z
    return ustr[2] & 0x7f;
  } else if (ustr[0] == 0xe3 && ustr[1] == 0x82) {
    // Hiragana 0xe3,0x82,Z -> Z
    return ustr[2] | 0x80;
  } else if (ustr[0] == 0xe3 && ustr[1] == 0x83 && ustr[2] == 0xbb) {
    return kKeyCharMiddleDot;
  } else if (ustr[0] == 0xe3 && ustr[1] == 0x83 && ustr[2] == 0xbc) {
    return kKeyCharProlongedSound;
  }
  return 0;
}

void DecodeKeyImpl(const string &src, string *dst) {
  DCHECK(dst);
  const uint8 *p = reinterpret_cast<const uint8 *>(src.c_str());
  while (*p != '\0') {
    if (p[0] == kKeyCharMarkEscape) {
      dst->push_back(p[1]);
      p += 2;
    } else if (p[0] == kKeyCharMiddleDot) {
      dst->push_back(0xe3);
      dst->push_back(0x83);
      dst->push_back(0xbb);
      ++p;
    } else if (p[0] == kKeyCharProlongedSound) {
      dst->push_back(0xe3);
      dst->push_back(0x83);
      dst->push_back(0xbc);
      ++p;
    } else if (!(p[0] & 0x80)) {
      dst->push_back(0xe3);
      dst->push_back(0x81);
      dst->push_back(p[0] | 0x80);
      ++p;
    } else {
      dst->push_back(0xe3);
      dst->push_back(0x82);
      dst->push_back(p[0]);
      ++p;
    }
  }
}

// Return flags for token
uint8 GetFlagsForToken(const vector<TokenInfo> &tokens,
                       int index) {
  // Determines the flags for this token.
  uint8 flags = 0;
  if (index == tokens.size() - 1) {
    flags |= kLastTokenFlag;
  }

  const TokenInfo &token_info = tokens[index];
  const Token *token = token_info.token;

  // Special treatment for spelling correction.
  if (token->attributes & Token::SPELLING_CORRECTION) {
    flags |= kSpellingCorrectionFlag;
  }

  // Pos flag
  flags |= GetFlagForPos(token_info, token);

  if (index == 0) {
    CHECK_NE(flags & kPosTypeFlagMask, kSameAsPrevPosFlag)
        << "First token cannot become the SameAsPrevPos.";
  }

  // Value flag
  flags |= GetFlagForValue(token_info, token);
  if (index == 0) {
    CHECK_NE(flags &  kValueTypeFlagMask, kSameAsPrevValueFlag)
        << "First token cannot become the SameAsPrevValue.";
  }

  if ((flags & kUpperCrammedIDMask) == 0) {
    // Lower 6bits are available. Use it for value trie id.
    flags |= kCrammedIDFlag;
  }
  return flags;
}

uint8 GetFlagForPos(
    const TokenInfo &token_info,
    const Token *token) {
  CHECK(token);
  const uint16 lid = token->lid;
  const uint16 rid = token->rid;
  if (lid > kPosMax || rid > kPosMax) {
    // We can use LOG(FATAL) here, as this code runs in dictionary_builder.
    LOG(FATAL) << "Too large pos id: lid " << lid << ", rid " << rid;
  }

  if (token_info.pos_type == TokenInfo::FREQUENT_POS) {
    return kFrequentPosFlag;
  } else if (token_info.pos_type == TokenInfo::SAME_AS_PREV_POS) {
    return kSameAsPrevPosFlag;
  } else if (lid == rid) {
    return kMonoPosFlag;
  } else {
    return kFullPosFlag;
  }
}

uint8 GetFlagForValue(
    const TokenInfo &token_info,
    const Token *token) {
  CHECK(token);
  if (token_info.value_type == TokenInfo::SAME_AS_PREV_VALUE) {
    return kSameAsPrevValueFlag;
  } else if (token_info.value_type == TokenInfo::AS_IS_HIRAGANA) {
    return kAsIsHiraganaValueFlag;
  } else if (token_info.value_type == TokenInfo::AS_IS_KATAKANA) {
    return kAsIsKatakanaValueFlag;
  } else {
    return kNormalValueFlag;
  }
}

void EncodeCost(
    const TokenInfo &token_info, uint8 *dst, int *offset) {
  const Token *token = token_info.token;
  CHECK_LE(token->cost, kCostMax) << "Assuming cost is within 15bits.";
  if (token_info.cost_type == TokenInfo::CAN_USE_SMALL_ENCODING) {
    dst[*offset] = (token->cost >> 8) | kSmallCostFlag;
    *offset += 1;
  } else {
    dst[*offset] = token->cost >> 8;
    dst[*offset + 1] = token->cost & 0xff;
    *offset += 2;
  }
}

void EncodePos(
    const TokenInfo &token_info, uint8 flags, uint8 *dst, int *offset) {
  const uint8 pos_flag = flags & kPosTypeFlagMask;
  const Token *token = token_info.token;
  const uint16 lid = token->lid;
  const uint16 rid = token->rid;
  switch (pos_flag) {
    case kFullPosFlag: {
      // 3 bytes
      dst[*offset] = (lid & 255);
      dst[*offset + 1] = ((rid << 4) & 255) | (lid >> 8);
      dst[*offset + 2] = (rid >> 4) & 255;
      *offset += 3;
      break;
    }
    case kMonoPosFlag: {
      // 2 bytes
      dst[*offset] = (lid & 255);
      dst[*offset + 1] = (lid >> 8);
      *offset += 2;
      break;
    }
    case kFrequentPosFlag: {
      // Frequent 1 byte pos.
      const int id = token_info.id_in_frequent_pos_map;
      CHECK_GE(id, 0);
      dst[*offset] = id;
      *offset += 1;
      break;
    }
    case kSameAsPrevPosFlag: {
      break;
    }
    default: {
      // We can use LOG(FATAL) here. This code runs in dictionary_builder.
      LOG(FATAL) << "Should not come here";
      break;
    }
  }
}

void EncodeValueInfo(
    const TokenInfo &token_info, uint8 flags, uint8 *dst, int *offset) {
  const uint8 value_type_flag = flags & kValueTypeFlagMask;
  if (value_type_flag != kNormalValueFlag) {
    // No need to store id for word trie
    return;
  }
  const uint32 id = token_info.id_in_value_trie;
  if (id > kValueTrieIdMax) {  // 22 bits
    // We can use LOG(FATAL) here.
    LOG(FATAL) << "Too large word rx (should be less than 2^22)\t" << id;
  }

  if (flags & kCrammedIDFlag) {
    dst[*offset] = id & 255;
    dst[*offset + 1] = (id >> 8) & 255;
    // Uses lower 6 bits of flags.
    dst[0] |= (id >> 16) & kUpperCrammedIDMask;
    *offset += 2;
  } else {
    dst[*offset] = id & 255;
    dst[*offset + 1] = (id >> 8) & 255;
    dst[*offset + 2] = (id >> 16) & 255;
    *offset += 3;
  }
}

uint8 ReadFlags(uint8 val) {
  uint8 ret = val;
  if (ret & kCrammedIDFlag) {
    ret &= kUpperFlagsMask;
  }
  return ret;
}

void DecodeCost(const uint8 *ptr, TokenInfo *token_info, int *offset) {
  DCHECK(ptr);
  DCHECK(token_info);
  DCHECK(offset);
  if (ptr[*offset] & kSmallCostFlag) {
    token_info->token->cost = ((ptr[*offset] & kSmallCostMask) << 8);
    *offset += 1;
  } else {
    token_info->token->cost = (ptr[*offset] << 8);
    token_info->token->cost += ptr[*offset + 1];
    *offset += 2;
  }
}

void DecodePos(
    const uint8 *ptr, uint8 flags, TokenInfo *token_info, int *offset) {
  DCHECK(ptr);
  DCHECK(token_info);
  DCHECK(offset);
  const uint8 pos_flag = (flags & kPosTypeFlagMask);
  Token *token = token_info->token;
  switch (pos_flag) {
    case kFrequentPosFlag: {
      const int pos_id = ptr[*offset];
      token_info->pos_type = TokenInfo::FREQUENT_POS;
      token_info->id_in_frequent_pos_map = pos_id;
      *offset += 1;
      break;
    }
    case kSameAsPrevPosFlag: {
      token_info->pos_type = TokenInfo::SAME_AS_PREV_POS;
      break;
    }
    case kMonoPosFlag: {
      const uint16 id = ((ptr[*offset + 1] << 8) | ptr[*offset]);
      token->lid = id;
      token->rid = id;
      *offset += 2;
      break;
    }
    case kFullPosFlag: {
      token->lid = ptr[*offset];
      token->lid += ((ptr[*offset + 1] & 0x0f) << 8);
      token->rid = (ptr[*offset + 1] >> 4);
      token->rid += (ptr[*offset + 2] << 4);
      *offset += 3;
      break;
    }
    default: {
      DLOG(FATAL) << "should never come here";
      break;
    }
  }
}

void DecodeValueInfo(const uint8 *ptr,
                     uint8 flags,
                     TokenInfo *token_info,
                     int *offset) {
  DCHECK(ptr);
  DCHECK(token_info);
  DCHECK(offset);
  const uint8 value_flag = (flags & kValueTypeFlagMask);
  switch (value_flag) {
    case kAsIsHiraganaValueFlag: {
      token_info->value_type = TokenInfo::AS_IS_HIRAGANA;
      break;
    }
    case kAsIsKatakanaValueFlag: {
      token_info->value_type = TokenInfo::AS_IS_KATAKANA;
      break;
    }
    case kSameAsPrevValueFlag: {
      token_info->value_type = TokenInfo::SAME_AS_PREV_VALUE;
      break;
    }
    case kNormalValueFlag: {
      token_info->value_type = TokenInfo::DEFAULT_VALUE;
      uint32 id = ((ptr[*offset + 1] << 8) | ptr[*offset]);
      if (flags & kCrammedIDFlag) {
        id |= ((ptr[0] & kUpperCrammedIDMask) << 16);
        *offset += 2;
      } else {
        id |= (ptr[*offset + 2] << 16);
        *offset += 3;
      }
      token_info->id_in_value_trie = id;
      break;
    }
    default: {
      DLOG(FATAL) << "should never come here";
      break;
    }
  }
}

// Get value id only for reverse conversion
void ReadValueInfo(const uint8 *ptr, uint8 flags, int *value_id, int *offset) {
  DCHECK(ptr);
  DCHECK(value_id);
  DCHECK(offset);
  *value_id = -1;
  const uint8 value_flag = (flags & kValueTypeFlagMask);
  if (value_flag == kNormalValueFlag) {
    uint32 id = ((ptr[*offset + 1] << 8) | ptr[*offset]);
    if (flags & kCrammedIDFlag) {
      id |= ((ptr[0] & kUpperCrammedIDMask) << 16);
      *offset += 2;
    } else {
      id |= (ptr[*offset + 2] << 16);
      *offset += 3;
    }
    *value_id = id;
  }
}
}  // namespace

namespace {
SystemDictionaryCodecInterface *g_system_dictionary_codec = NULL;
typedef SystemDictionaryCodec DefaultCodec;
}  // namespace

SystemDictionaryCodecInterface *SystemDictionaryCodecFactory::GetCodec() {
  if (g_system_dictionary_codec == NULL) {
    return Singleton<DefaultCodec>::get();
  } else {
    return g_system_dictionary_codec;
  }
}

void SystemDictionaryCodecFactory::SetCodec(
    SystemDictionaryCodecInterface *codec) {
  g_system_dictionary_codec = codec;
}
}  // namespace dictionary
}  // namespace mozc

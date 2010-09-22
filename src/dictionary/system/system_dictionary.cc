// Copyright 2010, Google Inc.
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

#include "dictionary/system/system_dictionary.h"

#include <climits>
#include <string>

#include "base/base.h"
#include "base/util.h"
#include "base/flags.h"
#include "base/mmap.h"
#include "base/singleton.h"
#include "converter/node.h"
#include "dictionary/file/dictionary_file.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/text_dictionary_loader.h"

namespace mozc {
namespace {

struct RxResults {
  vector<string> retStr;
  vector<int> retID;
  int limit;
};

static int rx_cb(void *cookie, const char *s, int len, int id) {
  RxResults *res = reinterpret_cast<RxResults *>(cookie);
  if (res->limit <= 0) {
    // stops traversal.
    return -1;
  }
  --res->limit;
  // truncates to len byte.
  string str(s, len);
  res->retStr.push_back(str);
  res->retID.push_back(id);
  return 0;
}
}  // namespace

SystemDictionary::SystemDictionary()
    : rx_(NULL), token_rx_(NULL), rbx_(NULL), frequent_pos_(NULL) {}

bool SystemDictionary::Open(const char *filename) {
  VLOG(1) << "Opening Rx dictionary" << filename;
  DictionaryFile *df = new DictionaryFile();
  if (!df->Open(filename, false)) {
    delete df;
    return false;
  }
  return OpenDictionaryFile(df);
}

bool SystemDictionary::OpenFromArray(const char *ptr, int len) {
  DictionaryFile *df = new DictionaryFile();
  if (!df->SetPtr(ptr, len)) {
    delete df;
    return false;
  }
  return OpenDictionaryFile(df);
}

void SystemDictionary::Close() {
  if (rx_ != NULL) {
    rx_close(rx_);
    rx_close(token_rx_);
    rbx_close(rbx_);
  }
}

Node *SystemDictionary::LookupPredictive(const char *str, int size,
                                         NodeAllocatorInterface *allocator) const {
  // Predictive lookup. Gets all tokens matche the key.
  return LookupInternal(str, size, allocator, true, NULL);
}

Node *SystemDictionary::LookupExact(const char *str, int size,
                                    NodeAllocatorInterface *allocator) const {
  return NULL;
}

Node *SystemDictionary::LookupPrefix(const char *str, int size,
                                     NodeAllocatorInterface *allocator) const {
  // Not a predictive lookup. Gets all tokens match the key.
  return LookupInternal(str, size, allocator, false, NULL);
}

Node *SystemDictionary::LookupReverse(const char *str, int size,
                                      NodeAllocatorInterface *allocator) const {
  return NULL;
}

Node *SystemDictionary::LookupInternal(const char *str, int size,
                                       NodeAllocatorInterface *allocator,
                                       bool is_predictive,
                                       int *max_nodes_size) const {
  string key_str;
  EncodeIndexString(string(str, size), &key_str);

  RxResults res;
  res.limit = kMaxTokensPerLookup;

  int dummy_limit = kMaxTokensPerLookup;
  int *limit = &dummy_limit;
  if (max_nodes_size) {
    // Assuming one key has more than one values.
    res.limit = *max_nodes_size;
    limit = max_nodes_size;
  } else if (allocator != NULL) {
    res.limit = allocator->max_nodes_size();
  }

  if (is_predictive) {
    rx_search(rx_, 1, key_str.c_str(), rx_cb, &res);
  } else {
    rx_search(rx_, 0, key_str.c_str(), rx_cb, &res);
  }

  Node *resultNode = NULL;
  vector<Token *> tokens;
  for (size_t i = 0; i < res.retStr.size() && *limit > 0; i++) {
    // gets tokens block of this key.
    const uint8 *ptr = rbx_get(rbx_, res.retID[i], NULL);
    string ret_str;
    DecodeIndexString(res.retStr[i], &ret_str);
    ReadTokens(ret_str, ptr, -1, &tokens);

    for (vector<Token *>::iterator it = tokens.begin();
         it != tokens.end(); ++it) {
      if (*limit > 0) {
        Node *new_node = CopyTokenToNode(allocator, *it);
        new_node->bnext = resultNode;
        resultNode = new_node;
        --(*limit);
      }
      delete *it;
    }
    tokens.clear();
  }
  return resultNode;
}

Node *SystemDictionary::CopyTokenToNode(NodeAllocatorInterface *allocator,
                                        const Token *token) const {
  Node *new_node = NULL;
  if (allocator != NULL) {
    new_node = allocator->NewNode();
  } else {
    // for test
    new_node = new Node();
  }

  if (token->lid >= kSpellingCorrectionPosOffset) {
    new_node->lid = token->lid - kSpellingCorrectionPosOffset;
    new_node->is_spelling_correction = true;
  } else {
    new_node->lid = token->lid;
    new_node->is_spelling_correction = false;
  }
  new_node->rid = token->rid;
  new_node->wcost = token->cost;
  new_node->key = token->key;
  new_node->value = token->value;
  new_node->node_type = Node::NOR_NODE;
  return new_node;
}

void SystemDictionary::EncodeIndexString(const string &src,
                                         string *dst) {
  for (const char *p = src.c_str(); *p != '\0'; p++) {
    uint8 hc = HiraganaCode(p);
    if (hc) {
      dst->push_back(hc);
      p += 2;
    } else {
      dst->push_back(INDEX_CHAR_MARK_ESCAPE);
      dst->push_back(p[0]);
    }
  }
}

void SystemDictionary::DecodeIndexString(const string &src,
                                         string *dst) {
  for (const uint8 *p =
           reinterpret_cast<const uint8 *>(src.c_str());
       *p != '\0'; p++) {
    if (*p == INDEX_CHAR_MARK_ESCAPE) {
      dst->push_back(p[1]);
      p += 1;
    } else if (*p == INDEX_CHAR_MIDDLE_DOT) {
      dst->push_back(0xe3);
      dst->push_back(0x83);
      dst->push_back(0xbb);
    } else if (*p == INDEX_CHAR_PROLONGED_SOUND) {
      dst->push_back(0xe3);
      dst->push_back(0x83);
      dst->push_back(0xbc);
    } else if (!(*p & 0x80)) {
      dst->push_back(0xe3);
      dst->push_back(0x81);
      dst->push_back(*p | 0x80);
    } else {
      dst->push_back(0xe3);
      dst->push_back(0x82);
      dst->push_back(*p);
    }
  }
}

// The trickier part in this encoding is handling of \0 byte in UCS2
// character. To avoid \0 in converted string, this function uses
// TOKEN_CHAR_MARK_* markers.
//
// This encodes each UCS2 character into following areas
//  Kanji in 0x4e00~0x9800 -> 0x01~0x6a (74*256 characters)
//  Hiragana 0x3041~0x3095 -> 0x6b~0x9f (84 characters)
//  Katakana 0x30a1~0x30fc -> 0x9f~0xfb (92 characters)
//  0x?? (ASCII) -> TOKEN_CHAR_MARK ??
//  0x??00 -> TOKEN_CHAR_MARK_XX00 ??
//  Other 0x?? ?? -> TOKEN_CHAR_MARK_OTHER ?? ??
//
void SystemDictionary::EncodeTokenStringWithLength(const string &src,
                                                   int length,
                                                   string *dst) {
  const char *cstr = src.c_str();
  const char *end = src.c_str() + src.size();
  int pos = 0;
  while (pos < length) {
    size_t mblen;
    const uint16 c = Util::UTF8ToUCS2(&cstr[pos], end, &mblen);
    pos += mblen;
    if (c >= 0x3041 && c < 0x3095) {
      // Hiragana(85 characters) are encoded into 1 byte.
      dst->push_back(c - 0x3041 + HIRAGANA_OFFSET);
    } else if (((c >> 8) & 255) == 0) {
      // 0x00?? (ASCII) are encoded into 2 bytes.
      dst->push_back(TOKEN_CHAR_MARK_ASCII);
      dst->push_back(c & 255);
    } else if ((c & 255) == 0) {
      // 0x??00 are encoded into 2 bytes.
      dst->push_back(TOKEN_CHAR_MARK_XX00);
      dst->push_back((c >> 8) & 255);
    } else if (c >= 0x4e00 && c < 0x9800) {
      // Frequent Kanji and others (74*256 characters) are encoded
      // into 2 bytes.
      // (Kanji in 0x9800 to 0x9fff are encoded in 3 bytes)
      const int h = ((c - 0x4e00) >> 8) + KANJI_OFFSET;
      dst->push_back(h);
      dst->push_back(c & 255);
    } else if (c >= 0x30a1 && c < 0x31fc) {
      // Katakana (92 characters)
      dst->push_back(c - 0x30a1 + KATAKANA_OFFSET);
    } else {
      // Other charaters are encoded into 3bytes.
      dst->push_back(TOKEN_CHAR_MARK_OTHER);
      dst->push_back((c >> 8) & 255);
      dst->push_back(c & 255);
    }
  }
}

// This compresses each UCS2 character in string into small bytes.
void SystemDictionary::EncodeTokenString(const string &src,
                                         string *dst) {
  EncodeTokenStringWithLength(src, src.size(), dst);
}

// See comments in EncodeTokenString().
void SystemDictionary::DecodeTokenString(const string &src,
                                         string *dst) {
  const uint8 *cstr =
      reinterpret_cast<const uint8 *>(src.c_str());
  while (*cstr != '\0') {
    int cc = *cstr;
    int c;
    if (cc >= HIRAGANA_OFFSET && cc < KATAKANA_OFFSET) {
      // Hiragana
      c = 0x3041 + cstr[0] - HIRAGANA_OFFSET;
      cstr += 1;
    } else if (cc >= KATAKANA_OFFSET && cc < TOKEN_CHAR_MARK_MIN) {
      // Katakana
      c = 0x30a1 + cc - KATAKANA_OFFSET;
      cstr += 1;
    } else if (cc == TOKEN_CHAR_MARK_ASCII) {
      // 0x00?? (ASCII)
      c = cstr[1];
      cstr += 2;
    } else if (cc == TOKEN_CHAR_MARK_XX00) {
      // 0x??00
      c = (cstr[1] << 8);
      cstr += 2;
    } else if (cc == TOKEN_CHAR_MARK_OTHER) {
      // Other 2bytes.
      c = cstr[1] * 256;
      c += cstr[2];
      cstr += 3;
    } else {
      // Frequent Kanji and others.
      // Kanji area starts from 0x4e00
      c = ((cc - KANJI_OFFSET) + 0x4e) * 256;
      c += cstr[1];
      cstr += 2;
    }
    Util::UCS2ToUTF8Append(c, dst);
  }
}

uint8 SystemDictionary::HiraganaCode(const char *str) {
  const uint8 *ustr =
      reinterpret_cast<const uint8 *>(str);
  if (ustr[0] == 0xe3 && ustr[1] == 0x81) {
    // Hiragana 0xe3,0x81,0x80|Z -> Z
    return ustr[2] & 0x7f;
  } else if (ustr[0] == 0xe3 && ustr[1] == 0x82) {
    // Hiragana 0xe3,0x82,Z -> Z
    return ustr[2] | 0x80;
  } else if (ustr[0] == 0xe3 && ustr[1] == 0x83 && ustr[2] == 0xbb) {
    return INDEX_CHAR_MIDDLE_DOT;
  } else if (ustr[0] == 0xe3 && ustr[1] == 0x83 && ustr[2] == 0xbc) {
    return INDEX_CHAR_PROLONGED_SOUND;
  }
  return 0;
}

SystemDictionary::~SystemDictionary() {
  Close();
}

bool SystemDictionary::OpenDictionaryFile(DictionaryFile *df) {
  df_.reset(df);
  int len;
  const unsigned char *index_image =
      reinterpret_cast<const unsigned char *>(df->GetSection("i", &len));
  CHECK(index_image) << "can not find index section";
  if (!(rx_ = rx_open(index_image))) {
    return false;
  }
  const unsigned char *token_rx =
      reinterpret_cast<const unsigned char *>(df->GetSection("R", &len));
  CHECK(token_rx) << "can not find token_rx section";
  if (!(token_rx_ = rx_open(token_rx))) {
    return false;
  }

  const unsigned char *tokens =
      reinterpret_cast<const unsigned char *>(df->GetSection("T",
                                                             &len));
  CHECK(tokens) << "can not find tokens section";
  if (!(rbx_ = rbx_open(tokens))) {
    return false;
  }

  frequent_pos_ =
      reinterpret_cast<const uint32*>(df->GetSection("f", &len));
  CHECK(frequent_pos_) << "can not find frequent pos section";

  return true;
}

void SystemDictionary::ReadTokens(const string& key,
                                  const uint8* ptr,
                                  int new_pos,
                                  vector<Token *>* res) const {
  uint8 cur_flags;
  Token* prev_token = NULL;
  int offset = 0;
  vector<Token *> unused_tokens;
  do {
    const uint8 *p = &ptr[offset];
    Token *t = new Token();
    t->key = key;
    int idx;
    offset += DecodeToken(key, p, prev_token, t, &idx);
    cur_flags = p[0];
    if (new_pos < 0 || new_pos == idx) {
      res->push_back(t);
    } else {
      // discard.
      unused_tokens.push_back(t);
    }
    prev_token = t;
  } while (!(cur_flags & LAST_TOKEN_FLAG));
  for (int i = 0; i < unused_tokens.size(); ++i) {
    delete unused_tokens[i];
  }
}

int SystemDictionary::DecodeToken(const string& key, const uint8* ptr,
                                  const Token *prev_token,
                                  Token* t, int *rx_idx) const {
  uint8 flags;

  flags = ptr[0];
  int offset = 1;
  t->cost = (ptr[1] << 8);
  t->cost += ptr[2];
  offset += 2;

  // Decodes pos.
  if (flags & FULL_POS_FLAG) {
    t->lid = ptr[offset];
    t->lid += (ptr[offset + 1] << 8);
    t->rid = ptr[offset + 2];
    t->rid += (ptr[offset + 3] << 8);
    offset += 4;
  } else if (flags & SAME_POS_FLAG) {
    t->lid = prev_token->lid;
    t->rid = prev_token->rid;
  } else {
    int pos_id = ptr[offset];
    uint32 pos = frequent_pos_[pos_id];
    t->lid = pos >> 16;
    t->rid = pos & 0xffff;
    offset += 1;
  }

  // Decodes value of the token.
  if (flags & AS_IS_TOKEN_FLAG) {
    t->value = key;
  } else if (flags & KATAKANA_TOKEN_FLAG) {
    string value;
    Util::HiraganaToKatakana(key, &value);
    t->value = value;
  } else if (flags & SAME_VALUE_FLAG) {
    t->value = prev_token->value;
  } else {
    uint32 idx;
    idx = ptr[offset];
    idx += (ptr[offset + 1] << 8);
    idx += (ptr[offset + 2] << 16);
    offset += 3;
    *rx_idx = idx;
    char buf[256];
    if (!rx_reverse(token_rx_, idx, buf, sizeof(buf))) {
      VLOG(2) << "failed to reverse rx look up"
              << t->key << ":" << idx;
    } else {
      DecodeTokenString(string(buf), &t->value);
    }
  }

  return offset;
}

SystemDictionary *SystemDictionary::GetSystemDictionary() {
  return Singleton<SystemDictionary>::get();
}
}  // namespace mozc

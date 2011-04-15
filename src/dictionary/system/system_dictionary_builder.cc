// Copyright 2010-2011, Google Inc.
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

#include "dictionary/system/system_dictionary_builder.h"

#include <algorithm>
#include <climits>
#include <sstream>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "dictionary/file/dictionary_file.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/text_dictionary_loader.h"
#include "dictionary/system/system_dictionary.h"

DEFINE_bool(preserve_intermediate_dictionary, false,
            "preserve inetemediate dictionary file.");

namespace mozc {
void SystemDictionaryBuilder::Compile(const char *text_file,
                                      const char *binary_file) {
  SystemDictionaryBuilder builder(text_file, binary_file);
  builder.Build();
}

// This structure represents tokens with same index.
struct RxKeyStringInfo {
  RxKeyStringInfo();

  vector<RxTokenInfo *> tokens;
  int offset_in_index_rx;
};

RxKeyStringInfo::RxKeyStringInfo() : offset_in_index_rx(-1) {
}

struct RxTokenInfo {
  RxTokenInfo(Token *t, bool frequent);

  Token *token;
  int offset_in_word_rx;
  bool same_as_key;
  bool same_as_katakana;
  // POS of this token is in top 256.
  bool is_frequent_pos;
};

RxTokenInfo::RxTokenInfo(Token *t,
                         bool frequent_pos) : token(t),
                                              offset_in_word_rx(0),
                                              is_frequent_pos(frequent_pos) {
  same_as_key = (t->key == t->value);
  string s;
  Util::HiraganaToKatakana(t->key, &s);
  same_as_katakana = (s == t->value);
}

struct TokenGreaterThan {
  inline bool operator()(const RxTokenInfo* lhs,
                         const RxTokenInfo* rhs) const {
    return ((lhs->token->lid > rhs->token->lid) ||
            ((lhs->token->lid == rhs->token->lid) &&
             (lhs->token->rid > rhs->token->rid)) ||
            ((lhs->token->lid == rhs->token->lid) &&
             (lhs->token->rid == rhs->token->rid) &&
             (lhs->offset_in_word_rx < rhs->offset_in_word_rx)));
  }
};

SystemDictionaryBuilder::SystemDictionaryBuilder(const string& input,
                                                 const string& output)
    : input_filename_(input),
      output_filename_(output) {
  index_rx_filename_ = output_filename_ + ".index_rx";
  tokens_filename_ = output_filename_ + ".tokens";
  token_rx_filename_ = output_filename_ + ".token_rx";
  frequent_pos_filename_ = output_filename_ + ".freq_pos";
}

SystemDictionaryBuilder::~SystemDictionaryBuilder() {
  map<string, RxKeyStringInfo *>::iterator it;
  for (it = key_info_.begin(); it != key_info_.end(); ++it) {
    RxKeyStringInfo *ki = it->second;
    vector<RxTokenInfo *>::iterator jt;
    for (jt = ki->tokens.begin(); jt != ki->tokens.end(); ++jt) {
      delete *jt;
    }
    delete it->second;
  }
}

bool SystemDictionaryBuilder::WriteIndexRx(const vector<Token *>& tokens,
                                           hash_map<string, int>* mapping) {
  vector<string> word_list;
  vector<Token *>::const_iterator it;
  for (it = tokens.begin(); it != tokens.end(); ++it) {
    string key;
    SystemDictionary::EncodeIndexString((*it)->key, &key);
    word_list.push_back(key);
  }

  LOG(INFO) << "start to build index rx\n";
  return BuildRxFile(word_list, index_rx_filename_, mapping);
}

bool SystemDictionaryBuilder::BuildRxFile(const vector<string>& word_list,
                                          const string& rx_fn,
                                          hash_map<string, int>* mapping) {
  rx_builder *b = rx_builder_create();
  for (vector<string>::const_iterator it = word_list.begin();
       it != word_list.end(); ++it) {
    rx_builder_add(b, it->c_str());
  }
  rx_builder_build(b);
  for (vector<string>::const_iterator it = word_list.begin();
       it != word_list.end(); ++it) {
    (*mapping)[*it] = rx_builder_get_key_index(b, it->c_str());
  }
  unsigned char *image = rx_builder_get_image(b);
  int size = rx_builder_get_size(b);
  OutputFileStream ofs(rx_fn.c_str(), ios::binary|ios::out);
  ofs.write(reinterpret_cast<const char *>(image), size);
  rx_builder_release(b);
  LOG(INFO) << "Wrote rx file to :" << rx_fn;
  return true;
}

bool SystemDictionaryBuilder::WriteTokenRx(hash_map<string, int>* mapping) {
  vector<string> token_array;
  for (map<string, RxKeyStringInfo *>::iterator it = key_info_.begin();
       it != key_info_.end(); ++it) {
    vector<RxTokenInfo *>::iterator jt;
    for (jt = it->second->tokens.begin(); jt != it->second->tokens.end();
         ++jt) {
      RxTokenInfo *t = *jt;
      if (!t->same_as_key &&
          !t->same_as_katakana) {
        // This token should be in token Rx
        string token_str;
        SystemDictionary::EncodeTokenString(t->token->value, &token_str);
        token_array.push_back(token_str);
        token_rx_map_[token_str] = 0;
      }
    }
  }
  LOG(INFO) << "start to build token rx\n";
  return BuildRxFile(token_array, token_rx_filename_, mapping);
}

bool SystemDictionaryBuilder::BuildTokenRxMap(
    const hash_map<string, int>& mapping) {
  map<string, int>::iterator it;
  for (it = token_rx_map_.begin(); it != token_rx_map_.end(); ++it) {
    hash_map<string, int>::const_iterator jt = mapping.find(it->first);
    if (jt != mapping.end()) {
      it->second = jt->second;
    }
  }
  return true;
}

void SystemDictionaryBuilder::SetIndexInTokenRx() {
  map<string, RxKeyStringInfo *>::iterator it;
  for (it = key_info_.begin(); it != key_info_.end(); ++it) {
    for (vector<RxTokenInfo *>::iterator jt = it->second->tokens.begin();
         jt != it->second->tokens.end(); ++jt) {
      RxTokenInfo *t = *jt;
      string token_str;
      SystemDictionary::EncodeTokenString(t->token->value, &token_str);
      t->offset_in_word_rx = token_rx_map_[token_str];
    }
  }
}

void SystemDictionaryBuilder::SortTokenInfo() {
  map<string, RxKeyStringInfo *>::iterator it;
  for (it = key_info_.begin(); it != key_info_.end(); ++it) {
    RxKeyStringInfo *ki = it->second;
    sort(ki->tokens.begin(), ki->tokens.end(), TokenGreaterThan());
  }
}

void SystemDictionaryBuilder::WriteInt(int offset,
                                       OutputFileStream *ofs) {
  ofs->write(reinterpret_cast<const char *>(&offset), sizeof(offset));
}

// Each token is encoded as following.
// Flags: 1 byte (see comments in rx_dictionary.h)
// Cost:
//  For words without homonyms, 1 bytes
//  Other words, 2 bytes
// Pos:
//  For frequent pos, 1 byte
//  For other pos-es left id+right id 2 byte
// Index: 3 bytes (4 bytes when the offset is larger than 2^24)
void SystemDictionaryBuilder::WriteToken(const RxKeyStringInfo &key, int n,
                                         ostream *ofs) {
  const RxTokenInfo *token_info = key.tokens[n];
  const RxTokenInfo *prev_token = (n > 0 ? key.tokens[n - 1] : NULL);
  // Determines the flags for this token.
  uint8 flags = 0;
  if (n == key.tokens.size() - 1) {
    flags |= SystemDictionary::LAST_TOKEN_FLAG;
  }
  const Token *t = token_info->token;
  uint32 pos = (t->lid << 16) | t->rid;
  // Determines pos encoding.
  if (frequent_pos_.find(pos) == frequent_pos_.end()) {
    if (prev_token &&
        prev_token->token->lid == t->lid &&
        prev_token->token->rid == t->rid) {
      flags |= SystemDictionary::SAME_POS_FLAG;
    } else {
      flags |= SystemDictionary::FULL_POS_FLAG;
    }
  }
  // Determine key encoding.
  if (token_info->same_as_key) {
    flags |= SystemDictionary::AS_IS_TOKEN_FLAG;
  } else if (token_info->same_as_katakana) {
    flags |= SystemDictionary::KATAKANA_TOKEN_FLAG;
  } else if (prev_token &&
             prev_token->token->value == t->value) {
    flags |= SystemDictionary::SAME_VALUE_FLAG;
  }

  // Encodes token into bytes.
  uint8 b[12];
  b[0] = flags;
  int offset;
  CHECK(t->cost < 32768) << "Assuming cost is within 15bits.";
  b[1] = t->cost >> 8;
  b[2] = t->cost & 0xff;
  offset = 3;

  if (flags & SystemDictionary::FULL_POS_FLAG) {
    b[offset] = t->lid & 255;
    b[offset + 1] = t->lid >> 8;
    b[offset + 2] = t->rid & 255;
    b[offset + 3] = t->rid >> 8;
    offset += 4;
  } else if (!(flags & SystemDictionary::SAME_POS_FLAG)) {
    // Frequent 1 byte pos.
    b[offset] = frequent_pos_[pos];
    offset += 1;
  }

  if (!(flags & (SystemDictionary::AS_IS_TOKEN_FLAG |
                 SystemDictionary::KATAKANA_TOKEN_FLAG |
                 SystemDictionary::SAME_VALUE_FLAG))) {
    uint32 idx = token_info->offset_in_word_rx;
    if (idx >= 0x400000) {
      // This code runs in dictionary_compiler. So, we can use
      // LOG(FATAL) here.
      LOG(FATAL) << "Too large word rx (should be less than 2^22).";
    }
    b[offset] = idx & 255;
    b[offset + 1] = (idx >> 8) & 255;
    b[offset + 2] = (idx >> 16) & 255;
    offset += 3;
  }

  ofs->write(reinterpret_cast<char *>(b), offset);
}

void SystemDictionaryBuilder::WriteFrequentPos() {
  OutputFileStream ofs(frequent_pos_filename_.c_str(),
                       ios::binary|ios::out);
  uint32 vec[256];
  memset(vec, 0, sizeof(vec));
  map<uint32, int>::iterator it;
  for (it = frequent_pos_.begin(); it != frequent_pos_.end(); ++it) {
    vec[it->second] = it->first;
  }
  int i;
  for (i = 0; i < sizeof(vec) / sizeof(uint32); i++) {
    WriteInt(vec[i], &ofs);
  }
}

void SystemDictionaryBuilder::WriteTokenSection() {
  map<string, RxKeyStringInfo *>::iterator it;
  vector<string> id_map;
  id_map.resize(key_info_.size());
  for (it = key_info_.begin(); it != key_info_.end(); ++it) {
    id_map[it->second->offset_in_index_rx] = it->first;
  }

  rbx_builder *rb = rbx_builder_create();
  rbx_builder_set_length_coding(rb, SystemDictionary::kMinRBXBlobSize, 1);
  // Appends token information ordered by index id.
  for (vector<string>::const_iterator jt = id_map.begin();
       jt != id_map.end(); ++jt) {
    RxKeyStringInfo *ki = key_info_[*jt];
    WriteTokensForKey(*ki, rb);
  }
  // Appends termination flag.
  const char termination = SystemDictionary::TERMINATION_FLAG;
  rbx_builder_push(rb, &termination, 1);

  rbx_builder_build(rb);
  LOG(INFO) << "rbx size=" << rbx_builder_get_size(rb);
  unsigned char *image = rbx_builder_get_image(rb);
  int size = rbx_builder_get_size(rb);
  OutputFileStream ofs(tokens_filename_.c_str(), ios::binary|ios::out);
  ofs.write(reinterpret_cast<const char *>(image), size);
  rbx_builder_release(rb);
}

void SystemDictionaryBuilder::WriteTokensForKey(const RxKeyStringInfo &key,
                                                rbx_builder *rb) {
  ostringstream os;
  for (int i = 0; i < key.tokens.size(); ++i) {
    WriteToken(key, i, &os);
  }
  const string &output = os.str();
  rbx_builder_push(rb, output.data(), output.size());
}

bool SystemDictionaryBuilder::BuildIndexRxMap(
    const vector<Token *>& tokens,
    const hash_map<string, int> &mapping) {
  vector<Token *>::const_iterator it;
  LOG(INFO) << "building index rx map for " << tokens.size();
  for (it = tokens.begin(); it != tokens.end(); ++it) {
    string s;
    SystemDictionary::EncodeIndexString((*it)->key, &s);
    int value = 0;
    hash_map<string, int>::const_iterator jt = mapping.find(s);
    if (jt != mapping.end()) value = jt->second;
    key_info_[(*it)->key]->offset_in_index_rx = value;
  }
  LOG(INFO) << "done";
  return true;
}

void SystemDictionaryBuilder::ConcatFiles() {
  scoped_ptr<DictionaryFile> f(new DictionaryFile());
  f->Open(output_filename_.c_str(), true);
  f->AddSection("f", frequent_pos_filename_.c_str());
  f->AddSection("i", index_rx_filename_.c_str());
  f->AddSection("T", tokens_filename_.c_str());
  f->AddSection("R", token_rx_filename_.c_str());
  f->Write();
}

void SystemDictionaryBuilder::CollectFrequentPos(
    const vector<Token *>& tokens) {
  // Calculate frequency of each pos
  map<uint32, int> pos_map;
  for (vector<Token *>::const_iterator it = tokens.begin();
       it != tokens.end(); ++it) {
    uint32 pos = 0;
    pos = (*it)->lid;
    pos <<= 16;
    pos |= (*it)->rid;
    pos_map[pos]++;
  }
  // Get histgram of frequency
  map<int, int> freq_map;
  for (map<uint32, int>::iterator jt = pos_map.begin();
       jt != pos_map.end(); ++jt) {
    freq_map[jt->second]++;
  }
  // Compute the lower threshold of frequence
  int num_freq_pos = 0;
  int freq_threshold = INT_MAX;
  for (map<int, int>::reverse_iterator kt = freq_map.rbegin();
       kt != freq_map.rend(); ++kt) {
    if (num_freq_pos + kt->second > 255) {
      break;
    }
    freq_threshold = kt->first;
    num_freq_pos += kt->second;
  }

  // Collect high frequent pos
  LOG(INFO) << "num_freq_pos" << num_freq_pos;
  LOG(INFO) << "POS threshold=" << freq_threshold;
  int freq_pos_idx = 0;
  int num_tokens = 0;
  map<uint32, int>::iterator lt;
  for (lt = pos_map.begin(); lt != pos_map.end(); ++lt) {
    if (lt->second >= freq_threshold) {
      frequent_pos_[lt->first] = freq_pos_idx;
      freq_pos_idx++;
      num_tokens += lt->second;
    }
  }
  CHECK(freq_pos_idx == num_freq_pos)
      << "inconsistent result to find frequent pos";
  LOG(INFO) << freq_pos_idx << " high frequent POS has "
            << num_tokens << " tokens";
}

void SystemDictionaryBuilder::Build() {
  LOG(INFO) << "reading file\n";
  TextDictionaryLoader d;
  d.Open(input_filename_.c_str());
  // Get all tokens
  vector<Token *> tokens;
  d.CollectTokens(&tokens);
  LOG(INFO) << tokens.size() << " tokens\n";
  BuildFromTokenVector(tokens);
}

void SystemDictionaryBuilder::BuildFromTokenVector(
    const vector<Token *>& tokens) {
  CollectFrequentPos(tokens);
  BuildTokenInfo(tokens);

  // Token Rx
  {
    hash_map<string, int> token_mapping;
    CHECK(WriteTokenRx(&token_mapping)) << "failed to write token Rx";
    CHECK(BuildTokenRxMap(token_mapping)) << "failed to build token Rx map";
    SetIndexInTokenRx();
    SortTokenInfo();
  }

  // Index Rx
  {
    hash_map<string, int> index_mapping;
    CHECK(WriteIndexRx(tokens, &index_mapping))
        << "failed to write index Rx";
    // mapping from index number in Rx to key string.
    CHECK(BuildIndexRxMap(tokens, index_mapping))
        << "failed to build index Rx map";
  }

  LOG(INFO) << "start to write dictionary\n";

  // writes tokens
  WriteTokenSection();

  // writes frequent pos
  WriteFrequentPos();

  ConcatFiles();

  if (!FLAGS_preserve_intermediate_dictionary) {
    Util::Unlink(index_rx_filename_);
    Util::Unlink(token_rx_filename_);
    Util::Unlink(tokens_filename_);
    Util::Unlink(frequent_pos_filename_);
  }
}

void SystemDictionaryBuilder::BuildTokenInfo(const vector<Token *>& tokens) {
  vector<Token *>::const_iterator it;
  for (it = tokens.begin(); it != tokens.end(); ++it) {
    CHECK(!(*it)->key.empty()) << "empty key string in input";
    CHECK(!(*it)->value.empty()) << "empty value string in input";
    const string &key = (*it)->key;
    RxKeyStringInfo *ki = NULL;
    map<string, RxKeyStringInfo*>::const_iterator jt = key_info_.find(key);
    if (jt == key_info_.end()) {
      ki = new RxKeyStringInfo;
      key_info_[key] = ki;
    } else {
      ki = key_info_[key];
    }
    uint32 pos = ((*it)->lid << 16) | (*it)->rid;
    bool is_frequent_pos = (frequent_pos_.find(pos) != frequent_pos_.end());
    RxTokenInfo *t = new RxTokenInfo(*it, is_frequent_pos);
    // inserts into map
    ki->tokens.push_back(t);
  }
}
}  // namespace mozc

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

#ifndef IME_MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_BUILDER_H_
#define IME_MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_BUILDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/hash_tables.h"

struct rbx_builder;

namespace mozc {

class OutputFileStream;
struct Token;
struct RxKeyStringInfo;
struct RxTokenInfo;

class SystemDictionaryBuilder {
 public:
  // This constructor sets input/output file names
  SystemDictionaryBuilder(const string& input, const string& output);
  virtual ~SystemDictionaryBuilder();

  // Builds actual dictionary file
  void Build();
  void BuildFromTokenVector(const vector<Token *>& tokens);

  // compile dictionary
  static void Compile(const char *text_file,
                      const char *binary_file);

 private:
  bool BuildRxFile(const vector<string>& word_list, const string& fn,
                   hash_map<string, int>* mapping);
  // Writes an int value
  void WriteInt(int val, OutputFileStream *ofs);
  bool WriteTokenRx(hash_map<string, int>* mapping);
  bool WriteIndexRx(const vector<Token *>& tokens,
                    hash_map<string, int>* mapping);
  void WriteFrequentPos();
  void WriteToken(const RxKeyStringInfo &key, int n,
                  ostream *ofs);
  // Writes tokens
  void WriteTokenSection();
  void WriteTokensForKey(const RxKeyStringInfo &key, rbx_builder *rb);
  // Generates the output dictionary file from section files
  void ConcatFiles();

  void BuildTokenInfo(const vector<Token *>& tokens);
  bool BuildTokenRxMap(const hash_map<string, int>& mapping);
  void SetIndexInTokenRx();
  void SortTokenInfo();
  bool BuildIndexRxMap(const vector<Token *>& tokens,
                       const hash_map<string, int>& mapping);
  void CollectFrequentPos(const vector<Token *>& tokens);

  // source TSV file name
  const string input_filename_;
  // output dictionary name
  const string output_filename_;

  // mapping from a key string to Tokens.
  map<string, RxKeyStringInfo *> key_info_;
  // maps from token string to index in token Rx.
  map<string, int> token_rx_map_;

  // maps from {left_id,right_id} to POS index (0~255)
  map<uint32, int> frequent_pos_;

  // file names of each section
  string index_rx_filename_;
  string token_rx_filename_;
  string tokens_filename_;
  string frequent_pos_filename_;
};
}  // namespace mozc

#endif  // IME_MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_BUILDER_H_

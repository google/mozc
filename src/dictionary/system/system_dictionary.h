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

// This class wraps Rx library code for mozc

#ifndef IME_MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_
#define IME_MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_

#include <map>
#include <string>
#include <vector>

#include "base/base.h"
#include "dictionary/dictionary_interface.h"
#include "testing/base/public/gunit_prod.h"  // for FRIEND_TEST
#include "third_party/rx/v1_0rc2/rx.h"

namespace mozc {

class NodeAllocatorInterface;
class DictionaryFile;
struct Token;

class SystemDictionary : public DictionaryInterface {
 public:
  SystemDictionary();
  virtual ~SystemDictionary();

  virtual bool Open(const char *filename);
  virtual bool OpenFromArray(const char *ptr, int len);
  virtual void Close();

  virtual Node *LookupPredictive(const char *str, int size,
                                 NodeAllocatorInterface *allocator) const;
  virtual Node *LookupExact(const char *str, int size,
                            NodeAllocatorInterface *allocator) const;
  virtual Node *LookupPrefix(const char *str, int size,
                             NodeAllocatorInterface *allocator) const;
  virtual Node *LookupReverse(const char *str, int size,
                              NodeAllocatorInterface *allocator) const;


  // Most of key strings are Hiragana or Katakana, so we modify
  // utf8 encoding and assign 1byte character code instead of
  // 3 bytes.
  static void EncodeIndexString(const string &src,
                                string *dst);
  static void DecodeIndexString(const string &src,
                                string *dst);
  // Encodes/Decodes Japanese characters into 1 or 2 bytes.
  static void EncodeTokenString(const string &src,
                                string *dst);
  // Helper function of EncodeTokenString()
  static void EncodeTokenStringWithLength(const string &src,
                                          int length,
                                          string *dst);
  static void DecodeTokenString(const string &src,
                                string *dst);

  // Returns some non-zero number if 1st character of str is hiragana.
  static uint8 HiraganaCode(const char *str);

  static SystemDictionary *GetSystemDictionary();

  // flags of each token in dictionary file
  // Same as index hiragana word
  static const uint8 AS_IS_TOKEN_FLAG = 0x01;
  // Same as index katakana word
  static const uint8 KATAKANA_TOKEN_FLAG = 0x2;
  // has same left/right id as previous token
  static const uint8 SAME_POS_FLAG = 0x04;
  // has same word
  static const uint8 SAME_VALUE_FLAG = 0x08;
  // POS(left/right ID) is coded into 16 bits
  static const uint8 FULL_POS_FLAG = 0x10;
  // This token is last token for a index word
  static const uint8 LAST_TOKEN_FLAG = 0x80;
  // Mask to get upper 6bits from flags value
  static const uint8 UPPER_INDEX_MASK = 0x3f;
  // Last blob
  static const uint8 TERMINATION_FLAG = 0xff;

  // rbx setting (4 is same as the default).
  static const int kMinRBXBlobSize = 4;

  static const int kMaxTokensPerLookup = 10000;

  // Spelling Correction tokens are distinguished by offset of lid
  static const int kSpellingCorrectionPosOffset = 10000;

 private:
  FRIEND_TEST(SystemDictionaryTest, test_words);
  FRIEND_TEST(SystemDictionaryTest, test_prefix);
  FRIEND_TEST(SystemDictionaryTest, test_predictive);
  FRIEND_TEST(SystemDictionaryTest, index_coding);
  FRIEND_TEST(SystemDictionaryTest, index_coding_all);
  FRIEND_TEST(SystemDictionaryTest, token_coding);
  FRIEND_TEST(SystemDictionaryTest, nodes_size);


  // This symbol in encoded index string escapes following 1 byte.
  static const uint8 INDEX_CHAR_MARK_ESCAPE = 0xff;
  // following 2 characters in index string is encoded into 1 byte,
  // since they are frequent.
  static const uint8 INDEX_CHAR_PROLONGED_SOUND = 0xfd;
  static const uint8 INDEX_CHAR_MIDDLE_DOT = 0xfe;

  static const uint8 TOKEN_CHAR_MARK_MIN = 0xfd;
  // ASCII character.
  static const uint8 TOKEN_CHAR_MARK_ASCII = 0xfd;
  // UCS2 character 0x??00.
  static const uint8 TOKEN_CHAR_MARK_XX00 = 0xfe;
  // This UCS2 character is neither Hiragana nor above 2 patterns.
  static const uint8 TOKEN_CHAR_MARK_OTHER = 0xff;

  static const int KANJI_OFFSET = 1;
  static const int HIRAGANA_OFFSET = 75;
  static const int KATAKANA_OFFSET = 159;

  bool OpenDictionaryFile(DictionaryFile *file);
  // Only populates token pointed by positition when it is specified.
  // Otherwise (position==NULL) it scans all the tokens for same reading.
  void ReadTokens(const string& key, const uint8* ptr,
                  int new_pos,
                  vector<Token *>* res) const;
  int DecodeToken(const string& key, const uint8* ptr,
                  const Token* prev_token, Token* t, int *pos) const;
  // Returns list of nodes.
  // This method updates max_nodes_size value if non NULL value is given.
  Node *LookupInternal(const char *str, int size,
                       NodeAllocatorInterface *allocator,
                       bool is_predictive,
                       int *max_nodes_size) const;
  Node *CopyTokenToNode(NodeAllocatorInterface *allocator,
                        const Token *token) const;

  // Rx stores a trie. rx_ stores key strings and token_rx_ stores
  // value strings.
  rx *rx_;
  rx *token_rx_;
  // rbx stores array of blobs. It stores pos/cost information of each token.
  rbx *rbx_;
  scoped_ptr<DictionaryFile> df_;
  const uint32 *frequent_pos_;
  bool opened_;

  DISALLOW_COPY_AND_ASSIGN(SystemDictionary);
};
}  // namespace mozc

#endif  // IME_MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_

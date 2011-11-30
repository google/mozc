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

#ifndef MOZC_DICTIONARY_TEXT_DICTIONARY_LOADER_H_
#define MOZC_DICTIONARY_TEXT_DICTIONARY_LOADER_H_

#include <string>
#include <vector>

#include "testing/base/public/gunit_prod.h"
// for FRIEND_TEST

namespace mozc {

struct Token;

class TextDictionaryLoader {
 public:
  TextDictionaryLoader();
  virtual ~TextDictionaryLoader();

  // Reads source dictionary file.
  bool Open(const char *filename);
  // Reads source dictionary file at most lines_limit lines. No limit will
  // be applied when the limit is -1.
  // This is mainly used for test to reduce run time.
  // You can pass multiple filenames delimiterd by ",".
  bool OpenWithLineLimit(const char *filename, int lines_limit);

  void Close();

  // Get the pointers to dictionary tokens.
  // Note that tokens will be deleted when TextDictionaryLoader is deleted
  // or Close is called.
  void CollectTokens(vector<Token *> *res);

 private:
  FRIEND_TEST(TextDictionaryLoaderTest, RewriteSpecialTokenTest);

  // Encode special information into |token| with the |label|.
  // Currently, label must be empty, "SPELLINC_CORRECITON" or "ZIP_CODE".
  bool RewriteSpecialToken(Token *token, const string &label);

  void ParseTSV(const string &line);

  vector<Token *> tokens_;
};
}  // namespace mozc

#endif  // MOZC_DICTIONARY_TEXT_DICTIONARY_LOADER_H_

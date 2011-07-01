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

#ifndef MOZC_DICTIONARY_SUFFIX_DICTIONARY_H_
#define MOZC_DICTIONARY_SUFFIX_DICTIONARY_H_

#include <string>
#include "base/base.h"
#include "dictionary/dictionary_interface.h"

namespace mozc {

// SuffixDictionary is a special dictionary which handles
// Japanese bunsetsu suffixes.
//
// Japanese bunsetsu consists of two parts.
// Content words: ("自立語") and Functional words ("付属語")
// Japanese Bunsets = (Content word){1,1}(Functional words){1,}
//
// Suffix dictionary contains sequences of functional words
// frequently appear on the web.
// When user inputs a content word, we can predict an appropriate
// functional word with this dictionary.
class SuffixDictionary : public DictionaryInterface {
 public:
  SuffixDictionary();
  virtual ~SuffixDictionary();

  // Even if size == 0, suffix dictionary returns all the suffix
  // in this dictionary.
  virtual Node *LookupPredictive(const char *str, int size,
                                 NodeAllocatorInterface *allocator) const;

  // SuffixDictionary doesn't support Prefix/Revese Lookup.
  virtual Node *LookupPrefix(const char *str, int size,
                             NodeAllocatorInterface *allocator) const;
  virtual Node *LookupReverse(const char *str, int size,
                              NodeAllocatorInterface *allocator) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SuffixDictionary);
};

class SuffixDictionaryFactory {
 public:
  // return singleton object
  static DictionaryInterface *GetSuffixDictionary();
  static void SetSuffixDictionary(DictionaryInterface *suffix_dictionary);

 private:
  SuffixDictionaryFactory() {}
  ~SuffixDictionaryFactory() {}
};
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SUFFIX_DICTIONARY_H_

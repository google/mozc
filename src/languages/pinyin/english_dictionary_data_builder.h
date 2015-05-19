// Copyright 2010-2013, Google Inc.
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

// Build english dictionary data for ibus-mozc-pinyin.

#ifndef MOZC_LANGUAGES_PINYIN_ENGLISH_DICTIONARY_DATA_BUILDER_H_
#define MOZC_LANGUAGES_PINYIN_ENGLISH_DICTIONARY_DATA_BUILDER_H_

#include "base/base.h"

namespace mozc {
namespace storage {
namespace louds {
class LoudsTrieBuilder;
}  // namespace louds
}  // namespace storage

namespace pinyin {
namespace english {

// This class expects that word number of the english dictionary is less than
// 65536 because we use short int to reduce footprint.
class EnglishDictionaryDataBuilder {
 public:
  EnglishDictionaryDataBuilder();
  ~EnglishDictionaryDataBuilder();

  void BuildFromStream(istream *input_stream);
  void WriteToStream(ostream *output_stream) const;

 private:
  scoped_ptr<mozc::storage::louds::LoudsTrieBuilder> builder_;
  scoped_array<float> louds_id_to_priority_;
  int words_num_;

  DISALLOW_COPY_AND_ASSIGN(EnglishDictionaryDataBuilder);
};

}  // namespace english
}  // namespace pinyin
}  // namespace mozc

// MOZC_LANGUAGES_PINYIN_ENGLISH_DICTIONARY_DATA_BUILDER_H_
#endif

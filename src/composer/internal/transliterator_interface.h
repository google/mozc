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

#ifndef MOZC_COMPOSER_INTERNAL_TRANSLITERATOR_INTERFACE_H_
#define MOZC_COMPOSER_INTERNAL_TRANSLITERATOR_INTERFACE_H_

#include <string>

namespace mozc {
namespace composer {

class TransliteratorInterface {
 public:
  virtual ~TransliteratorInterface() {}

  // Return the transliterated string of either raw or converted.
  // Determination of which argument is used depends on the
  // implementation.
  //
  // Expected usage examples:
  // - HalfKatakanaTransliterator("a", "あ") => "ｱ"
  // - FullAsciiTransliterator("a", "あ") => "ａ"
  virtual string Transliterate(const string &raw,
                               const string &converted) const = 0;

  // Split raw and converted strings based on the transliteration
  // rule.  If raw or converted could not be deterministically split,
  // fall back strings are fill and false is returned.  The first
  // arugment, position, is in character (rather than byte).
  //
  // Expected usage examples:
  // - HiraganaTransliterator(1, "kk", "っk") => true
  //   (raw_lhs, raw_rhs) => ("k", "k")
  //   (conv_lhs, conv_rhs) => ("っ", "k")
  // - HalfKatakanaTransliterator(1, "zu", "ず") => false
  //   (raw_lhs, raw_rhs) => ("す", "゛")  fall back strings.
  //   (conv_lhs, conv_rhs) => ("す", "゛")
  virtual bool Split(size_t position,
                     const string &raw,
                     const string &converted,
                     string *raw_lhs, string *raw_rhs,
                     string *converted_lhs, string *converted_rhs) const = 0;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_TRANSLITERATOR_INTERFACE_H_

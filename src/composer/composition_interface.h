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

#ifndef MOZC_COMPOSER_INTERNAL_COMPOSITION_INTERFACE_H_
#define MOZC_COMPOSER_INTERNAL_COMPOSITION_INTERFACE_H_

#include <string>

namespace mozc {
namespace composer {

class Table;
class TransliteratorInterface;

enum TrimMode {
  TRIM,  // "かn" => "か"
  ASIS,  // "かn" => "かn"
  FIX,   // "かn" => "かん"
};

class CompositionInterface {
 public:
  virtual ~CompositionInterface() {}
  virtual size_t DeleteAt(size_t position) = 0;
  virtual size_t InsertAt(size_t position, const string &input) = 0;
  virtual size_t InsertKeyAndPreeditAt(size_t pos,
                                       const string &key,
                                       const string &preedit) = 0;

  virtual void Erase() = 0;

  // Get the position on mode_to from position_from on mode_from.
  virtual size_t ConvertPosition(
      size_t position_from,
      const TransliteratorInterface *transliterator_from,
      const TransliteratorInterface *transliterator_to) = 0;

  // TODO(komatsu): To be deleted.
  virtual size_t SetDisplayMode(
      size_t position,
      const TransliteratorInterface *transliterator) = 0;

  virtual void SetTransliterator(
      size_t position_from,
      size_t position_to,
      const TransliteratorInterface *transliterator) = 0;
  virtual const TransliteratorInterface *GetTransliterator(size_t position) = 0;

  virtual size_t GetLength() const = 0;

  // Return string with the default translitarator of each char_chunk
  // and TrimeMode::ASIS.
  virtual void GetString(string *composition) const = 0;

  // Return string with the specified transliterator and TrimeMode::FIX.
  virtual void GetStringWithTransliterator(
      const TransliteratorInterface *transliterator,
      string *output) const = 0;

  // Return string with the specified trim mode and the current display mode.
  virtual void GetStringWithTrimMode(TrimMode trim_mode,
                                     string *output) const = 0;

  // Return string with the default translitarator of each char_chunk
  // and TrimMode::ASIS.
  virtual void GetPreedit(size_t position,
                          string *left,
                          string *focused,
                          string *right) const = 0;
  virtual void SetTable(const Table *table) = 0;
  virtual void SetInputMode(const TransliteratorInterface *transliterator) = 0;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_COMPOSITION_INTERFACE_H_

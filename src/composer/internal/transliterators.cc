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

#include "composer/internal/transliterators.h"

#include "base/singleton.h"
#include "base/util.h"

namespace mozc {
namespace composer {

namespace {

bool SplitPrimaryString(const size_t position,
                        const string &primary,
                        const string &secondary,
                        string *primary_lhs, string *primary_rhs,
                        string *secondary_lhs, string *secondary_rhs) {
  DCHECK(primary_lhs);
  DCHECK(primary_rhs);
  DCHECK(secondary_lhs);
  DCHECK(secondary_rhs);

  *primary_lhs = Util::SubString(primary, 0, position);
  *primary_rhs = Util::SubString(primary, position, string::npos);

  // Set fallback strings.
  *secondary_lhs = *primary_lhs;
  *secondary_rhs = *primary_rhs;

  // If secondary and primary have the same suffix like "ttk" and "っtk",
  // Separation of primary can be more intelligent.

  if (secondary.size() < primary_rhs->size()) {
    // If the size of suffix of primary is greater than the whole size
    // of secondary, it must not a "ttk" and "っtk" case.
    return false;
  }

  // Position on the secondary string, where secondary and primary have the
  // same suffix.
  const size_t secondary_position = secondary.size() - primary_rhs->size();

  // Check if the secondary and primary have the same suffix.
  if (secondary_position != secondary.rfind(*primary_rhs)) {
    return false;
  }

  *secondary_rhs = *primary_rhs;
  *secondary_lhs = secondary.substr(0, secondary_position);
  return true;
}


// Singleton class which always uses a converted string rather than a
// raw string.
class ConversionStringSelector : public TransliteratorInterface {
 public:
  virtual ~ConversionStringSelector() {}

  string Transliterate(const string &raw, const string &converted) const {
    return converted;
  }

  // NOTE(komatsu): The first argument, size_t postion, should not be
  // const because this function overrides the virtual function of
  // TransliterateInterface whose first argument is not const.
  // Otherwise the Windows compiler (cl.exe) raises an error.
  bool Split(size_t position,
             const string &raw,
             const string &converted,
             string *raw_lhs, string *raw_rhs,
             string *converted_lhs, string *converted_rhs) const {
    return Transliterators::SplitConverted(
        position, raw, converted,
        raw_lhs, raw_rhs, converted_lhs, converted_rhs);
  }
};

// Singleton class which always uses a raw string rather than a
// converted string.
class RawStringSelector : public TransliteratorInterface {
 public:
  virtual ~RawStringSelector() {}

  string Transliterate(const string &raw, const string &converted) const {
    return raw;
  }

  bool Split(size_t position,
             const string &raw,
             const string &converted,
             string *raw_lhs, string *raw_rhs,
             string *converted_lhs, string *converted_rhs) const {
    return Transliterators::SplitRaw(
        position, raw, converted,
        raw_lhs, raw_rhs, converted_lhs, converted_rhs);
  }
};

}  // anonymous namespace

// static
const TransliteratorInterface*
Transliterators::GetConversionStringSelector() {
  return Singleton<ConversionStringSelector>::get();
}

// static
const TransliteratorInterface*
Transliterators::GetRawStringSelector() {
  return Singleton<RawStringSelector>::get();
}

// static
bool Transliterators::SplitRaw(const size_t position,
                               const string &raw,
                               const string &converted,
                               string *raw_lhs, string *raw_rhs,
                               string *converted_lhs, string *converted_rhs) {
  return SplitPrimaryString(position, raw, converted,
                            raw_lhs, raw_rhs, converted_lhs, converted_rhs);
}

// static
bool Transliterators::SplitConverted(
    const size_t position,
    const string &raw,
    const string &converted,
    string *raw_lhs, string *raw_rhs,
    string *converted_lhs, string *converted_rhs) {
  return SplitPrimaryString(position, converted, raw,
                            converted_lhs, converted_rhs, raw_lhs, raw_rhs);
}

}  // namespace composer
}  // namespace mozc

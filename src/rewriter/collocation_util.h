// Copyright 2010-2021, Google Inc.
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

// Util class for collocation

#ifndef MOZC_REWRITER_COLLOCATION_UTIL_H_
#define MOZC_REWRITER_COLLOCATION_UTIL_H_

#include <string>

#include "absl/strings/string_view.h"

namespace mozc {

class CollocationUtil {
 public:
  CollocationUtil() = delete;
  CollocationUtil(const CollocationUtil&) = delete;
  CollocationUtil& operator=(const CollocationUtil&) = delete;

  // Gets normalized script
  // Removes or rewrites some symbols.
  // for example:
  // "一個" -> "個" (removes 'number' if |remove_number| is true)
  // "%％" -> "%%" (full width '%' to half width)
  static void GetNormalizedScript(const absl::string_view str,
                                  bool remove_number, std::string* output);

  // Returns true if given char is number including kanji.
  static bool IsNumber(char32_t c);

 private:
  // Removes characters for normalizing.
  static void RemoveExtraCharacters(const absl::string_view input,
                                    bool remove_number, std::string* output);
};

}  // namespace mozc

#endif  // MOZC_REWRITER_COLLOCATION_UTIL_H_

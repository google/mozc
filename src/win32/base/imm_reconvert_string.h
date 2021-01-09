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

#ifndef MOZC_WIN32_BASE_IMM_RECONVERT_STRING_H_
#define MOZC_WIN32_BASE_IMM_RECONVERT_STRING_H_

// clang-format off
#include <windows.h>
#include <ime.h>
// clang-format on

#include <string>

#include "base/port.h"

namespace mozc {
namespace win32 {
// This class is designed to compose/decompose RECONVERTSTRING structure in
// safe way.
// According to MSDN http://msdn.microsoft.com/en-us/library/dd319107.aspx,
// a valid RECONVERTSTRING satisfies following conditions:
//  - the target string range is a subset of the composition string range.
//  - the composition string range is a subset of the entire string range.
// which means RECONVERTSTRING can always be decomposed into 5 substrings.
// For example, "He[CB]llo[TB],[TE] worl[CE]d!" can be decomposed into
//   - preceding_text       :  "He"
//   - preceding_composition:  "llo"
//   - target               :  ","
//   - following_composition:  " worl"
//   - following_text       :  "d!"
// where,
//   - [CB]: CompositionBegin
//   - [CE]: CompositionEnd
//   - [TB]: TargetBegin
//   - [TE]: TargetEnd
class ReconvertString {
 public:
  // Returns true if given substrings are copied into |reconvert_string|.
  // The caller is responsible for allocating enough memory for
  // |reconvert_string|.
  static bool Compose(const std::wstring &preceding_text,
                      const std::wstring &preceding_composition,
                      const std::wstring &target,
                      const std::wstring &following_composition,
                      const std::wstring &following_text,
                      RECONVERTSTRING *reconvert_string);

  // Returns true if substrings are copied from |reconvert_string|.
  static bool Decompose(const RECONVERTSTRING *reconvert_string,
                        std::wstring *preceding_text,
                        std::wstring *preceding_composition,
                        std::wstring *target,
                        std::wstring *following_composition,
                        std::wstring *following_text);

  // Returns true if the given |reconvert_string| is valid.
  static bool Validate(const RECONVERTSTRING *reconvert_string);

  // If the composition range is empty, this function tries to update
  // |reconvert_string| so that characters in the compositoin consist of the
  // same script type.
  // If the composition range is not empty, this function does nothing.
  // Returns true if |reconvert_string| is valid and has a non-empty
  // composition range finally.
  static bool EnsureCompositionIsNotEmpty(RECONVERTSTRING *reconvert_string);

 private:
  DISALLOW_COPY_AND_ASSIGN(ReconvertString);
};

}  // namespace win32
}  // namespace mozc
#endif  // MOZC_WIN32_BASE_IMM_RECONVERT_STRING_H_

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

#include <memory>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>

#include "absl/base/attributes.h"

namespace mozc {
namespace win32 {

class ReconvertString;

namespace reconvert_string_internal {
void Deleter(ReconvertString *ptr);
}  // namespace reconvert_string_internal

// std::unique_ptr of ReconvertString with a custom deleter.
using UniqueReconvertString =
    std::unique_ptr<ReconvertString,
                    decltype(&reconvert_string_internal::Deleter)>;

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
class ReconvertString : public RECONVERTSTRING {
 public:
  struct Strings {
    std::wstring_view preceding_text;
    std::wstring_view preceding_composition;
    std::wstring_view target;
    std::wstring_view following_composition;
    std::wstring_view following_text;
  };

  enum class RequestType {
    kReconvertString,  // IMR_RECONVERTSTRING
    kDocumentFeed,     // IMR_DOCUMENTFEED
  };

  // Sends WM_IME_REQUEST to the windows specified by hwnd and returns the
  // result. The result is nullptr if it fails.
  static UniqueReconvertString Request(HWND hwnd, RequestType request_type);
  // Constructs and returns a UniqueReconvertString with the substrings copied
  // into the buffer.
  static UniqueReconvertString Compose(Strings ss);

  // Returns true if substrings are copied from |reconvert_string|.
  std::optional<Strings> Decompose() const ABSL_ATTRIBUTE_LIFETIME_BOUND;
  // Returns true if the ReconvertString is valid.
  bool Validate() const;

  // If the composition range is empty, this function tries to update
  // the ReconvertString so that characters in the composition consist of the
  // same script type.
  // If the composition range is not empty, this function does nothing.
  // Returns true if the ReconvertString is valid and has a non-empty
  // composition range finally.
  bool EnsureCompositionIsNotEmpty();

 private:
  bool ValidateOffsetSize(DWORD buf_size, DWORD offset, DWORD size) const;
  wchar_t *StringBuffer() {
    return std::launder(reinterpret_cast<wchar_t *>(this)) +
           dwStrOffset / sizeof(wchar_t);
  }
  const wchar_t *StringBuffer() const {
    return std::launder(reinterpret_cast<const wchar_t *>(this)) +
           dwStrOffset / sizeof(wchar_t);
  }
};

static_assert(std::is_standard_layout_v<ReconvertString>);
static_assert(std::alignment_of_v<wchar_t> <=
              std::alignment_of_v<ReconvertString>);

namespace reconvert_string_internal {
inline constexpr std::align_val_t kAlignment =
    std::align_val_t(std::alignment_of_v<ReconvertString>);

inline void Deleter(ReconvertString *ptr) {
  ptr->~ReconvertString();
  ::operator delete(ptr, kAlignment);
}
}  // namespace reconvert_string_internal

}  // namespace win32
}  // namespace mozc
#endif  // MOZC_WIN32_BASE_IMM_RECONVERT_STRING_H_

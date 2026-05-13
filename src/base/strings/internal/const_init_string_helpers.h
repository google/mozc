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

#ifndef MOZC_BASE_STRINGS_INTERNAL_CONST_INIT_STRING_HELPERS_H_
#define MOZC_BASE_STRINGS_INTERNAL_CONST_INIT_STRING_HELPERS_H_

#include <concepts>
#include <cstddef>
#include <memory>
#include <string_view>

namespace mozc::const_init_string_internal {

// The character types supported by the const-init string family. Used to
// constrain template parameters in the public class headers and in the
// helper templates below.
template <typename T>
concept SupportedChar = std::same_as<T, char> || std::same_as<T, wchar_t>;

// `StageHeapFallback` and `CommitStagedValue` are declarations only;
// definitions and explicit instantiations for `char` and `wchar_t`
// live in const_init_string_helpers.cc. Instantiating with any other
// character type is a link error by design.

// Stages a copy of `value` for publication. If `value.size() + 1`
// characters (including a NUL terminator) fit in `inline_capacity`,
// returns a null pointer and the caller is expected to commit to its
// own inline buffer; otherwise returns a fresh, NUL-terminated heap
// copy.
template <SupportedChar CharT>
std::unique_ptr<CharT[]> StageHeapFallback(std::basic_string_view<CharT> value,
                                           size_t inline_capacity);

// Commits the staged value to its destination: if `heap_fallback` is
// non-null its pointer is released and returned; otherwise `value` is
// copied into `inline_buffer` (which the caller must have sized to
// accommodate `value.size() + 1` characters; see `StageHeapFallback`)
// and a pointer to it is returned. The result is always NUL-terminated.
template <SupportedChar CharT>
CharT* CommitStagedValue(std::basic_string_view<CharT> value,
                         std::unique_ptr<CharT[]>& heap_fallback,
                         CharT* inline_buffer);

}  // namespace mozc::const_init_string_internal

#endif  // MOZC_BASE_STRINGS_INTERNAL_CONST_INIT_STRING_HELPERS_H_

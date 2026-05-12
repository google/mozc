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

#include "base/strings/internal/const_init_string_helpers.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string_view>

namespace mozc::const_init_string_internal {

template <SupportedChar CharT>
std::unique_ptr<CharT[]> StageHeapFallback(std::basic_string_view<CharT> value,
                                           size_t inline_capacity) {
  const size_t size_with_null = value.size() + 1;
  if (size_with_null <= inline_capacity) {
    return nullptr;
  }
  // `make_unique_for_overwrite` (default-init) over `make_unique`
  // (value-init) avoids zero-filling the buffer before `std::copy_n`
  // overwrites it.
  auto heap = std::make_unique_for_overwrite<CharT[]>(size_with_null);
  std::copy_n(value.data(), value.size(), heap.get());
  heap[value.size()] = CharT(0);
  return heap;
}

template <SupportedChar CharT>
CharT* CommitStagedValue(std::basic_string_view<CharT> value,
                         std::unique_ptr<CharT[]>& heap_fallback,
                         CharT* inline_buffer) {
  if (heap_fallback) {
    return heap_fallback.release();
  }
  std::copy_n(value.data(), value.size(), inline_buffer);
  inline_buffer[value.size()] = CharT(0);
  return inline_buffer;
}

// Explicit instantiations: only `char` and `wchar_t` are supported.
template std::unique_ptr<char[]> StageHeapFallback<char>(
    std::basic_string_view<char>, size_t);
template std::unique_ptr<wchar_t[]> StageHeapFallback<wchar_t>(
    std::basic_string_view<wchar_t>, size_t);

template char* CommitStagedValue<char>(std::basic_string_view<char>,
                                       std::unique_ptr<char[]>&, char*);
template wchar_t* CommitStagedValue<wchar_t>(std::basic_string_view<wchar_t>,
                                             std::unique_ptr<wchar_t[]>&,
                                             wchar_t*);

}  // namespace mozc::const_init_string_internal

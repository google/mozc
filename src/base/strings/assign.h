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

#ifndef MOZC_BASE_STRINGS_ASSIGN_H_
#define MOZC_BASE_STRINGS_ASSIGN_H_

#include <cstddef>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/strings/string_view.h"

namespace mozc::strings {

// Assigns the value to std::string and return the reference to the string. This
// is a workaround for environments where absl::string_view is not an alias of
// std::string_view and cannot be directly assigned to std::string. std::string
// has a operator= for types convertible to std::string_view, but
// absl::string_view is not.
//
// This is more efficient than assigning by `s = std::string(sv)` because it can
// reuse the existing string buffer while cleaner than writing
// `s.assign(sv.data(), sv.size())` every time. It's also easier to refactor
// later once we migrate from absl::string_view to std::string_view.
//
// Use this function when assigning from absl::string_view or using template.
// Continue to use std::string::operator= if the operand is known to work.
template <typename T>
std::string& Assign(std::string& to,
                    T&& value) noexcept(noexcept(to = std::forward<T>(value))) {
  static_assert(std::is_assignable_v<std::string, T>,
                "T must be assignable to std::string.");
  to = std::forward<T>(value);
  return to;
}

inline std::string& Assign(std::string& to, const absl::string_view value) {
  to.assign(value.data(), value.size());
  return to;
}

inline std::string& Assign(std::string& to,
                           const std::initializer_list<char> ilist) {
  to = ilist;
  return to;
}

std::string& Assign(std::string& to, std::nullptr_t) = delete;

}  // namespace mozc::strings

#endif  // MOZC_BASE_STRINGS_ASSIGN_H_

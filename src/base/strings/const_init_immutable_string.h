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

#ifndef MOZC_BASE_STRINGS_CONST_INIT_IMMUTABLE_STRING_H_
#define MOZC_BASE_STRINGS_CONST_INIT_IMMUTABLE_STRING_H_

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>

#include "base/strings/internal/const_init_string_helpers.h"

namespace mozc {

// A utility class to deal with constant global strings whose value is known
// only at runtime. It has the following capabilities:
// 1. It allows the library users to lazily initialize the string by calling
//    `GetOrInit()` only after it becomes ready, e.g. only after dependent
//    modules are fully loaded.
// 2. It is thread-safe, meaning that multiple threads can call `GetOrInit()`
//    concurrently without causing data race, with an assumption that the
//    `idempotent_initializer` is idempotent (i.e., it always returns the same
//    value when called multiple times) and thread-safe.
// 3. It guarantees that the string is null-terminated.
// 4. It is trivially destructible, which means it can be used in static
//    storage duration objects without causing destructor order issues, with a
//    caveat that it may leak memory if the string is larger than the fixed
//    array size provided by the template parameter.
//
// Once the string is published it cannot be replaced. See
// `ConstInitMutableString` for a variant that additionally supports
// thread-safe `Set()`.
template <size_t fixed_array_size,
          const_init_string_internal::SupportedChar CharT = char>
class ConstInitImmutableString {
 public:
  using StringT = std::basic_string<CharT>;
  using StringViewT = std::basic_string_view<CharT>;

  // A data initializer that is guaranteed to return the same value no matter
  // how many times it is called. It must also be reentrant and safe to call
  // from multiple threads concurrently.
  using IdempotentInitializer = std::add_pointer_t<StringT()>;

  ConstInitImmutableString() = delete;
  ~ConstInitImmutableString() = default;

  consteval explicit ConstInitImmutableString(
      IdempotentInitializer idempotent_initializer)
      : idempotent_initializer_(idempotent_initializer) {}

  [[nodiscard]] StringViewT GetOrInit() {
    // Fast path: result already published.
    if (const CharT* ptr = result_ptr_.load(std::memory_order::acquire))
        [[likely]] {
      return StringViewT(ptr, result_size_);
    }

    // The initializer is called outside any lock so that no foreign code
    // runs under a lock held by this class. This avoids classic deadlock
    // scenarios such as Win32 Loader Lock recursion when `GetOrInit()` is
    // called from `DllMain` and the initializer internally invokes
    // `LoadLibrary`. Racing threads may each call it; per the
    // `IdempotentInitializer` contract that is acceptable.
    // https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices#deadlocks-caused-by-lock-order-inversion
    const StringT value = idempotent_initializer_();

    // Stage any heap fallback outside the spinlock so that the critical
    // section is bounded by `fixed_array_size` and a throwing
    // allocation cannot leave the spinlock held.
    std::unique_ptr<CharT[]> heap_fallback =
        const_init_string_internal::StageHeapFallback<CharT>(value,
                                                             std::size(value_));

    std::lock_guard<const_init_string_internal::TrivialMicroSpinLock> l(
        committing_);
    if (const CharT* ptr = result_ptr_.load(std::memory_order::relaxed)) {
      // Another thread already published; staged `heap_fallback` is
      // freed by its destructor when this scope unwinds.
      return StringViewT(ptr, result_size_);
    }

    // Winner: any staged heap buffer is intentionally leaked for the
    // remainder of the process so that `ConstInitImmutableString` itself
    // remains trivially destructible.
    CharT* dest = const_init_string_internal::CommitStagedValue<CharT>(
        value, heap_fallback, value_.data());
    result_size_ = value.size();
    result_ptr_.store(dest, std::memory_order::release);
    return StringViewT(dest, value.size());
  }

 private:
  std::atomic<const CharT*> result_ptr_ = nullptr;
  size_t result_size_ = 0;
  const_init_string_internal::TrivialMicroSpinLock committing_;
  std::array<CharT, fixed_array_size> value_ = {};
  const IdempotentInitializer idempotent_initializer_;
};

// Verify the trivial destructibility contract for both supported `CharT`
// instantiations so misuse (e.g. accidentally adding a non-trivial member)
// is caught at compile time even if no instance is constructed.
static_assert(
    std::is_trivially_destructible_v<ConstInitImmutableString<1, char>>);
static_assert(
    std::is_trivially_destructible_v<ConstInitImmutableString<1, wchar_t>>);

}  // namespace mozc

#endif  // MOZC_BASE_STRINGS_CONST_INIT_IMMUTABLE_STRING_H_

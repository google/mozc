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

#ifndef MOZC_BASE_STRINGS_CONST_INIT_MUTABLE_STRING_H_
#define MOZC_BASE_STRINGS_CONST_INIT_MUTABLE_STRING_H_

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

// A variant of `ConstInitImmutableString` that additionally supports
// thread-safe `Set()`. Like `ConstInitImmutableString`,
// `ConstInitMutableString` is trivially destructible and may be used in
// `constinit` globals.
//
// Semantics:
//   * `Get()` returns a snapshot of the most recent `Set` value, or an
//     empty string if `Set` has never been called. Empty thus doubles as
//     the "never set" sentinel; callers that need lazy default behaviour
//     should compute the default themselves on observing empty and call
//     `Set` to publish it.
//   * `Set(value)` atomically replaces the stored value.
//
// Storage:
//   * Values that fit in `fixed_array_size` characters (including the
//     terminating NUL) live in an inline array.
//   * Larger values live in a heap buffer. When `Set` replaces a heap-backed
//     value, the prior heap allocation is freed before `Set` returns.
//   * The currently-installed heap buffer (if any) is intentionally leaked at
//     process exit so that the class itself remains trivially destructible.
template <size_t fixed_array_size,
          const_init_string_internal::SupportedChar CharT = char>
class ConstInitMutableString {
 public:
  using StringT = std::basic_string<CharT>;
  using StringViewT = std::basic_string_view<CharT>;

  consteval ConstInitMutableString() noexcept = default;
  ~ConstInitMutableString() = default;

  void Set(StringViewT value);
  StringT Get();

 private:
  std::atomic<bool> initialized_ = {};
  CharT* current_ptr_ = nullptr;
  size_t current_size_ = 0;
  const_init_string_internal::TrivialMicroSpinLock committing_;
  std::array<CharT, fixed_array_size> value_ = {};
};

template <size_t fixed_array_size,
          const_init_string_internal::SupportedChar CharT>
void ConstInitMutableString<fixed_array_size, CharT>::Set(StringViewT value) {
  // Stage any heap fallback outside the spinlock so the critical section
  // is bounded by `fixed_array_size` and a throwing `bad_alloc` cannot
  // leave the spinlock held.
  std::unique_ptr<CharT[]> heap_fallback =
      const_init_string_internal::StageHeapFallback<CharT>(value,
                                                           std::size(value_));

  std::unique_ptr<CharT[]> old_heap;
  {
    std::lock_guard<const_init_string_internal::TrivialMicroSpinLock> l(
        committing_);
    // If the previously-installed buffer was on the heap, hand it to a
    // local `unique_ptr` so it is freed after we drop the lock. Readers
    // complete their copy under the same lock, so no one is left
    // holding the old pointer.
    if (current_ptr_ != nullptr && current_ptr_ != value_.data()) {
      old_heap.reset(current_ptr_);
    }
    current_ptr_ = const_init_string_internal::CommitStagedValue<CharT>(
        value, heap_fallback, value_.data());
    current_size_ = value.size();
    initialized_.store(true, std::memory_order::release);
  }
}

template <size_t fixed_array_size,
          const_init_string_internal::SupportedChar CharT>
auto ConstInitMutableString<fixed_array_size, CharT>::Get() -> StringT {
  // Lock-free fast exit when nothing has ever been set.
  if (!initialized_.load(std::memory_order::acquire)) {
    return StringT();
  }
  // Snapshot under the lock since `Set` may concurrently swap pointers.
  // The `lock_guard` releases the lock if the `StringT` allocation
  // here throws.
  std::lock_guard<const_init_string_internal::TrivialMicroSpinLock> l(
      committing_);
  return StringT(current_ptr_, current_size_);
}

// Verify the trivial destructibility contract for both supported `CharT`
// instantiations so misuse (e.g. accidentally adding a non-trivial member)
// is caught at compile time even if no instance is constructed.
static_assert(
    std::is_trivially_destructible_v<ConstInitMutableString<1, char>>);
static_assert(
    std::is_trivially_destructible_v<ConstInitMutableString<1, wchar_t>>);

}  // namespace mozc

#endif  // MOZC_BASE_STRINGS_CONST_INIT_MUTABLE_STRING_H_

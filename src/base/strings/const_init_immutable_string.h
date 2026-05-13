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
#include <string>
#include <string_view>
#include <type_traits>

#if defined(MOZC_NO_ATOMIC_FLAG_WAIT)
#include "absl/base/const_init.h"
#include "absl/synchronization/mutex.h"
#endif  // defined(MOZC_NO_ATOMIC_FLAG_WAIT)

#include "base/strings/internal/const_init_string_helpers.h"

namespace mozc {

// A utility class to deal with constant global strings whose value is known
// only at runtime. It has the following capabilities:
//
// 1. It allows the library users to lazily initialize the string by calling
//    GetOrInit() only after it becomes ready, e.g. only after dependent
//    modules are fully loaded.
// 2. It is thread-safe, meaning that multiple threads can call GetOrInit()
//    concurrently without causing data race, with an assumption that the
//    idempotent_initializer is thread-safe and *idempotent* (i.e., it always
//    returns the same value when called multiple times).
// 3. It guarantees that the string is null-terminated.
// 4. It accepts the constinit keyword.
//
// Synchronization model:
// ---------------------
// The initializer is invoked without any class-held state. This avoids lock
// order inversion when the initializer triggers foreign code that acquires
// its own locks -- e.g. a LoadLibrary() call from within DllMain on Windows,
// which Microsoft documents as a classic deadlock pattern:
//
//   https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices#deadlocks-caused-by-lock-order-inversion
//
// Racing threads may each invoke the initializer; per the IdempotentInitializer
// contract that is acceptable.
//
// Once the string is published it cannot be replaced. See
// ConstInitMutableString for a variant that additionally supports thread-safe
// Set().
//
// Cavets on destruction:
// ---------------------
//   * Without MOZC_NO_ATOMIC_FLAG_WAIT, this class is guaranteed to be
//     trivially destructible and can be safely used in the global scope without
//     worrying about destruction order at exit.
//   * With MOZC_NO_ATOMIC_FLAG_WAIT (e.g. server-side configurations where
//     std::atomic_flag::wait is not allowed), it falls back to absl::Mutex,
//     which offers better integration with TSAN. In this case it is no longer
//     trivially destructible, which means atexit-style destructions will happen
//     for globally instantiated objects. As explained in
//     absl/base/const_init.h, strictly speaking this can fall into undefined
//     behavior and we are just relying on known toolchain-specific behaviors
//     that are not guaranteed by the standard.
//
// Storage model:
// -------------
//   * Values that fit in fixed_array_size characters (including the terminating
//     NUL) live in an inline array.
//   * Larger values live in a heap buffer. When Set() replaces a heap-backed
//     value, the prior heap allocation is freed before Set() returns.
//   * The currently-installed heap buffer (if any) is intentionally leaked at
//     process exit.
#if !defined(MOZC_NO_ATOMIC_FLAG_WAIT)

// Trivially-destructible variant using `std::atomic_flag::wait`.
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

  consteval explicit ConstInitImmutableString(IdempotentInitializer init)
      : idempotent_initializer_(init) {}

  [[nodiscard]] StringViewT GetOrInit() {
    // Fast path: latch observed. Synchronization is on init_done_; the acquire
    // on test() already publishes all writes prior to the publisher's
    // release on init_done_.test_and_set(), so result_ptr_ can can be loaded
    // with relaxed ordering.
    if (init_done_.test(std::memory_order::acquire)) [[likely]] {
      return StringViewT(result_ptr_.load(std::memory_order::relaxed),
                         result_size_);
    }
    // Invoke the initializer and stage any heap fallback *outside* of
    // init_started_. Holding init_started_ across foreign code would introduce
    // the Loader-Lock-style lock-order-inversion deadlock described above;
    // staging outside also lets a throwing initializer on one thread leave the
    // instance recoverable for other threads.
    const StringT value = idempotent_initializer_();
    std::unique_ptr<CharT[]> heap_fallback =
        const_init_string_internal::StageHeapFallback<CharT>(value,
                                                             std::size(value_));
    if (!init_started_.test_and_set(std::memory_order::acquire)) {
      // Won the publish race. Commit the staged value.
      CharT* dest = const_init_string_internal::CommitStagedValue<CharT>(
          value, heap_fallback, value_.data());
      result_size_ = value.size();
      result_ptr_.store(dest, std::memory_order::relaxed);
      init_done_.test_and_set(std::memory_order::release);
      init_done_.notify_all();
      return StringViewT(dest, value.size());
    }
    // Lost the publish race. Block at the OS level until the winner publishes.
    init_done_.wait(false, std::memory_order::acquire);
    return StringViewT(result_ptr_.load(std::memory_order::relaxed),
                       result_size_);
  }

 private:
  // Hot fields: all three are touched on the fast path. Keep them together at
  // the front so they share a cache line regardless of `fixed_array_size`.
  std::atomic_flag init_done_ = {};
  std::atomic<const CharT*> result_ptr_ = nullptr;
  size_t result_size_ = 0;
  // Cold fields: only the slow init path touches these.
  const IdempotentInitializer idempotent_initializer_;
  std::atomic_flag init_started_ = {};
  std::array<CharT, fixed_array_size> value_ = {};
};

static_assert(
    std::is_trivially_destructible_v<ConstInitImmutableString<1, char>>);
static_assert(
    std::is_trivially_destructible_v<ConstInitImmutableString<1, wchar_t>>);

#else  // !defined(MOZC_NO_ATOMIC_FLAG_WAIT)

// absl::Mutex-based variant, which offers better integration with TSAN by
// giving up trivial destructibility and relying on toolchain-specific behaviors
// that are not guaranteed by the standard.
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

  consteval explicit ConstInitImmutableString(IdempotentInitializer init)
      : idempotent_initializer_(init) {}

  [[nodiscard]] StringViewT GetOrInit() {
    // Fast path: publication observed (no mutex_ involvement).
    if (const CharT* p = result_ptr_.load(std::memory_order::acquire))
        [[likely]] {
      return StringViewT(p, result_size_);
    }
    // Invoke the initializer and stage any heap fallback *outside* of mutex_.
    // See the class comment for the Loader-Lock-style lock-order-inversion
    // hazard this avoids.
    const StringT value = idempotent_initializer_();
    std::unique_ptr<CharT[]> heap_fallback =
        const_init_string_internal::StageHeapFallback<CharT>(value,
                                                             std::size(value_));
    absl::MutexLock l(mutex_);
    if (const CharT* p = result_ptr_.load(std::memory_order::relaxed)) {
      return StringViewT(p, result_size_);
    }
    CharT* dest = const_init_string_internal::CommitStagedValue<CharT>(
        value, heap_fallback, value_.data());
    result_size_ = value.size();
    result_ptr_.store(dest, std::memory_order::release);
    return StringViewT(dest, value.size());
  }

 private:
  // Hot fields: all two are touched on the fast path. Keep them together at
  // the front so they share a cache line regardless of `fixed_array_size`.
  std::atomic<const CharT*> result_ptr_ = nullptr;
  size_t result_size_ = 0;
  // Cold fields: only the slow init path touches these.
  const IdempotentInitializer idempotent_initializer_;
  absl::Mutex mutex_{absl::kConstInit};
  std::array<CharT, fixed_array_size> value_ = {};
};

#endif  // !defined(MOZC_NO_ATOMIC_FLAG_WAIT)

}  // namespace mozc

#endif  // MOZC_BASE_STRINGS_CONST_INIT_IMMUTABLE_STRING_H_

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

#include <atomic>
#include <concepts>
#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>

#if defined(_MSC_VER)
#include <intrin.h>  // _mm_pause, __yield
#endif  // _MSC_VER

namespace mozc::const_init_string_internal {

// The character types supported by the const-init string family. Used to
// constrain template parameters in the public class headers and in the
// helper templates below.
template <typename T>
concept SupportedChar = std::same_as<T, char> || std::same_as<T, wchar_t>;

// CPU spin-loop hint. Emits `PAUSE` on x86 or `YIELD` on ARM, and is a
// no-op on other architectures. On SMT cores it lets the sibling thread
// make progress and reduces power draw; on modern x86 it also avoids the
// memory-order machine clear that would otherwise fire when the spin
// finally observes its target write.
//
// Kept inline (not out-of-lined into the .cc): the whole purpose of
// this function is to emit a single CPU instruction at the call site,
// which a function call would defeat.
inline void SpinLoopHint() noexcept {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
  _mm_pause();
#elif defined(_MSC_VER) && defined(_M_ARM64)
  __yield();
#elif (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))
  __builtin_ia32_pause();
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__aarch64__)
  __asm__ __volatile__("yield" ::: "memory");
#endif  // (__GNUC__ || __clang__) && __aarch64__
}

// `atomic_flag`-based spinlock satisfying the BasicLockable named requirement,
// so it composes with std::lock_guard, std::scoped_lock and std::unique_lock.
//
// Trivially destructible and `constexpr`-default-constructible (via
// `std::atomic_flag`'s C++20 default ctor), so instances may live as
// `mutable` members of `consteval`-constructed constinit globals.
//
// For those who are wondering if we can use absl::base_internal::SpinLock
// instead, Abseil's one is intended to be used as an internal primitive for
// absl::Mutex or a special primitive for async signal safety as of Abseil
// 20260107.1. As Abseil's compatibility guideline implies, it is not a
// general-purpose "SpinLock" implementation.
//
//   Do not depend upon internal details. If something is in a namespace, file,
//   directory, or simply contains the string internal, impl, test, detail,
//   benchmark, sample, or example, unless it is explicitly called out, it is
//   not part of the public API. It’s an implementation detail. You cannot
//   friend it, you cannot include it, you cannot mention it or refer to it in
//   any way.
//                   https://abseil.io/about/compatibility#c-symbols-and-files
//
// Here are detailed reasons why absl::base_internal::SpinLock is not suitable
// for the const-init string implementations:
//
//  1. Its default constructor is not constexpr, and its destructor is not
//     trivial in TSAN builds. Both properties are load-bearing for the
//     const-init string classes that own this lock as a member of a constinit
//     global with no static initializer or atexit-registered destructor.
//  2. Its slow path is a bare relaxed-load loop with no PAUSE/YIELD CPU
//     spin-loop hint (it falls through to Sleep(0) on Windows and sched_yield
//     on POSIX). For use cases in const-init strings, the microscopic critical
//     sections protected by the Test-and-Test-and-Set + PAUSE/YIELD pattern
//     should work well because lock contention should be low and and lock hold
//     times should be quite short.
//  3. It does not satisfy the BasicLockable named requirement, which means we
//     cannot use it with std::lock_guard and friends without a custom holder.
class TrivialMicroSpinLock {
 public:
  void lock() noexcept {
    while (f_.test_and_set(std::memory_order::acquire)) {
      // Test-and-test-and-set: spin using a relaxed read while the
      // lock is held, so contending threads share the cache line
      // read-only instead of fighting for it via RMWs. Only re-attempt
      // the acquiring RMW once the inner read observes a release.
      while (f_.test(std::memory_order::relaxed)) {
        SpinLoopHint();
      }
    }
  }
  void unlock() noexcept { f_.clear(std::memory_order::release); }

 private:
  // `std::atomic_flag` deletes copy/move, so this class inherits that.
  std::atomic_flag f_ = {};
};

static_assert(std::is_trivially_destructible_v<TrivialMicroSpinLock>);

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

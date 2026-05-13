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
#include <string>
#include <string_view>

#if !defined(MOZC_NO_ATOMIC_FLAG_WAIT)
#include <mutex>
#include <type_traits>
#else  // !defined(MOZC_NO_ATOMIC_FLAG_WAIT)
#include "absl/base/const_init.h"
#include "absl/synchronization/mutex.h"
#endif  // !defined(MOZC_NO_ATOMIC_FLAG_WAIT)

#include "base/strings/internal/const_init_string_helpers.h"

namespace mozc {

// A variant of ConstInitImmutableString that additionally supports thread-safe
// Set(). The same synchronization model, caveats on destruction, and storage
// model apply -- see the comments in ConstInitImmutableString for details.
//
// Semantics:
// ---------
//   * Get() returns a snapshot of the most recent Set() value, or an empty
//     string if Set() has never been called. Empty thus doubles as the
//     "never set" sentinel; callers that need lazy default behaviour should
//     compute the default themselves on observing empty and call Set() to
//     publish it.
//   * Set(value) atomically replaces the stored value.
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
#if !defined(MOZC_NO_ATOMIC_FLAG_WAIT)
  // Inlined BasicLockable mutex used to serialize Set() and Get(). Trivially
  // destructible and `constexpr`-default-constructible (via
  // `std::atomic_flag`'s C++20 default ctor) so it composes with the
  // `consteval` ctor above.
  class CommittingMutex {
   public:
    void lock() noexcept {
      while (f_.test_and_set(std::memory_order::acquire)) {
        // Block until another thread clears the flag. The acquire on the next
        // test_and_set() synchronizes with the releasing unlock(), so the
        // wait-load itself can be relaxed.
        f_.wait(true, std::memory_order::relaxed);
      }
    }
    void unlock() noexcept {
      f_.clear(std::memory_order::release);
      f_.notify_one();
    }

   private:
    // std::atomic_flag deletes copy/move, so this class inherits that.
    std::atomic_flag f_ = {};
  };
  CommittingMutex committing_;
  using LockGuard = std::lock_guard<CommittingMutex>;
#else   // !defined(MOZC_NO_ATOMIC_FLAG_WAIT)
  absl::Mutex committing_{absl::kConstInit};
  using LockGuard = absl::MutexLock;
#endif  // !defined(MOZC_NO_ATOMIC_FLAG_WAIT)

  std::atomic<bool> initialized_ = {};
  CharT* current_ptr_ = nullptr;
  size_t current_size_ = 0;
  std::array<CharT, fixed_array_size> value_ = {};
};

template <size_t fixed_array_size,
          const_init_string_internal::SupportedChar CharT>
void ConstInitMutableString<fixed_array_size, CharT>::Set(StringViewT value) {
  std::unique_ptr<CharT[]> heap_fallback =
      const_init_string_internal::StageHeapFallback<CharT>(value,
                                                           std::size(value_));

  std::unique_ptr<CharT[]> old_heap;
  {
    LockGuard l(committing_);
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
  // Main-path: acquire the lock to synchronize with Set() and return a snapshot
  // of the current value.
  LockGuard l(committing_);
  return StringT(current_ptr_, current_size_);
}

#if !defined(MOZC_NO_ATOMIC_FLAG_WAIT)
// Verify the trivial destructibility contract for both supported CharT
// instantiations so misuse (e.g. accidentally adding a non-trivial member) is
// caught at compile time even if no instance is constructed. Only enforced in
// the atomic_flag arm; the absl::Mutex arm intentionally carries the mutex
// destructor at exit (see the class comment).
static_assert(
    std::is_trivially_destructible_v<ConstInitMutableString<1, char>>);
static_assert(
    std::is_trivially_destructible_v<ConstInitMutableString<1, wchar_t>>);
#endif  // !defined(MOZC_NO_ATOMIC_FLAG_WAIT)

}  // namespace mozc

#endif  // MOZC_BASE_STRINGS_CONST_INIT_MUTABLE_STRING_H_

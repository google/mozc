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

#ifndef MOZC_BASE_CONTAINER_FLAT_MAP_H_
#define MOZC_BASE_CONTAINER_FLAT_MAP_H_

#include <stddef.h>

#include <algorithm>
#include <array>
#include <functional>
#include <utility>

#include "absl/types/span.h"
#include "base/absl_nullability.h"
#include "base/container/flat_internal.h"

namespace mozc {

// Read-only map that is backed by `constexpr`-sorted array.
template <class K, class V, class CompareKey, size_t N>
class FlatMap {
 public:
  // Consider calling `CreateFlatMap` instead, so you don't have to manually
  // specify the number of entries, `N`.
  constexpr FlatMap(std::array<std::pair<K, V>, N> entries,
                    const CompareKey &cmp_key = {})
      : entries_(std::move(entries)), cmp_key_(cmp_key) {
    internal::SortAndVerifyUnique(
        absl::MakeSpan(entries_),
        [&](const std::pair<K, V> &a, const std::pair<K, V> &b) {
          return cmp_key_(a.first, b.first);
        });
  }

  // Finds the value associated with the given key, or `nullptr` if not found.
  constexpr const V *absl_nullable FindOrNull(const K &key) const {
    auto span = absl::MakeSpan(entries_);
    auto lb = internal::FindFirst(span, [&](const std::pair<K, V> &e) {
      return !cmp_key_(e.first, key);
    });
    return lb == span.end() || cmp_key_(key, lb->first) ? nullptr : &lb->second;
  }

 private:
  std::array<std::pair<K, V>, N> entries_;
  CompareKey cmp_key_;
};

// Creates a `FlatMap` from a C-style array of `std::pair`s.
//
// Example:
//
//   constexpr auto kMap = CreateFlatMap<int, absl::string_view>({
//       {1, "one"},
//       {2, "two"},
//       {3, "three"},
//   });
//
// Declare the variable as auto and use `CreateFlatMap`. The actual type is
// complex and explicitly declaring it would leak the number of entries, `N`.
template <class K, class V, class CompareKey = std::less<>, size_t N>
constexpr auto CreateFlatMap(std::pair<K, V> (&&entries)[N],
                             const CompareKey &cmp_key = CompareKey()) {
  return FlatMap<K, V, CompareKey, N>(std::to_array(std::move(entries)),
                                      cmp_key);
}

}  // namespace mozc

#endif  // MOZC_BASE_CONTAINER_FLAT_MAP_H_

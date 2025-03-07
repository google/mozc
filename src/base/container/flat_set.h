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

#ifndef MOZC_BASE_CONTAINER_FLAT_SET_H_
#define MOZC_BASE_CONTAINER_FLAT_SET_H_

#include <stddef.h>

#include <algorithm>
#include <array>
#include <functional>
#include <utility>

#include "absl/types/span.h"
#include "base/container/flat_internal.h"

namespace mozc {

// Read-only set that is backed by `constexpr`-sorted array.
template <class T, class Compare, size_t N>
class FlatSet {
 public:
  // Consider calling `CreateFlatSet` instead, so you don't have to manually
  // specify the number of elements, `N`.
  constexpr FlatSet(std::array<T, N> elements, const Compare &cmp = {})
      : elements_(std::move(elements)), cmp_(cmp) {
    internal::SortAndVerifyUnique(absl::MakeSpan(elements_), cmp_);
  }

  // Returns if the given element is in the set.
  constexpr bool contains(const T &x) const {
    auto span = absl::MakeSpan(elements_);
    auto lb =
        internal::FindFirst(span, [&](const T &e) { return !cmp_(e, x); });
    return lb != span.end() && !cmp_(x, *lb);
  }

 private:
  std::array<T, N> elements_;
  Compare cmp_;
};

// Creates a `FlatSet` from a C-style array of `Entry`s.
//
// Example:
//
//   constexpr auto kSet = CreateFlatSet<absl::string_view>({
//       "one",
//       "two",
//       "three",
//   });
// Declare the variable as auto and use `CreateFlatSet`. The actual type is
// complex and explicitly declaring it would leak the number of elements, `N`.
template <class T, class Compare = std::less<>, size_t N>
constexpr auto CreateFlatSet(T (&&elements)[N],
                             const Compare &cmp = Compare()) {
  return FlatSet<T, Compare, N>(internal::ToArray(std::move(elements)), cmp);
}

}  // namespace mozc

#endif  // MOZC_BASE_CONTAINER_FLAT_SET_H_

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

#ifndef MOZC_BASE_CONTAINER_FLAT_INTERNAL_H_
#define MOZC_BASE_CONTAINER_FLAT_INTERNAL_H_

#include <algorithm>
#include <functional>

#include "absl/log/log.h"
#include "absl/types/span.h"

namespace mozc::internal {

// Non-constexpr function to be called when a duplicate entry is found, causing
// a compile-time error.
inline void DuplicateEntryFound() { LOG(FATAL) << "Duplicate entry found"; }

// Sorts the given span in place, and verifies that the elements are unique
// according to `cmp`.
template <class T, class Compare = std::less<>>
constexpr void SortAndVerifyUnique(absl::Span<T> span,
                                   const Compare &cmp = Compare()) {
  std::sort(span.begin(), span.end(), cmp);
  for (int i = 0; i + 1 < span.size(); ++i) {
    if (!cmp(span[i], span[i + 1]) && !cmp(span[i + 1], span[i])) {
      DuplicateEntryFound();
    }
  }
}

}  // namespace mozc::internal

#endif  // MOZC_BASE_CONTAINER_FLAT_INTERNAL_H_

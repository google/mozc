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

#ifndef MOZC_BASE_PORT_REVERSED_VIEW_INTERNAL_H_
#define MOZC_BASE_PORT_REVERSED_VIEW_INTERNAL_H_

#include <iterator>

namespace mozc {
namespace port {
namespace internal {

// A wrapper class that provides a reversed view of a container.
//
// This class is intended to be used as a handy port of
// std::ranges::reverse_view in C++20.
template <typename Container>
class ReversedView {
 public:
  constexpr explicit ReversedView(Container& container)
      : container_(container) {}

  constexpr auto begin() const {
    using std::rbegin;
    return rbegin(container_);
  }

  constexpr auto end() const {
    using std::rend;
    return rend(container_);
  }

  constexpr auto rbegin() const {
    using std::begin;
    return begin(container_);
  }

  constexpr auto rend() const {
    using std::end;
    return end(container_);
  }

 private:
  Container& container_;
};

template <typename Container>
constexpr ReversedView<Container> reversed_view(Container& container) {
  return ReversedView<Container>(container);
}

template <typename Container>
constexpr ReversedView<const Container> reversed_view(
    const Container& container) {
  return ReversedView<const Container>(container);
}

}  // namespace internal
}  // namespace port
}  // namespace mozc

#endif  // MOZC_BASE_PORT_REVERSED_VIEW_INTERNAL_H_

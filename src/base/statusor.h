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

#ifndef MOZC_BASE_STATUSOR_H_
#define MOZC_BASE_STATUSOR_H_


#include <utility>

#include "base/logging.h"
#include "base/status.h"

namespace mozc {

// Holds a value T or a status.
template <typename T>
class StatusOr final {
 public:
  StatusOr() : status_(mozc::StatusCode::kUnknown, "Uninitialized") {}

  StatusOr(const StatusOr &) = delete;
  StatusOr &operator=(const StatusOr &) = delete;

  StatusOr(StatusOr &&) = default;
  StatusOr &operator=(StatusOr &&) = default;

  // Not explicit so that we can construct StatusOr<T> from return statements
  // with a value or a status.
  StatusOr(mozc::Status s) : status_(std::move(s)) {}  // NOLINT
  StatusOr(const T &v) : value_(v) {}                  // NOLINT
  StatusOr(T &&v) : value_(std::move(v)) {}            // NOLINT

  ~StatusOr() = default;

  bool ok() const { return status_.ok(); }

  mozc::Status &&status() && { return std::move(status_); }
  const mozc::Status &status() const & { return status_; }

  // To move the contained value, call message() on rvalues, e.g.:
  //
  //   StatusOr<std::unique_ptr<T>> s = Foo();
  //
  //   // Note: value() crashes if the status is not OK.
  //   std::unique_ptr<T> ptr = std::move(s).value();
  //
  //   // If the status is checked in advance, one can use operator*, which
  //   // doesn't check the status.
  //   if (s.ok()) {
  //     std::unique_ptr<T> ptr = *std::move(s);
  //   }
  T &&value() &&;
  const T &value() const &;
  const T &&value() const &&;

  T &operator*() & { return value_; }
  T &&operator*() && { return std::move(value_); }
  const T &operator*() const & { return value_; }
  const T &&operator*() const && { return std::move(value_); }

  T *operator->() { return &value_; }
  const T *operator->() const { return &value_; }

 private:
  mozc::Status status_;
  T value_;
};

template <typename T>
const T &StatusOr<T>::value() const & {
  CHECK(status_.ok()) << status_;
  return value_;
}

template <typename T>
T &&StatusOr<T>::value() && {
  CHECK(status_.ok()) << status_;
  return std::move(value_);
}

template <typename T>
const T &&StatusOr<T>::value() const && {
  CHECK(status_.ok()) << status_;
  return std::move(value_);
}

}  // namespace mozc

#endif  // MOZC_BASE_STATUSOR_H_

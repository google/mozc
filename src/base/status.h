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

#ifndef MOZC_BASE_STATUS_H_
#define MOZC_BASE_STATUS_H_

#include <ostream>
#include <string>
#include <utility>


namespace mozc {

// The following code is consistent with status code used in protobuf.
enum class StatusCode : int {
  kOk = 0,
  kCancelled = 1,
  kUnknown = 2,
  kInvalidArgument = 3,
  kDeadlineExceeded = 4,
  kNotFound = 5,
  kAlreadyExists = 6,
  kPermissionDenied = 7,
  kResourceExhausted = 8,
  kFailedPrecondition = 9,
  kAborted = 10,
  kOutOfRange = 11,
  kUnimplemented = 12,
};

// A value type holding a status and message.  Performance is not optimized, so
// do not use this class in performance critical code.
class Status final {
 public:
  // Creates an OK status.
  Status() = default;

  // Creates a status with code and message.
  Status(StatusCode code, std::string message)
      : code_{code}, message_(std::move(message)) {}

  // Copyable.
  Status(const Status &) = default;
  Status &operator=(const Status &) = default;

  // Movable.
  Status(Status &&) = default;
  Status &operator=(Status &&) = default;

  ~Status() = default;

  bool ok() const { return code_ == StatusCode::kOk; }
  StatusCode code() const { return code_; }

  // To move the contained message, call message() on rvalues, e.g.:
  //
  // Status s = Foo();
  // std::string msg = std::move(s).message();
  const std::string &message() const & { return message_; }
  std::string &&message() && { return std::move(message_); }

 private:
  StatusCode code_ = StatusCode::kOk;
  std::string message_;
};

// Status can be written to streams, e.g., LOG(INFO) << status;
std::ostream &operator<<(std::ostream &os, const Status &status);

inline Status UnknownError(std::string message) {
  return Status(StatusCode::kUnknown, std::move(message));
}

inline Status InvalidArgumentError(std::string message) {
  return Status(StatusCode::kInvalidArgument, std::move(message));
}

inline Status ResourceExhaustedError(std::string message) {
  return Status(StatusCode::kResourceExhausted, std::move(message));
}

inline Status FailedPreconditionError(std::string message) {
  return Status(StatusCode::kFailedPrecondition, std::move(message));
}

inline Status OutOfRangeError(std::string message) {
  return Status(StatusCode::kOutOfRange, std::move(message));
}

}  // namespace mozc

#endif  // MOZC_BASE_STATUS_H_

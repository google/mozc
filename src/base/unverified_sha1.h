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

#ifndef MOZC_BASE_UNVERIFIED_SHA1_H_
#define MOZC_BASE_UNVERIFIED_SHA1_H_

#include <string>

#include "absl/strings/string_view.h"

namespace mozc {
namespace internal {

// Note that this implementation is kept just for the backward compatibility
// so that we can read previously obfuscated data.
// !!! Not FIPS-certified.
// !!! Performance optimization is not well considered.
// !!! Side-channel attack is not well considered.
// TODO(team): Consider to remove this class and stop doing obfuscation.
class UnverifiedSHA1 {
 public:
  UnverifiedSHA1() = delete;
  UnverifiedSHA1(const UnverifiedSHA1&) = delete;
  UnverifiedSHA1& operator=(const UnverifiedSHA1&) = delete;

  // Returns 20-byte-length SHA1 digest.
  // CAVEATS: See the above comment.
  static std::string MakeDigest(absl::string_view source);
};

}  // namespace internal
}  // namespace mozc

#endif  // MOZC_BASE_UNVERIFIED_SHA1_H_

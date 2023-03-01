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

#include "composer/internal/special_key.h"

#include "base/util.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"

namespace mozc::composer::internal {

absl::string_view TrimLeadingSpecialKey(absl::string_view input) {
  // Check if the first character is a Unicode PUA converted from a special key.
  char32_t first_char;
  absl::string_view rest;
  Util::SplitFirstChar32(input, &first_char, &rest);
  if (IsSpecialKey(first_char)) {
    return input.substr(input.size() - rest.size());
  }

  // Check if the input starts from open and close of a special key.
  if (!absl::StartsWith(input, kSpecialKeyOpen)) {
    return input;
  }
  size_t close_pos = input.find(kSpecialKeyClose, 1);
  if (close_pos == absl::string_view::npos) {
    return input;
  }
  return input.substr(close_pos + 1);
}

}  // namespace mozc::composer::internal

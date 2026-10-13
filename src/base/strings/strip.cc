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

#include "base/strings/strip.h"

#include "absl/strings/string_view.h"

namespace mozc::strings {
namespace {

constexpr absl::string_view kFullwidthWhitespace = "　";

}  // namespace

absl::string_view StripLeadingWhitespace(absl::string_view text) {
  while (!text.empty()) {
    if (text.starts_with(' ')) {
      text.remove_prefix(1);
    } else if (text.starts_with(kFullwidthWhitespace)) {
      text.remove_prefix(kFullwidthWhitespace.size());
    } else {
      break;
    }
  }
  return text;
}

absl::string_view StripTrailingWhitespace(absl::string_view text) {
  while (!text.empty()) {
    if (text.ends_with(' ')) {
      text.remove_suffix(1);
    } else if (text.ends_with(kFullwidthWhitespace)) {
      text.remove_suffix(kFullwidthWhitespace.size());
    } else {
      break;
    }
  }
  return text;
}

absl::string_view StripWhitespace(absl::string_view text) {
  return StripTrailingWhitespace(StripLeadingWhitespace(text));
}

}  // namespace mozc::strings

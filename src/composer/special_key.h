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

#ifndef MOZC_COMPOSER_SPECIAL_KEY_H_
#define MOZC_COMPOSER_SPECIAL_KEY_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"

namespace mozc::composer::internal {

class SpecialKeyMap {
 public:
  // Parses special key strings escaped with the pair of "{" and "}"" and
  // registers them to be used by Parse(). Also returns the parsed string.
  std::string Register(absl::string_view input);

  // Parses special key strings escaped with the pair of "{" and "}" and returns
  // the parsed string.
  std::string Parse(absl::string_view input) const;

 private:
  absl::flat_hash_map<std::string, std::string> map_;
};

// Trims a special key from input and returns the rest.
// If the input doesn't have any special keys at the beginning, it returns the
// entire string.
absl::string_view TrimLeadingSpecialKey(absl::string_view input);

// Deletes invisible special keys wrapped with ("\x0F", "\x0E") and returns the
// trimmed visible string.
std::string DeleteSpecialKeys(absl::string_view input);

}  // namespace mozc::composer::internal

#endif  // MOZC_COMPOSER_SPECIAL_KEY_H_

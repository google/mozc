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

// Simple word patterns matcher which will be used in composer objects
// for auto swtiching input mode.

#ifndef MOZC_COMPOSER_MODE_SWITCHING_HANDLER_H_
#define MOZC_COMPOSER_MODE_SWITCHING_HANDLER_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace composer {

class ModeSwitchingHandler {
 public:
  ModeSwitchingHandler();
  ~ModeSwitchingHandler() = default;

  enum ModeSwitching {
    NO_CHANGE,
    REVERT_TO_PREVIOUS_MODE,
    PREFERRED_ALPHANUMERIC,
    HALF_ALPHANUMERIC,
    FULL_ALPHANUMERIC,
  };

  struct Rule {
    // |display_mode| affects the existing composition the user typed.
    ModeSwitching display_mode;
    // |input_mode| affects current input mode to be used for the user's new
    // typing.
    ModeSwitching input_mode;
  };

  // Returns a Rule for the current preedit. |key| is the string which the user
  // actually typed. display_mode and input_mode are stored rules controlling
  // the composer. Returns NO_CHANGE if the key doesn't match the stored rules.
  Rule GetModeSwitchingRule(absl::string_view key) const;

  // Gets the singleton instance of this class.
  static ModeSwitchingHandler *GetModeSwitchingHandler();

  // Matcher to Windows drive letters like "C:\".
  // TODO(team): This static method is internal use only.  It's public for
  // testing purpose.
  static bool IsDriveLetter(absl::string_view key);

 private:
  // Adds a rule for mode switching. The rule is passed by value as it's small.
  void AddRule(absl::string_view key, Rule rule);

  absl::flat_hash_map<std::string, Rule> patterns_;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_MODE_SWITCHING_HANDLER_H_

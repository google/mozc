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

#include "composer/internal/mode_switching_handler.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/singleton.h"
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace composer {

ModeSwitchingHandler::ModeSwitchingHandler() {
  // Default patterns are fixed right now.
  // AddRule(key, display_mode, input_mode);
  AddRule("google", PREFERRED_ALPHANUMERIC, REVERT_TO_PREVIOUS_MODE);
  AddRule("Google", PREFERRED_ALPHANUMERIC, REVERT_TO_PREVIOUS_MODE);
  AddRule("Chrome", PREFERRED_ALPHANUMERIC, REVERT_TO_PREVIOUS_MODE);
  AddRule("chrome", PREFERRED_ALPHANUMERIC, REVERT_TO_PREVIOUS_MODE);
  AddRule("Android", PREFERRED_ALPHANUMERIC, REVERT_TO_PREVIOUS_MODE);
  AddRule("android", PREFERRED_ALPHANUMERIC, REVERT_TO_PREVIOUS_MODE);
  AddRule("http", HALF_ALPHANUMERIC, HALF_ALPHANUMERIC);
  AddRule("www.", HALF_ALPHANUMERIC, HALF_ALPHANUMERIC);
  AddRule("\\\\", HALF_ALPHANUMERIC, HALF_ALPHANUMERIC);
}

bool ModeSwitchingHandler::GetModeSwitchingRule(
    const absl::string_view key, ModeSwitching *display_mode,
    ModeSwitching *input_mode) const {
  if (display_mode == nullptr || input_mode == nullptr) {
    LOG(ERROR) << "display_mode/input_mode is nullptr.";
    return false;
  }

  auto it = patterns_.find(key);
  if (it != patterns_.end()) {
    *display_mode = it->second.first;
    *input_mode = it->second.second;
    return true;
  }

  if (IsDriveLetter(key)) {
    *display_mode = HALF_ALPHANUMERIC;
    *input_mode = HALF_ALPHANUMERIC;
    return true;
  }

  *display_mode = NO_CHANGE;
  *input_mode = NO_CHANGE;
  return false;
}

bool ModeSwitchingHandler::IsDriveLetter(const absl::string_view key) {
  return key.size() == 3 && absl::ascii_isalpha(key[0]) && key[1] == ':' &&
         key[2] == '\\';
}

void ModeSwitchingHandler::AddRule(const absl::string_view key,
                                   const ModeSwitching display_mode,
                                   const ModeSwitching input_mode) {
  patterns_.emplace(key, std::make_pair(display_mode, input_mode));
}

ModeSwitchingHandler *ModeSwitchingHandler::GetModeSwitchingHandler() {
  return Singleton<ModeSwitchingHandler>::get();
}

}  // namespace composer
}  // namespace mozc

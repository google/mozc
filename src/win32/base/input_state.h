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

#ifndef THIRD_PARTY_MOZC_SRC_WIN32_BASE_INPUT_STATE_H_
#define THIRD_PARTY_MOZC_SRC_WIN32_BASE_INPUT_STATE_H_

#include <windows.h>

#include <vector>

#include "session/key_info_util.h"
#include "win32/base/keyboard.h"

namespace mozc {
namespace win32 {

struct InputState {
  // Represents the IME is turned on or not.
  bool open = false;
  // Represents the expected conversion mode visible from the input method
  // framework (IMM32/TSF).
  DWORD logical_conversion_mode = 0;
  // Represents the expected conversion mode visible from the user. So the
  // language bar should show this mode.
  DWORD visible_conversion_mode = 0;
  // Tracks the last down key mainly for handling modifier key-up event.
  VirtualKey last_down_key;
};

struct InputBehavior {
  bool initialized = false;
  bool disabled = false;
  bool prefer_kana_input = false;
  bool use_mode_indicator = false;
  bool use_romaji_key_to_toggle_input_style = false;
  std::vector<KeyInformation> direct_mode_keys;
};

}  // namespace win32
}  // namespace mozc
#endif  // THIRD_PARTY_MOZC_SRC_WIN32_BASE_INPUT_STATE_H_

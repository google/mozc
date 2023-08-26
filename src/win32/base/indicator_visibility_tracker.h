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

#ifndef THIRD_PARTY_MOZC_SRC_WIN32_BASE_INDICATOR_VISIBILITY_TRACKER_H_
#define THIRD_PARTY_MOZC_SRC_WIN32_BASE_INDICATOR_VISIBILITY_TRACKER_H_

#include <windows.h>

#include "base/stopwatch.h"
#include "win32/base/keyboard.h"

namespace mozc {
namespace win32 {

class IndicatorVisibilityTracker {
 public:
  enum Action {
    kNothing,   // The caller has nothing to do.
    kUpdateUI,  // The caller must update UI for the indicator.
  };

  // Event call back endpoints.
  Action OnDissociateContext();
  Action OnTestKey(const VirtualKey &key, bool is_down, bool eaten);
  Action OnKey(const VirtualKey &key, bool is_down, bool eaten);
  Action OnMoveFocusedWindow();
  Action OnChangeInputMode();

  // Returns if the indicator should be displayed or not.
  constexpr bool IsVisible() const { return visible_; }

 private:
  bool visible_ = false;
  Stopwatch mode_changed_;
};

}  // namespace win32
}  // namespace mozc

#endif  // THIRD_PARTY_MOZC_SRC_WIN32_BASE_INDICATOR_VISIBILITY_TRACKER_H_

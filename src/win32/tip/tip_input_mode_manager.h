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

#ifndef MOZC_WIN32_TIP_TIP_INPUT_MODE_MANAGER_H_
#define MOZC_WIN32_TIP_TIP_INPUT_MODE_MANAGER_H_

#include <inputscope.h>
#include <windows.h>

#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "win32/base/indicator_visibility_tracker.h"
#include "win32/base/keyboard.h"

namespace mozc {
namespace win32 {
namespace tsf {

// Exposed for unit testing.
class TipInputModeManagerImpl {
 public:
  enum ConversionMode {
    kDirect,
    kHiragana,
    kFullKatakana,
    kHalfAscii,
    kFullAscii,
    kHalfKatakana,
  };
  struct StatePair {
    StatePair() = default;
    StatePair(bool open_close, ConversionMode conversion_mode)
        : open_close(open_close), conversion_mode(conversion_mode) {}

    bool open_close = false;
    ConversionMode conversion_mode = kHiragana;
  };

 protected:
  static StatePair GetOverriddenState(
      const StatePair &base_state, absl::Span<const InputScope> input_scopes);
};

// In TSF, IME open/close mode and conversion mode are managed per thread not
// per context. This class is designed to be instantiated per thread so that it
// can manage such IME status.
// Another design goal of this class is to manage two IME status separately: one
// is for Mozc (effective status) and the other is for TSF (TSF status). Note
// that TSF status is now shared in all the applications by default starting
// with Windows 8, while Mozc status is context local and valid only in the
// Mozc session. For example, if InputScope that is associated with a local
// context represents that IME should be turned off, its status can be Mozc
// local because so that the behavior can be the same to password field.
// This class is a composition of input mode manager itself and
// IndicatorVisibilityTracker.
class TipInputModeManager : public TipInputModeManagerImpl {
 public:
  enum Action {
    kDoNothing = 0,
    kUpdateUI = 1,
  };
  enum NotifyAction {
    kNotifyNothing = 0,
    kNotifySystemOpenClose = (1 << 0),
    kNotifySystemConversionMode = (1 << 1),
  };
  struct Config {
    bool use_global_mode = false;
  };
  typedef uint32_t NotifyActionSet;

  explicit TipInputModeManager(const Config &config)
      : use_global_mode_(config.use_global_mode) {}
  // Movable
  TipInputModeManager(TipInputModeManager &&) = default;
  TipInputModeManager &operator=(TipInputModeManager &&) = default;

  // Functions to access embedded IndicatorVisibilityTracker.
  Action OnDissociateContext();
  Action OnTestKey(const VirtualKey &key, bool is_down, bool eaten);
  Action OnKey(const VirtualKey &key, bool is_down, bool eaten);
  Action OnMoveFocusedWindow();
  bool IsIndicatorVisible() const;

  void OnInitialize(bool system_open_close_mode, DWORD system_conversion_mode);
  NotifyActionSet OnReceiveCommand(bool mozc_open_close_mode,
                                   DWORD mozc_logical_mode,
                                   DWORD mozc_visible_mode);
  Action OnSetFocus(bool system_open_close_mode, DWORD system_conversion_mode,
                    absl::Span<const InputScope> input_scopes);
  Action OnChangeOpenClose(bool new_open_close_mode);
  Action OnChangeConversionMode(DWORD new_conversion_mode);
  Action OnChangeInputScope(absl::Span<const InputScope> input_scopes);

  // Returns an IME open/close state that is visible from the Mozc session.
  bool GetEffectiveOpenClose() const;
  // Returns an IME open/close state that is visible from TSF.
  bool GetTsfOpenClose() const;
  // Returns IME conversion mode that is visible from the Mozc session.
  ConversionMode GetEffectiveConversionMode() const;
  // Returns IME conversion mode that is visible from TSF.
  ConversionMode GetTsfConversionMode() const;

 private:
  bool use_global_mode_ = false;
  StatePair mozc_state_;
  StatePair tsf_state_;
  IndicatorVisibilityTracker indicator_visibility_tracker_;
  std::vector<InputScope> input_scope_;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_INPUT_MODE_MANAGER_H_

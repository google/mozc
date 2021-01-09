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

#include "win32/tip/tip_input_mode_manager.h"

#include <algorithm>

#include "win32/base/conversion_mode_util.h"
#include "win32/base/indicator_visibility_tracker.h"

namespace mozc {

typedef ::mozc::commands::CompositionMode CompositionMode;

namespace win32 {
namespace tsf {
namespace {

template <typename T>
void Dedup(std::vector<T> *container) {
  std::sort(container->begin(), container->end());
  auto new_end = unique(container->begin(), container->end());
  container->erase(new_end, container->end());
}

}  // namespace

TipInputModeManagerImpl::StatePair::StatePair()
    : open_close(false), conversion_mode(kHiragana) {}

TipInputModeManagerImpl::StatePair::StatePair(bool open_close,
                                              ConversionMode conversion_mode)
    : open_close(open_close), conversion_mode(conversion_mode) {}

TipInputModeManagerImpl::StatePair TipInputModeManagerImpl::GetOverriddenState(
    const StatePair &base_state, const std::vector<InputScope> &input_scopes) {
  if (input_scopes.empty()) {
    return base_state;
  }
  std::vector<ConversionMode> states;
  for (size_t i = 0; i < input_scopes.size(); ++i) {
    const InputScope input_scope = input_scopes[i];
    switch (input_scope) {
      // Some InputStope can be mapped to commands::Context::InputFieldType.
      // TODO(yukawa): Pass context information to the converter.
      case IS_URL:
      case IS_EMAIL_USERNAME:
      case IS_EMAIL_SMTPEMAILADDRESS:
      case IS_DIGITS:
      case IS_NUMBER:
      case IS_PASSWORD:
      case IS_TELEPHONE_FULLTELEPHONENUMBER:
      case IS_TELEPHONE_COUNTRYCODE:
      case IS_TELEPHONE_AREACODE:
      case IS_TELEPHONE_LOCALNUMBER:
      case IS_TIME_FULLTIME:
      case IS_TIME_HOUR:
      case IS_TIME_MINORSEC:
        states.push_back(kDirect);
        break;
      case IS_HIRAGANA:
        states.push_back(kHiragana);
        break;
      case IS_ALPHANUMERIC_HALFWIDTH:
        states.push_back(kHalfAscii);
        break;
      case IS_NUMBER_FULLWIDTH:
      case IS_ALPHANUMERIC_FULLWIDTH:
        states.push_back(kFullAscii);
        break;
      case IS_KATAKANA_HALFWIDTH:
        states.push_back(kHalfKatakana);
        break;
      case IS_KATAKANA_FULLWIDTH:
        states.push_back(kFullKatakana);
        break;
      default:
        break;
    }
  }
  if (states.empty()) {
    return base_state;
  }
  Dedup(&states);
  if (states.size() > 1) {
    // Multiple mode found.
    // TODO(yukawa): consider this case.
    return base_state;
  }
  if (states[0] == kDirect) {
    return StatePair(false, base_state.conversion_mode);
  }
  return StatePair(true, states[0]);
}

TipInputModeManager::Config::Config() : use_global_mode(false) {}

class TipInputModeManager::InternalState {
 public:
  InternalState() : use_global_mode(false) {}
  bool use_global_mode;
  StatePair mozc_state;
  StatePair tsf_state;
  IndicatorVisibilityTracker indicator_visibility_tracker;
  std::vector<InputScope> input_scope;
};

// For Mode Indicator.
TipInputModeManager::Action TipInputModeManager::OnDissociateContext() {
  const IndicatorVisibilityTracker::Action action =
      state_->indicator_visibility_tracker.OnDissociateContext();
  switch (action) {
    case IndicatorVisibilityTracker::kUpdateUI:
      return kUpdateUI;
    default:
      return kDoNothing;
  }
}

TipInputModeManager::Action TipInputModeManager::OnTestKey(
    const VirtualKey &key, bool is_down, bool eaten) {
  const IndicatorVisibilityTracker::Action action =
      state_->indicator_visibility_tracker.OnTestKey(key, is_down, eaten);
  switch (action) {
    case IndicatorVisibilityTracker::kUpdateUI:
      return kUpdateUI;
    default:
      return kDoNothing;
  }
}

TipInputModeManager::Action TipInputModeManager::OnKey(const VirtualKey &key,
                                                       bool is_down,
                                                       bool eaten) {
  const IndicatorVisibilityTracker::Action action =
      state_->indicator_visibility_tracker.OnKey(key, is_down, eaten);
  switch (action) {
    case IndicatorVisibilityTracker::kUpdateUI:
      return kUpdateUI;
    default:
      return kDoNothing;
  }
}

TipInputModeManager::Action TipInputModeManager::OnMoveFocusedWindow() {
  const IndicatorVisibilityTracker::Action action =
      state_->indicator_visibility_tracker.OnMoveFocusedWindow();
  switch (action) {
    case IndicatorVisibilityTracker::kUpdateUI:
      return kUpdateUI;
    default:
      return kDoNothing;
  }
}

TipInputModeManager::TipInputModeManager(const Config &config)
    : state_(new InternalState) {
  state_->use_global_mode = config.use_global_mode;
}

TipInputModeManager::~TipInputModeManager() {}

TipInputModeManager::NotifyActionSet TipInputModeManager::OnReceiveCommand(
    bool mozc_open_close_mode, DWORD mozc_logical_mode,
    DWORD mozc_visible_mode) {
  const StatePair prev_tsf_state = state_->tsf_state;

  state_->tsf_state.open_close = mozc_open_close_mode;
  state_->tsf_state.conversion_mode =
      static_cast<ConversionMode>(mozc_logical_mode);
  state_->mozc_state.open_close = mozc_open_close_mode;
  state_->mozc_state.conversion_mode =
      static_cast<ConversionMode>(mozc_visible_mode);

  NotifyActionSet action_set = kNotifyNothing;
  if (prev_tsf_state.open_close != state_->tsf_state.open_close) {
    action_set |= kNotifySystemOpenClose;
  }
  if (prev_tsf_state.conversion_mode != state_->tsf_state.conversion_mode) {
    action_set |= kNotifySystemConversionMode;
  }
  if (action_set != kNotifyNothing) {
    state_->indicator_visibility_tracker.OnChangeInputMode();
  }
  return action_set;
}

void TipInputModeManager::OnInitialize(bool system_open_close_mode,
                                       DWORD system_conversion_mode) {
  state_->mozc_state.open_close = system_open_close_mode;
  state_->tsf_state.open_close = system_open_close_mode;
  if (state_->use_global_mode) {
    return;
  }
  CompositionMode mozc_mode = commands::HIRAGANA;
  if (ConversionModeUtil::ToMozcMode(system_conversion_mode, &mozc_mode)) {
    state_->tsf_state.conversion_mode = static_cast<ConversionMode>(mozc_mode);
  }
  state_->mozc_state.conversion_mode = state_->tsf_state.conversion_mode;
}

TipInputModeManager::Action TipInputModeManager::OnSetFocus(
    bool system_open_close_mode, DWORD system_conversion_mode,
    const std::vector<InputScope> &input_scopes) {
  const StatePair prev_effective = state_->mozc_state;

  state_->indicator_visibility_tracker.OnMoveFocusedWindow();

  std::vector<InputScope> new_input_scopes = input_scopes;
  Dedup(&new_input_scopes);

  state_->tsf_state.open_close = system_open_close_mode;
  CompositionMode mozc_mode = commands::HIRAGANA;
  if (!state_->use_global_mode) {
    if (ConversionModeUtil::ToMozcMode(system_conversion_mode, &mozc_mode)) {
      state_->tsf_state.conversion_mode =
          static_cast<ConversionMode>(mozc_mode);
    }
  }

  if (new_input_scopes.size() > 0 &&
      (new_input_scopes == state_->input_scope)) {
    // The same input scope is specified. Use the previous mode.
    return kDoNothing;
  }

  swap(state_->input_scope, new_input_scopes);
  state_->mozc_state = GetOverriddenState(state_->tsf_state, input_scopes);
  if ((state_->mozc_state.open_close != prev_effective.open_close) ||
      (state_->mozc_state.conversion_mode != prev_effective.conversion_mode)) {
    state_->indicator_visibility_tracker.OnChangeInputMode();
    return kUpdateUI;
  }
  return kDoNothing;
}

TipInputModeManager::Action TipInputModeManager::OnChangeOpenClose(
    bool new_open_close_mode) {
  const bool prev_open = state_->mozc_state.open_close;  // effective on/off

  state_->tsf_state.open_close = new_open_close_mode;
  if (prev_open != new_open_close_mode) {
    state_->mozc_state.open_close = new_open_close_mode;
    state_->indicator_visibility_tracker.OnChangeInputMode();
    return kUpdateUI;
  }
  return kDoNothing;
}

TipInputModeManager::Action TipInputModeManager::OnChangeConversionMode(
    DWORD new_conversion_mode) {
  const StatePair prev_effective = state_->mozc_state;

  if (state_->use_global_mode) {
    // Ignore mode change.
    return kDoNothing;
  }

  CompositionMode mozc_mode = commands::HIRAGANA;
  if (ConversionModeUtil::ToMozcMode(new_conversion_mode, &mozc_mode)) {
    state_->tsf_state.conversion_mode = static_cast<ConversionMode>(mozc_mode);
    state_->mozc_state.conversion_mode = static_cast<ConversionMode>(mozc_mode);
  }

  if (prev_effective.conversion_mode != state_->mozc_state.conversion_mode) {
    state_->indicator_visibility_tracker.OnChangeInputMode();
    return kUpdateUI;
  }
  return kDoNothing;
}

TipInputModeManager::Action TipInputModeManager::OnChangeInputScope(
    const std::vector<InputScope> &input_scopes) {
  const StatePair prev_effective = state_->mozc_state;

  std::vector<InputScope> new_input_scopes = input_scopes;
  Dedup(&new_input_scopes);
  if (new_input_scopes == state_->input_scope) {
    // The same input scope is specified. Use the previous mode.
    return kDoNothing;
  }

  swap(state_->input_scope, new_input_scopes);
  state_->mozc_state = GetOverriddenState(state_->tsf_state, input_scopes);
  if ((state_->mozc_state.open_close != prev_effective.open_close) ||
      (state_->mozc_state.conversion_mode != prev_effective.conversion_mode)) {
    state_->indicator_visibility_tracker.OnChangeInputMode();
    return kUpdateUI;
  }
  return kDoNothing;
}

bool TipInputModeManager::GetEffectiveOpenClose() const {
  return state_->mozc_state.open_close;
}

bool TipInputModeManager::GetTsfOpenClose() const {
  return state_->tsf_state.open_close;
}

TipInputModeManagerImpl::ConversionMode
TipInputModeManager::GetEffectiveConversionMode() const {
  return state_->mozc_state.conversion_mode;
}

TipInputModeManagerImpl::ConversionMode
TipInputModeManager::GetTsfConversionMode() const {
  return state_->tsf_state.conversion_mode;
}

bool TipInputModeManager::IsIndicatorVisible() const {
  return state_->indicator_visibility_tracker.IsVisible();
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

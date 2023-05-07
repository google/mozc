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

#include <inputscope.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "protocol/commands.pb.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/indicator_visibility_tracker.h"
#include "win32/base/keyboard.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

typedef ::mozc::commands::CompositionMode CompositionMode;

template <typename T>
void Dedup(std::vector<T> *container) {
  std::sort(container->begin(), container->end());
  auto new_end = std::unique(container->begin(), container->end());
  container->erase(new_end, container->end());
}

}  // namespace

TipInputModeManagerImpl::StatePair TipInputModeManagerImpl::GetOverriddenState(
    const StatePair &base_state, const std::vector<InputScope> &input_scopes) {
  if (input_scopes.empty()) {
    return base_state;
  }
  std::vector<ConversionMode> states;
  for (const auto &input_scope : input_scopes) {
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
    // Multiple modes found.
    // TODO(yukawa): consider this case.
    return base_state;
  }
  if (states[0] == kDirect) {
    return StatePair(false, base_state.conversion_mode);
  }
  return StatePair(true, states[0]);
}

// For Mode Indicator.
TipInputModeManager::Action TipInputModeManager::OnDissociateContext() {
  const IndicatorVisibilityTracker::Action action =
      indicator_visibility_tracker_.OnDissociateContext();
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
      indicator_visibility_tracker_.OnTestKey(key, is_down, eaten);
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
      indicator_visibility_tracker_.OnKey(key, is_down, eaten);
  switch (action) {
    case IndicatorVisibilityTracker::kUpdateUI:
      return kUpdateUI;
    default:
      return kDoNothing;
  }
}

TipInputModeManager::Action TipInputModeManager::OnMoveFocusedWindow() {
  const IndicatorVisibilityTracker::Action action =
      indicator_visibility_tracker_.OnMoveFocusedWindow();
  switch (action) {
    case IndicatorVisibilityTracker::kUpdateUI:
      return kUpdateUI;
    default:
      return kDoNothing;
  }
}

TipInputModeManager::NotifyActionSet TipInputModeManager::OnReceiveCommand(
    bool mozc_open_close_mode, DWORD mozc_logical_mode,
    DWORD mozc_visible_mode) {
  const StatePair prev_tsf_state = tsf_state_;

  tsf_state_.open_close = mozc_open_close_mode;
  tsf_state_.conversion_mode = static_cast<ConversionMode>(mozc_logical_mode);
  mozc_state_.open_close = mozc_open_close_mode;
  mozc_state_.conversion_mode = static_cast<ConversionMode>(mozc_visible_mode);

  NotifyActionSet action_set = kNotifyNothing;
  if (prev_tsf_state.open_close != tsf_state_.open_close) {
    action_set |= kNotifySystemOpenClose;
  }
  if (prev_tsf_state.conversion_mode != tsf_state_.conversion_mode) {
    action_set |= kNotifySystemConversionMode;
  }
  if (action_set != kNotifyNothing) {
    indicator_visibility_tracker_.OnChangeInputMode();
  }
  return action_set;
}

void TipInputModeManager::OnInitialize(bool system_open_close_mode,
                                       DWORD system_conversion_mode) {
  mozc_state_.open_close = system_open_close_mode;
  tsf_state_.open_close = system_open_close_mode;
  if (use_global_mode_) {
    return;
  }
  CompositionMode mozc_mode = commands::HIRAGANA;
  if (ConversionModeUtil::ToMozcMode(system_conversion_mode, &mozc_mode)) {
    tsf_state_.conversion_mode = static_cast<ConversionMode>(mozc_mode);
  }
  mozc_state_.conversion_mode = tsf_state_.conversion_mode;
}

TipInputModeManager::Action TipInputModeManager::OnSetFocus(
    bool system_open_close_mode, DWORD system_conversion_mode,
    const std::vector<InputScope> &input_scopes) {
  const StatePair prev_effective = mozc_state_;

  indicator_visibility_tracker_.OnMoveFocusedWindow();

  std::vector<InputScope> new_input_scopes = input_scopes;
  Dedup(&new_input_scopes);

  tsf_state_.open_close = system_open_close_mode;
  CompositionMode mozc_mode = commands::HIRAGANA;
  if (!use_global_mode_) {
    if (ConversionModeUtil::ToMozcMode(system_conversion_mode, &mozc_mode)) {
      tsf_state_.conversion_mode = static_cast<ConversionMode>(mozc_mode);
    }
  }

  if (!new_input_scopes.empty() && (new_input_scopes == input_scope_)) {
    // The same input scope is specified. Use the previous mode.
    return kDoNothing;
  }

  std::swap(input_scope_, new_input_scopes);
  mozc_state_ = GetOverriddenState(tsf_state_, input_scopes);
  if ((mozc_state_.open_close != prev_effective.open_close) ||
      (mozc_state_.conversion_mode != prev_effective.conversion_mode)) {
    indicator_visibility_tracker_.OnChangeInputMode();
    return kUpdateUI;
  }
  return kDoNothing;
}

TipInputModeManager::Action TipInputModeManager::OnChangeOpenClose(
    bool new_open_close_mode) {
  const bool prev_open = mozc_state_.open_close;  // effective on/off

  tsf_state_.open_close = new_open_close_mode;
  if (prev_open != new_open_close_mode) {
    mozc_state_.open_close = new_open_close_mode;
    indicator_visibility_tracker_.OnChangeInputMode();
    return kUpdateUI;
  }
  return kDoNothing;
}

TipInputModeManager::Action TipInputModeManager::OnChangeConversionMode(
    DWORD new_conversion_mode) {
  const StatePair prev_effective = mozc_state_;

  if (use_global_mode_) {
    // Ignore mode change.
    return kDoNothing;
  }

  CompositionMode mozc_mode = commands::HIRAGANA;
  if (ConversionModeUtil::ToMozcMode(new_conversion_mode, &mozc_mode)) {
    tsf_state_.conversion_mode = static_cast<ConversionMode>(mozc_mode);
    mozc_state_.conversion_mode = static_cast<ConversionMode>(mozc_mode);
  }

  if (prev_effective.conversion_mode != mozc_state_.conversion_mode) {
    indicator_visibility_tracker_.OnChangeInputMode();
    return kUpdateUI;
  }
  return kDoNothing;
}

TipInputModeManager::Action TipInputModeManager::OnChangeInputScope(
    const std::vector<InputScope> &input_scopes) {
  const StatePair prev_effective = mozc_state_;

  std::vector<InputScope> new_input_scopes = input_scopes;
  Dedup(&new_input_scopes);
  if (new_input_scopes == input_scope_) {
    // The same input scope is specified. Use the previous mode.
    return kDoNothing;
  }

  std::swap(input_scope_, new_input_scopes);
  mozc_state_ = GetOverriddenState(tsf_state_, input_scopes);
  if ((mozc_state_.open_close != prev_effective.open_close) ||
      (mozc_state_.conversion_mode != prev_effective.conversion_mode)) {
    indicator_visibility_tracker_.OnChangeInputMode();
    return kUpdateUI;
  }
  return kDoNothing;
}

bool TipInputModeManager::GetEffectiveOpenClose() const {
  return mozc_state_.open_close;
}

bool TipInputModeManager::GetTsfOpenClose() const {
  return tsf_state_.open_close;
}

TipInputModeManagerImpl::ConversionMode
TipInputModeManager::GetEffectiveConversionMode() const {
  return mozc_state_.conversion_mode;
}

TipInputModeManagerImpl::ConversionMode
TipInputModeManager::GetTsfConversionMode() const {
  return tsf_state_.conversion_mode;
}

bool TipInputModeManager::IsIndicatorVisible() const {
  return indicator_visibility_tracker_.IsVisible();
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

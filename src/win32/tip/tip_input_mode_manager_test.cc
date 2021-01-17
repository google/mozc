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

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

typedef TipInputModeManagerImpl::StatePair StatePair;
class TestableTipInputModeManagerImpl : public TipInputModeManagerImpl {
 public:
  using TipInputModeManagerImpl::GetOverriddenState;
};

TipInputModeManager::Config GetGlobalMode() {
  TipInputModeManager::Config config;
  config.use_global_mode = true;
  return config;
}

TipInputModeManager::Config GetThreadLocalMode() {
  TipInputModeManager::Config config;
  config.use_global_mode = false;
  return config;
}

const DWORD kNativeHiragana =
    IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
const DWORD kNativeHalfAlpha = IME_CMODE_ROMAN;

TEST(TipInputModeManagerImplTest, GetOverriddenState) {
  // Check if input scopes for turning off IME temporarily.
  {
    std::vector<InputScope> input_scope_off;
    input_scope_off.push_back(IS_NUMBER);
    input_scope_off.push_back(IS_EMAIL_SMTPEMAILADDRESS);
    {
      const StatePair state =
          TestableTipInputModeManagerImpl::GetOverriddenState(
              StatePair(false, TipInputModeManagerImpl::kHiragana),
              input_scope_off);
      EXPECT_FALSE(state.open_close);
      EXPECT_EQ(TipInputModeManagerImpl::kHiragana, state.conversion_mode);
    }
    {
      const StatePair state =
          TestableTipInputModeManagerImpl::GetOverriddenState(
              StatePair(true, TipInputModeManagerImpl::kHiragana),
              input_scope_off);
      EXPECT_FALSE(state.open_close) << "Should be temporarily off.";
      EXPECT_EQ(TipInputModeManagerImpl::kHiragana, state.conversion_mode);
    }
  }

  // Check if input scopes for turning on IME temporarily.
  {
    std::vector<InputScope> input_scope_full_hiragana;
    input_scope_full_hiragana.push_back(IS_HIRAGANA);
    {
      const StatePair state =
          TestableTipInputModeManagerImpl::GetOverriddenState(
              StatePair(false, TipInputModeManagerImpl::kHiragana),
              input_scope_full_hiragana);
      EXPECT_TRUE(state.open_close) << "Should be temporarily on.";
      EXPECT_EQ(TipInputModeManagerImpl::kHiragana, state.conversion_mode);
    }
    {
      const StatePair state =
          TestableTipInputModeManagerImpl::GetOverriddenState(
              StatePair(true, TipInputModeManagerImpl::kHiragana),
              input_scope_full_hiragana);
      EXPECT_TRUE(state.open_close) << "Should be temporarily on.";
      EXPECT_EQ(TipInputModeManagerImpl::kHiragana, state.conversion_mode);
    }
    {
      const StatePair state =
          TestableTipInputModeManagerImpl::GetOverriddenState(
              StatePair(true, TipInputModeManagerImpl::kFullAscii),
              input_scope_full_hiragana);
      EXPECT_TRUE(state.open_close) << "Should be temporarily on.";
      EXPECT_EQ(TipInputModeManagerImpl::kHiragana, state.conversion_mode)
          << "Should be temporarily Hiragana";
    }
  }

  // If there are multiple input scopes and they are not aggregatable, use the
  // original state as is.
  {
    std::vector<InputScope> input_scope_invalid;
    input_scope_invalid.push_back(IS_HIRAGANA);
    input_scope_invalid.push_back(IS_KATAKANA_FULLWIDTH);
    {
      const StatePair state =
          TestableTipInputModeManagerImpl::GetOverriddenState(
              StatePair(false, TipInputModeManagerImpl::kHiragana),
              input_scope_invalid);
      EXPECT_FALSE(state.open_close);
      EXPECT_EQ(TipInputModeManagerImpl::kHiragana, state.conversion_mode);
    }
    {
      const StatePair state =
          TestableTipInputModeManagerImpl::GetOverriddenState(
              StatePair(true, TipInputModeManagerImpl::kHiragana),
              input_scope_invalid);
      EXPECT_TRUE(state.open_close);
      EXPECT_EQ(TipInputModeManagerImpl::kHiragana, state.conversion_mode);
    }
    {
      const StatePair state =
          TestableTipInputModeManagerImpl::GetOverriddenState(
              StatePair(true, TipInputModeManagerImpl::kFullAscii),
              input_scope_invalid);
      EXPECT_TRUE(state.open_close);
      EXPECT_EQ(TipInputModeManagerImpl::kFullAscii, state.conversion_mode);
    }
  }
}

TEST(TipInputModeManagerTest, IgnoreConversionModeByGlobalConfig_Issue8583505) {
  // When per-session input mode is enabled, mode change notification from the
  // application should be ignored.
  TipInputModeManager input_mode_manager(GetGlobalMode());

  // Initialize (Off + Hiragana)
  input_mode_manager.OnInitialize(false, kNativeHiragana);

  // SetFocus (Off + Hiragana)
  std::vector<InputScope> input_scope_empty;
  auto action =
      input_mode_manager.OnSetFocus(false, kNativeHiragana, input_scope_empty);
  EXPECT_EQ(TipInputModeManager::kDoNothing, action);
  EXPECT_FALSE(input_mode_manager.IsIndicatorVisible());
  EXPECT_FALSE(input_mode_manager.GetEffectiveOpenClose());
  EXPECT_EQ(TipInputModeManager::kHiragana,
            input_mode_manager.GetEffectiveConversionMode());

  // On
  input_mode_manager.OnReceiveCommand(true, TipInputModeManager::kHiragana,
                                      TipInputModeManager::kHiragana);
  EXPECT_TRUE(input_mode_manager.GetEffectiveOpenClose());
  EXPECT_TRUE(input_mode_manager.IsIndicatorVisible());

  // Key
  action = input_mode_manager.OnKey(VirtualKey(), true, true);
  EXPECT_TRUE(input_mode_manager.GetEffectiveOpenClose());
  EXPECT_FALSE(input_mode_manager.IsIndicatorVisible());

  // Hiragana -> HalfAlpha (ignored)
  action = input_mode_manager.OnChangeConversionMode(kNativeHalfAlpha);
  EXPECT_EQ(TipInputModeManager::kDoNothing, action);
  EXPECT_FALSE(input_mode_manager.IsIndicatorVisible());
  EXPECT_EQ(TipInputModeManager::kHiragana,
            input_mode_manager.GetEffectiveConversionMode());
}

TEST(TipInputModeManagerTest, HonorConversionMode_Issue8583505) {
  // When thread local input mode is enabled, mode change notification from the
  // application should be honored.
  TipInputModeManager input_mode_manager(GetThreadLocalMode());

  // Initialize (Off + Hiragana)
  input_mode_manager.OnInitialize(false, kNativeHiragana);

  // SetFocus (Off + Hiragana)
  std::vector<InputScope> input_scope_empty;
  auto action =
      input_mode_manager.OnSetFocus(false, kNativeHiragana, input_scope_empty);
  EXPECT_EQ(TipInputModeManager::kDoNothing, action);
  EXPECT_FALSE(input_mode_manager.IsIndicatorVisible());
  EXPECT_FALSE(input_mode_manager.GetEffectiveOpenClose());
  EXPECT_EQ(TipInputModeManager::kHiragana,
            input_mode_manager.GetEffectiveConversionMode());

  // On
  input_mode_manager.OnReceiveCommand(true, TipInputModeManager::kHiragana,
                                      TipInputModeManager::kHiragana);
  EXPECT_TRUE(input_mode_manager.GetEffectiveOpenClose());
  EXPECT_TRUE(input_mode_manager.IsIndicatorVisible());

  // Key
  action = input_mode_manager.OnKey(VirtualKey(), true, true);
  EXPECT_TRUE(input_mode_manager.GetEffectiveOpenClose());
  EXPECT_FALSE(input_mode_manager.IsIndicatorVisible());

  // Hiragana -> HalfAlpha (honored)
  action = input_mode_manager.OnChangeConversionMode(kNativeHalfAlpha);
  EXPECT_EQ(TipInputModeManager::kUpdateUI, action);
  EXPECT_TRUE(input_mode_manager.IsIndicatorVisible());
  EXPECT_EQ(TipInputModeManager::kHalfAscii,
            input_mode_manager.GetEffectiveConversionMode());
}

TEST(TipInputModeManagerTest, ChangeInputScope) {
  TipInputModeManager input_mode_manager(GetThreadLocalMode());

  // Initialize (Off + Hiragana)
  input_mode_manager.OnInitialize(false, kNativeHiragana);

  // SetFocus (Off + Hiragana)
  std::vector<InputScope> input_scope_empty;
  auto action =
      input_mode_manager.OnSetFocus(false, kNativeHiragana, input_scope_empty);
  EXPECT_EQ(TipInputModeManager::kDoNothing, action);
  EXPECT_FALSE(input_mode_manager.IsIndicatorVisible());
  EXPECT_FALSE(input_mode_manager.GetEffectiveOpenClose());
  EXPECT_EQ(TipInputModeManager::kHiragana,
            input_mode_manager.GetEffectiveConversionMode());

  std::vector<InputScope> input_scope_full_katakana;
  input_scope_full_katakana.push_back(IS_KATAKANA_FULLWIDTH);

  // InputScope: IS_KATAKANA_FULLWIDTH
  // This should change the mode and make indicator visible.
  action = input_mode_manager.OnChangeInputScope(input_scope_full_katakana);
  EXPECT_EQ(TipInputModeManager::kUpdateUI, action);
  EXPECT_TRUE(input_mode_manager.GetEffectiveOpenClose());
  EXPECT_EQ(TipInputModeManager::kFullKatakana,
            input_mode_manager.GetEffectiveConversionMode());
  EXPECT_TRUE(input_mode_manager.IsIndicatorVisible());

  action = input_mode_manager.OnKey(VirtualKey(), true, true);
  EXPECT_FALSE(input_mode_manager.IsIndicatorVisible());

  // InputScope: IS_EMAIL_SMTPEMAILADDRESS
  // This should change the mode and make indicator visible.
  std::vector<InputScope> input_scope_email;
  input_scope_email.push_back(IS_EMAIL_SMTPEMAILADDRESS);
  action = input_mode_manager.OnChangeInputScope(input_scope_email);
  EXPECT_EQ(TipInputModeManager::kUpdateUI, action);
  EXPECT_FALSE(input_mode_manager.GetEffectiveOpenClose());
  EXPECT_EQ(TipInputModeManager::kHiragana,
            input_mode_manager.GetEffectiveConversionMode());
  EXPECT_TRUE(input_mode_manager.IsIndicatorVisible());

  // OnKey
  // This should make indicator invisible.
  action = input_mode_manager.OnKey(VirtualKey(), true, true);
  EXPECT_FALSE(input_mode_manager.IsIndicatorVisible());

  // InputScope: IS_NUMBER
  // This should not change the mode and keep indicator invisible.
  std::vector<InputScope> input_scope_number;
  input_scope_number.push_back(IS_NUMBER);
  action = input_mode_manager.OnChangeInputScope(input_scope_number);
  EXPECT_EQ(TipInputModeManager::kDoNothing, action);
  EXPECT_FALSE(input_mode_manager.GetEffectiveOpenClose());
  EXPECT_EQ(TipInputModeManager::kHiragana,
            input_mode_manager.GetEffectiveConversionMode());
  EXPECT_FALSE(input_mode_manager.IsIndicatorVisible());
}

TEST(TipInputModeManagerTest, HonorOpenCloseModeFromApps_Issue8661096) {
  TipInputModeManager input_mode_manager(GetGlobalMode());

  // Initialize (Off + Hiragana)
  input_mode_manager.OnInitialize(false, kNativeHiragana);

  // An application calls ImmSetOpenStatus(himc, TRUE)
  auto action = input_mode_manager.OnChangeOpenClose(true);
  EXPECT_EQ(TipInputModeManager::kUpdateUI, action);
  EXPECT_TRUE(input_mode_manager.IsIndicatorVisible());
  EXPECT_TRUE(input_mode_manager.GetEffectiveOpenClose());
  EXPECT_TRUE(input_mode_manager.GetTsfOpenClose());
}

}  // namespace
}  // namespace tsf
}  // namespace win32
}  // namespace mozc

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

#include "win32/tip/tip_lang_bar.h"

#include <ctfutb.h>
#include <guiddef.h>
#include <wil/com.h>
#include <windows.h>

#include <cstdint>
#include <iterator>
#include <utility>

#include "absl/log/log.h"
#include "base/win32/com.h"
#include "protocol/commands.pb.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_lang_bar_callback.h"
#include "win32/tip/tip_lang_bar_menu.h"
#include "win32/tip/tip_resource.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

constexpr bool kShowInTaskbar = true;

TipLangBarCallback::ItemId GetItemId(DWORD composition_mode) {
  switch (composition_mode) {
    case commands::DIRECT:
      return TipLangBarCallback::kDirect;
    case commands::HIRAGANA:
      return TipLangBarCallback::kHiragana;
    case commands::FULL_KATAKANA:
      return TipLangBarCallback::kFullKatakana;
    case commands::HALF_ASCII:
      return TipLangBarCallback::kHalfAlphanumeric;
    case commands::FULL_ASCII:
      return TipLangBarCallback::kFullAlphanumeric;
    case commands::HALF_KATAKANA:
      return TipLangBarCallback::kHalfKatakana;
    default:
      LOG(ERROR) << "Unknown composition mode: " << composition_mode;
      return TipLangBarCallback::kDirect;
  }
}

}  // namespace

// Initializes the input mode button, which is shown in the taskbar.
HRESULT TipLangBar::InitLangBar(TipLangBarCallback* text_service) {
  HRESULT result = S_OK;

  // TODO(yukawa): Optimize this method. We do not need to obtain an instance of
  // ITfLangBarItemMgr unless there remains something to be initialized for
  // LangBar.

  // A workaround to satisfy both b/6106437 and b/6641460.
  // Keep the instance in |lang_bar_item_mgr_|.
  if (!lang_bar_item_mgr_) {
    result = TF_CreateLangBarItemMgr(lang_bar_item_mgr_.put());
    if (FAILED(result)) {
      return result;
    }
    if (!lang_bar_item_mgr_) {
      return E_FAIL;
    }
  }

  const TipLangBarMenuItem kInputMenuDisabled = {kTipLangBarItemTypeDefault, 0,
                                                 IDS_DISABLED, IDI_DISABLED};

  if (!input_mode_button_for_win8_) {
    // Add the "Input Mode" button.
    constexpr TipLangBarMenuItem kInputMenu[] = {
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kHiragana,
         IDS_HIRAGANA, IDI_HIRAGANA},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kFullKatakana,
         IDS_FULL_KATAKANA, IDI_FULL_KATAKANA},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kFullAlphanumeric,
         IDS_FULL_ALPHANUMERIC, IDI_FULL_ALPHANUMERIC},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kHalfKatakana,
         IDS_HALF_KATAKANA, IDI_HALF_KATAKANA},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kHalfAlphanumeric,
         IDS_HALF_ALPHANUMERIC, IDI_HALF_ALPHANUMERIC},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kDirect, IDS_DIRECT,
         IDI_DIRECT},
        {kTipLangBarItemTypeSeparator, 0, 0, 0},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kDictionary,
         IDS_DICTIONARY, 0},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kWordRegister,
         IDS_WORD_REGISTER, 0},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kProperty,
         IDS_PROPERTY, 0},
        {kTipLangBarItemTypeSeparator, 0, 0, 0},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kAbout, IDS_ABOUT, 0},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kHelp, IDS_HELP, 0},
    };

    constexpr bool kNonMenuButton = false;
    auto input_mode_menu = MakeComPtr<TipLangBarToggleButton>(
        text_service, GUID_LBI_INPUTMODE, kNonMenuButton, kShowInTaskbar);
    if (input_mode_menu == nullptr) {
      return E_OUTOFMEMORY;
    }

    result = input_mode_menu->Init(TipDllModule::module_handle(),
                                   IDS_WIN8_TRAY_ITEM, kInputMenu,
                                   std::size(kInputMenu), kInputMenuDisabled);
    if (FAILED(result)) {
      return result;
    }
    result = lang_bar_item_mgr_->AddItem(input_mode_menu.get());
    input_mode_button_for_win8_ = std::move(input_mode_menu);
  }

  return result;
}

// IMPORTANT: See b/6106437 and b/6641460 before you change this method.
HRESULT TipLangBar::UninitLangBar() {
  HRESULT result = S_OK;

  // A workaround to satisfy both b/6106437 and b/6641460.
  // Retrieve the instance from |lang_bar_item_mgr_|.
  wil::com_ptr_nothrow<ITfLangBarItemMgr> item = std::move(lang_bar_item_mgr_);
  if (!item) {
    return E_FAIL;
  }

  if (input_mode_button_for_win8_) {
    item->RemoveItem(input_mode_button_for_win8_.get());
    input_mode_button_for_win8_.reset();
  }
  return result;
}

HRESULT TipLangBar::UpdateMenu(bool enabled, uint32_t composition_mode) {
  const UINT menu_id = GetItemId(composition_mode);
  input_mode_button_for_win8_->SelectMenuItem(menu_id);
  input_mode_button_for_win8_->SetEnabled(enabled);
  return S_OK;
}

bool TipLangBar::IsInitialized() const {
  return input_mode_button_for_win8_ != nullptr;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

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

#include <cstdint>

#include "base/logging.h"
#include "base/system_util.h"
#include "base/win_util.h"
#include "protocol/commands.pb.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_lang_bar_menu.h"
#include "win32/tip/tip_resource.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

using ATL::CComPtr;
using ATL::CComQIPtr;

// The GUID of the help menu in the system language bar.

// {ED9D5450-EBE6-4255-8289-F8A31E687228}
const GUID kSystemLangBarHelpMenu = {
    0xED9D5450,
    0xEBE6,
    0x4255,
    {0x82, 0x89, 0xF8, 0xA3, 0x1E, 0x68, 0x72, 0x28}};

// For Windows 8
// {2C77A81E-41CC-4178-A3A7-5F8A987568E6} == GUID_LBI_INPUTMODE
const GUID kSystemInputMode = {
    0x2C77A81E,
    0x41CC,
    0x4178,
    {0xA3, 0xA7, 0x5F, 0x8A, 0x98, 0x75, 0x68, 0xE6}};

#ifdef GOOGLE_JAPANESE_INPUT_BUILD

// {D8C8D5EB-8213-47CE-95B7-BA3F67757F94}
const GUID kTipLangBarItem_Button = {
    0xd8c8d5eb,
    0x8213,
    0x47ce,
    {0x95, 0xb7, 0xba, 0x3f, 0x67, 0x75, 0x7f, 0x94}};

// {0EAB48C4-F798-4CC8-91FA-087B24F520A8}
const GUID kTipLangBarItem_ToolButton = {
    0xeab48c4, 0xf798, 0x4cc8, {0x91, 0xfa, 0x8, 0x7b, 0x24, 0xf5, 0x20, 0xa8}};

// {6D46F0F2-2924-4666-9B89-4F23699B2203}
const GUID kTipLangBarItem_HelpMenu = {
    0x6d46f0f2,
    0x2924,
    0x4666,
    {0x9b, 0x89, 0x4f, 0x23, 0x69, 0x9b, 0x22, 0x3}};

#else  // GOOGLE_JAPANESE_INPUT_BUILD

// {FC8E2486-F5BA-4863-91C3-8D166B454604}
const GUID kTipLangBarItem_Button = {
    0xfc8e2486,
    0xf5ba,
    0x4863,
    {0x91, 0xc3, 0x8d, 0x16, 0x6b, 0x45, 0x46, 0x4}};

// {1BA637CA-7521-4F21-B51E-6516271A9FE3}
const GUID kTipLangBarItem_ToolButton = {
    0x1ba637ca,
    0x7521,
    0x4f21,
    {0xb5, 0x1e, 0x65, 0x16, 0x27, 0x1a, 0x9f, 0xe3}};

// {F78AD6B1-49D3-400E-8218-896F22A70011}
const GUID kTipLangBarItem_HelpMenu = {
    0xf78ad6b1,
    0x49d3,
    0x400e,
    {0x82, 0x18, 0x89, 0x6f, 0x22, 0xa7, 0x0, 0x11}};

#endif  // GOOGLE_JAPANESE_INPUT_BUILD

constexpr bool kShowInTaskbar = true;

CComPtr<ITfLangBarItemMgr> GetLangBarItemMgr() {
  // "msctf.dll" is not always available.  For example, Windows XP can disable
  // TSF completely.  In this case, the "msctf.dll" is not loaded.
  // Note that "msctf.dll" never be unloaded when it exists because we
  // increments its reference count here. This prevents weired crashes such as
  // b/4322508.
  const HMODULE module =
      WinUtil::GetSystemModuleHandleAndIncrementRefCount(L"msctf.dll");
  if (module == nullptr) {
    return nullptr;
  }
  void *function = ::GetProcAddress(module, "TF_CreateLangBarItemMgr");
  if (function == nullptr) {
    return nullptr;
  }
  typedef HRESULT(WINAPI * FPTF_CreateLangBarItemMgr)(ITfLangBarItemMgr *
                                                      *pplbim);
  CComPtr<ITfLangBarItemMgr> ptr;
  const HRESULT result =
      reinterpret_cast<FPTF_CreateLangBarItemMgr>(function)(&ptr);
  if (FAILED(result)) {
    return nullptr;
  }
  return ptr;
}

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

TipLangBarCallback::~TipLangBarCallback() {}

TipLangBar::TipLangBar()
    : tool_button_menu_(nullptr),
      help_menu_(nullptr),
      help_menu_cookie_(TF_INVALID_COOKIE) {}

TipLangBar::~TipLangBar() {}

// Initializes button menus in the language bar.
HRESULT TipLangBar::InitLangBar(TipLangBarCallback *text_service) {
  HRESULT result = S_OK;

  // TODO(yukawa): Optimize this method. We do not need to obtain an instance of
  // ITfLangBarItemMgr unless there remains something to be initialized for
  // LangBar.

  // A workaround to satisfy both b/6106437 and b/6641460.
  // On Windows 8, keep the instance into |lang_bar_item_mgr_for_win8_|.
  // On prior OSes, always instantiate new LangBarItemMgr object.
  CComPtr<ITfLangBarItemMgr> item;
  if (SystemUtil::IsWindows8OrLater()) {
    if (!lang_bar_item_mgr_for_win8_) {
      lang_bar_item_mgr_for_win8_ = GetLangBarItemMgr();
    }
    item = lang_bar_item_mgr_for_win8_;
  } else {
    item = GetLangBarItemMgr();
  }

  if (!item) {
    return E_FAIL;
  }

  const TipLangBarMenuItem kInputMenuDisabled = {kTipLangBarItemTypeDefault, 0,
                                                 IDS_DISABLED, IDI_DISABLED_NT,
                                                 IDI_DISABLED};

  if (input_button_menu_ == nullptr) {
    // Add the "Input Mode" button.
    const TipLangBarMenuItem kInputMenu[] = {
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kHiragana,
         IDS_HIRAGANA, IDI_HIRAGANA_NT, IDI_HIRAGANA},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kFullKatakana,
         IDS_FULL_KATAKANA, IDI_FULL_KATAKANA_NT, IDI_FULL_KATAKANA},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kFullAlphanumeric,
         IDS_FULL_ALPHANUMERIC, IDI_FULL_ALPHANUMERIC_NT,
         IDI_FULL_ALPHANUMERIC},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kHalfKatakana,
         IDS_HALF_KATAKANA, IDI_HALF_KATAKANA_NT, IDI_HALF_KATAKANA},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kHalfAlphanumeric,
         IDS_HALF_ALPHANUMERIC, IDI_HALF_ALPHANUMERIC_NT,
         IDI_HALF_ALPHANUMERIC},
        {kTipLangBarItemTypeRadioChecked, TipLangBarCallback::kDirect,
         IDS_DIRECT, IDI_DIRECT_NT, IDI_DIRECT},
        {kTipLangBarItemTypeSeparator, 0, 0, 0, 0},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kCancel, IDS_CANCEL, 0,
         0},
    };

    constexpr bool kMenuButton = true;
    CComPtr<TipLangBarToggleButton> input_button_menu(
        new TipLangBarToggleButton(text_service, kTipLangBarItem_Button,
                                   kMenuButton, kShowInTaskbar));
    if (input_button_menu == nullptr) {
      return E_OUTOFMEMORY;
    }

    result = input_button_menu->Init(TipDllModule::module_handle(),
                                     IDS_INPUTMODE, &kInputMenu[0],
                                     std::size(kInputMenu), kInputMenuDisabled);
    if (result != S_OK) {
      return result;
    }
    item->AddItem(input_button_menu);
    input_button_menu.QueryInterface(&input_button_menu_);
  }

  if (SystemUtil::IsWindows8OrLater() && !input_mode_button_for_win8_) {
    // Add the "Input Mode" button.
    const TipLangBarMenuItem kInputMenu[] = {
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kHiragana,
         IDS_HIRAGANA, IDI_HIRAGANA_NT, IDI_HIRAGANA},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kFullKatakana,
         IDS_FULL_KATAKANA, IDI_FULL_KATAKANA_NT, IDI_FULL_KATAKANA},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kFullAlphanumeric,
         IDS_FULL_ALPHANUMERIC, IDI_FULL_ALPHANUMERIC_NT,
         IDI_FULL_ALPHANUMERIC},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kHalfKatakana,
         IDS_HALF_KATAKANA, IDI_HALF_KATAKANA_NT, IDI_HALF_KATAKANA},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kHalfAlphanumeric,
         IDS_HALF_ALPHANUMERIC, IDI_HALF_ALPHANUMERIC_NT,
         IDI_HALF_ALPHANUMERIC},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kDirect, IDS_DIRECT,
         IDI_DIRECT_NT, IDI_DIRECT},
        {kTipLangBarItemTypeSeparator, 0, 0, 0, 0},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kDictionary,
         IDS_DICTIONARY, IDI_DICTIONARY_NT, IDI_DICTIONARY},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kWordRegister,
         IDS_WORD_REGISTER, IDI_DICTIONARY_NT, IDI_DICTIONARY},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kProperty,
         IDS_PROPERTY, IDI_PROPERTY_NT, IDI_PROPERTY},
        {kTipLangBarItemTypeSeparator, 0, 0, 0, 0},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kAbout, IDS_ABOUT, 0,
         0},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kHelp, IDS_HELP, 0, 0},
    };

    constexpr bool kNonMenuButton = false;
    CComPtr<TipLangBarToggleButton> input_mode_menu(new TipLangBarToggleButton(
        text_service, kSystemInputMode, kNonMenuButton, kShowInTaskbar));
    if (input_mode_menu == nullptr) {
      return E_OUTOFMEMORY;
    }

    result = input_mode_menu->Init(TipDllModule::module_handle(),
                                   IDS_WIN8_TRAY_ITEM, kInputMenu,
                                   std::size(kInputMenu), kInputMenuDisabled);
    if (FAILED(result)) {
      return result;
    }
    result = item->AddItem(input_mode_menu);
    input_mode_menu.QueryInterface(&input_mode_button_for_win8_);
  }

  if (tool_button_menu_ == nullptr) {
    // Add the "Tool" button.
    // TODO(yukawa): Make an Icon for kWordRegister.
    const TipLangBarMenuItem kToolMenu[] = {
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kDictionary,
         IDS_DICTIONARY, IDI_DICTIONARY_NT, IDI_DICTIONARY},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kWordRegister,
         IDS_WORD_REGISTER, IDI_DICTIONARY_NT,
         IDI_DICTIONARY},  // Use Dictionary icon temporarily
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kProperty,
         IDS_PROPERTY, IDI_PROPERTY_NT, IDI_PROPERTY},
        {kTipLangBarItemTypeSeparator, 0, 0, 0, 0},
        {kTipLangBarItemTypeDefault, TipLangBarCallback::kCancel, IDS_CANCEL, 0,
         0},
    };

    // Always show the tool icon so that a user can find the icon.
    // This setting is different from that of MS-IME but we believe this is
    // more friendly. See b/2275683
    CComPtr<TipLangBarMenuButton> tool_button(new TipLangBarMenuButton(
        text_service, kTipLangBarItem_ToolButton, kShowInTaskbar));
    if (tool_button == nullptr) {
      return E_OUTOFMEMORY;
    }

    result = tool_button->Init(TipDllModule::module_handle(), IDS_TOOL,
                               &kToolMenu[0], std::size(kToolMenu), IDI_TOOL_NT,
                               IDI_TOOL);
    if (result != S_OK) {
      return result;
    }
    item->AddItem(tool_button);
    tool_button.QueryInterface(&tool_button_menu_);
  }

  if (help_menu_ == nullptr) {
    // Add the "Help" items to the system language bar help menu.
    const TipLangBarMenuItem kHelpMenu[] = {
        {0, TipLangBarCallback::kAbout, IDS_ABOUT, 0, 0},
        {0, TipLangBarCallback::kHelp, IDS_HELP, 0, 0},
    };

    CComPtr<TipSystemLangBarMenu> help_menu(
        new TipSystemLangBarMenu(text_service, kTipLangBarItem_HelpMenu));
    if (help_menu == nullptr) {
      return E_OUTOFMEMORY;
    }

    result = help_menu->Init(TipDllModule::module_handle(), &kHelpMenu[0],
                             std::size(kHelpMenu));
    if (result != S_OK) {
      return result;
    }

    CComPtr<ITfLangBarItem> help_menu_item;
    result = item->GetItem(kSystemLangBarHelpMenu, &help_menu_item);
    if (result != S_OK) {
      return result;
    }
    CComPtr<ITfSource> source;
    result = help_menu_item.QueryInterface(&source);
    if (result != S_OK) {
      return result;
    }
    result = source->AdviseSink(
        IID_ITfSystemLangBarItemSink,
        static_cast<ITfSystemLangBarItemSink *>(help_menu), &help_menu_cookie_);
    if (result != S_OK) {
      return result;
    }

    help_menu_ = help_menu;
  }

  return result;
}

// IMPORTANT: See b/6106437 and b/6641460 before you change this method.
HRESULT TipLangBar::UninitLangBar() {
  HRESULT result = S_OK;

  // A workaround to satisfy both b/6106437 and b/6641460.
  // On Windows 8, retrieves the instance from |lang_bar_item_mgr_for_win8_|.
  // On prior OSes, always instantiates new LangBarItemMgr object.
  CComPtr<ITfLangBarItemMgr> item;
  if (SystemUtil::IsWindows8OrLater()) {
    // Move the ownership.
    item = lang_bar_item_mgr_for_win8_;
    lang_bar_item_mgr_for_win8_.Release();
  } else {
    item = GetLangBarItemMgr();
  }
  if (!item) {
    return E_FAIL;
  }

  if (input_mode_button_for_win8_ != nullptr) {
    item->RemoveItem(input_mode_button_for_win8_);
    input_mode_button_for_win8_ = nullptr;
  }
  if (input_button_menu_ != nullptr) {
    item->RemoveItem(input_button_menu_);
    input_button_menu_ = nullptr;
  }
  if (tool_button_menu_ != nullptr) {
    item->RemoveItem(tool_button_menu_);
    tool_button_menu_ = nullptr;
  }

  if ((help_menu_ != nullptr) && (help_menu_cookie_ != TF_INVALID_COOKIE)) {
    CComPtr<ITfLangBarItem> help_menu_item;
    result = item->GetItem(kSystemLangBarHelpMenu, &help_menu_item);
    if (result == S_OK) {
      CComPtr<ITfSource> source;
      result = help_menu_item.QueryInterface(&source);
      if (result == S_OK) {
        result = source->UnadviseSink(help_menu_cookie_);
        if (result == S_OK) {
          help_menu_cookie_ = TF_INVALID_COOKIE;
          help_menu_ = nullptr;
        }
      }
    }
  }

  return result;
}

HRESULT TipLangBar::UpdateMenu(bool enabled, uint32_t composition_mode) {
  HRESULT result = S_OK;

  const UINT menu_id = GetItemId(composition_mode);
  {
    CComQIPtr<IMozcLangBarToggleItem> mode_menu(input_button_menu_);
    if (mode_menu) {
      mode_menu->SelectMenuItem(menu_id);
    }
    CComQIPtr<IMozcLangBarToggleItem> mode_button(input_mode_button_for_win8_);
    if (mode_button) {
      mode_button->SelectMenuItem(menu_id);
    }
  }
  {
    CComQIPtr<IMozcLangBarItem> mode_menu_item(input_button_menu_);
    if (mode_menu_item) {
      mode_menu_item->SetEnabled(enabled);
    }
    CComQIPtr<IMozcLangBarItem> tool_menu_item(tool_button_menu_);
    if (tool_menu_item) {
      tool_menu_item->SetEnabled(enabled);
    }
    CComQIPtr<IMozcLangBarItem> mode_button_item(input_mode_button_for_win8_);
    if (mode_button_item) {
      mode_button_item->SetEnabled(enabled);
    }
  }
  return result;
}

bool TipLangBar::IsInitialized() const {
  return input_button_menu_ || input_mode_button_for_win8_;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

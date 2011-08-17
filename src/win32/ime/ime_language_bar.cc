// Copyright 2010-2011, Google Inc.
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

#include "win32/ime/ime_language_bar.h"

#include <windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#define _ATL_NO_HOSTING
#include <atlbase.h>
#include <atlcom.h>

#include "base/util.h"

#include "win32/ime/ime_impl_imm.h"
#include "win32/ime/ime_language_bar_menu.h"
#include "win32/ime/ime_resource.h"

using ATL::CComPtr;

namespace {
// The GUID of the help menu in the system language bar.
// TODO(mazda): Confirm this GUID is valid permanently for the system help menu
// since the GUID was programatically obtained.
// It is confirmed that the GUID is valid on Windows XP SP2 and Windows Vista.
// {ED9D5450-EBE6-4255-8289-F8A31E687228}
const GUID kSystemLangBarHelpMenu = {
  0xED9D5450, 0xEBE6, 0x4255, {0x82, 0x89, 0xF8, 0xA3, 0x1E, 0x68, 0x72, 0x28}
};

// {C4A8F44E-8100-44fe-BA5D-F226AA4B65CA}
const GUID kImeLangBarItem_Button = {
  0xc4a8f44e, 0x8100, 0x44fe, {0xba, 0x5d, 0xf2, 0x26, 0xaa, 0x4b, 0x65, 0xca}
};

// {EA1401B7-D2B3-4865-B321-2DC888079858}
const GUID kImeLangBarItem_ToolButton = {
  0xea1401b7, 0xd2b3, 0x4865, {0xb3, 0x21, 0x2d, 0xc8, 0x88, 0x07, 0x98, 0x58}
};

// {BBCA8C7B-C1E5-473d-8345-C65B2C02CDC8}
const GUID kImeLangBarItem_HelpMenu = {
  0xbbca8c7b, 0xc1e5, 0x473d, {0x83, 0x45, 0xc6, 0x5b, 0x2c, 0x02, 0xcd, 0xc8}
};

// Casts CComPtr<S> to D.
// Casting S* to D needs to be safe.
template <typename D, typename S>
D comptr_cast(CComPtr<S> ptr) {
  return reinterpret_cast<D>(static_cast<S*>(ptr));
}

CComPtr<ITfLangBarItemMgr> GetLangBarItemMgr() {
  // "msctf.dll" is not always available.  For example, Windows XP can disable
  // TSF completely.  In this case, the "msctf.dll" is not loaded.
  const HMODULE module = mozc::Util::GetSystemModuleHandle(L"msctf.dll");
  if (module == NULL) {
    return NULL;
  }
  void *function = ::GetProcAddress(module, "TF_CreateLangBarItemMgr");
  if (function == NULL) {
    return NULL;
  }
  typedef HRESULT (WINAPI *FPTF_CreateLangBarItemMgr)(
    ITfLangBarItemMgr **pplbim);
  CComPtr<ITfLangBarItemMgr> ptr;
  const HRESULT result = reinterpret_cast<FPTF_CreateLangBarItemMgr>(function)(
      &ptr);
  if (FAILED(result)) {
    return NULL;
  }
  return ptr;
}
}  // namespace

LanguageBar::LanguageBar()
    : input_button_menu_(NULL),
      input_button_cookie_(TF_INVALID_COOKIE),
      tool_button_menu_(NULL),
      help_menu_(NULL),
      help_menu_cookie_(TF_INVALID_COOKIE) {}

LanguageBar::~LanguageBar() {}

// Initializes button menus in the language bar.
HRESULT LanguageBar::InitLanguageBar(LangBarCallback *text_service) {
  HRESULT result = S_OK;

  CComPtr<ITfLangBarItemMgr> item = GetLangBarItemMgr();
  if (!item) {
    return E_FAIL;
  }

  if (input_button_menu_ == NULL) {
    // Add the "Input Mode" button.
    const ImeLangBarMenuItem kInputMenu[] = {
      {NULL, LangBarCallback::kHiragana, IDS_HIRAGANA,
       IDI_HIRAGANA_NT, IDI_HIRAGANA},
      {NULL, LangBarCallback::kFullKatakana, IDS_FULL_KATAKANA,
       IDI_FULL_KATAKANA_NT, IDI_FULL_KATAKANA},
      {NULL, LangBarCallback::kFullAlphanumeric, IDS_FULL_ALPHANUMERIC,
       IDI_FULL_ALPHANUMERIC_NT, IDI_FULL_ALPHANUMERIC},
      {NULL, LangBarCallback::kHalfKatakana, IDS_HALF_KATAKANA,
       IDI_HALF_KATAKANA_NT, IDI_HALF_KATAKANA},
      {NULL, LangBarCallback::kHalfAlphanumeric, IDS_HALF_ALPHANUMERIC,
       IDI_HALF_ALPHANUMERIC_NT, IDI_HALF_ALPHANUMERIC},
      {TF_LBMENUF_RADIOCHECKED, LangBarCallback::kDirect, IDS_DIRECT,
       IDI_DIRECT_NT, IDI_DIRECT},
      // TODO(yukawa): investigate the http://b/2104435 and
      // un-comment this line.
      // {TF_LBMENUF_SEPARATOR, 0, 0, 0, 0},
      {NULL, LangBarCallback::kCancel, IDS_CANCEL, 0, 0},
    };

    CComPtr<ImeToggleButtonMenu> input_button_menu;
    input_button_menu.Attach(
        new ImeToggleButtonMenu(text_service, kImeLangBarItem_Button));
    if (input_button_menu == NULL) {
      return E_OUTOFMEMORY;
    }

    result = input_button_menu->Init(ImeGetResource(), IDS_INPUTMODE,
                                      &kInputMenu[0], ARRAYSIZE(kInputMenu));
    if (result != S_OK) {
      return result;
    }
    item->AddItem(input_button_menu);

    input_button_menu_ = static_cast<ITfLangBarItemButton*>(input_button_menu);
  }

  if (tool_button_menu_ == NULL) {
    // Add the "Tool" button.
    // TODO(taku): Make an Icon for kWordRegister
    // TODO(yukawa): Make an Icon for kWordRegister kReconversion.
    // TODO(yukawa): Move kReconversion into other appropriate pull-down menu.
    const ImeLangBarMenuItem kToolMenu[] = {
      {NULL, LangBarCallback::kHandWriting, IDS_HAND_WRITING, 0, 0},
      {NULL, LangBarCallback::kCharacterPalette, IDS_CHARACTER_PALETTE, 0, 0},
      {NULL, LangBarCallback::kDictionary, IDS_DICTIONARY,
       IDI_DICTIONARY_NT, IDI_DICTIONARY},
      {NULL, LangBarCallback::kWordRegister, IDS_WORD_REGISTER,
       IDI_DICTIONARY_NT, IDI_DICTIONARY},  // Use Dictionary icon temporarily
      {NULL, LangBarCallback::kProperty, IDS_PROPERTY,
       IDI_PROPERTY_NT, IDI_PROPERTY},
      {TF_LBMENUF_SEPARATOR, 0, 0, 0, 0},
      {NULL, LangBarCallback::kCancel, IDS_CANCEL, 0, 0},
    };

    CComPtr<ImeIconButtonMenu> tool_button_menu;
    tool_button_menu.Attach(
        new ImeIconButtonMenu(text_service, kImeLangBarItem_ToolButton));
    if (tool_button_menu == NULL) {
      return E_OUTOFMEMORY;
    }

    result = tool_button_menu->Init(ImeGetResource(), IDS_TOOL,
                                    &kToolMenu[0], ARRAYSIZE(kToolMenu),
                                    IDI_TOOL_NT, IDI_TOOL);
    if (result != S_OK) {
      return result;
    }
    item->AddItem(tool_button_menu);

    tool_button_menu_ = static_cast<ITfLangBarItemButton*>(tool_button_menu);
  }

  if (help_menu_ == NULL) {
    // Add the "Help" items to the system language bar help menu.
    const ImeLangBarMenuItem kHelpMenu[] = {
      {NULL, LangBarCallback::kAbout, IDS_ABOUT, 0, 0},
      {NULL, LangBarCallback::kHelp, IDS_HELP, 0, 0},
    };

    CComPtr<ImeSystemLangBarMenu> help_menu;
    help_menu.Attach(
        new ImeSystemLangBarMenu(text_service, kImeLangBarItem_HelpMenu));
    if (help_menu == NULL) {
      return E_OUTOFMEMORY;
    }

    result = help_menu->Init(ImeGetResource(),
                             &kHelpMenu[0],
                             ARRAYSIZE(kHelpMenu));
    if (result != S_OK) {
      return result;
    }

    CComPtr<ITfLangBarItem> help_menu_item;
    result = item->GetItem(kSystemLangBarHelpMenu, &help_menu_item);
    if (result != S_OK) {
      return result;
    }
    CComPtr<ITfSource> source;
    result = help_menu_item->QueryInterface(IID_ITfSource,
                                            reinterpret_cast<void**>(&source));
    if (result != S_OK) {
      return result;
    }
    result = source->AdviseSink(
        IID_ITfSystemLangBarItemSink,
        static_cast<ITfSystemLangBarItemSink*>(help_menu),
        &help_menu_cookie_);
    if (result != S_OK) {
      return result;
    }

    help_menu_ = help_menu;
  }

  return result;
}

HRESULT LanguageBar::UninitLanguageBar() {
  HRESULT result = S_OK;

  CComPtr<ITfLangBarItemMgr> item = GetLangBarItemMgr();
  if (!item) {
    return E_FAIL;
  }

  if (input_button_menu_ != NULL) {
    item->RemoveItem(input_button_menu_);
    input_button_menu_ = NULL;
  }
  if (tool_button_menu_ != NULL) {
    item->RemoveItem(tool_button_menu_);
    tool_button_menu_ = NULL;
  }

  if ((help_menu_ != NULL) && (help_menu_cookie_ != TF_INVALID_COOKIE)) {
    CComPtr<ITfLangBarItem> help_menu_item;
    result = item->GetItem(kSystemLangBarHelpMenu, &help_menu_item);
    if (result == S_OK) {
      CComPtr<ITfSource> source;
      result = help_menu_item->QueryInterface(
          IID_ITfSource, reinterpret_cast<void**>(&source));
      if (result == S_OK) {
        result = source->UnadviseSink(help_menu_cookie_);
        if (result == S_OK) {
          help_menu_cookie_ = TF_INVALID_COOKIE;
          help_menu_ = NULL;
        }
      }
    }
  }

  return result;
}

HRESULT LanguageBar::UpdateLangbarMenu(
    mozc::commands::CompositionMode mode) {
  HRESULT result = S_OK;

  UINT menu_id = LangBarCallback::kDirect;
  if (input_button_menu_) {
    switch (mode) {
      case mozc::commands::DIRECT:
        menu_id = LangBarCallback::kDirect;
        break;
      case mozc::commands::HIRAGANA:
        menu_id = LangBarCallback::kHiragana;
        break;
      case mozc::commands::FULL_KATAKANA:
        menu_id = LangBarCallback::kFullKatakana;
        break;
      case mozc::commands::HALF_ASCII:
        menu_id = LangBarCallback::kHalfAlphanumeric;
        break;
      case mozc::commands::FULL_ASCII:
        menu_id = LangBarCallback::kFullAlphanumeric;
        break;
      case mozc::commands::HALF_KATAKANA:
        menu_id = LangBarCallback::kHalfKatakana;
        break;
      default:
        LOG(ERROR) << "Unknown composition mode: " << mode;
        return E_INVALIDARG;
    }
    comptr_cast<ImeToggleButtonMenu*>(input_button_menu_)->SelectMenuItem(
        menu_id);
  }

  return result;
}

HRESULT LanguageBar::SetLangbarMenuEnabled(bool enable) {
  HRESULT result = S_OK;

  if (input_button_menu_ != NULL) {
    comptr_cast<ImeToggleButtonMenu*>(input_button_menu_)->SetEnabled(enable);
  }
  if (tool_button_menu_ != NULL) {
    comptr_cast<ImeIconButtonMenu*>(tool_button_menu_)->SetEnabled(enable);
  }

  return result;
}

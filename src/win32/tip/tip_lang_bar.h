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

#ifndef MOZC_WIN32_TIP_TIP_LANG_BAR_H_
#define MOZC_WIN32_TIP_TIP_LANG_BAR_H_

#include <Unknwn.h>
#include <windows.h>

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>
#include <ctffunc.h>
#include <msctf.h>

#include <cstdint>

#include "base/port.h"

namespace mozc {
namespace win32 {
namespace tsf {

class TipSystemLangBarMenu;

class TipLangBarCallback : public IUnknown {
 public:
  enum ItemId {
    // Cancel something for general purpose
    kCancel = 1,

    // For input mode selection
    kDirect = 10,
    kHiragana = 11,
    kFullKatakana = 12,
    kHalfAlphanumeric = 13,
    kFullAlphanumeric = 14,
    kHalfKatakana = 15,

    // Tool menu
    kProperty = 20,
    kDictionary = 21,
    kWordRegister = 22,

    // Help Menu
    kHelp = 30,
    kAbout = 31,

    // Shortcut commands
    kReconversion = 41,
  };

  virtual ~TipLangBarCallback();

  virtual STDMETHODIMP OnMenuSelect(ItemId menu_id) = 0;
  virtual STDMETHODIMP OnItemClick(const wchar_t *description) = 0;
};

class TipLangBar {
 public:
  TipLangBar();
  TipLangBar(const TipLangBar &) = delete;
  TipLangBar &operator=(const TipLangBar &) = delete;
  ~TipLangBar();

  // initialize and uninitialize ImeLangBarItemButton object.
  HRESULT InitLangBar(TipLangBarCallback *text_service);
  HRESULT UninitLangBar();

  // Updates the selected menu in the language bar.
  HRESULT UpdateMenu(bool enabled, uint32_t composition_mode);

  // Returns true if this instance is already initialized.
  bool IsInitialized() const;

 private:
  // Represents the language bar item manager iff the running OS is Windows 8.
  // NOTE: We must use the same instance of this class to initialize and
  //     uninitialize LangBar items. Otherwise, you will see weird crashes
  //     around refcount on Windows 8 release preview. b/6106437
  // NOTE: Currently we cannot use the same logic for Windows 7 due to another
  //     crash issue as filed as b/6641460.
  ATL::CComPtr<ITfLangBarItemMgr> lang_bar_item_mgr_for_win8_;

  // Represents the button menu in the language bar.
  // NOTE: ImeToggleButtonMenu inherits ITfLangBarItemButton and ITfSource,
  // which inherit IUnknown. Because of this, using CComPtr<ImeIconButtonMenu>
  // causes a compile error due to the ambiguous overload resolution.
  // To avoid the compile error we use CComPtr<ITfLangBarItemButton> instead.
  ATL::CComPtr<ITfLangBarItemButton> input_button_menu_;
  ATL::CComPtr<ITfLangBarItemButton> input_mode_button_for_win8_;

  // Represents the tool button menu in the language bar.
  ATL::CComPtr<ITfLangBarItemButton> tool_button_menu_;

  // Represents the help menu in the system language bar.
  ATL::CComPtr<TipSystemLangBarMenu> help_menu_;

  // The cookie issued for installing ITfSystemLangBarItemSink of help_menu_.
  DWORD help_menu_cookie_;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_LANG_BAR_H_

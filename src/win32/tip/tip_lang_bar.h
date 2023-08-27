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

#include <ctffunc.h>
#include <msctf.h>
#include <unknwn.h>
#include <wil/com.h>
#include <windows.h>

#include <cstdint>

#include "win32/tip/tip_lang_bar_callback.h"
#include "win32/tip/tip_lang_bar_menu.h"

namespace mozc {
namespace win32 {
namespace tsf {

class TipSystemLangBarMenu;

class TipLangBar {
 public:
  TipLangBar()
      : tool_button_menu_(nullptr),
        help_menu_(nullptr),
        help_menu_cookie_(TF_INVALID_COOKIE) {}
  TipLangBar(const TipLangBar &) = delete;
  TipLangBar &operator=(const TipLangBar &) = delete;
  ~TipLangBar() = default;

  // initialize and uninitialize ImeLangBarItemButton object.
  HRESULT InitLangBar(TipLangBarCallback *text_service);
  HRESULT UninitLangBar();

  // Updates the selected menu in the language bar.
  HRESULT UpdateMenu(bool enabled, uint32_t composition_mode);

  // Returns true if this instance is already initialized.
  bool IsInitialized() const;

 private:
  // Represents the language bar item manager.
  // NOTE: We must use the same instance of this class to initialize and
  //     uninitialize LangBar items. Otherwise, you will see weird crashes
  //     around refcount on Windows 8 release preview. b/6106437
  wil::com_ptr_nothrow<ITfLangBarItemMgr> lang_bar_item_mgr_;

  // Represents the button menu in the language bar.
  // NOTE: ImeToggleButtonMenu inherits ITfLangBarItemButton and ITfSource,
  // which inherit IUnknown. Because of this, using CComPtr<ImeIconButtonMenu>
  // causes a compile error due to the ambiguous overload resolution.
  // To avoid the compile error we use CComPtr<ITfLangBarItemButton> instead.
  wil::com_ptr_nothrow<TipLangBarToggleButton> input_button_menu_;
  wil::com_ptr_nothrow<TipLangBarToggleButton> input_mode_button_for_win8_;

  // Represents the tool button menu in the language bar.
  wil::com_ptr_nothrow<TipLangBarMenuButton> tool_button_menu_;

  // Represents the help menu in the system language bar.
  wil::com_ptr_nothrow<TipSystemLangBarMenu> help_menu_;

  // The cookie issued for installing ITfSystemLangBarItemSink of help_menu_.
  DWORD help_menu_cookie_;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_LANG_BAR_H_

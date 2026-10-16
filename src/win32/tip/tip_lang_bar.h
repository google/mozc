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

// TODO: https://github.com/google/mozc/issues/1600 - Rename classes here.
class TipLangBar {
 public:
  TipLangBar() = default;
  TipLangBar(const TipLangBar&) = delete;
  TipLangBar& operator=(const TipLangBar&) = delete;
  ~TipLangBar() = default;

  // initialize and uninitialize ImeLangBarItemButton object.
  HRESULT InitLangBar(TipLangBarCallback* text_service);
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

  // Represents the input mode button, which is shown in the taskbar as the IME
  // mode icon on Windows 8 and later.
  wil::com_ptr_nothrow<TipLangBarToggleButton> input_mode_button_for_win8_;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_LANG_BAR_H_

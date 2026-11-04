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

#ifndef MOZC_WIN32_TIP_TIP_LANG_BAR_MENU_H_
#define MOZC_WIN32_TIP_TIP_LANG_BAR_MENU_H_

#include <ctfutb.h>
#include <msctf.h>
#include <unknwn.h>
#include <wil/com.h>
#include <windows.h>

#include <cstddef>
#include <string>
#include <vector>

#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_lang_bar_callback.h"

namespace mozc {
namespace win32 {

// This declaration is necessary because this header file is included multiple
// times.
template <>
bool IsIIDOf<ITfLangBarItemButton>(REFIID riid);

namespace tsf {

enum TipLangBarItemFlags {
  kTipLangBarItemTypeDefault = 0,
  kTipLangBarItemTypeChecked = TF_LBMENUF_CHECKED,  // 0x1
  // ImeLangBarItemTypeSubMenu = TF_LBMENUF_SUBMENU  // 0x2 (not supported)
  kTipLangBarItemTypeSeparator = TF_LBMENUF_SEPARATOR,        // 0x4
  kTipLangBarItemTypeRadioChecked = TF_LBMENUF_RADIOCHECKED,  // 0x8
  kTipLangBarItemTypeGrayed = TF_LBMENUF_GRAYED,              // 0x10
};

// Represents a tuple to specify the content of a language bar menu item.
struct TipLangBarMenuItem {
  int flags_;
  UINT item_id_;
  UINT text_id_;
  UINT icon_id_;
};

// Represents the data possessed internally by a language bar menu item.
struct TipLangBarMenuData {
  int flags_;
  UINT item_id_;
  UINT text_id_;
  UINT icon_id_;
  wchar_t text_[TF_LBI_DESC_MAXLEN];
};

// Represents the data possessed by a language bar menu.
class TipLangBarMenuDataArray {
 public:
  HRESULT Init(HINSTANCE instance, const TipLangBarMenuItem* menu, int count);

  size_t size() const;
  TipLangBarMenuData* data(size_t i);

 private:
  std::vector<TipLangBarMenuData> data_;
};

// Represents the input mode button, which is shown in the taskbar as the IME
// mode icon. Left click toggles the IME open/close status and right click
// shows the context menu.
class TipLangBarButton
    : public TipComImplements<ITfLangBarItemButton, ITfSource> {
 public:
  explicit TipLangBarButton(
      wil::com_ptr_nothrow<TipLangBarCallback> lang_bar_callback);

  // The ITfLangBarItem interface methods
  STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* item_info) override;
  STDMETHODIMP GetStatus(DWORD* status) override;
  STDMETHODIMP Show(BOOL show) override;
  STDMETHODIMP GetTooltipString(BSTR* tooltip) override;

  // The ITfLangBarItemButton interface methods
  STDMETHODIMP OnClick(TfLBIClick click, POINT point,
                       const RECT* rect) override;
  STDMETHODIMP InitMenu(ITfMenu* menu) override;
  STDMETHODIMP OnMenuSelect(UINT menu_id) override;
  STDMETHODIMP GetIcon(HICON* icon) override;
  STDMETHODIMP GetText(BSTR* text) override;

  // The ITfSource interface methods
  STDMETHODIMP AdviseSink(REFIID interface_id, IUnknown* unknown,
                          DWORD* cookie) override;
  STDMETHODIMP UnadviseSink(DWORD cookie) override;

  // Initializes a TipLangBarButton instance.
  // This function allocates resources for a TipLangBarButton instance.
  HRESULT Init(HINSTANCE instance, int string_id,
               const TipLangBarMenuItem* menu, int count,
               const TipLangBarMenuItem& menu_for_disabled);

  // Selects a menu item which has the given |menu_id|.
  HRESULT SelectMenuItem(UINT menu_id);

  // Sets the status of this language bar button.
  HRESULT SetEnabled(bool enabled);

 private:
  // Notifies the language bar of a change in a language bar item.
  HRESULT OnUpdate(DWORD update_flag);

  // Update the item description. The caller is also responsible for calling
  // OnUpdate method to notify the change to the system.
  void SetDescription(const std::wstring& description);

  // Returns the i-th data in the language bar menu.
  // Returns nullptr if i is out of bounds.
  TipLangBarMenuData* menu_data(size_t i);

  // Returns the size of the language bar menu.
  size_t menu_data_size() const;

  wil::com_ptr_nothrow<ITfLangBarItemSink> item_sink_;
  // Save the TipLangBarCallback object that owns this button to prevent the
  // object from being deleted.
  wil::com_ptr_nothrow<TipLangBarCallback> lang_bar_callback_;

  // Represents the information of an instance copied to the TSF manager.
  // The TSF manager uses this information to identifies an instance as
  // a button.
  TF_LANGBARITEMINFO item_info_;

  // Represents the data possessed by the language bar menu.
  TipLangBarMenuDataArray menu_data_;

  // Represents the index of the selected menu item.
  UINT menu_selected_;
  bool disabled_;
  std::wstring description_for_enabled_;
  TipLangBarMenuData menu_data_for_disabled_;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_LANG_BAR_MENU_H_

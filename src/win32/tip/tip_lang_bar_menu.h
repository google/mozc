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

#include <msctf.h>
#include <rpcsal.h>
#include <windows.h>

#include <string>
#include <vector>

#include "win32/tip/tip_lang_bar.h"
#include "win32/tip/tip_ref_count.h"

namespace mozc {
namespace win32 {
namespace tsf {

// MIDL_INTERFACE expects a string literal rather than a constant array of
// characters.
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
#define IIDSTR_IMozcLangBarItem "C6057858-8A94-4B40-8C99-D1C4B4A0B9DB"
#define IIDSTR_IMozcLangBarToggleItem "72B4C4E3-B9F3-478A-B8A8-753AFF37EB94"
#else
#define IIDSTR_IMozcLangBarItem "75B2153A-504B-48C9-9257-BA8D60E523E6"
#define IIDSTR_IMozcLangBarToggleItem "9ABF0C3B-4AC6-4DED-9EF6-97E728852CF3"
#endif

class TipBarCallback;

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
  UINT icon_id_for_non_theme_;
  UINT icon_id_for_theme_;
};

// Reperesents the data possessed internally by a language bar menu item.
struct TipLangBarMenuData {
  int flags_;
  UINT item_id_;
  UINT text_id_;
  UINT icon_id_for_non_theme_;
  UINT icon_id_for_theme_;
  int length_;
  wchar_t text_[TF_LBI_DESC_MAXLEN];
};

// Reperesents the data possessed by a language bar menu.
class TipLangBarMenuDataArray {
 public:
  HRESULT Init(HINSTANCE instance, const TipLangBarMenuItem *menu, int count);

  size_t size() const;
  TipLangBarMenuData *data(size_t i);

 private:
  std::vector<TipLangBarMenuData> data_;
};

MIDL_INTERFACE(IIDSTR_IMozcLangBarItem)
IMozcLangBarItem : public IUnknown {
  // Sets the status of this language bar menu.
  virtual STDMETHODIMP SetEnabled(bool enabled) = 0;
};

MIDL_INTERFACE(IIDSTR_IMozcLangBarToggleItem)
IMozcLangBarToggleItem : public IUnknown {
  // Selects a menu item which has the given |menu_id|.
  virtual STDMETHODIMP SelectMenuItem(UINT menu_id) = 0;
};

// Represents the common operations for a button-menu item in the language bar.
class TipLangBarButton : public ITfLangBarItemButton,
                         public ITfSource,
                         public IMozcLangBarItem {
 public:
  TipLangBarButton(TipLangBarCallback *langbar_callback, const GUID &guid,
                   bool is_menu, bool show_in_tray);
  // It is OK to declare the destructor as non-virtual because all the
  // insbtances will be deleted from and only from the Release() method by
  // |delete this| where |this| has the true (derived) type of the instance.
  ~TipLangBarButton();

  // The IUnknown interface methods
  virtual STDMETHODIMP QueryInterface(REFIID guid, void **object) = 0;
  virtual STDMETHODIMP_(ULONG) AddRef() = 0;
  virtual STDMETHODIMP_(ULONG) Release() = 0;

  // The ITfLangBarItem interface methods
  virtual STDMETHODIMP GetInfo(TF_LANGBARITEMINFO *item_info) = 0;
  virtual STDMETHODIMP GetStatus(DWORD *status);
  virtual STDMETHODIMP Show(BOOL show);
  virtual STDMETHODIMP GetTooltipString(BSTR *tooltip);

  // The ITfLangBarItemButton interface methods
  virtual STDMETHODIMP OnClick(TfLBIClick clink, POINT point, const RECT *rect);
  virtual STDMETHODIMP InitMenu(ITfMenu *menu) = 0;
  virtual STDMETHODIMP OnMenuSelect(UINT menu_id) = 0;
  virtual STDMETHODIMP GetIcon(HICON *icon) = 0;
  virtual STDMETHODIMP GetText(BSTR *text);

  // The ITfSource interface methods
  virtual STDMETHODIMP AdviseSink(REFIID interface_id, IUnknown *unknown,
                                  DWORD *cookie);
  virtual STDMETHODIMP UnadviseSink(DWORD cookie);

  // The IMozcLangBarItem interface method
  virtual STDMETHODIMP SetEnabled(bool enabled);

  // Initializes an ImeButtonMenu instance.
  // This function allocates resources for an ImeButtonMenu instance.
  HRESULT Init(HINSTANCE instance, int string_id,
               const TipLangBarMenuItem *menu, int count);

  // Notifies the language bar of a change in a language bar item.
  STDMETHODIMP OnUpdate(DWORD update_flag);

  // Returns true if a 32-bpp icon can be displayed as a context menu item on
  // the LangBar.  See http://b/2260057 and http://b/2265755
  // for details.
  static bool CanContextMenuDisplay32bppIcon();

 protected:
  STDMETHODIMP QueryInterfaceBase(REFIID guid, void **object);

  // Update the item description. The caller is also responsible for calling
  // OnUpdate method to notify the change to the system.
  void SetDescription(const std::wstring &description);

  // Returns the i-th data in the language bar menu.
  // Returns nullptr if i is out of bounds.
  TipLangBarMenuData *menu_data(size_t i);

  // Returns the size of the language bar menu.
  size_t menu_data_size() const;

  // Returns the item information for langbar menu.
  const TF_LANGBARITEMINFO *item_info() const;

  // Returns true if the attribute is menu button.
  bool IsMenuButton() const;

  // Sets if context menu is enabled or not. Is setting affects only when
  // IsMenuButton() returns false.
  void SetContextMenuEnabled(bool enabled);

  ITfLangBarItemSink *item_sink_;
  TipLangBarCallback *langbar_callback_;

 private:
  // Represents the information of an instance copied to the TSF manager.
  // The TSF manager uses this information to identifies an instance as
  // a menu button.
  TF_LANGBARITEMINFO item_info_;

  // Represents the data possessed by the language bar menu.
  TipLangBarMenuDataArray menu_data_;

  // Records TF_LBI_STATUS_* bits and represents the status of the this langbar
  // menu.
  DWORD status_;

  // Represents if context menu is enabled or not. This flag is not used when
  // IsMenuButton() returns true.
  bool context_menu_enabled_;
};

// Represents the common operations for a button-menu item with an icon in the
// language bar.
class TipLangBarMenuButton : public TipLangBarButton {
 public:
  TipLangBarMenuButton(TipLangBarCallback *langbar_callback, const GUID &guid,
                       bool show_in_tray);

  // The IUnknown interface methods
  virtual STDMETHODIMP QueryInterface(REFIID guid, void **object);
  virtual STDMETHODIMP_(ULONG) AddRef();
  virtual STDMETHODIMP_(ULONG) Release();

  virtual STDMETHODIMP GetInfo(TF_LANGBARITEMINFO *item_info);

  // A part of the ITfLangBarItemButton interface methods
  virtual STDMETHODIMP InitMenu(ITfMenu *menu);
  virtual STDMETHODIMP OnMenuSelect(UINT menu_id);
  virtual STDMETHODIMP GetIcon(HICON *icon);

  // Initializes an ImeButtonMenu instance.
  // This function allocates resources for an ImeButtonMenu instance.
  HRESULT Init(HINSTANCE instance, UINT string_id,
               const TipLangBarMenuItem *menu, int count,
               UINT menu_icon_id_for_non_theme, UINT menu_icon_id_for_theme);

 private:
  TipRefCount ref_count_;

  // Represents the icon of the language bar menu.
  UINT menu_icon_id_for_theme_;
  UINT menu_icon_id_for_non_theme_;
};

// Represents the common operations for a toggle button-menu item in the
// language bar.
class TipLangBarToggleButton : public TipLangBarButton,
                               public IMozcLangBarToggleItem {
 public:
  TipLangBarToggleButton(TipLangBarCallback *langbar_callback, const GUID &guid,
                         bool is_menu, bool show_in_tray);
  virtual ~TipLangBarToggleButton();

  // The IUnknown interface methods
  virtual STDMETHODIMP QueryInterface(REFIID guid, void **object);
  virtual STDMETHODIMP_(ULONG) AddRef();
  virtual STDMETHODIMP_(ULONG) Release();

  // The IMozcLangBarToggleItem interface methods
  virtual STDMETHODIMP SelectMenuItem(UINT menu_id);

  // The IMozcLangBarItem interface method
  // Overridden from TipLangBarButton.
  virtual STDMETHODIMP SetEnabled(bool enabled);

  virtual STDMETHODIMP GetInfo(TF_LANGBARITEMINFO *item_info);

  virtual STDMETHODIMP InitMenu(ITfMenu *menu);
  virtual STDMETHODIMP OnMenuSelect(UINT menu_id);
  virtual STDMETHODIMP GetIcon(HICON *icon);

  // Initializes an ImeButtonMenu instance.
  // This function allocates resources for an ImeButtonMenu instance.
  HRESULT Init(HINSTANCE instance, int string_id,
               const TipLangBarMenuItem *menu, int count,
               const TipLangBarMenuItem &menu_for_disabled);

 private:
  TipRefCount ref_count_;

  // Represents the index of the selected menu item.
  UINT menu_selected_;
  bool disabled_;
  std::wstring description_for_enabled_;
  TipLangBarMenuData menu_data_for_disabled_;
};

// Represents the common operations for a button-menu item in the system
// language bar.
class TipSystemLangBarMenu : public ITfSystemLangBarItemSink {
 public:
  TipSystemLangBarMenu(TipLangBarCallback *langbar_callback, const GUID &guid);
  virtual ~TipSystemLangBarMenu();

  // The IUnknown interface methods
  virtual STDMETHODIMP QueryInterface(REFIID guid, void **object);
  virtual STDMETHODIMP_(ULONG) AddRef();
  virtual STDMETHODIMP_(ULONG) Release();

  // The ITfLangBarSystemItem interface methods
  virtual STDMETHODIMP InitMenu(ITfMenu *menu);
  virtual STDMETHODIMP OnMenuSelect(UINT menu_id);

  // Initializes an ImeButtonMenu instance.
  // This function allocates resources for an ImeButtonMenu instance.
  HRESULT Init(HINSTANCE instance, const TipLangBarMenuItem *menu, int count);

 private:
  TipRefCount ref_count_;
  TipLangBarCallback *langbar_callback_;

  // Represents the data possessed by the language bar menu.
  TipLangBarMenuDataArray menu_data_;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_LANG_BAR_MENU_H_

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

#include "win32/tip/tip_lang_bar_menu.h"

// clang-format off
#include <atlbase.h>
#include <atltypes.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <atlgdi.h>
#include <atluser.h>
#include <ctfutb.h>
#include <ole2.h>
#include <olectl.h>
#include <strsafe.h>
// clang-format on
#include <wil/com.h>

#include <cstddef>
#include <string>
#include <utility>

#include "base/win32/com.h"
#include "base/win32/com_implements.h"
#include "absl/base/casts.h"
#include "absl/base/macros.h"
#include "win32/base/text_icon.h"
#include "win32/base/tsf_profile.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_lang_bar_callback.h"
#include "win32/tip/tip_resource.h"

namespace mozc {
namespace win32 {

template <>
bool IsIIDOf<ITfLangBarItemButton>(REFIID riid) {
  return IsIIDOf<ITfLangBarItemButton, ITfLangBarItem>(riid);
}

namespace tsf {
namespace {

using WTL::CBitmap;
using WTL::CDC;
using WTL::CIcon;
using WTL::CMenu;
using WTL::CMenuItemInfo;

// Represents the cookie for the sink to a TipLangBarButton object.
constexpr int kTipLangBarMenuCookie =
    (('M' << 24) | ('o' << 16) | ('z' << 8) | ('c' << 0));

constexpr char kTextIconFont[] = "ＭＳ ゴシック";

// TODO(yukawa): Refactor LangBar code so that we can configure following
// settings as a part of initialization.
std::string GetIconStringIfNecessary(UINT icon_id) {
  switch (icon_id) {
    case IDI_DIRECT_NT:
      return "A";
    case IDI_HIRAGANA_NT:
      return "あ";
    case IDI_FULL_KATAKANA_NT:
      return "ア";
    case IDI_HALF_ALPHANUMERIC_NT:
      return "_A";
    case IDI_FULL_ALPHANUMERIC_NT:
      return "Ａ";
    case IDI_HALF_KATAKANA_NT:
      return "_ｱ";
  }
  return "";
}

// Loads an icon which is appropriate for the current theme.
// An icon ID 0 represents "no icon".
HICON LoadIconFromResource(HINSTANCE instance, UINT icon_id) {
  const auto icon_size = ::GetSystemMetrics(SM_CYSMICON);

  // Replace some text icons with on-the-fly image drawn with MS-Gothic.
  const auto &icon_text = GetIconStringIfNecessary(icon_id);
  if (!icon_text.empty()) {
    const COLORREF text_color = ::GetSysColor(COLOR_WINDOWTEXT);
    return TextIcon::CreateMonochromeIcon(icon_size, icon_size, icon_text,
                                          kTextIconFont, text_color);
  }

  return static_cast<HICON>(::LoadImage(instance, MAKEINTRESOURCE(icon_id),
                                        IMAGE_ICON, icon_size, icon_size,
                                        LR_CREATEDIBSECTION));
}

// Retrieves the bitmap handle loaded by using an icon ID.
// Returns true if the specified icons is available as bitmaps.
// Caller can set nullptr for |color| and/or |mask| to represent not to receive
// the specified handle even if it exists.  Caller should releases any returned
// bitmap handle and this function releases any handle which is not received
// by the caller.
bool LoadIconAsBitmap(HINSTANCE instance, UINT icon_id_for_non_theme,
                      UINT icon_id_for_theme, HBITMAP *color, HBITMAP *mask) {
  if (color != nullptr) {
    *color = nullptr;
  }
  if (mask != nullptr) {
    *mask = nullptr;
  }

  CIcon icon(LoadIconFromResource(instance, icon_id_for_theme));

  if (icon.IsNull()) {
    return false;
  }

  ICONINFO icon_info = {};
  if (!::GetIconInfo(icon, &icon_info)) {
    return false;
  }

  CBitmap color_bitmap(icon_info.hbmColor);
  CBitmap mask_bitmap(icon_info.hbmMask);

  if (color != nullptr) {
    *color = color_bitmap.Detach();
  }
  if (mask != nullptr) {
    *mask = mask_bitmap.Detach();
  }

  return true;
}

}  // namespace

HRESULT TipLangBarMenuDataArray::Init(HINSTANCE instance,
                                      const TipLangBarMenuItem *menu,
                                      int count) {
  HRESULT result = S_OK;

  // Attach menu texts and icons.
  for (int i = 0; i < count; ++i) {
    TipLangBarMenuData data = {};
    int length = 0;
    HICON icon = nullptr;
    if ((menu[i].flags_ & TF_LBMENUF_SEPARATOR) == 0) {
      // Retrieve the menu text and button icon.
      length = ::LoadString(instance, menu[i].text_id_, &data.text_[0],
                            std::size(data.text_));
    }
    data.flags_ = menu[i].flags_;
    data.item_id_ = menu[i].item_id_;
    data.text_id_ = menu[i].text_id_;
    data.length_ = length;
    data.icon_id_for_non_theme_ = menu[i].icon_id_for_non_theme_;
    data.icon_id_for_theme_ = menu[i].icon_id_for_theme_;
    data_.push_back(data);
  }

  return result;
}

size_t TipLangBarMenuDataArray::size() const { return data_.size(); }

TipLangBarMenuData *TipLangBarMenuDataArray::data(size_t i) {
  if (i >= size()) {
    return nullptr;
  }
  return &data_[i];
}

// Implements the constructor of the TipLangBarButton class.
TipLangBarButton::TipLangBarButton(
    wil::com_ptr_nothrow<TipLangBarCallback> lang_bar_callback,
    const GUID &guid, bool is_menu, bool show_in_tray)
    : lang_bar_callback_(std::move(lang_bar_callback)),
      status_(0),
      context_menu_enabled_(true) {
  // Initialize its TF_LANGBARITEMINFO object, which contains the properties of
  // this item and is copied to the TSF manager in GetInfo() function.
  item_info_.clsidService = TsfProfile::GetTextServiceGuid();
  item_info_.guidItem = guid;
  // The visibility of a langbar-item in the minimized langbar is actually
  // controlled by TF_LBI_STYLE_SHOWNINTRAY flag despite the fact that the
  // document says "This flag is not currently supported".
  // http://msdn.microsoft.com/en-us/library/ms629078.aspx
  // http://b/2275633
  item_info_.dwStyle = 0;
  if (is_menu) {
    item_info_.dwStyle |= TF_LBI_STYLE_BTN_MENU;
  } else {
    item_info_.dwStyle |= TF_LBI_STYLE_BTN_BUTTON;
  }
  if (show_in_tray) {
    item_info_.dwStyle |= TF_LBI_STYLE_SHOWNINTRAY;
  }
  item_info_.ulSort = 0;
  item_info_.szDescription[0] = L'\0';
}

// Implements the ITfLangBarItem::GetInfo() function.
// This function is called by Windows to update this button menu.
STDMETHODIMP TipLangBarButton::GetInfo(TF_LANGBARITEMINFO *item_info) {
  // Just copies the cached TF_LANGBARITEMINFO object.
  *item_info = item_info_;
  return S_OK;
}

// Implements the ITfLangBarItem::GetStatus() function.
// This function is called by Windows to retrieve the current status of this
// button menu.
STDMETHODIMP TipLangBarButton::GetStatus(DWORD *status) {
  *status = status_;
  return S_OK;
}

// Implements the ITfLangBarItem::Show() function.
// This function is called by Windows to notify the display status of this
// button menu has been updated.
STDMETHODIMP TipLangBarButton::Show(BOOL show) {
  // Just return because this button is always shown, i.e. we do not have to
  // manage the display state of this button menu.
  return E_NOTIMPL;
}

// Implements the ITfLangBarItem::GetTooltipString() function.
// This function is called when Windows to retrieve the tool-tip string of
// this button menu.
STDMETHODIMP TipLangBarButton::GetTooltipString(BSTR *tooltip) {
  // Created a COM string from the description and copy it.
  *tooltip = ::SysAllocString(&item_info_.szDescription[0]);
  return (*tooltip ? S_OK : E_OUTOFMEMORY);
}

// Implements the ITfLangBarItemButton::OnClick() function.
// This function is not used for a button menu.
STDMETHODIMP TipLangBarButton::OnClick(TfLBIClick click, POINT point,
                                       const RECT *rect) {
  if (IsMenuButton()) {
    // ITfLangBarItem object is a button menu.
    return S_OK;
  }
  if (click == TF_LBI_CLK_LEFT) {
    return lang_bar_callback_->OnItemClick(item_info_.szDescription);
  }

  // If context menu is disabled, do nothing.
  if (!context_menu_enabled_) {
    return S_OK;
  }

  CMenu menu;
  menu.CreatePopupMenu();
  for (size_t i = 0; i < menu_data_size(); ++i) {
    TipLangBarMenuData *data = menu_data(i);
    const UINT id = static_cast<UINT>(data->item_id_);
    const wchar_t *text = data->text_;
    CMenuItemInfo info;
    if (data->flags_ == TF_LBMENUF_SEPARATOR) {
      info.fMask |= MIIM_FTYPE;
      info.fType |= MFT_SEPARATOR;
    } else {
      info.fMask |= MIIM_ID;
      info.wID = id;

      info.fMask |= MIIM_FTYPE;
      info.fType |= MFT_STRING;

      info.fMask |= MIIM_STRING;
      info.dwTypeData = data->text_;

      switch (data->flags_) {
        case TF_LBMENUF_RADIOCHECKED:
          info.fMask |= MIIM_STATE;
          info.fState |= MFS_CHECKED;
          info.fMask |= MIIM_FTYPE;
          info.fType |= MFT_RADIOCHECK;
          break;
        case TF_LBMENUF_CHECKED:
          info.fMask |= MIIM_STATE;
          info.fState |= MFS_CHECKED;
          break;
        case TF_LBMENUF_SUBMENU:
          // TODO(yukawa): Support this.
          break;
        case TF_LBMENUF_GRAYED:
          info.fMask |= MIIM_STATE;
          info.fState |= MFS_GRAYED;
          break;
        default:
          info.fMask |= MIIM_STATE;
          info.fState |= MFS_ENABLED;
          break;
      }
    }
    menu.InsertMenuItemW(i, TRUE, &info);
  }

  // Caveats: TPM_NONOTIFY is important because the attached window may
  // change the menu state unless this flag is specified. We actually suffered
  // from this issue with Internet Explorer 10 on Windows 8. b/10217103.
  constexpr DWORD kMenuFlags = TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN |
                               TPM_TOPALIGN | TPM_LEFTBUTTON;
  const BOOL result =
      menu.TrackPopupMenu(kMenuFlags, point.x, point.y, ::GetFocus());
  if (!result) {
    return E_FAIL;
  }
  return lang_bar_callback_->OnMenuSelect(
      static_cast<TipLangBarCallback::ItemId>(result));
}

// Implements the ITfLangBarItemButton::GetText() function.
// This function is called by Windows to retrieve the text label of this
// button menu.
STDMETHODIMP TipLangBarButton::GetText(BSTR *text) {
  *text = ::SysAllocString(&item_info_.szDescription[0]);
  return (*text ? S_OK : E_OUTOFMEMORY);
}

// Implements the ITfSource::AdviseSink() function.
STDMETHODIMP TipLangBarButton::AdviseSink(REFIID interface_id,
                                          IUnknown *unknown, DWORD *cookie) {
  // Return if the caller tries to start advising any events except the
  // ITfLangBarItemSink events.
  if (!IsEqualIID(IID_ITfLangBarItemSink, interface_id))
    return CONNECT_E_CANNOTCONNECT;

  // Exit if this object has a sink which advising ITfLangBarItemSink events.
  if (item_sink_ != nullptr) return CONNECT_E_ADVISELIMIT;

  // Retrieve the ITfLangBarItemSink interface from the given object and store
  // it into |item_sink_|.
  item_sink_ = ComQuery<ITfLangBarItemSink>(unknown);
  if (item_sink_ == nullptr) {
    return E_INVALIDARG;
  }

  // Return the cookie of this object.
  *cookie = kTipLangBarMenuCookie;
  return S_OK;
}

STDMETHODIMP TipLangBarButton::UnadviseSink(DWORD cookie) {
  if (cookie != kTipLangBarMenuCookie) {
    return E_INVALIDARG;
  }
  if (item_sink_ == nullptr) {
    return CONNECT_E_NOCONNECTION;
  }

  // Release the copy of this event.
  item_sink_.reset();

  return S_OK;
}

// Initializes a TipLangBarButton instance.
// This function is called by a text service to provide the information
// required for creating a menu button. A text service MUST call this function
// before calling the ITfLangBarItemMgr::AddItem() function and adding this
// button menu to a language bar.
HRESULT TipLangBarButton::Init(HINSTANCE instance, int string_id,
                               const TipLangBarMenuItem *menu, int count) {
  HRESULT result = S_OK;
  // Retrieve the text label from the resource.
  // This string is also used as a tool-tip text.
  ::LoadString(instance, string_id, &item_info_.szDescription[0],
               std::size(item_info_.szDescription));

  // Create a new TipLangBarMenuItem object.
  menu_data_.Init(instance, menu, count);

  return S_OK;
}

HRESULT TipLangBarButton::OnUpdate(DWORD update_flag) {
  // For some reason, this method might be called when the event sink is not
  // available. See b/2977835 for details.
  if (item_sink_ == nullptr) {
    return E_FAIL;
  }
  return item_sink_->OnUpdate(update_flag);
}

HRESULT TipLangBarButton::SetEnabled(bool enabled) {
  HRESULT result = S_OK;

  if (enabled) {
    status_ &= ~TF_LBI_STATUS_DISABLED;
  } else {
    status_ |= TF_LBI_STATUS_DISABLED;
  }
  result = TipLangBarButton::OnUpdate(TF_LBI_STATUS);

  return result;
}

bool TipLangBarButton::CanContextMenuDisplay32bppIcon() {
  // We always use a non-theme icon for a context menu icon on the LangBar
  // unless the current display mode is 32-bpp.  We cannot assume we can
  // display a 32-bpp icon for a context menu icon on the LangBar unless the
  // current display mode is 32-bpp.  See http://b/2260057
  CDC display_dc(::GetDC(nullptr));
  return !display_dc.IsNull() && display_dc.GetDeviceCaps(PLANES) == 1 &&
         display_dc.GetDeviceCaps(BITSPIXEL) == 32;
}

TipLangBarMenuData *TipLangBarButton::menu_data(size_t i) {
  return menu_data_.data(i);
}

size_t TipLangBarButton::menu_data_size() const { return menu_data_.size(); }

const TF_LANGBARITEMINFO *TipLangBarButton::item_info() const {
  return &item_info_;
}

bool TipLangBarButton::IsMenuButton() const {
  return (item_info_.dwStyle & TF_LBI_STYLE_BTN_MENU) == TF_LBI_STYLE_BTN_MENU;
}

void TipLangBarButton::SetContextMenuEnabled(bool enabled) {
  context_menu_enabled_ = enabled;
}

void TipLangBarButton::SetDescription(const std::wstring &description) {
  ::StringCchCopy(item_info_.szDescription, std::size(item_info_.szDescription),
                  description.c_str());
}

TipLangBarMenuButton::TipLangBarMenuButton(TipLangBarCallback *langbar_callback,
                                           const GUID &guid, bool show_in_tray)
    : TipLangBarButton(langbar_callback, guid, true, show_in_tray),
      menu_icon_id_for_theme_(0),
      menu_icon_id_for_non_theme_(0) {}

STDMETHODIMP TipLangBarMenuButton::InitMenu(ITfMenu *menu) {
  HRESULT result = S_OK;

  // Do nothing if the element is not menu botton.
  if (!IsMenuButton()) {
    return result;
  }

  // Add the menu items of this object to the given ITfMenu object.
  for (size_t i = 0; i < menu_data_size(); ++i) {
    const TipLangBarMenuData *data = menu_data(i);

    const UINT icon_id_for_theme = CanContextMenuDisplay32bppIcon()
                                       ? data->icon_id_for_theme_
                                       : data->icon_id_for_non_theme_;
    CBitmap bitmap;
    CBitmap mask;
    // If LoadIconAsBitmap fails, |bitmap.m_hBitmap| and |mask.m_hBitmap|
    // remain nullptr bitmap handles.
    LoadIconAsBitmap(TipDllModule::module_handle(),
                     data->icon_id_for_non_theme_, icon_id_for_theme,
                     &bitmap.m_hBitmap, &mask.m_hBitmap);
    result = menu->AddMenuItem(i, data->flags_, bitmap, mask, data->text_,
                               data->length_, nullptr);
    if (result != S_OK) {
      break;
    }
  }

  return result;
}

STDMETHODIMP TipLangBarMenuButton::OnMenuSelect(UINT menu_id) {
  // Call the TipLangBarCallback::OnMenuSelect() function to dispatch the
  // given event.
  TipLangBarMenuData *data = menu_data(menu_id);
  if (data == nullptr) {
    return E_INVALIDARG;
  }
  if (data->item_id_ == TipLangBarCallback::kCancel) {
    return S_OK;
  }
  const HRESULT result = lang_bar_callback_->OnMenuSelect(
      static_cast<TipLangBarCallback::ItemId>(data->item_id_));
  return result;
}

// Implements the ITfLangBarItem::GetInfo() function.
// This function is called by Windows to update this button menu.
STDMETHODIMP TipLangBarMenuButton::GetInfo(TF_LANGBARITEMINFO *item_info) {
  HRESULT result = S_OK;

  if (item_info == nullptr) {
    return E_INVALIDARG;
  }

  // Just copies the cached TF_LANGBARITEMINFO object.
  *item_info = *this->item_info();

  CIcon icon;
  if (FAILED(GetIcon(&icon.m_hIcon)) || icon.IsNull()) {
    return S_OK;
  }

  ICONINFO icon_info = {};
  const BOOL get_icon_succeeded = icon.GetIconInfo(&icon_info);
  CBitmap color(icon_info.hbmColor);
  CBitmap mask(icon_info.hbmMask);
  if (!get_icon_succeeded) {
    return S_OK;
  }

  if (color.IsNull() && !mask.IsNull()) {
    item_info->dwStyle |= TF_LBI_STYLE_TEXTCOLORICON;
    return S_OK;
  }

  return S_OK;
}

STDMETHODIMP TipLangBarMenuButton::GetIcon(HICON *icon) {
  if (icon == nullptr) {
    return E_INVALIDARG;
  }

  //  Excerpt: http://msdn.microsoft.com/en-us/library/ms628718.aspx
  //  The caller must free this icon when it is no longer required by
  //  calling DestroyIcon.
  *icon = LoadIconFromResource(TipDllModule::module_handle(),
                               menu_icon_id_for_theme_);
  return (*icon ? S_OK : E_FAIL);
}

// Initializes an ImeButtonMenu instance.
// This function allocates resources for an ImeButtonMenu instance.
HRESULT TipLangBarMenuButton::Init(HINSTANCE instance, UINT string_id,
                                   const TipLangBarMenuItem *menu, int count,
                                   UINT menu_icon_id_for_non_theme,
                                   UINT menu_icon_id_for_theme) {
  menu_icon_id_for_theme_ = menu_icon_id_for_theme;
  menu_icon_id_for_non_theme_ = menu_icon_id_for_non_theme;
  return TipLangBarButton::Init(instance, string_id, menu, count);
}

TipLangBarToggleButton::TipLangBarToggleButton(
    TipLangBarCallback *langbar_callback, const GUID &guid, bool is_menu,
    bool show_in_tray)
    : TipLangBarButton(langbar_callback, guid, is_menu, show_in_tray),
      menu_selected_(0),
      disabled_(false) {}

// Implements the IUnknown::QueryInterface() function.
// This function is used by Windows to retrieve the interfaces implemented by
// this class.
STDMETHODIMP TipLangBarToggleButton::QueryInterface(REFIID interface_id,
                                                    void **object) {
  if (!object) {
    return E_POINTER;
  }

  // Respond to IMozcLangBarToggleItem in addition to base.
  if (IsIIDOf<IMozcLangBarToggleItem>(interface_id)) {
    *object = absl::implicit_cast<IMozcLangBarToggleItem *>(this);
    AddRef();
    return S_OK;
  }
  return TipLangBarButton::QueryInterface(interface_id, object);
}

// Implements the ITfLangBarItem::GetInfo() function.
// This function is called by Windows to update this button menu.
STDMETHODIMP TipLangBarToggleButton::GetInfo(TF_LANGBARITEMINFO *item_info) {
  HRESULT result = S_OK;

  if (item_info == nullptr) {
    return E_INVALIDARG;
  }

  // Just copies the cached TF_LANGBARITEMINFO object.
  *item_info = *this->item_info();

  if (FAILED(result)) {
    return result;
  }

  CIcon icon;
  if (FAILED(GetIcon(&icon.m_hIcon)) || icon.IsNull()) {
    return S_OK;
  }

  ICONINFO icon_info = {};
  const BOOL get_icon_succeeded = icon.GetIconInfo(&icon_info);
  CBitmap color(icon_info.hbmColor);
  CBitmap mask(icon_info.hbmMask);
  if (!get_icon_succeeded) {
    return S_OK;
  }

  if (color.IsNull() && !mask.IsNull()) {
    item_info->dwStyle |= TF_LBI_STYLE_TEXTCOLORICON;
    return S_OK;
  }

  return S_OK;
}

STDMETHODIMP TipLangBarToggleButton::InitMenu(ITfMenu *menu) {
  HRESULT result = S_OK;

  // Do nothing if the langbar item is not menu botton.
  if (!IsMenuButton()) {
    return result;
  }

  // Add the menu items of this object to the given ITfMenu object.
  for (size_t i = 0; i < menu_data_size(); ++i) {
    const TipLangBarMenuData *data = menu_data(i);
    result = menu->AddMenuItem(i, data->flags_, nullptr, nullptr, data->text_,
                               data->length_, nullptr);
    if (result != S_OK) {
      break;
    }
  }

  return result;
}

STDMETHODIMP TipLangBarToggleButton::OnMenuSelect(UINT menu_id) {
  // Call the TipLangBarCallback::OnMenuSelect() function to dispatch the
  // given event.
  TipLangBarMenuData *data = menu_data(menu_id);
  if (data == nullptr) {
    return E_INVALIDARG;
  }
  if (data->item_id_ == TipLangBarCallback::kCancel) {
    return S_OK;
  }
  const HRESULT result = lang_bar_callback_->OnMenuSelect(
      static_cast<TipLangBarCallback::ItemId>(data->item_id_));
  if (result != S_OK) {
    return result;
  }
  TipLangBarMenuData *selected = menu_data(menu_selected_);
  selected->flags_ &= ~TF_LBMENUF_RADIOCHECKED;
  data->flags_ |= TF_LBMENUF_RADIOCHECKED;
  menu_selected_ = menu_id;
  return result;
}

STDMETHODIMP TipLangBarToggleButton::GetIcon(HICON *icon) {
  if (icon == nullptr) {
    return E_INVALIDARG;
  }

  // MSIME 2012 shows special icon when the LangBar item is disabled. Here we
  // adopt this behavior for the consistency.
  // TODO(yukawa): Refactor this design. We should split TipLangBarToggleButton
  //     into two different classes anyway.
  const TipLangBarMenuData &data = !IsMenuButton() && disabled_
                                       ? menu_data_for_disabled_
                                       : *menu_data(menu_selected_);

  //  Excerpt: http://msdn.microsoft.com/en-us/library/ms628718.aspx
  //  The caller must free this icon when it is no longer required by
  //  calling DestroyIcon.
  *icon = LoadIconFromResource(TipDllModule::module_handle(),
                               data.icon_id_for_theme_);
  return (*icon ? S_OK : E_FAIL);
}

HRESULT TipLangBarToggleButton::Init(
    HINSTANCE instance, int string_id, const TipLangBarMenuItem *menu,
    int count, const TipLangBarMenuItem &menu_for_disabled) {
  TipLangBarMenuDataArray array;
  if (SUCCEEDED(array.Init(instance, &menu_for_disabled, 1))) {
    menu_data_for_disabled_ = *array.data(0);
  }
  // TODO(yuryu): Change ABSL_ARRAYSIZE to std::size when we upgrade to a  C++17
  // compiler.
  wchar_t buffer[ABSL_ARRAYSIZE(menu_data_for_disabled_.text_)];
  ::LoadString(instance, string_id, buffer, std::size(buffer));
  description_for_enabled_ = buffer;
  return TipLangBarButton::Init(instance, string_id, menu, count);
}

// Implements the IMozcToggleButtonMenu::SelectMenuItem() function.
HRESULT TipLangBarToggleButton::SelectMenuItem(UINT menu_id) {
  // Now SelectMenuItem may be called frequently to update LangbarItem for
  // every key input.  So we call TipLangBarButton::OnUpdate only if any item
  // state is updated.
  bool item_state_changed = false;
  for (size_t i = 0; i < menu_data_size(); ++i) {
    TipLangBarMenuData *data = menu_data(i);
    if (data->item_id_ == menu_id) {
      if ((data->flags_ & TF_LBMENUF_RADIOCHECKED) != 0 ||
          menu_selected_ != static_cast<UINT>(i)) {
        item_state_changed |= true;
      }
      data->flags_ |= TF_LBMENUF_RADIOCHECKED;
      menu_selected_ = static_cast<UINT>(i);
    } else {
      if (data->flags_ != 0) {
        item_state_changed |= true;
      }
      data->flags_ &= ~TF_LBMENUF_RADIOCHECKED;
    }
  }
  if (item_state_changed) {
    TipLangBarButton::OnUpdate(TF_LBI_ICON | TF_LBI_STATUS | TF_LBI_TEXT);
  }
  return S_OK;
}

HRESULT TipLangBarToggleButton::SetEnabled(bool enabled) {
  disabled_ = !enabled;

  // For menu-button, use parent implementation.
  if (IsMenuButton()) {
    return TipLangBarButton::SetEnabled(enabled);
  }

  // For button type element, adopt special behavior to be consistent with
  // MSIME 2012's behavior.
  // TODO(yukawa): Refactor this design. We should split TipLangBarToggleButton
  //     into two different classes anyway.
  SetContextMenuEnabled(enabled);
  if (enabled) {
    const HRESULT result = SelectMenuItem(menu_data(menu_selected_)->item_id_);
    SetDescription(description_for_enabled_);
    TipLangBarButton::OnUpdate(TF_LBI_ICON | TF_LBI_STATUS | TF_LBI_TEXT);
    return result;
  }
  SetDescription(menu_data_for_disabled_.text_);
  TipLangBarButton::OnUpdate(TF_LBI_ICON | TF_LBI_STATUS | TF_LBI_TEXT);
  return S_OK;
}

// Implements the constructor of the TipSystemLangBarMenu class.
TipSystemLangBarMenu::TipSystemLangBarMenu(
    wil::com_ptr_nothrow<TipLangBarCallback> lang_bar_callback,
    const GUID &guid)
    : lang_bar_callback_(std::move(lang_bar_callback)) {}

// Implements the ITfLangBarItemButton::InitMenu() function.
// This function is called by Windows to create the button menu.
STDMETHODIMP TipSystemLangBarMenu::InitMenu(ITfMenu *menu) {
  HRESULT result = S_OK;

  // Add the menu items of this object to the given ITfMenu object.
  for (size_t i = 0; i < menu_data_.size(); ++i) {
    const TipLangBarMenuData *data = menu_data_.data(i);
    const UINT icon_id_for_theme =
        TipLangBarButton::CanContextMenuDisplay32bppIcon()
            ? data->icon_id_for_theme_
            : data->icon_id_for_non_theme_;
    CBitmap bitmap;
    CBitmap mask;
    // If LoadIconAsBitmap fails, |bitmap.m_hBitmap| and |mask.m_hBitmap|
    // remain nullptr bitmap handles.
    LoadIconAsBitmap(TipDllModule::module_handle(),
                     data->icon_id_for_non_theme_, icon_id_for_theme,
                     &bitmap.m_hBitmap, &mask.m_hBitmap);
    result = menu->AddMenuItem(i, data->flags_, bitmap, mask, data->text_,
                               data->length_, nullptr);
    if (result != S_OK) {
      break;
    }
  }

  return result;
}

// Implements the ITfLangBarItemButton::OnMenuSelect() function.
// This function is called by Windows to notify a user selects an item in
// this button menu.
STDMETHODIMP TipSystemLangBarMenu::OnMenuSelect(UINT menu_id) {
  // Call the TipLangBarCallback::OnMenuSelect() function to dispatch the
  // given event.
  TipLangBarMenuData *data = menu_data_.data(menu_id);
  if (data == nullptr) {
    return E_INVALIDARG;
  }
  if (data->item_id_ == TipLangBarCallback::kCancel) {
    return S_OK;
  }
  const HRESULT result = lang_bar_callback_->OnMenuSelect(
      static_cast<TipLangBarCallback::ItemId>(data->item_id_));
  return S_OK;
}

// Initializes a TipLangBarButton instance.
// This function is called by a text service to provide the information
// required for creating a menu button. A text service MUST call this function
// before calling the ITfLangBarItemMgr::AddItem() function and adding this
// button menu to a language bar.
HRESULT TipSystemLangBarMenu::Init(HINSTANCE instance,
                                   const TipLangBarMenuItem *menu, int count) {
  return menu_data_.Init(instance, menu, count);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

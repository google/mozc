// Copyright 2010-2014, Google Inc.
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

#include "gui/base/win_util.h"

#ifdef OS_WIN
#define NTDDI_VERSION NTDDI_WIN7  // for JumpList.
#include <dwmapi.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <windows.h>
#include <winuser.h>
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>
#include <atlstr.h>
#include <atlwin.h>

#include <knownfolders.h>
#include <objectarray.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif  // OS_WIN

#include <QtCore/QFile>
#include <QtCore/QLibrary>
#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QPainter>
#include <QtGui/QPaintEngine>
#include <QtGui/QWidget>

#ifdef OS_WIN
#include "base/const.h"
#endif  // OS_WIN
#include "base/logging.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/win_util.h"

#ifdef OS_WIN
#ifndef WM_DWMCOMPOSITIONCHANGED
#define WM_DWMCOMPOSITIONCHANGED        0x031E
#endif  // WM_DWMCOMPOSITIONCHANGED

// DWM API
typedef HRESULT (WINAPI *FPDwmIsCompositionEnabled)
    (BOOL *pfEnabled);
typedef HRESULT (WINAPI *FPDwmExtendFrameIntoClientArea)
    (HWND hWnd,
     const MARGINS *pMarInset);
typedef HRESULT (WINAPI *FPDwmEnableBlurBehindWindow)
    (HWND hWnd,
     const DWM_BLURBEHIND *pBlurBehind);

// Theme API
typedef HANDLE (WINAPI *FPOpenThemeData)
    (HWND hwnd, LPCWSTR pszClassList);
typedef HRESULT (WINAPI *FPCloseThemeData)
    (HANDLE hTheme);
typedef HRESULT (WINAPI *FPDrawThemeTextEx)
    (HANDLE hTheme, HDC hdc, int iPartId, int iStateId,
     LPCWSTR pszText, int cchText, DWORD dwTextFlags,
     LPRECT pRect, const DTTOPTS *pOptions);
typedef HRESULT (WINAPI *FPGetThemeSysFont)
    (HANDLE hTheme, int iFontId, LOGFONTW *plf);

#endif  // OS_WIN

namespace mozc {
namespace gui {

#ifdef OS_WIN
namespace {

FPDwmIsCompositionEnabled      gDwmIsCompositionEnabled      = NULL;
FPDwmEnableBlurBehindWindow    gDwmEnableBlurBehindWindow    = NULL;
FPDwmExtendFrameIntoClientArea gDwmExtendFrameIntoClientArea = NULL;
FPOpenThemeData                gOpenThemeData                = NULL;
FPCloseThemeData               gCloseThemeData               = NULL;
FPDrawThemeTextEx              gDrawThemeTextEx              = NULL;
FPGetThemeSysFont              gGetThemeSysFont              = NULL;

class WindowNotifier : public QWidget {
 public:
  WindowNotifier() {
    winId();   // to make a window handle
  }

  virtual ~WindowNotifier() {}

  void AddWidget(QWidget *widget) {
    widgets_.append(widget);
  }

  void RemoveWidget(QWidget *widget) {
    widgets_.removeAll(widget);
  }

  bool winEvent(MSG *message, long *result);

  void InstallStyleSheets(const QString &dwm_on_style,
                          const QString &dwm_off_style) {
    dwm_on_style_ = dwm_on_style;
    dwm_off_style_ = dwm_off_style;
  }

  static WindowNotifier *Get() {
    return Singleton<WindowNotifier>::get();
  }

 private:
  QWidgetList widgets_;
  QString dwm_on_style_;
  QString dwm_off_style_;
};

class DwmResolver {
 public:
  DwmResolver() : dwmlib_(NULL), themelib_(NULL) {
    dwmlib_ = mozc::WinUtil::LoadSystemLibrary(L"dwmapi.dll");
    themelib_ = mozc::WinUtil::LoadSystemLibrary(L"uxtheme.dll");

    if (NULL != dwmlib_) {
      gDwmIsCompositionEnabled =
          reinterpret_cast<FPDwmIsCompositionEnabled>
          (::GetProcAddress(dwmlib_,
                            "DwmIsCompositionEnabled"));
      gDwmExtendFrameIntoClientArea =
          reinterpret_cast<FPDwmExtendFrameIntoClientArea>
          (::GetProcAddress(dwmlib_,
                            "DwmExtendFrameIntoClientArea"));
      gDwmEnableBlurBehindWindow =
          reinterpret_cast<FPDwmEnableBlurBehindWindow>
          (::GetProcAddress(dwmlib_,
                            "DwmEnableBlurBehindWindow"));
    }

    if (NULL != themelib_) {
      gOpenThemeData =
          reinterpret_cast<FPOpenThemeData>(
              ::GetProcAddress(themelib_, "OpenThemeData"));
      gCloseThemeData =
          reinterpret_cast<FPCloseThemeData>(
              ::GetProcAddress(themelib_, "CloseThemeData"));
      gDrawThemeTextEx =
          reinterpret_cast<FPDrawThemeTextEx>(
              ::GetProcAddress(themelib_, "DrawThemeTextEx"));
      gGetThemeSysFont =
          reinterpret_cast<FPGetThemeSysFont>(
              ::GetProcAddress(themelib_, "GetThemeSysFont"));
    }
  }

  bool IsAvailable() const {
    return (gDwmIsCompositionEnabled != NULL &&
            gDwmExtendFrameIntoClientArea != NULL &&
            gDwmEnableBlurBehindWindow != NULL &&
            gOpenThemeData != NULL &&
            gCloseThemeData != NULL &&
            gDrawThemeTextEx != NULL &&
            gGetThemeSysFont != NULL);
  }

  static bool ResolveLibs() {
    return Singleton<DwmResolver>::get()->IsAvailable();
  }

 private:
  HMODULE dwmlib_;
  HMODULE themelib_;
};

CComPtr<IShellLink> InitializeShellLinkItem(const char *argument,
                                            const char *item_title) {
  HRESULT hr = S_OK;
  CComPtr<IShellLink> link;
  hr = link.CoCreateInstance(CLSID_ShellLink);
  if (FAILED(hr)) {
    DLOG(INFO) << "Failed to instanciate CLSID_ShellLink. hr = " << hr;
    return NULL;
  }

  {
    wstring mozc_tool_path_wide;
    Util::UTF8ToWide(SystemUtil::GetToolPath(), &mozc_tool_path_wide);
    hr = link->SetPath(mozc_tool_path_wide.c_str());
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetPath failed. hr = " << hr;
      return NULL;
    }
  }

  {
    wstring argument_wide;
    Util::UTF8ToWide(argument, &argument_wide);
    hr = link->SetArguments(argument_wide.c_str());
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetArguments failed. hr = " << hr;
      return NULL;
    }
  }

  CComQIPtr<IPropertyStore> property_store(link);
  if (property_store == NULL) {
    DLOG(ERROR) << "QueryInterface failed.";
    return NULL;
  }

  {
    wstring item_title_wide;
    Util::UTF8ToWide(item_title, &item_title_wide);
    PROPVARIANT prop_variant;
    hr = ::InitPropVariantFromString(item_title_wide.c_str(), &prop_variant);
    if (FAILED(hr)) {
      DLOG(ERROR) << "QueryInterface failed. hr = " << hr;
      return NULL;
    }
    hr = property_store->SetValue(PKEY_Title, prop_variant);
    ::PropVariantClear(&prop_variant);
  }

  if (FAILED(hr)) {
    DLOG(ERROR) << "SetValue failed. hr = " << hr;
    return NULL;
  }

  hr = property_store->Commit();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Commit failed. hr = " << hr;
    return NULL;
  }

  return link;
}

struct LinkInfo {
  const char *argument;
  const char *title_english;
  const char *title_japanese;
};

bool AddTasksToList(CComPtr<ICustomDestinationList> destination_list) {
  HRESULT hr = S_OK;
  CComPtr<IObjectCollection> object_collection;

  hr = object_collection.CoCreateInstance(CLSID_EnumerableObjectCollection);
  if (FAILED(hr)) {
    DLOG(INFO) << "Failed to instanciate CLSID_EnumerableObjectCollection."
                  " hr = " << hr;
    return false;
  }

  // TODO(yukawa): Investigate better way to localize strings.
  const LinkInfo kLinks[] = {
    // "手書き文字入力"
    {"--mode=hand_writing",
     "Hand Wrinting",
     "\xE6\x89\x8B\xE6\x9B\xB8\xE3\x81\x8D\xE6\x96\x87\xE5\xAD\x97"
     "\xE5\x85\xA5\xE5\x8A\x9B"},
    // "文字パレット"
    {"--mode=character_palette",
     "Character Palette",
     "\xE6\x96\x87\xE5\xAD\x97\xE3\x83\x91\xE3\x83\xAC\xE3\x83\x83"
     "\xE3\x83\x88"},
    // "辞書ツール"
    {"--mode=dictionary_tool",
     "Dictionary Tool",
     "\xE8\xBE\x9E\xE6\x9B\xB8\xE3\x83\x84\xE3\x83\xBC\xE3\x83\xAB"},
    // "単語登録"
    {"--mode=word_register_dialog",
     "Add Word",
     "\xE5\x8D\x98\xE8\xAA\x9E\xE7\x99\xBB\xE9\x8C\xB2"},
    // "プロパティ"
    {"--mode=config_dialog",
     "Properties",
     "\xE3\x83\x97\xE3\x83\xAD\xE3\x83\x91\xE3\x83\x86\xE3\x82\xA3"},
  };

  const LANGID kJapaneseLangId = MAKELANGID(LANG_JAPANESE,
                                            SUBLANG_JAPANESE_JAPAN);
  const bool use_japanese_ui =
      (kJapaneseLangId == ::GetUserDefaultUILanguage());

  for (size_t i = 0; i < arraysize(kLinks); ++i) {
    CComPtr<IShellLink> link;
    if (use_japanese_ui) {
      link = InitializeShellLinkItem(kLinks[i].argument,
                                     kLinks[i].title_japanese);
    } else {
      link = InitializeShellLinkItem(kLinks[i].argument,
                                     kLinks[i].title_english);
    }
    if (link != NULL) {
      object_collection->AddObject(link);
    }
  }

  CComQIPtr<IObjectArray> object_array(object_collection);
  if (object_array == NULL) {
    DLOG(ERROR) << "QueryInterface failed.";
    return false;
  }

  hr = destination_list->AddUserTasks(object_array);
  if (FAILED(hr)) {
    DLOG(ERROR) << "AddUserTasks failed. hr = " << hr;
    return false;
  }

  return true;
}

void InitializeJumpList() {
  HRESULT hr = S_OK;

  CComPtr<ICustomDestinationList> destination_list;
  hr = destination_list.CoCreateInstance(CLSID_DestinationList);
  if (FAILED(hr)) {
    DLOG(INFO) << "Failed to instanciate CLSID_DestinationList. hr = " << hr;
    return;
  }

  UINT min_slots = 0;
  CComPtr<IObjectArray> removed_objects;
  hr = destination_list->BeginList(&min_slots, IID_IObjectArray,
                                   reinterpret_cast<void **>(&removed_objects));
  if (FAILED(hr)) {
    DLOG(INFO) << "BeginList failed. hr = " << hr;
    return;
  }

  if (!AddTasksToList(destination_list)) {
    return;
  }

  hr = destination_list->CommitList();
  if (FAILED(hr)) {
    DLOG(INFO) << "Commit failed. hr = " << hr;
    return;
  }
}
}  // namespace
#endif  // OS_WIN

bool WinUtil::IsCompositionEnabled() {
#ifdef OS_WIN
  if (!DwmResolver::ResolveLibs()) {
    return false;
  }

  HRESULT hr = S_OK;
  BOOL is_enabled = false;
  hr = gDwmIsCompositionEnabled(&is_enabled);
  if (SUCCEEDED(hr)) {
    return is_enabled;
  } else {
    LOG(ERROR) << "DwmIsCompositionEnabled() failed: "
               << static_cast<long>(hr);
  }
#endif  // OS_WIN

  return false;
}

bool WinUtil::ExtendFrameIntoClientArea(QWidget *widget,
                                        int left, int top,
                                        int right, int bottom) {
  DCHECK(widget);

#ifdef OS_WIN
  if (!DwmResolver::ResolveLibs()) {
    return false;
  }

  HRESULT hr = S_OK;
  MARGINS margin = { left, top, right, bottom };
  hr = gDwmExtendFrameIntoClientArea(widget->winId(), &margin);
  if (SUCCEEDED(hr)) {
    WindowNotifier::Get()->AddWidget(widget);
    widget->setAttribute(Qt::WA_TranslucentBackground, true);
    return true;
  } else {
    LOG(ERROR) << "DwmExtendFrameIntoClientArea() failed: "
               << static_cast<long>(hr);
  }
#endif  // OS_WIN

  return false;
}

#ifdef OS_WIN
bool WindowNotifier::winEvent(MSG *message, long *result) {
  if (message != NULL && message->message == WM_DWMCOMPOSITIONCHANGED) {
    const bool composition_enabled = WinUtil::IsCompositionEnabled();

    // switch styles if need be
    if (!dwm_on_style_.isEmpty() && !dwm_off_style_.isEmpty()) {
      if (composition_enabled) {
        qApp->setStyleSheet(dwm_on_style_);
      } else {
        qApp->setStyleSheet(dwm_off_style_);
      }
    }

    foreach(QWidget *widget, widgets_) {
      if (widget != NULL) {
        widget->setAttribute(Qt::WA_NoSystemBackground,
                             composition_enabled);
        // TODO(taku): left/top/right/bottom are not updated.
        // need to be fixed.
        if (composition_enabled) {
          MARGINS margin = { -1, 0, 0, 0 };
          gDwmExtendFrameIntoClientArea(widget->winId(), &margin);
          widget->setAttribute(Qt::WA_TranslucentBackground, true);
        }
        widget->update();
      }
    }
  }

  // call default
  return QWidget::winEvent(message, result);
}
#endif  // OS_WIN

QRect WinUtil::GetTextRect(QWidget *widget, const QString &text) {
  DCHECK(widget);
  const QFont font = QApplication::font(widget);
  const QFontMetrics fontMetrics(font);
  return fontMetrics.boundingRect(text);
}

void WinUtil::InstallStyleSheets(const QString &dwm_on_style,
                                 const QString &dwm_off_style) {
#ifdef OS_WIN
  WindowNotifier::Get()->InstallStyleSheets(dwm_on_style,
                                            dwm_off_style);
#endif  // OS_WIN
}

void WinUtil::InstallStyleSheetsFiles(const QString &dwm_on_style_file,
                                      const QString &dwm_off_style_file) {
#ifdef OS_WIN
  QFile file1(dwm_on_style_file);
  file1.open(QFile::ReadOnly);
  QFile file2(dwm_off_style_file);
  file2.open(QFile::ReadOnly);
  WinUtil::InstallStyleSheets(QLatin1String(file1.readAll()),
                              QLatin1String(file2.readAll()));
#endif  // OS_WIN
}

void WinUtil::DrawThemeText(const QString &text,
                            const QRect &rect,
                            int glow_size,
                            QPainter *painter) {
  DCHECK(painter);

#ifdef OS_WIN
  if (!DwmResolver::ResolveLibs()) {
    return;
  }

  HDC hdc = painter->paintEngine()->getDC();
  if (NULL == hdc) {
    LOG(ERROR) << "hdc is NULL";
    return;
  }

  HANDLE theme = gOpenThemeData(qApp->desktop()->winId(), L"WINDOW");
  if (theme == NULL) {
    LOG(ERROR) << "::OpenThemaData() failed";
    return;
  }

  // Set up a memory DC and bitmap that we'll draw into
  HDC dc_mem = ::CreateCompatibleDC(hdc);
  if (dc_mem == NULL) {
    gCloseThemeData(theme);
    LOG(ERROR) << "::CreateCompatibleDC() failed: " << ::GetLastError();
    return;
  }

  BITMAPINFO dib = { 0 };
  dib.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  dib.bmiHeader.biWidth = rect.width();
  dib.bmiHeader.biHeight = -rect.height();
  dib.bmiHeader.biPlanes = 1;
  dib.bmiHeader.biBitCount = 32;
  dib.bmiHeader.biCompression = BI_RGB;

  HBITMAP bmp = ::CreateDIBSection(hdc, &dib,
                                   DIB_RGB_COLORS, NULL, NULL, 0);
  if (NULL == bmp) {
    ::DeleteDC(dc_mem);
    gCloseThemeData(theme);
    LOG(ERROR) << "::CreateDIBSection() failed: " << ::GetLastError();
    return;
  }

  LOGFONT lf = { 0 };
  if (NULL != theme) {
    gGetThemeSysFont(theme, TMT_CAPTIONFONT, &lf);
  } else {
    NONCLIENTMETRICS ncm = { sizeof(NONCLIENTMETRICS) };
    ::SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
                           sizeof(NONCLIENTMETRICS), &ncm, false);
    lf = ncm.lfMessageFont;
  }

  HFONT caption_font = ::CreateFontIndirect(&lf);
  HBITMAP old_bmp = reinterpret_cast<HBITMAP>(
      ::SelectObject(dc_mem,
                     reinterpret_cast<HGDIOBJ>(bmp)));
  HFONT old_font = reinterpret_cast<HFONT>(
      ::SelectObject(dc_mem,
                     reinterpret_cast<HGDIOBJ>(caption_font)));

  DTTOPTS dto = { sizeof(DTTOPTS) };
  const UINT format = DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX;
  RECT rctext = { 0, 0, rect.width(), rect.height() };

  dto.dwFlags = DTT_COMPOSITED | DTT_GLOWSIZE;
  dto.iGlowSize = glow_size;

  const HRESULT hr =
      gDrawThemeTextEx(theme, dc_mem, 0, 0,
                       reinterpret_cast<LPCWSTR>(text.utf16()),
                       -1, format, &rctext, &dto);
  if (SUCCEEDED(hr)) {
    // Copy to the painter's HDC
    if (!::BitBlt(hdc, rect.left(), rect.top(), rect.width(), rect.height(),
                  dc_mem, 0, 0, SRCCOPY)) {
      LOG(ERROR) << "::BitBlt() failed: " << ::GetLastError();
    }
  } else {
    LOG(ERROR) << "::DrawThemeTextEx() failed: " << static_cast<long>(hr);
  }

  ::SelectObject(dc_mem, reinterpret_cast<HGDIOBJ>(old_bmp));
  ::SelectObject(dc_mem, reinterpret_cast<HGDIOBJ>(old_font));
  ::DeleteObject(bmp);
  ::DeleteObject(caption_font);
  ::DeleteDC(dc_mem);
  gCloseThemeData(theme);

#endif  // OS_WIN
}

#ifdef OS_WIN
namespace {

struct FindVisibleWindowInfo {
  HWND found_window_handle;
  DWORD target_process_id;
};

BOOL CALLBACK FindVisibleWindowProc(HWND hwnd, LPARAM lp) {
  DWORD process_id = 0;
  ::GetWindowThreadProcessId(hwnd, &process_id);
  FindVisibleWindowInfo *info = reinterpret_cast<FindVisibleWindowInfo *>(lp);
  if (process_id != info->target_process_id) {
    // continue enum
    return TRUE;
  }
  if (::IsWindowVisible(hwnd) == FALSE) {
    // continue enum
    return TRUE;
  }
  info->found_window_handle = hwnd;
  return FALSE;
}

}  // namespace
#endif  // OS_WIN

void WinUtil::ActivateWindow(uint32 process_id) {
#ifdef OS_WIN
  FindVisibleWindowInfo info = {};
  info.target_process_id = process_id;

  // The target process may contain several top-level windows.
  // We do not care about the invisible windows.
  if (::EnumWindows(FindVisibleWindowProc,
                    reinterpret_cast<LPARAM>(&info)) != 0) {
    LOG(ERROR) << "Could not find the exsisting window.";
  }
  const CWindow window(info.found_window_handle);
  wstring window_title_wide;
  {
    CString buf;
    window.GetWindowTextW(buf);
    window_title_wide.assign(buf.GetString(), buf.GetLength());
  }
  string window_title_utf8;
  Util::WideToUTF8(window_title_wide, &window_title_utf8);
  LOG(INFO) << "A visible window found. hwnd: " << window.m_hWnd
            << ", title: " << window_title_utf8;

  // SetForegroundWindow API does not automatically restore the minimized
  // window. Use explicitly OpenIcon API in this case.
  if (window.IsIconic()) {
    if (::OpenIcon(window.m_hWnd) == FALSE) {
      LOG(ERROR) << "::OpenIcon() failed.";
    }
  }

  // SetForegroundWindow API works wll iff the caller process satisfies the
  // condition described in the following document.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms633539.aspx
  // Never use AttachThreadInput API to work around this restriction.
  // http://blogs.msdn.com/b/oldnewthing/archive/2008/08/01/8795860.aspx
  if (::SetForegroundWindow(window.m_hWnd) == FALSE) {
    LOG(ERROR) << "::SetForegroundWindow() failed.";
  }
#endif  // OS_WIN
}

#ifdef OS_WIN
namespace {
const wchar_t kIMEHotKeyEntryKey[]   = L"Keyboard Layout\\Toggle";
const wchar_t kIMEHotKeyEntryValue[] = L"Layout Hotkey";
const wchar_t kIMEHotKeyEntryData[]  = L"3";
}
#endif  // OS_WIN

// static
bool WinUtil::GetIMEHotKeyDisabled() {
#ifdef OS_WIN
  CRegKey key;
  LONG result = key.Open(HKEY_CURRENT_USER, kIMEHotKeyEntryKey, KEY_READ);

  // When the key doesn't exist, can return |false| as well.
  if (ERROR_SUCCESS != result) {
    return false;
  }

  wchar_t data[4] = {};
  ULONG num_chars = arraysize(data);
  result = key.QueryStringValue(kIMEHotKeyEntryValue, data, &num_chars);
  // Returned |num_char| includes NULL character.

  // This is only the condition when this function
  // can return |true|
  if (ERROR_SUCCESS == result &&
      num_chars < arraysize(data) &&
      wstring(data) == kIMEHotKeyEntryData) {
    return true;
  }

  return false;
#else   // OS_WIN
  return false;
#endif  // OS_WIN
}

// static
bool WinUtil::SetIMEHotKeyDisabled(bool disabled) {
#ifdef OS_WIN
  if (WinUtil::GetIMEHotKeyDisabled() == disabled) {
    // Do not need to update this entry.
    return true;
  }

  if (disabled) {
    CRegKey key;
    LONG result = key.Create(HKEY_CURRENT_USER, kIMEHotKeyEntryKey);
    if (ERROR_SUCCESS != result) {
      return false;
    }

    // set "3"
    result = key.SetStringValue(kIMEHotKeyEntryValue, kIMEHotKeyEntryData);

    return ERROR_SUCCESS == result;
  } else {
    CRegKey key;
    LONG result = key.Open(HKEY_CURRENT_USER, kIMEHotKeyEntryKey,
                           KEY_SET_VALUE | DELETE);
    if (result == ERROR_FILE_NOT_FOUND) {
      return true;  // default value will be used.
    }

    if (ERROR_SUCCESS != result) {
      return false;
    }

    result = key.DeleteValue(kIMEHotKeyEntryValue);

    return (ERROR_SUCCESS == result || ERROR_FILE_NOT_FOUND == result);
  }
#endif  // OS_WIN

  return false;
}

void WinUtil::KeepJumpListUpToDate() {
#ifdef OS_WIN
  HRESULT hr = S_OK;

  hr = ::CoInitializeEx(NULL,
                        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) {
    DLOG(INFO) << "CoInitializeEx failed. hr = " << hr;
    return;
  }
  InitializeJumpList();
  ::CoUninitialize();
#endif  // OS_WIN
}
}  // namespace gui
}  // namespace mozc

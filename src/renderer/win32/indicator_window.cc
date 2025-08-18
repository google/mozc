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

#include "renderer/win32/indicator_window.h"

#include <atlbase.h>
#include <atltypes.h>
#include <atlwin.h>
#include <wil/resource.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "absl/log/check.h"
#include "base/const.h"
#include "base/win32/wide_char.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/win32_font_util.h"
#include "renderer/win32/win32_image_util.h"
#include "renderer/win32/win32_renderer_util.h"

namespace mozc {
namespace renderer {
namespace win32 {

namespace {

using ATL::CWindow;
using ATL::CWindowImpl;
using ATL::CWinTraits;

using ::mozc::commands::Status;
typedef ::mozc::commands::RendererCommand::ApplicationInfo ApplicationInfo;

// 96 DPI is the default DPI in Windows.
constexpr int kDefaultDPI = 96;

// As Discussed in b/2317702, UI windows are disabled by default because it is
// hard for a user to find out what caused the problem than finding that the
// operations seems to be disabled on the UI window when
// SPI_GETACTIVEWINDOWTRACKING is enabled.
// TODO(yukawa): Support mouse operations before we add a GUI feature which
//     requires UI interaction by mouse and/or touch. (b/2954874)
typedef CWinTraits<WS_POPUP | WS_DISABLED, WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                                               WS_EX_TOPMOST | WS_EX_NOACTIVATE>
    IndicatorWindowTraits;

struct Sprite {
  wil::unique_hbitmap bitmap;
  CPoint offset;
};

// Timer event IDs
const UINT_PTR kTimerEventFadeStart = 0;
const UINT_PTR kTimerEventFading = 1;

constexpr DWORD kStartFadingOutDelay = 2500;  // msec
constexpr DWORD kFadingOutInterval = 16;      // msec
constexpr int kFadingOutAlphaDelta = 32;

double GetDPIScaling() {
  wil::unique_hdc desktop_dc(::GetDC(nullptr));
  const int dpi_x = ::GetDeviceCaps(desktop_dc.get(), LOGPIXELSX);
  return static_cast<double>(dpi_x) / kDefaultDPI;
}

}  // namespace

class IndicatorWindow::WindowImpl
    : public CWindowImpl<IndicatorWindow::WindowImpl, CWindow,
                         IndicatorWindowTraits> {
 public:
  DECLARE_WND_CLASS_EX(kIndicatorWindowClassName, 0, COLOR_WINDOW);
  WindowImpl() : alpha_(255), dpi_scaling_(GetDPIScaling()) {
    sprites_.resize(commands::NUM_OF_COMPOSITIONS);
  }
  WindowImpl(const WindowImpl&) = delete;
  WindowImpl& operator=(const WindowImpl&) = delete;

  BEGIN_MSG_MAP(WindowImpl)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_TIMER, OnTimer)
  MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
  END_MSG_MAP()

  void OnUpdate(const commands::RendererCommand& command,
                LayoutManager* layout_manager) {
    KillTimer(kTimerEventFading);
    KillTimer(kTimerEventFadeStart);

    bool visible = false;
    IndicatorWindowLayout indicator_layout;
    if (command.has_visible() && command.visible() &&
        command.has_application_info() &&
        command.application_info().has_indicator_info() &&
        command.application_info().indicator_info().has_status()) {
      const ApplicationInfo& app_info = command.application_info();
      visible =
          layout_manager->LayoutIndicatorWindow(app_info, &indicator_layout);
    }
    if (!visible) {
      HideIndicator();
      return;
    }
    DCHECK(command.has_application_info());
    DCHECK(command.application_info().has_indicator_info());
    DCHECK(command.application_info().indicator_info().has_status());
    const Status& status = command.application_info().indicator_info().status();

    alpha_ = 255;
    current_image_ = sprites_[commands::DIRECT].bitmap.get();
    CPoint offset = sprites_[commands::DIRECT].offset;
    if (!status.has_activated() || !status.has_mode() || !status.activated()) {
      current_image_ = sprites_[commands::DIRECT].bitmap.get();
      offset = sprites_[commands::DIRECT].offset;
    } else {
      const int mode = status.mode();
      switch (mode) {
        case commands::HIRAGANA:
        case commands::FULL_KATAKANA:
        case commands::HALF_ASCII:
        case commands::FULL_ASCII:
        case commands::HALF_KATAKANA:
          current_image_ = sprites_[mode].bitmap.get();
          offset = sprites_[mode].offset;
          break;
      }
    }
    if (current_image_ == nullptr) {
      HideIndicator();
      return;
    }
    top_left_ = CPoint(indicator_layout.window_rect.left - offset.x,
                       indicator_layout.window_rect.bottom - offset.y);
    UpdateWindow();

    // Start fading out.
    SetTimer(kTimerEventFadeStart, kStartFadingOutDelay);
  }

  void HideIndicator() {
    KillTimer(kTimerEventFading);
    KillTimer(kTimerEventFadeStart);
    ShowWindow(SW_HIDE);
  }

 private:
  void UpdateWindow() {
    BITMAP bm = {};
    ::GetObject(current_image_, sizeof(bm), &bm);
    CSize size(bm.bmWidth, bm.bmHeight);

    wil::unique_hdc dc(::CreateCompatibleDC(nullptr));

    // Fading out animation.
    CPoint top_left = top_left_;
    top_left.y += (255 - alpha_) / 32;

    CPoint src_left_top(0, 0);
    BLENDFUNCTION func = {AC_SRC_OVER, 0, alpha_, AC_SRC_ALPHA};

    {
      wil::unique_select_object old_bitmap =
          wil::SelectObject(dc.get(), current_image_);
      ::UpdateLayeredWindow(m_hWnd, nullptr, &top_left, &size, dc.get(),
                            &src_left_top, 0, &func, ULW_ALPHA);
    }
    ShowWindow(SW_SHOWNA);
  }

  LRESULT OnCreate(LPCREATESTRUCT create_struct) {
    EnableOrDisableWindowForWorkaround();
    constexpr int kModes[] = {
        commands::DIRECT,     commands::HIRAGANA,   commands::FULL_KATAKANA,
        commands::HALF_ASCII, commands::FULL_ASCII, commands::HALF_KATAKANA,
    };
    for (size_t i = 0; i < std::size(kModes); ++i) {
      LoadSprite(kModes[i]);
    }
    return 1;
  }

  void OnTimer(UINT_PTR event_id) {
    switch (event_id) {
      case kTimerEventFadeStart:
        KillTimer(kTimerEventFadeStart);
        SetTimer(kTimerEventFading, kFadingOutInterval);
        break;
      case kTimerEventFading:
        alpha_ = std::max<int>(alpha_ - kFadingOutAlphaDelta, 0);
        if (alpha_ == 0) {
          KillTimer(kTimerEventFading);
        }
        UpdateWindow();
        break;
    }
  }

  void OnSettingChange(UINT flags, LPCTSTR /*lpszSection*/) {
    switch (flags) {
      case SPI_SETACTIVEWINDOWTRACKING:
        EnableOrDisableWindowForWorkaround();
        [[fallthrough]];
      default:
        // We ignore other changes.
        break;
    }
  }

  void EnableOrDisableWindowForWorkaround() {
    // Disable the window if SPI_GETACTIVEWINDOWTRACKING is enabled.
    // See b/2317702 for details.
    // TODO(yukawa): Support mouse operations before we add a GUI feature which
    //   requires UI interaction by mouse and/or touch. (b/2954874)
    BOOL is_tracking_enabled = FALSE;
    if (::SystemParametersInfo(SPI_GETACTIVEWINDOWTRACKING, 0,
                               &is_tracking_enabled, 0)) {
      EnableWindow(!is_tracking_enabled);
    }
  }

  void LoadSprite(int mode) {
    BalloonImage::BalloonImageInfo info;
    LOGFONT logfont = GetMessageBoxLogFont();
    info.label_font = mozc::win32::WideToUtf8(logfont.lfFaceName);

    info.frame_color = RGBColor(1, 122, 204);
    info.blur_color = RGBColor(1, 122, 204);
    info.rect_width = ceil(dpi_scaling_ * 45.0);   // snap to pixel alignment
    info.rect_height = ceil(dpi_scaling_ * 45.0);  // snap to pixel alignment
    info.corner_radius = dpi_scaling_ * 0.0;
    info.tail_height = dpi_scaling_ * 5.0;
    info.tail_width = dpi_scaling_ * 10.0;
    info.blur_sigma = dpi_scaling_ * 3.0;
    info.blur_alpha = 0.5;
    info.frame_thickness = dpi_scaling_ * 1.0;
    info.label_size = 13.0;  // no need to be scaled.
    info.label_color = RGBColor(0, 0, 0);
    info.blur_offset_x = 0;
    info.blur_offset_y = 0;

    switch (mode) {
      case commands::DIRECT:
        info.blur_sigma = dpi_scaling_ * 0.0;
        info.frame_color = RGBColor(186, 186, 186);
        info.label_color = RGBColor(0, 0, 0);
        info.blur_sigma = dpi_scaling_ * 0.0;
        info.frame_thickness = dpi_scaling_ * 1.0;
        info.corner_radius = dpi_scaling_ * 0.0;
        info.blur_offset_x = 0;
        info.blur_offset_y = 0;
        info.label = "A";
        break;
      case commands::HIRAGANA:
        info.label = "あ";
        break;
      case commands::FULL_KATAKANA:
        info.label = "ア";
        break;
      case commands::HALF_ASCII:
        info.label = "_A";
        break;
      case commands::FULL_ASCII:
        info.label = "Ａ";
        break;
      case commands::HALF_KATAKANA:
        info.label = "_ｱ";
        break;
    }
    if (!info.label.empty()) {
      sprites_[mode].bitmap.reset(
          BalloonImage::Create(info, &sprites_[mode].offset));
    }
  }

  inline LRESULT OnCreate(UINT msg_id, WPARAM wparam, LPARAM lparam,
                          BOOL& handled) {
    return static_cast<LRESULT>(
        OnCreate(reinterpret_cast<LPCREATESTRUCT>(lparam)));
  }
  inline LRESULT OnTimer(UINT msg_id, WPARAM wparam, LPARAM lparam,
                         BOOL& handled) {
    OnTimer(static_cast<UINT_PTR>(wparam));
    return 0;
  }
  inline LRESULT OnSettingChange(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                 BOOL& handled) {
    OnSettingChange(static_cast<UINT>(wparam),
                    reinterpret_cast<LPCTSTR>(lparam));
    return 0;
  }

  HBITMAP current_image_;
  CPoint top_left_;
  BYTE alpha_;
  double dpi_scaling_;
  std::vector<Sprite> sprites_;
};

IndicatorWindow::IndicatorWindow() : impl_(new WindowImpl) {}

IndicatorWindow::~IndicatorWindow() { impl_->DestroyWindow(); }

void IndicatorWindow::Initialize() {
  impl_->Create(nullptr);
  impl_->ShowWindow(SW_HIDE);
}

void IndicatorWindow::Destroy() { impl_->DestroyWindow(); }

void IndicatorWindow::OnUpdate(const commands::RendererCommand& command,
                               LayoutManager* layout_manager) {
  impl_->OnUpdate(command, layout_manager);
}

void IndicatorWindow::Hide() { impl_->HideIndicator(); }

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

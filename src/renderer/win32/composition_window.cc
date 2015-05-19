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

#include "renderer/win32/composition_window.h"

#include <windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlgdi.h>
#include <atlmisc.h>
#include <atlwin.h>

#include <string>
#include <vector>

#include "base/const.h"
#include "base/coordinates.h"
#include "base/logging.h"
#include "base/util.h"
#include "renderer/renderer_command.pb.h"
#include "renderer/win32/win32_renderer_util.h"

namespace mozc {
namespace renderer {
namespace win32 {
using WTL::CBitmap;
using WTL::CDC;
using WTL::CDCHandle;
using WTL::CFont;
using WTL::CFontHandle;
using WTL::CLogFont;
using WTL::CMemoryDC;
using WTL::CPaintDC;
using WTL::CPenHandle;
using WTL::CPoint;
using WTL::CRect;
using WTL::CSize;
using WTL::CPen;
using WTL::CPenHandle;

namespace {
// As Discussed in b/2317702, UI windows are disabled by default because it is
// hard for a user to find out what caused the problem than finding that the
// operations seems to be disabled on the UI window when
// SPI_GETACTIVEWINDOWTRACKING is enabled.
// TODO(yukawa): Support mouse operations before we add a GUI feature which
//   requires UI interaction by mouse and/or touch. (b/2954874)
typedef ATL::CWinTraits<
            WS_POPUP | WS_DISABLED,
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE>
        CompositionLineWindowTraits;

// A class which implements an IME composition window for Windows. This class
// is derived from an ATL CWindowImpl<T> class, which provides methods for
// creating a window and handling windows messages.
class CompositionLineWindow
    : public ATL::CWindowImpl<CompositionLineWindow,
                              ATL::CWindow,
                              CompositionLineWindowTraits> {
 public:
  DECLARE_WND_CLASS_EX(kCompositionWindowClassName, CS_SAVEBITS, COLOR_WINDOW);

  BEGIN_MSG_MAP_EX(CandidateWindow)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_ERASEBKGND(OnEraseBkgnd)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_PRINTCLIENT(OnPrintClient)
  END_MSG_MAP()

  CompositionLineWindow() {}
  ~CompositionLineWindow() {}

  LRESULT OnCreate(LPCREATESTRUCT create_struct) {
    // Make sure the window is disabled for b/2317702.
    DCHECK(!IsWindowEnabled()) << "The window should no be enabled.";
    return 0;
  }
  BOOL OnEraseBkgnd(WTL::CDCHandle dc) {
    // We do not have to erase background
    // because all pixels in client area will be drawn in the DoPaint method.
    return TRUE;
  }
  void OnPaint(CDCHandle dc) {
    CRect client_rect;
    this->GetClientRect(&client_rect);

    if (dc != nullptr) {
      CMemoryDC memdc(dc, client_rect);
      DoPaint(memdc.m_hDC);
    } else  {
      CPaintDC paint_dc(this->m_hWnd);
      { // Create a copy of |paint_dc| and render the candidate strings in it.
        // The image rendered to this |memdc| is to be copied into the original
        // |paint_dc| in its destructor. So, we don't have to explicitly call
        // any functions that copy this |memdc| to the |paint_dc| but putting
        // the following code into a local block.
        CMemoryDC memdc(paint_dc, client_rect);
        DoPaint(memdc.m_hDC);
      }
    }
  }
  void OnPrintClient(CDCHandle dc, UINT uFlags) {
    OnPaint(dc);
  }
  void UpdateLayout(const CompositionWindowLayout &layout) {
    layout_ = layout;
    font_ = CLogFont(layout.log_font).CreateFontIndirect();
  }
 private:
  void DoPaint(WTL::CDCHandle dc) {
    const CFontHandle old_font = dc.SelectFont(font_);
    CRect client_rect;
    GetClientRect(&client_rect);
    dc.SetBkMode(TRANSPARENT);
    dc.FillSolidRect(&client_rect, RGB(0xff, 0xff, 0xff));
    dc.ExtTextOutW(
      layout_.base_position.x,
      layout_.base_position.y,
      0,
      &layout_.text_area,
      layout_.text.c_str(),
      layout_.text.size());
    dc.SelectFont(old_font);
    dc.SetDCPenColor(RGB(0, 0, 0));
    CPenHandle old_pen = dc.GetCurrentPen();
    for (size_t i = 0; i < layout_.marker_layouts.size(); ++i) {
      const SegmentMarkerLayout &marker = layout_.marker_layouts[i];
      if (marker.highlighted) {
        if (highlighted_pen_.IsNull()) {
          highlighted_pen_.CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
        }
        old_pen = dc.SelectPen(highlighted_pen_);
      } else {
        if (dotted_pen_.IsNull()) {
          const LOGBRUSH logbrush = {
            BS_SOLID,      // lbStyle
            RGB(0, 0, 0),  // lbColor;
            0,             // lbHatch;
          };
          // TODO(yukawa): Check the following issue on remote desktop.
          // http://msdn.microsoft.com/en-us/library/dd162705.aspx#1
          dotted_pen_.CreatePen(PS_ALTERNATE, 1, &logbrush);
        }
        old_pen = dc.SelectPen(dotted_pen_);
      }
      dc.MoveTo(marker.from);
      dc.LineTo(marker.to);
    }
    if (!::IsRectEmpty(&layout_.caret_rect)) {
      dc.FillSolidRect(&layout_.caret_rect, RGB(0, 0, 0));
    }
    if (old_pen) {
      dc.SelectPen(old_pen);
    }
  }
  CompositionWindowLayout layout_;
  CFont font_;
  CPen  dotted_pen_;
  CPen  highlighted_pen_;
  DISALLOW_COPY_AND_ASSIGN(CompositionLineWindow);
};

class CompositionWindowListImpl : public CompositionWindowList {
 public:
  CompositionWindowListImpl() {
  }
  virtual ~CompositionWindowListImpl() {
    Destroy();
  }

  virtual void UpdateLayout(const vector<CompositionWindowLayout> &layouts) {
    // Create windows if needed.
    if (line_windows_.size() < layouts.size()) {
      const size_t num_windows = layouts.size() - line_windows_.size();
      for (size_t i = 0; i < num_windows; ++i) {
        CompositionLineWindow *window = new CompositionLineWindow();
        window->Create(nullptr);
        line_windows_.push_back(window);
      }
    }
    for (size_t i = 0; i < line_windows_.size(); ++i) {
      if (i >= layouts.size()) {
        line_windows_[i]->ShowWindow(SW_HIDE);
      } else {
        const CompositionWindowLayout &window_layout = layouts[i];
        const CRect rect(window_layout.window_position_in_screen_coordinate);
        line_windows_[i]->UpdateLayout(window_layout);
        // We have to ensure the composition window will be placed at the top
        // most of the TOPMOST layer because the attached window might also be
        // in the TOPMOST layer.
        line_windows_[i]->SetWindowPos(
            HWND_TOPMOST, rect.left, rect.top, rect.Width(), rect.Height(),
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        line_windows_[i]->Invalidate(FALSE);
      }
    }
  }
  virtual void Initialize() {
    const int kInitialNumberOfWindows = 3;
    for (size_t i = 0; i < kInitialNumberOfWindows; ++i) {
      CompositionLineWindow *window = new CompositionLineWindow();
      window->Create(nullptr);
      line_windows_.push_back(window);
    }
  }
  virtual void AsyncHide() {
    for (size_t i = 0; i < line_windows_.size(); ++i) {
      line_windows_[i]->ShowWindow(SW_HIDE);
    }
  }
  virtual void AsyncQuit() {
    // TODO(yukawa): Implement this.
  }
  virtual void Destroy() {
    for (size_t i = 0; i < line_windows_.size(); ++i) {
      line_windows_[i]->DestroyWindow();
      delete line_windows_[i];
    }
    line_windows_.clear();
  }
  virtual void Hide() {
    for (size_t i = 0; i < line_windows_.size(); ++i) {
      line_windows_[i]->ShowWindow(SW_HIDE);
    }
  }

 private:
  vector<CompositionLineWindow *> line_windows_;
  DISALLOW_COPY_AND_ASSIGN(CompositionWindowListImpl);
};
}  // anonymous namespace

CompositionWindowList *CompositionWindowList::CreateInstance() {
  return new CompositionWindowListImpl();
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

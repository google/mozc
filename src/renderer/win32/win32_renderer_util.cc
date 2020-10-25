// Copyright 2010-2020, Google Inc.
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

#include "renderer/win32/win32_renderer_util.h"

// clang-format off
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlapp.h>
#include <atlgdi.h>
#include <atlmisc.h>
#include <winuser.h>
// clang-format on

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/win_util.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/win32_font_util.h"

namespace mozc {
namespace renderer {
namespace win32 {
using ATL::CWindow;
using std::unique_ptr;
using WTL::CDC;
using WTL::CDCHandle;
using WTL::CFont;
using WTL::CFontHandle;
using WTL::CLogFont;
using WTL::CPoint;
using WTL::CRect;
using WTL::CSize;

typedef mozc::commands::RendererCommand::CompositionForm CompositionForm;
typedef mozc::commands::RendererCommand::CandidateForm CandidateForm;

namespace {
// This template class is used to represent rendering information which may
// or may not be available depends on the application and operations.
template <typename T>
class Optional {
 public:
  Optional() : value_(T()), has_value_(false) {}

  explicit Optional(const T &src) : value_(src), has_value_(true) {}

  const T &value() const {
    DCHECK(has_value_);
    return value_;
  }

  T *mutable_value() {
    has_value_ = true;
    return &value_;
  }

  bool has_value() const { return has_value_; }

  void Clear() { has_value_ = false; }

 private:
  T value_;
  bool has_value_;
};

// A set of rendering information relevant to the target application.  Note
// that all of the positional fields are (logical) screen coordinates.
struct CandidateWindowLayoutParams {
  Optional<HWND> window_handle;
  Optional<IMECHARPOSITION> char_pos;
  Optional<CPoint> composition_form_topleft;
  Optional<CandidateWindowLayout> candidate_form;
  Optional<CRect> caret_rect;
  Optional<CLogFont> composition_font;
  Optional<CLogFont> default_gui_font;
  Optional<CRect> client_rect;
  Optional<bool> vertical_writing;
};

bool IsValidRect(const commands::RendererCommand::Rectangle &rect) {
  return rect.has_left() && rect.has_top() && rect.has_right() &&
         rect.has_bottom();
}

CRect ToRect(const commands::RendererCommand::Rectangle &rect) {
  DCHECK(IsValidRect(rect));
  return CRect(rect.left(), rect.top(), rect.right(), rect.bottom());
}

bool IsValidPoint(const commands::RendererCommand::Point &point) {
  return point.has_x() && point.has_y();
}

CPoint ToPoint(const commands::RendererCommand::Point &point) {
  DCHECK(IsValidPoint(point));
  return CPoint(point.x(), point.y());
}

// Returns an absolute font height of the composition font.
// Note that this function does not take the DPI virtualization into
// consideration so the caller is responsible to multiply an appropriate
// scaling factor.
// If a composition font is not available, this function try to use
// the default GUI font on this session.
int GetAbsoluteFontHeight(const CandidateWindowLayoutParams &params) {
  // A negative font height is also valid in the LONGFONT structure.
  // Use |abs| for normalization.
  // http://msdn.microsoft.com/en-us/library/dd145037.aspx
  if (params.composition_font.has_value()) {
    return abs(params.composition_font.value().lfHeight);
  }
  if (params.default_gui_font.has_value()) {
    return abs(params.default_gui_font.value().lfHeight);
  }
  return 0;
}

// "base_pos" is an ideal position where the candidate window is placed.
// Basically the ideal position depends on historical reason for each
// country and language.
// As for Japanese IME, the bottom-left (for horizontal writing) and
// top-left (for vertical writing) corner of the target segment have been
// used for many years.
CPoint GetBasePositionFromExcludeRect(const CRect &exclude_rect,
                                      bool is_vertical) {
  if (is_vertical) {
    // Vertical
    return exclude_rect.TopLeft();
  }

  // Horizontal
  return CPoint(exclude_rect.left, exclude_rect.bottom);
}

CPoint GetBasePositionFromIMECHARPOSITION(const IMECHARPOSITION &char_pos,
                                          bool is_vertical) {
  if (is_vertical) {
    return CPoint(char_pos.pt.x - char_pos.cLineHeight, char_pos.pt.y);
  }
  // Horizontal
  return CPoint(char_pos.pt.x, char_pos.pt.y + char_pos.cLineHeight);
}

// Returns false if given |form| should be ignored for some compatibility
// reason.  Otherwise, returns true.
bool IsCompatibleCompositionForm(const CompositionForm &form,
                                 int compatibility_mode) {
  // If IGNORE_DEFAULT_COMPOSITION_FORM flag is specified and all the fields
  // in the CompositionForm is 0, returns false.
  if ((compatibility_mode & IGNORE_DEFAULT_COMPOSITION_FORM) !=
      IGNORE_DEFAULT_COMPOSITION_FORM) {
    return true;
  }

  // Note that CompositionForm::DEFAULT is defined as 0.
  if (form.style_bits() != CompositionForm::DEFAULT) {
    return true;
  }

  if (!IsValidPoint(form.current_position())) {
    return true;
  }
  const CPoint current_position = ToPoint(form.current_position());
  if (current_position != CPoint(0, 0)) {
    return true;
  }

  if (!IsValidRect(form.area())) {
    return true;
  }
  const CRect area = ToRect(form.area());
  if (!area.IsRectNull()) {
    return true;
  }

  return false;
}

bool ExtractParams(LayoutManager *layout, int compatibility_mode,
                   const commands::RendererCommand::ApplicationInfo &app_info,
                   CandidateWindowLayoutParams *params) {
  DCHECK_NE(nullptr, layout);
  DCHECK_NE(nullptr, params);

  params->window_handle.Clear();
  params->char_pos.Clear();
  params->composition_form_topleft.Clear();
  params->candidate_form.Clear();
  params->caret_rect.Clear();
  params->composition_font.Clear();
  params->default_gui_font.Clear();
  params->client_rect.Clear();
  params->vertical_writing.Clear();

  if (!app_info.has_target_window_handle()) {
    return false;
  }
  const HWND target_window =
      WinUtil::DecodeWindowHandle(app_info.target_window_handle());

  *params->window_handle.mutable_value() = target_window;

  if (app_info.has_composition_target()) {
    const commands::RendererCommand::CharacterPosition &char_pos =
        app_info.composition_target();
    // Check the availability of optional fields.
    if (char_pos.has_position() && char_pos.has_top_left() &&
        IsValidPoint(char_pos.top_left()) && char_pos.has_line_height() &&
        char_pos.line_height() > 0 && char_pos.has_document_area() &&
        IsValidRect(char_pos.document_area())) {
      // Positional fields are (logical) screen coordinate.
      IMECHARPOSITION *dest = params->char_pos.mutable_value();
      dest->dwCharPos = char_pos.position();
      dest->pt = ToPoint(char_pos.top_left());
      dest->cLineHeight = char_pos.line_height();
      dest->rcDocument = ToRect(char_pos.document_area());
    }
  }

  if (app_info.has_composition_form()) {
    const commands::RendererCommand::CompositionForm &form =
        app_info.composition_form();

    // Check the availability of optional fields.
    if (form.has_current_position() && IsValidPoint(form.current_position()) &&
        IsCompatibleCompositionForm(form, compatibility_mode)) {
      Optional<CPoint> screen_pos;
      if (!layout->ClientPointToScreen(
              target_window, ToPoint(form.current_position()),
              params->composition_form_topleft.mutable_value())) {
        params->composition_form_topleft.Clear();
      }
    }
  }

  if (app_info.has_candidate_form()) {
    const commands::RendererCommand::CandidateForm &form =
        app_info.candidate_form();

    const uint32 candidate_style_bits = form.style_bits();

    const bool has_candidate_pos_style_bit =
        ((candidate_style_bits & CandidateForm::CANDIDATEPOS) ==
         CandidateForm::CANDIDATEPOS);

    const bool has_exclude_style_bit =
        ((candidate_style_bits & CandidateForm::EXCLUDE) ==
         CandidateForm::EXCLUDE);

    // Check the availability of optional fields.
    if ((has_candidate_pos_style_bit || has_exclude_style_bit) &&
        form.has_current_position() && IsValidPoint(form.current_position())) {
      const bool use_local_coord =
          (compatibility_mode & USE_LOCAL_COORD_FOR_CANDIDATE_FORM) ==
          USE_LOCAL_COORD_FOR_CANDIDATE_FORM;
      Optional<CPoint> screen_pos;
      if (use_local_coord) {
        if (!layout->LocalPointToScreen(target_window,
                                        ToPoint(form.current_position()),
                                        screen_pos.mutable_value())) {
          screen_pos.Clear();
        }
      } else {
        if (!layout->ClientPointToScreen(target_window,
                                         ToPoint(form.current_position()),
                                         screen_pos.mutable_value())) {
          screen_pos.Clear();
        }
      }
      if (screen_pos.has_value()) {
        // Here, we got an appropriate position where the candidate window
        // can be placed.
        params->candidate_form.mutable_value()->InitializeWithPosition(
            screen_pos.value());

        // If |CandidateForm::EXCLUDE| is specified, try to use the |area|
        // field as an exclude region.
        if (has_exclude_style_bit && form.has_area() &&
            IsValidRect(form.area())) {
          Optional<CRect> screen_rect;
          if (use_local_coord) {
            if (!layout->LocalRectToScreen(target_window, ToRect(form.area()),
                                           screen_rect.mutable_value())) {
              screen_rect.Clear();
            }
          } else {
            if (!layout->ClientRectToScreen(target_window, ToRect(form.area()),
                                            screen_rect.mutable_value())) {
              screen_rect.Clear();
            }
          }
          if (screen_rect.has_value()) {
            // Here we got an appropriate exclude region too.
            // Update the |candidate_form| with them.
            params->candidate_form.mutable_value()
                ->InitializeWithPositionAndExcludeRegion(screen_pos.value(),
                                                         screen_rect.value());
          }
        }
      }
    }
  }

  if (app_info.has_caret_info()) {
    const commands::RendererCommand::CaretInfo &caret_info =
        app_info.caret_info();

    // Check the availability of optional fields.
    if (caret_info.has_blinking() && caret_info.has_caret_rect() &&
        IsValidRect(caret_info.caret_rect()) &&
        caret_info.has_target_window_handle()) {
      const HWND caret_window =
          WinUtil::DecodeWindowHandle(caret_info.target_window_handle());
      const CRect caret_rect_in_client_coord(ToRect(caret_info.caret_rect()));
      // It seems (0, 0, 0, 0) represents that the application does not have a
      // valid caret now.
      if (!caret_rect_in_client_coord.IsRectNull()) {
        if (!layout->ClientRectToScreen(caret_window,
                                        caret_rect_in_client_coord,
                                        params->caret_rect.mutable_value())) {
          params->caret_rect.Clear();
        }
      }
    }
  }

  if (app_info.has_composition_font()) {
    if (!mozc::win32::FontUtil::ToLOGFONT(
            app_info.composition_font(),
            params->composition_font.mutable_value())) {
      params->composition_font.Clear();
    }
  }

  if (!layout->GetDefaultGuiFont(params->default_gui_font.mutable_value())) {
    params->default_gui_font.Clear();
  }

  {
    CRect client_rect_in_local_coord;
    CRect client_rect_in_logical_screen_coord;
    if (layout->GetClientRect(target_window, &client_rect_in_local_coord) &&
        layout->ClientRectToScreen(target_window, client_rect_in_local_coord,
                                   &client_rect_in_logical_screen_coord)) {
      *params->client_rect.mutable_value() =
          client_rect_in_logical_screen_coord;
    }
  }

  {
    const LayoutManager::WritingDirection direction =
        layout->GetWritingDirection(app_info);
    if (direction == LayoutManager::VERTICAL_WRITING) {
      *params->vertical_writing.mutable_value() = true;
    } else if (direction == LayoutManager::HORIZONTAL_WRITING) {
      *params->vertical_writing.mutable_value() = false;
    }
  }
  return true;
}

bool CanUseExcludeRegionInCandidateFrom(
    const CandidateWindowLayoutParams &params, int compatibility_mode,
    bool for_suggestion) {
  if (for_suggestion &&
      ((compatibility_mode & CAN_USE_CANDIDATE_FORM_FOR_SUGGEST) !=
       CAN_USE_CANDIDATE_FORM_FOR_SUGGEST)) {
    // This is suggestion and |CAN_USE_CANDIDATE_FORM_FOR_SUGGEST| is not
    // specified.  We cannot assume that |CANDIDATEFORM| is valid in this case.
    return false;
  }
  if (!params.candidate_form.has_value()) {
    return false;
  }
  if (!params.candidate_form.value().has_exclude_region()) {
    return false;
  }
  return true;
}

std::wstring ComposePreeditText(const commands::Preedit &preedit,
                                string *preedit_utf8,
                                std::vector<int> *segment_indices,
                                std::vector<CharacterRange> *segment_ranges) {
  if (preedit_utf8 != nullptr) {
    preedit_utf8->clear();
  }
  if (segment_indices != nullptr) {
    segment_indices->clear();
  }
  if (segment_ranges != nullptr) {
    segment_ranges->clear();
  }
  std::wstring value;
  int total_characters = 0;
  for (size_t segment_index = 0; segment_index < preedit.segment_size();
       ++segment_index) {
    const commands::Preedit::Segment &segment = preedit.segment(segment_index);
    std::wstring segment_value;
    mozc::Util::UTF8ToWide(segment.value(), &segment_value);
    value.append(segment_value);
    if (preedit_utf8 != nullptr) {
      preedit_utf8->append(segment.value());
    }
    const int text_length = segment_value.size();
    if (segment_indices != nullptr) {
      for (size_t i = 0; i < text_length; ++i) {
        segment_indices->push_back(segment_index);
      }
    }
    if (segment_ranges != nullptr) {
      CharacterRange range;
      range.begin = total_characters;
      range.length = text_length;
      segment_ranges->push_back(range);
    }
    total_characters += text_length;
  }
  return value;
}

bool CalcLayoutWithTextWrappingInternal(CDCHandle dc, const std::wstring &str,
                                        const int maximum_line_length,
                                        const int initial_offset,
                                        std::vector<LineLayout> *line_layouts) {
  DCHECK(line_layouts != nullptr);
  if (initial_offset < 0 || maximum_line_length <= 0 ||
      maximum_line_length < initial_offset) {
    LOG(ERROR) << "(initial_offset, maximum_line_length) = (" << initial_offset
               << ", " << maximum_line_length << ")";
    return false;
  }

  TEXTMETRIC metrics = {0};
  if (!dc.GetTextMetrics(&metrics)) {
    const int error = ::GetLastError();
    LOG(ERROR) << "GetTextMetrics failed. error = " << error;
    return false;
  }

  int string_index = 0;
  int current_offset = initial_offset;
  while (true) {
    const int remaining_chars = str.size() - string_index;
    if (remaining_chars <= 0) {
      return true;
    }

    // Here we can assume the following conditions are satisfied.
    //  0 <= current_offset
    //  0 <= maximum_line_length
    //  current_offset <= maximum_line_length
    const int remaining_extent = maximum_line_length - current_offset;
    DCHECK_GE(remaining_extent, 0)
        << "(remaining_extent, maximum_line_length) = (" << remaining_extent
        << ", " << maximum_line_length << ")";
    DCHECK_GE(maximum_line_length, remaining_extent)
        << "(remaining_extent, maximum_line_length) = (" << remaining_extent
        << ", " << maximum_line_length << ")";

    int allowable_chars = 0;
    CSize dummy;
    BOOL result = dc.GetTextExtentExPoint(
        str.c_str() + string_index, remaining_chars, &dummy, remaining_extent,
        &allowable_chars, nullptr);
    if (result == FALSE) {
      const int error = ::GetLastError();
      LOG(ERROR) << "GetTextExtentExPoint failed. error = " << error;
      return false;
    }

    if (allowable_chars == 0 && current_offset == 0) {
      // In this case, the region does not have enough space to display the
      // next character.
      return false;
    }

    // Just in case GetTextExtentExPoint returns true but the returned value
    // is invalid.  We have not seen any problem around this API though.
    if (allowable_chars < 0 || remaining_chars < allowable_chars) {
      // Something wrong.
      LOG(ERROR) << "(allowable_chars, remaining_chars) = (" << allowable_chars
                 << ", " << remaining_chars << ")";
      return false;
    }

    {
      LineLayout layout;
      layout.text = str.substr(string_index, allowable_chars);
      if (allowable_chars == 0) {
        // This case occurs only when this line does not have enough space to
        // render the next character from |current_offset|.  We will try to
        // render text in the next line.  Note that an infinite loop should
        // never occur because we have already checked the case above.  This is
        // why |current_offset| should be positive here.
        DCHECK_GT(current_offset, 0);
        layout.line_width = metrics.tmHeight;
      } else {
        CSize line_size;
        int allowable_chars_for_confirmation = 0;
        std::unique_ptr<int[]> size_buffer(new int[allowable_chars]);
        result = dc.GetTextExtentExPoint(
            layout.text.c_str(), layout.text.size(), &line_size,
            remaining_extent, &allowable_chars_for_confirmation,
            size_buffer.get());
        if (result == FALSE) {
          const int error = ::GetLastError();
          LOG(ERROR) << "GetTextExtentExPoint failed. error = " << error;
          return false;
        }
        if (allowable_chars != allowable_chars_for_confirmation) {
          LOG(ERROR)
              << "(allowable_chars, allowable_chars_for_confirmation) = ("
              << allowable_chars << ", " << allowable_chars_for_confirmation
              << ")";
          return false;
        }
        layout.line_length = line_size.cx;
        layout.line_width = line_size.cy;
        layout.line_start_offset = current_offset;
        int next_char_begin = 0;
        for (size_t character_index = 0; character_index < allowable_chars;
             ++character_index) {
          CharacterRange range;
          range.begin = next_char_begin;
          range.length = size_buffer[character_index] - next_char_begin;
          layout.character_positions.push_back(range);
          next_char_begin = size_buffer[character_index];
        }
      }
      DCHECK_EQ(layout.text.size(), layout.character_positions.size());
      line_layouts->push_back(layout);
    }

    string_index += allowable_chars;
    current_offset = 0;
  }

  return false;
}

class NativeSystemPreferenceAPI : public SystemPreferenceInterface {
 public:
  virtual ~NativeSystemPreferenceAPI() {}

  virtual bool GetDefaultGuiFont(LOGFONTW *log_font) {
    if (log_font == nullptr) {
      return false;
    }

    CLogFont message_box_font;
    // Use message box font as a default font to be consistent with
    // the candidate window.
    // TODO(yukawa): make a theme layer which is responsible for
    //   the look and feel of both composition window and candidate window.
    // TODO(yukawa): verify the font can render U+005C as a yen sign.
    //               (http://b/1992773)
    message_box_font.SetMessageBoxFont();
    // Use factor "3" to be consistent with the candidate window.
    message_box_font.MakeLarger(3);
    message_box_font.lfWeight = FW_NORMAL;

    *log_font = message_box_font;
    return true;
  }
};

class NativeWorkingAreaAPI : public WorkingAreaInterface {
 public:
  NativeWorkingAreaAPI() {}

 private:
  virtual bool GetWorkingAreaFromPoint(const POINT &point, RECT *working_area) {
    if (working_area == nullptr) {
      return false;
    }
    ::SetRect(working_area, 0, 0, 0, 0);

    // Obtain the monitor's working area
    const HMONITOR monitor =
        ::MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr) {
      return false;
    }

    MONITORINFO monitor_info = {};
    monitor_info.cbSize = CCSIZEOF_STRUCT(MONITORINFO, dwFlags);
    if (!::GetMonitorInfo(monitor, &monitor_info)) {
      const DWORD error = GetLastError();
      LOG(ERROR) << "GetMonitorInfo failed. Error: " << error;
      return false;
    }

    *working_area = monitor_info.rcWork;
    return true;
  }
};

class NativeWindowPositionAPI : public WindowPositionInterface {
 public:
  NativeWindowPositionAPI()
      : logical_to_physical_point_for_per_monitor_dpi_(
            GetLogicalToPhysicalPointForPerMonitorDPI()) {}

  virtual ~NativeWindowPositionAPI() {}

  virtual bool LogicalToPhysicalPoint(HWND window_handle,
                                      const POINT &logical_coordinate,
                                      POINT *physical_coordinate) {
    if (physical_coordinate == nullptr) {
      return false;
    }
    DCHECK_NE(nullptr, physical_coordinate);
    if (!::IsWindow(window_handle)) {
      return false;
    }

    // The attached window is likely to be a child window but only root
    // windows are fully supported by LogicalToPhysicalPoint API.  Using
    // root window handle instead of target window handle is likely to make
    // this API happy.
    const HWND root_window_handle = GetRootWindow(window_handle);

    // The document of LogicalToPhysicalPoint API is somewhat ambiguous.
    // http://msdn.microsoft.com/en-us/library/ms633533.aspx
    // Both input coordinates and output coordinates of this API are so-called
    // screen coordinates (offset from the upper-left corner of the screen).
    // Note that the input coordinates are logical coordinates, which means you
    // should pass screen coordinates obtained in a DPI-unaware process to
    // this API.  For example, coordinates returned by ClientToScreen API in a
    // DPI-unaware process are logical coordinates.  You can copy these
    // coordinates to a DPI-aware process and convert them to physical screen
    // coordinates by LogicalToPhysicalPoint API.
    *physical_coordinate = logical_coordinate;

    // Despite its name, LogicalToPhysicalPoint API no longer converts
    // coordinates on Windows 8.1 and later. We must use
    // LogicalToPhysicalPointForPerMonitorDPI API instead when it is available.
    // See http://go.microsoft.com/fwlink/?LinkID=307061
    if (SystemUtil::IsWindows8_1OrLater()) {
      if (logical_to_physical_point_for_per_monitor_dpi_ == nullptr) {
        return false;
      }
      return logical_to_physical_point_for_per_monitor_dpi_(
                 root_window_handle, physical_coordinate) != FALSE;
    }
    // On Windows 8 and prior, it's OK to rely on LogicalToPhysicalPoint API.
    return ::LogicalToPhysicalPoint(root_window_handle, physical_coordinate) !=
           FALSE;
  }

  // This method is not const to implement Win32WindowInterface.
  virtual bool GetWindowRect(HWND window_handle, RECT *rect) {
    return (::GetWindowRect(window_handle, rect) != FALSE);
  }

  // This method is not const to implement Win32WindowInterface.
  virtual bool GetClientRect(HWND window_handle, RECT *rect) {
    return (::GetClientRect(window_handle, rect) != FALSE);
  }

  // This method is not const to implement Win32WindowInterface.
  virtual bool ClientToScreen(HWND window_handle, POINT *point) {
    return (::ClientToScreen(window_handle, point) != FALSE);
  }

  // This method is not const to implement Win32WindowInterface.
  virtual bool IsWindow(HWND window_handle) {
    return (::IsWindow(window_handle) != FALSE);
  }

  // This method is not const to implement Win32WindowInterface.
  virtual HWND GetRootWindow(HWND window_handle) {
    // See the following document for Win32 window system.
    // http://msdn.microsoft.com/en-us/library/ms997562.aspx
    return ::GetAncestor(window_handle, GA_ROOT);
  }

  // This method is not const to implement Win32WindowInterface.
  virtual bool GetWindowClassName(HWND window_handle,
                                  std::wstring *class_name) {
    if (class_name == nullptr) {
      return false;
    }
    wchar_t class_name_buffer[1024] = {};
    const size_t num_copied_without_null = ::GetClassNameW(
        window_handle, class_name_buffer, ARRAYSIZE(class_name_buffer));
    if (num_copied_without_null >= (ARRAYSIZE(class_name_buffer) - 1)) {
      DLOG(ERROR) << "buffer length is insufficient.";
      return false;
    }
    class_name_buffer[num_copied_without_null] = L'\0';
    class_name->assign(class_name_buffer);
    return (::IsWindow(window_handle) != FALSE);
  }

 private:
  typedef BOOL(WINAPI *LogicalToPhysicalPointForPerMonitorDPIFunc)(
      HWND window_handle, POINT *point);
  static LogicalToPhysicalPointForPerMonitorDPIFunc
  GetLogicalToPhysicalPointForPerMonitorDPI() {
    // LogicalToPhysicalPointForPerMonitorDPI API is available on Windows 8.1
    // and later.
    if (!SystemUtil::IsWindows8_1OrLater()) {
      return nullptr;
    }

    const HMODULE module = WinUtil::GetSystemModuleHandle(L"user32.dll");
    if (module == nullptr) {
      return nullptr;
    }
    return reinterpret_cast<LogicalToPhysicalPointForPerMonitorDPIFunc>(
        ::GetProcAddress(module, "LogicalToPhysicalPointForPerMonitorDPI"));
  }

  const LogicalToPhysicalPointForPerMonitorDPIFunc
      logical_to_physical_point_for_per_monitor_dpi_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowPositionAPI);
};

struct WindowInfo {
  std::wstring class_name;
  CRect window_rect;
  CPoint client_area_offset;
  CSize client_area_size;
  double scale_factor;
  WindowInfo() : scale_factor(1.0) {}
};

class SystemPreferenceEmulatorImpl : public SystemPreferenceInterface {
 public:
  explicit SystemPreferenceEmulatorImpl(const LOGFONTW &gui_font)
      : default_gui_font_(gui_font) {}

  virtual ~SystemPreferenceEmulatorImpl() {}

  virtual bool GetDefaultGuiFont(LOGFONTW *log_font) {
    if (log_font == nullptr) {
      return false;
    }
    *log_font = default_gui_font_;
    return true;
  }

 private:
  CLogFont default_gui_font_;
};

class WorkingAreaEmulatorImpl : public WorkingAreaInterface {
 public:
  explicit WorkingAreaEmulatorImpl(const CRect &area) : area_(area) {}

 private:
  virtual bool GetWorkingAreaFromPoint(const POINT &point, RECT *working_area) {
    if (working_area == nullptr) {
      return false;
    }
    *working_area = area_;
    return true;
  }
  const CRect area_;
};

class WindowPositionEmulatorImpl : public WindowPositionEmulator {
 public:
  WindowPositionEmulatorImpl() {}
  virtual ~WindowPositionEmulatorImpl() {}

  // This method is not const to implement Win32WindowInterface.
  virtual bool GetWindowRect(HWND window_handle, RECT *rect) {
    if (rect == nullptr) {
      return false;
    }
    const WindowInfo *info = GetWindowInformation(window_handle);
    if (info == nullptr) {
      return false;
    }
    *rect = info->window_rect;
    return true;
  }

  // This method is not const to implement Win32WindowInterface.
  virtual bool GetClientRect(HWND window_handle, RECT *rect) {
    if (rect == nullptr) {
      return false;
    }
    const WindowInfo *info = GetWindowInformation(window_handle);
    if (info == nullptr) {
      return false;
    }
    *rect = CRect(CPoint(0, 0), info->client_area_size);
    return true;
  }

  // This method is not const to implement Win32WindowInterface.
  virtual bool ClientToScreen(HWND window_handle, POINT *point) {
    if (point == nullptr) {
      return false;
    }
    const WindowInfo *info = GetWindowInformation(window_handle);
    if (info == nullptr) {
      return false;
    }
    *point = (info->window_rect.TopLeft() + info->client_area_offset + *point);
    return true;
  }

  // This method is not const to implement Win32WindowInterface.
  virtual bool IsWindow(HWND window_handle) {
    const WindowInfo *info = GetWindowInformation(window_handle);
    if (info == nullptr) {
      return false;
    }
    return true;
  }

  // This method wraps API call of GetAncestor/GA_ROOT.
  virtual HWND GetRootWindow(HWND window_handle) {
    const std::map<HWND, HWND>::const_iterator it =
        root_map_.find(window_handle);
    if (it == root_map_.end()) {
      return window_handle;
    }
    return it->second;
  }

  // This method is not const to implement Win32WindowInterface.
  virtual bool GetWindowClassName(HWND window_handle,
                                  std::wstring *class_name) {
    if (class_name == nullptr) {
      return false;
    }
    const WindowInfo *info = GetWindowInformation(window_handle);
    if (info == nullptr) {
      return false;
    }
    *class_name = info->class_name;
    return true;
  }

  // This method is not const to implement Win32WindowInterface.
  virtual bool LogicalToPhysicalPoint(HWND window_handle,
                                      const POINT &logical_coordinate,
                                      POINT *physical_coordinate) {
    if (physical_coordinate == nullptr) {
      return false;
    }

    DCHECK_NE(nullptr, physical_coordinate);
    const WindowInfo *root_info =
        GetWindowInformation(GetRootWindow(window_handle));
    if (root_info == nullptr) {
      return false;
    }

    // BottomRight is treated inside of the rect in this scenario.
    const CRect &bottom_right_inflated_rect = CRect(
        root_info->window_rect.left, root_info->window_rect.top,
        root_info->window_rect.right + 1, root_info->window_rect.bottom + 1);
    if (bottom_right_inflated_rect.PtInRect(logical_coordinate) == FALSE) {
      return false;
    }
    physical_coordinate->x = logical_coordinate.x * root_info->scale_factor;
    physical_coordinate->y = logical_coordinate.y * root_info->scale_factor;
    return true;
  }

  virtual HWND RegisterWindow(const std::wstring &class_name,
                              const RECT &window_rect,
                              const POINT &client_area_offset,
                              const SIZE &client_area_size,
                              double scale_factor) {
    const HWND hwnd = GetNextWindowHandle();
    window_map_[hwnd].class_name = class_name;
    window_map_[hwnd].window_rect = window_rect;
    window_map_[hwnd].client_area_offset = client_area_offset;
    window_map_[hwnd].client_area_size = client_area_size;
    window_map_[hwnd].scale_factor = scale_factor;
    return hwnd;
  }

  virtual void SetRoot(HWND child_window, HWND root_window) {
    root_map_[child_window] = root_window;
  }

 private:
  HWND GetNextWindowHandle() const {
    if (window_map_.size() > 0) {
      const HWND last_hwnd = window_map_.rbegin()->first;
      return WinUtil::DecodeWindowHandle(
          WinUtil::EncodeWindowHandle(last_hwnd) + 7);
    }
    return WinUtil::DecodeWindowHandle(0x12345678);
  }

  // This method is not const to implement Win32WindowInterface.
  const WindowInfo *GetWindowInformation(HWND hwnd) {
    if (window_map_.find(hwnd) == window_map_.end()) {
      return nullptr;
    }
    return &(window_map_.find(hwnd)->second);
  }

  std::map<HWND, WindowInfo> window_map_;
  std::map<HWND, HWND> root_map_;

  DISALLOW_COPY_AND_ASSIGN(WindowPositionEmulatorImpl);
};

bool IsVerticalWriting(const CandidateWindowLayoutParams &params) {
  return params.vertical_writing.has_value() && params.vertical_writing.value();
}

// This is a helper function for LayoutCandidateWindowByCandidateForm.
// Some applications give us only the base position of candidate window.
// However, the exclude region is definitely important in terms of UX around
// candidate/suggest window.  As a workaround, we try to use font height to
// compose a virtual exclude region from the base position.
//   Expected applications and controls are:
//     - Candidate Window on Pidgin 2.6.1
//     - Candidate Window on V2C 2.1.6 on JRE 1.6.0.21
//     - Candidate Window on Fudemame 21
//   See also relevant unit tests.
// Returns true if the |candidate_layout| is determined in successful.
bool UpdateCandidateWindowFromBasePosAndFontHeight(
    const CandidateWindowLayoutParams &params, int compatibility_mode,
    const CPoint &base_pos_in_logical_coord, LayoutManager *layout_manager,
    CandidateWindowLayout *candidate_layout) {
  DCHECK(candidate_layout);
  candidate_layout->Clear();

  if (!params.window_handle.has_value()) {
    return false;
  }
  const HWND target_window = params.window_handle.value();

  const int font_height = GetAbsoluteFontHeight(params);
  if (font_height == 0) {
    return false;
  }
  DCHECK_LT(0, font_height);

  const bool is_vertical = IsVerticalWriting(params);
  CRect exclude_region_in_logical_coord;
  if (is_vertical) {
    // Vertical
    exclude_region_in_logical_coord.SetRect(
        base_pos_in_logical_coord.x, base_pos_in_logical_coord.y,
        base_pos_in_logical_coord.x + font_height,
        base_pos_in_logical_coord.y + 1);
  } else {
    // Horizontal
    exclude_region_in_logical_coord.SetRect(
        base_pos_in_logical_coord.x, base_pos_in_logical_coord.y - font_height,
        base_pos_in_logical_coord.x + 1, base_pos_in_logical_coord.y);
  }

  CRect exclude_region_in_physical_coord;
  layout_manager->GetRectInPhysicalCoords(target_window,
                                          exclude_region_in_logical_coord,
                                          &exclude_region_in_physical_coord);

  const CPoint base_pos_in_physical_coord = GetBasePositionFromExcludeRect(
      exclude_region_in_physical_coord, is_vertical);
  candidate_layout->InitializeWithPositionAndExcludeRegion(
      base_pos_in_physical_coord, exclude_region_in_physical_coord);
  return true;
}

// CANDIDATEFORM is most standard way to specify the position of a candidate
// window.  There are two major cases; one has an exclude region and the other
// has no exclude region.  The second case is virtually handled by
// UpdateCandidateWindowFromBasePosAndFontHeight function.
//   Expected applications and controls (for the first case) are:
//     - Suggest Window on apps with CAN_USE_CANDIDATE_FORM_FOR_SUGGEST
//     -- Qt-related windows whose class name is "QWidget"
//     -- Google Chrome-related windows whose class name is
//        "Chrome_RenderWidgetHostHWND"
//     - Candidate Window on windows which do not support IMECHARPOSITION
//     -- Internet Explorer
//     -- Open Office Writer
//     -- Qt-based applications
//   See also relevant unit tests.
// Returns true if the |candidate_layout| is determined in successful.
bool LayoutCandidateWindowByCandidateForm(
    const CandidateWindowLayoutParams &params, int compatibility_mode,
    LayoutManager *layout_manager, CandidateWindowLayout *candidate_layout) {
  DCHECK(candidate_layout);
  candidate_layout->Clear();

  if (!params.window_handle.has_value()) {
    return false;
  }
  if (!params.candidate_form.has_value()) {
    return false;
  }

  const HWND target_window = params.window_handle.value();
  const CandidateWindowLayout &form = params.candidate_form.value();

  CPoint base_pos_in_physical_coord;
  layout_manager->GetPointInPhysicalCoords(target_window, form.position(),
                                           &base_pos_in_physical_coord);

  if (!form.has_exclude_region()) {
    // If the candidate form does not have the exclude region, try to compose
    // supplemental exclude region by using font height information.
    CandidateWindowLayout layout;
    if (UpdateCandidateWindowFromBasePosAndFontHeight(
            params, compatibility_mode, form.position(), layout_manager,
            &layout)) {
      // succeeded to compose exclude region.
      DCHECK(layout.initialized());
      *candidate_layout = layout;
      return true;
    }
    candidate_layout->InitializeWithPosition(base_pos_in_physical_coord);
    return true;
  }

  DCHECK(form.has_exclude_region());

  CRect exclude_region_in_physical_coord;
  layout_manager->GetRectInPhysicalCoords(target_window, form.exclude_region(),
                                          &exclude_region_in_physical_coord);

  candidate_layout->InitializeWithPositionAndExcludeRegion(
      base_pos_in_physical_coord, exclude_region_in_physical_coord);
  return true;
}

// This function tries to use IMECHARPOSITION structure, which gives us
// sufficient information around the focused segment to use EXCLUDE-style
// positioning.  However, relatively small number of applications support
// this structure.
//   Expected applications and controls are:
//     - Microsoft Word
//     - Built-in RichEdit control
//     -- Chrome's Omni-box
//     - Built-in Edit control
//     -- Internet Explorer's address bar
//     - Firefox
//   See also relevant unit tests.
// Returns true if the |candidate_layout| is determined in successful.
bool LayoutCandidateWindowByCompositionTarget(
    const CandidateWindowLayoutParams &params, int compatibility_mode,
    bool for_suggestion, LayoutManager *layout_manager,
    CandidateWindowLayout *candidate_layout) {
  DCHECK(candidate_layout);
  candidate_layout->Clear();

  if (!params.window_handle.has_value()) {
    return false;
  }
  if (!params.char_pos.has_value()) {
    return false;
  }

  const HWND target_window = params.window_handle.value();
  const IMECHARPOSITION &char_pos = params.char_pos.value();

  // From the behavior of MS Office, we assume that an application fills
  // members in IMECHARPOSITION as follows, even though other interpretations
  // might be possible from the document especially for the vertical writing.
  //   http://msdn.microsoft.com/en-us/library/dd318162.aspx
  //
  // [Horizontal Writing]
  //
  //    (pt)
  //     v_____
  //     |     |
  //     |     | (cLineHeight)
  //     |     |
  //   --+-----+---------->  (Base Line)
  //
  // [Vertical Writing]
  //
  //    |
  //    +-----< (pt)
  //    |     |
  //    |-----+
  //    | (cLineHeight)
  //    |
  //    |
  //    v
  //   (Base Line)

  const bool can_use_candidate_form_exclude_region =
      CanUseExcludeRegionInCandidateFrom(params, compatibility_mode,
                                         for_suggestion);
  const bool is_vertical = IsVerticalWriting(params);
  CRect exclude_region_in_logical_coord;
  if (can_use_candidate_form_exclude_region) {
    exclude_region_in_logical_coord =
        params.candidate_form.value().exclude_region();
  } else if (is_vertical) {
    // Vertical
    exclude_region_in_logical_coord.left = char_pos.pt.x - char_pos.cLineHeight;
    exclude_region_in_logical_coord.top = char_pos.pt.y;
    exclude_region_in_logical_coord.right = char_pos.pt.x;
    exclude_region_in_logical_coord.bottom = char_pos.pt.y + 1;
  } else {
    // Horizontal
    exclude_region_in_logical_coord.left = char_pos.pt.x;
    exclude_region_in_logical_coord.top = char_pos.pt.y;
    exclude_region_in_logical_coord.right = char_pos.pt.x + 1;
    exclude_region_in_logical_coord.bottom =
        char_pos.pt.y + char_pos.cLineHeight;
  }

  const CPoint base_pos_in_logical_coord =
      GetBasePositionFromIMECHARPOSITION(char_pos, is_vertical);

  CPoint base_pos_in_physical_coord;
  layout_manager->GetPointInPhysicalCoords(
      target_window, base_pos_in_logical_coord, &base_pos_in_physical_coord);

  CRect exclude_region_in_physical_coord;
  layout_manager->GetRectInPhysicalCoords(target_window,
                                          exclude_region_in_logical_coord,
                                          &exclude_region_in_physical_coord);

  candidate_layout->InitializeWithPositionAndExcludeRegion(
      base_pos_in_physical_coord, exclude_region_in_physical_coord);
  return true;
}

// COMPOSITIONFORM contains the expected position of top-left corner of the
// composition window, which might be used to determine the position of the
// candidate window if no other relevant information is available.
// Actually the top left corner might be good enough for vertical writing.
// As for horizontal writing, the base position can be calculated if the height
// of the composition string is available.  This function supposes that the
// font height approximates to the height of the composition string.
// In both cases, this function also tries to compose an exclude region to
// improve the UX around suggest/candidate window if font height is available.
//   Expected applications and controls are:
//     - Suggest window on Pidgin 2.6.1
//   See also relevant unit tests.
// Returns true if the |candidate_layout| is determined in successful.
bool LayoutCandidateWindowByCompositionForm(
    const CandidateWindowLayoutParams &params, int compatibility_mode,
    LayoutManager *layout_manager, CandidateWindowLayout *candidate_layout) {
  DCHECK(candidate_layout);
  candidate_layout->Clear();

  if (!params.window_handle.has_value()) {
    return false;
  }
  if (!params.composition_form_topleft.has_value()) {
    return false;
  }

  const HWND target_window = params.window_handle.value();
  const CPoint &topleft_in_logical_coord =
      params.composition_form_topleft.value();
  const bool is_vertical = IsVerticalWriting(params);
  const int font_height = GetAbsoluteFontHeight(params);

  if (!is_vertical) {
    // For horizontal writing, a valid |font_height| is necessary to calculate
    // an appropriate position of suggest/candidate window.
    if (font_height == 0) {
      return false;
    }
    DCHECK_LT(0, font_height);

    const CRect rect_in_logical_coord(topleft_in_logical_coord.x,
                                      topleft_in_logical_coord.y,
                                      topleft_in_logical_coord.x + 1,
                                      topleft_in_logical_coord.y + font_height);

    CRect rect_in_physical_coord;
    layout_manager->GetRectInPhysicalCoords(
        target_window, rect_in_logical_coord, &rect_in_physical_coord);

    const CPoint bottom_left_in_physical_coord(rect_in_physical_coord.left,
                                               rect_in_physical_coord.bottom);
    candidate_layout->InitializeWithPositionAndExcludeRegion(
        bottom_left_in_physical_coord, rect_in_physical_coord);
    return true;
  }

  // Vertical
  DCHECK(is_vertical);
  CPoint topleft_in_physical_coord;
  layout_manager->GetPointInPhysicalCoords(
      target_window, topleft_in_logical_coord, &topleft_in_physical_coord);

  if (font_height == 0) {
    // For vertical writing, top-left cornier is acceptable.
    // Use CANDIDATEPOS-style by compromise.
    candidate_layout->InitializeWithPosition(topleft_in_physical_coord);
    return true;
  }
  DCHECK_LT(0, font_height);

  const CRect rect_in_logical_coord(
      topleft_in_logical_coord.x, topleft_in_logical_coord.y,
      topleft_in_logical_coord.x + font_height, topleft_in_logical_coord.y + 1);

  CRect rect_in_physical_coord;
  layout_manager->GetRectInPhysicalCoords(target_window, rect_in_logical_coord,
                                          &rect_in_physical_coord);

  candidate_layout->InitializeWithPositionAndExcludeRegion(
      topleft_in_physical_coord, rect_in_physical_coord);
  return true;
}

// This function calculates the candidate window position by using caret
// information, which is generally unreliable but sometimes becomes a good
// alternative even when no other positional information is available.
// In fact, the position of suggest window sometimes relies on the caret
// position because it is not guaranteed that the CANDIDATEFORM is valid before
// the application receives IMN_OPENCANDIDATE message.
// Another important consideration is how to calculate the exclude region.
// One may consider that the caret rect seems to be used but very small number
// of applications always use 1x1 rect regardless of the actual caret size.
// To improve the positional accuracy of the exclude region, this function
// adopt larger one between the caret height and font height when the exclude
// region is calculated.
//   Relevant applications and controls are:
//     - Workaround against Google Chrome (b/3104035)
//     - Suggest window on Hidemaru
//     - Suggest window on Open Office Writer
//     - Suggest window on Internet Explorer 8
//   See also relevant unit tests.
// Returns true if the |candidate_layout| is determined in successful.
bool LayoutCandidateWindowByCaretInfo(const CandidateWindowLayoutParams &params,
                                      int compatibility_mode,
                                      LayoutManager *layout_manager,
                                      CandidateWindowLayout *candidate_layout) {
  DCHECK(candidate_layout);
  candidate_layout->Clear();

  if (!params.window_handle.has_value()) {
    return false;
  }
  if (!params.caret_rect.has_value()) {
    return false;
  }

  const HWND target_window = params.window_handle.value();
  CRect exclude_region_in_logical_coord = params.caret_rect.value();

  // Use font height if available to improve the accuracy of exclude region.
  const int font_height = GetAbsoluteFontHeight(params);
  const bool is_vertical = IsVerticalWriting(params);

  if (font_height > 0) {
    if (is_vertical &&
        (exclude_region_in_logical_coord.Width() < font_height)) {
      // Vertical
      exclude_region_in_logical_coord.right =
          exclude_region_in_logical_coord.left + font_height;
    } else if (!is_vertical &&
               (exclude_region_in_logical_coord.Height() < font_height)) {
      // Horizontal
      exclude_region_in_logical_coord.bottom =
          exclude_region_in_logical_coord.top + font_height;
    }
  }

  CRect exclude_region_in_physical_coord;
  layout_manager->GetRectInPhysicalCoords(target_window,
                                          exclude_region_in_logical_coord,
                                          &exclude_region_in_physical_coord);

  const CPoint base_pos_in_physical_coord = GetBasePositionFromExcludeRect(
      exclude_region_in_physical_coord, is_vertical);

  candidate_layout->InitializeWithPositionAndExcludeRegion(
      base_pos_in_physical_coord, exclude_region_in_physical_coord);
  return true;
}

// On some applications, no positional information is available especially when
// the client want to show the suggest window.  In this case, we might want to
// show the (suggest) window next to the target window so that the candidate
// window will not cover the target window.
//   Expected applications and controls are:
//     - Suggest window on Fudemame 21
//   See also relevant unit tests.
// Returns true if the |candidate_layout| is determined in successful.
bool LayoutCandidateWindowByClientRect(
    const CandidateWindowLayoutParams &params, int compatibility_mode,
    LayoutManager *layout_manager, CandidateWindowLayout *candidate_layout) {
  DCHECK(candidate_layout);
  candidate_layout->Clear();

  if (!params.window_handle.has_value()) {
    return false;
  }
  if (!params.client_rect.has_value()) {
    return false;
  }

  const HWND target_window = params.window_handle.value();
  const CRect &client_rect_in_logical_coord = params.client_rect.value();
  const bool is_vertical = IsVerticalWriting(params);

  CRect client_rect_in_physical_coord;
  layout_manager->GetRectInPhysicalCoords(target_window,
                                          client_rect_in_logical_coord,
                                          &client_rect_in_physical_coord);

  if (is_vertical) {
    // Vertical
    // Current candidate window has not fully supported vertical writing yet so
    // it would be rather better to show the candidate window at the right side
    // of the target window.
    // This is why we do not use GetBasePositionFromExcludeRect here.
    // TODO(yukawa): use GetBasePositionFromExcludeRect once the vertical
    //   writing is fully supported by the candidate window.
    candidate_layout->InitializeWithPosition(
        CPoint(client_rect_in_physical_coord.right,
               client_rect_in_physical_coord.top));
  } else {
    // Horizontal
    candidate_layout->InitializeWithPosition(
        CPoint(client_rect_in_physical_coord.left,
               client_rect_in_physical_coord.bottom));
  }
  return true;
}

bool LayoutIndicatorWindowByCompositionTarget(
    const CandidateWindowLayoutParams &params,
    const LayoutManager &layout_manager, CRect *target_rect) {
  DCHECK(target_rect);
  *target_rect = CRect();

  if (!params.window_handle.has_value()) {
    return false;
  }
  if (!params.char_pos.has_value()) {
    return false;
  }

  const HWND target_window = params.window_handle.value();
  const IMECHARPOSITION &char_pos = params.char_pos.value();

  // From the behavior of MS Office, we assume that an application fills
  // members in IMECHARPOSITION as follows, even though other interpretations
  // might be possible from the document especially for the vertical writing.
  //   http://msdn.microsoft.com/en-us/library/dd318162.aspx

  const bool is_vertical = IsVerticalWriting(params);
  CRect rect_in_logical_coord;
  if (is_vertical) {
    // [Vertical Writing]
    //
    //    |
    //    +-----< (pt)
    //    |     |
    //    |-----+
    //    | (cLineHeight)
    //    |
    //    |
    //    v
    //   (Base Line)
    rect_in_logical_coord =
        CRect(char_pos.pt.x - char_pos.cLineHeight, char_pos.pt.y,
              char_pos.pt.x, char_pos.pt.y + 1);
  } else {
    // [Horizontal Writing]
    //
    //    (pt)
    //     v_____
    //     |     |
    //     |     | (cLineHeight)
    //     |     |
    //   --+-----+---------->  (Base Line)
    rect_in_logical_coord =
        CRect(char_pos.pt.x, char_pos.pt.y, char_pos.pt.x + 1,
              char_pos.pt.y + char_pos.cLineHeight);
  }

  layout_manager.GetRectInPhysicalCoords(target_window, rect_in_logical_coord,
                                         target_rect);
  return true;
}

bool LayoutIndicatorWindowByCompositionForm(
    const CandidateWindowLayoutParams &params,
    const LayoutManager &layout_manager, CRect *target_rect) {
  DCHECK(target_rect);
  *target_rect = CRect();
  if (!params.window_handle.has_value()) {
    return false;
  }
  if (!params.composition_form_topleft.has_value()) {
    return false;
  }

  const HWND target_window = params.window_handle.value();
  const CPoint &topleft_in_logical_coord =
      params.composition_form_topleft.value();
  const bool is_vertical = IsVerticalWriting(params);
  const int font_height = GetAbsoluteFontHeight(params);
  if (font_height <= 0) {
    return false;
  }

  const CRect rect_in_logical_coord(
      topleft_in_logical_coord,
      is_vertical ? CSize(font_height, 1) : CSize(1, font_height));

  layout_manager.GetRectInPhysicalCoords(target_window, rect_in_logical_coord,
                                         target_rect);
  return true;
}

bool LayoutIndicatorWindowByCaretInfo(const CandidateWindowLayoutParams &params,
                                      const LayoutManager &layout_manager,
                                      CRect *target_rect) {
  DCHECK(target_rect);
  *target_rect = CRect();
  if (!params.window_handle.has_value()) {
    return false;
  }
  if (!params.caret_rect.has_value()) {
    return false;
  }

  const HWND target_window = params.window_handle.value();
  CRect rect_in_logical_coord = params.caret_rect.value();

  // Use font height if available to improve the accuracy of exclude region.
  const int font_height = GetAbsoluteFontHeight(params);
  const bool is_vertical = IsVerticalWriting(params);

  if (font_height > 0) {
    if (is_vertical && (rect_in_logical_coord.Width() < font_height)) {
      // Vertical
      rect_in_logical_coord.right = rect_in_logical_coord.left + font_height;
    } else if (!is_vertical && (rect_in_logical_coord.Height() < font_height)) {
      // Horizontal
      rect_in_logical_coord.bottom = rect_in_logical_coord.top + font_height;
    }
  }

  layout_manager.GetRectInPhysicalCoords(target_window, rect_in_logical_coord,
                                         target_rect);
  return true;
}

bool GetTargetRectForIndicator(const CandidateWindowLayoutParams &params,
                               const LayoutManager &layout_manager,
                               CRect *focus_rect) {
  if (focus_rect == nullptr) {
    return false;
  }

  if (LayoutIndicatorWindowByCompositionTarget(params, layout_manager,
                                               focus_rect)) {
    return true;
  }
  if (LayoutIndicatorWindowByCompositionForm(params, layout_manager,
                                             focus_rect)) {
    return true;
  }
  if (LayoutIndicatorWindowByCaretInfo(params, layout_manager, focus_rect)) {
    return true;
  }

  // Clear the data just in case.
  *focus_rect = CRect();
  return false;
}

}  // namespace

SystemPreferenceInterface *SystemPreferenceFactory::CreateMock(
    const LOGFONTW &gui_font) {
  return new SystemPreferenceEmulatorImpl(gui_font);
}

WorkingAreaInterface *WorkingAreaFactory::Create() {
  return new NativeWorkingAreaAPI();
}

WorkingAreaInterface *WorkingAreaFactory::CreateMock(const RECT &working_area) {
  return new WorkingAreaEmulatorImpl(working_area);
}

WindowPositionEmulator *WindowPositionEmulator::Create() {
  return new WindowPositionEmulatorImpl();
}

CharacterRange::CharacterRange() : begin(0), length(0) {}

LineLayout::LineLayout()
    : line_length(0), line_width(0), line_start_offset(0) {}

CandidateWindowLayout::CandidateWindowLayout()
    : position_(CPoint()),
      exclude_region_(CRect()),
      has_exclude_region_(false),
      initialized_(false) {}

CandidateWindowLayout::~CandidateWindowLayout() {}

void CandidateWindowLayout::Clear() {
  position_ = CPoint();
  exclude_region_ = CRect();
  has_exclude_region_ = false;
  initialized_ = false;
}

void CandidateWindowLayout::InitializeWithPosition(const POINT &position) {
  position_ = position;
  exclude_region_ = CRect();
  has_exclude_region_ = false;
  initialized_ = true;
}

void CandidateWindowLayout::InitializeWithPositionAndExcludeRegion(
    const POINT &position, const RECT &exclude_region) {
  position_ = position;
  exclude_region_ = exclude_region;
  has_exclude_region_ = true;
  initialized_ = true;
}

const POINT &CandidateWindowLayout::position() const { return position_; }

const RECT &CandidateWindowLayout::exclude_region() const {
  DCHECK(has_exclude_region_);
  return exclude_region_;
}

bool CandidateWindowLayout::has_exclude_region() const {
  return has_exclude_region_;
}

bool CandidateWindowLayout::initialized() const { return initialized_; }

IndicatorWindowLayout::IndicatorWindowLayout() : is_vertical(false) {
  ::SetRect(&window_rect, 0, 0, 0, 0);
}

void IndicatorWindowLayout::Clear() {
  is_vertical = false;
  ::SetRect(&window_rect, 0, 0, 0, 0);
}

bool LayoutManager::CalcLayoutWithTextWrapping(
    const LOGFONTW &font, const std::wstring &text, int maximum_line_length,
    int initial_offset, std::vector<LineLayout> *line_layouts) {
  if (line_layouts == nullptr) {
    return false;
  }
  line_layouts->clear();

  CFont new_font(CLogFont(font).CreateFontIndirectW());
  if (new_font.IsNull()) {
    LOG(ERROR) << "CreateFont failed.";
    return false;
  }

  CDC dc;
  // Create a memory DC compatible with desktop DC.
  dc.CreateCompatibleDC(CDC(::GetDC(nullptr)));
  CFontHandle old_font = dc.SelectFont(new_font);

  const bool result = CalcLayoutWithTextWrappingInternal(
      dc.m_hDC, text, maximum_line_length, initial_offset, line_layouts);
  dc.SelectFont(old_font);

  return result;
}

void LayoutManager::GetPointInPhysicalCoords(HWND window_handle,
                                             const POINT &point,
                                             POINT *result) const {
  if (result == nullptr) {
    return;
  }

  DCHECK_NE(nullptr, window_position_.get());
  if (window_position_->LogicalToPhysicalPoint(window_handle, point, result)) {
    return;
  }

  // LogicalToPhysicalPoint API failed for some reason.
  // Emulate the result based on the scaling factor.
  const HWND root_window_handle =
      window_position_->GetRootWindow(window_handle);
  const double scale_factor = GetScalingFactor(root_window_handle);
  result->x = point.x * scale_factor;
  result->y = point.y * scale_factor;
  return;
}

void LayoutManager::GetRectInPhysicalCoords(HWND window_handle,
                                            const RECT &rect,
                                            RECT *result) const {
  if (result == nullptr) {
    return;
  }

  DCHECK_NE(nullptr, window_position_.get());

  CPoint top_left;
  GetPointInPhysicalCoords(window_handle, CPoint(rect.left, rect.top),
                           &top_left);
  CPoint bottom_right;
  GetPointInPhysicalCoords(window_handle, CPoint(rect.right, rect.bottom),
                           &bottom_right);
  *result = CRect(top_left, bottom_right);
  return;
}

SegmentMarkerLayout::SegmentMarkerLayout()
    : from(CPoint()), to(CPoint()), highlighted(false) {}

CompositionWindowLayout::CompositionWindowLayout()
    : window_position_in_screen_coordinate(CRect()),
      caret_rect(CRect()),
      text_area(CRect()),
      base_position(CPoint()),
      log_font(CLogFont()) {}

LayoutManager::LayoutManager()
    : system_preference_(new NativeSystemPreferenceAPI),
      window_position_(new NativeWindowPositionAPI) {}

LayoutManager::LayoutManager(SystemPreferenceInterface *mock_system_preference,
                             WindowPositionInterface *mock_window_position)
    : system_preference_(mock_system_preference),
      window_position_(mock_window_position) {}

LayoutManager::~LayoutManager() {}

// TODO(yukawa): Refactor this function into smaller functions as soon as
//   possible so that you can update the functionality and add new unit tests
//   more easily.
bool LayoutManager::LayoutCompositionWindow(
    const commands::RendererCommand &command,
    std::vector<CompositionWindowLayout> *composition_window_layouts,
    CandidateWindowLayout *candidate_layout) const {
  if (composition_window_layouts != nullptr) {
    composition_window_layouts->clear();
  }
  if (candidate_layout != nullptr) {
    candidate_layout->Clear();
  }

  if (!command.has_output() || !command.output().has_preedit() ||
      command.output().preedit().segment_size() <= 0 ||
      !command.output().preedit().has_cursor() ||
      !command.has_application_info() ||
      !command.application_info().has_target_window_handle()) {
    LOG(INFO) << "do nothing because of the lack of parameter(s)";
    return true;
  }
  const mozc::commands::Output &output = command.output();
  const HWND target_window_handle = WinUtil::DecodeWindowHandle(
      command.application_info().target_window_handle());

  const mozc::commands::RendererCommand::ApplicationInfo &app =
      command.application_info();
  CLogFont logfont;
  if (!app.has_composition_font() ||
      !mozc::win32::FontUtil::ToLOGFONT(app.composition_font(), &logfont)) {
    // If the composition font is not available, use default GUI font as a
    // fall back.
    if (!system_preference_->GetDefaultGuiFont(&logfont)) {
      LOG(ERROR) << "GetDefaultGuiFont failed.";
      return false;
    }
  }

  // Remove underline attribute.  See b/2935480 for details.
  logfont.lfUnderline = 0;

  // We only support lfEscapement == 0 or 2700.
  if (logfont.lfEscapement != 0 && logfont.lfEscapement != 2700) {
    LOG(ERROR) << "Unsupported escapement: " << logfont.lfEscapement;
    return false;
  }

  const mozc::commands::Preedit &preedit = output.preedit();

  const bool is_vertical = (GetWritingDirection(app) == VERTICAL_WRITING);

  CompositionForm composition_form;
  if (command.application_info().has_composition_form()) {
    composition_form.CopyFrom(command.application_info().composition_form());
  } else {
    // No composition form is available.  Use client rect instead.
    CRect client_rect;
    if (!window_position_->GetClientRect(target_window_handle, &client_rect)) {
      return false;
    }
    // We need not to use CompositionForm::RECT.  The client area will be used
    // for character wrapping anyway.
    composition_form.set_style_bits(CompositionForm::POINT);
    if (is_vertical) {
      composition_form.mutable_current_position()->set_x(client_rect.left);
      composition_form.mutable_current_position()->set_y(client_rect.top);
    } else {
      composition_form.mutable_current_position()->set_x(client_rect.left);
      composition_form.mutable_current_position()->set_y(client_rect.bottom);
    }
  }

  const uint32 style_bits = composition_form.style_bits();

  // Check the availability of optional fields.
  // Note that currently we always use |current_position| field even when
  // |style_bits| does not contain CompositionForm::POINT bit.
  if (!composition_form.has_current_position() ||
      !composition_form.current_position().has_x() ||
      !composition_form.current_position().has_y()) {
    return false;
  }

  const HWND root_window_handle =
      window_position_->GetRootWindow(target_window_handle);
  if (root_window_handle == nullptr) {
    LOG(ERROR) << "GetRootWindow failed.";
    return false;
  }

  const double scale = GetScalingFactor(root_window_handle);
  const bool no_dpi_virtualization = (scale == 1.0);

  const CPoint current_pos_in_client_coord(
      composition_form.current_position().x(),
      composition_form.current_position().y());

  CPoint current_pos_in_logical_coord;
  if (!ClientPointToScreen(target_window_handle, current_pos_in_client_coord,
                           &current_pos_in_logical_coord)) {
    LOG(ERROR) << "ClientPointToScreen failed.";
    return false;
  }

  CPoint current_pos;
  GetPointInPhysicalCoords(target_window_handle, current_pos_in_logical_coord,
                           &current_pos);

  // Check the availability of optional fields.
  // Note that some applications may set |CompositionForm::RECT| and other
  // style bits like |CompositionForm::POINT| at the same time.
  // See b/3200425 for details.
  bool use_area_in_composition_form = false;
  if (((style_bits & CompositionForm::RECT) == CompositionForm::RECT) &&
      composition_form.has_area() && composition_form.area().has_left() &&
      composition_form.area().has_top() &&
      composition_form.area().has_right() &&
      composition_form.area().has_bottom()) {
    use_area_in_composition_form = true;
  }

  CRect area_in_client_coord;
  if (use_area_in_composition_form) {
    area_in_client_coord.SetRect(
        composition_form.area().left(), composition_form.area().top(),
        composition_form.area().right(), composition_form.area().bottom());
  } else {
    if (window_position_->GetClientRect(target_window_handle,
                                        &area_in_client_coord) == FALSE) {
      const int error = ::GetLastError();
      DLOG(ERROR) << "GetClientRect failed.  error = " << error;
      return false;
    }
  }

  CRect area_in_logical_coord;
  if (!ClientRectToScreen(target_window_handle, area_in_client_coord,
                          &area_in_logical_coord)) {
    return false;
  }

  CPoint current_pos_in_physical_coord;
  GetPointInPhysicalCoords(target_window_handle, current_pos_in_logical_coord,
                           &current_pos_in_physical_coord);

  CRect area_in_physical_coord;
  GetRectInPhysicalCoords(target_window_handle, area_in_logical_coord,
                          &area_in_physical_coord);

  // Adjust the font size to be equal to that in the target process with
  // taking DPI virtualization into account.
  if (!no_dpi_virtualization) {
    logfont.lfHeight = static_cast<int>(logfont.lfHeight * scale);
  }

  // Ensure the escapement and orientation are consistent with writing
  // direction.  Note that some applications always set 0 to |lfOrientation|.
  if (is_vertical) {
    logfont.lfEscapement = 2700;
    logfont.lfOrientation = 2700;
  } else {
    logfont.lfEscapement = 0;
    logfont.lfOrientation = 0;
  }

  string preedit_utf8;
  std::vector<int> segment_indices;
  std::vector<CharacterRange> segment_lengths;
  const std::wstring composition_text = ComposePreeditText(
      preedit, &preedit_utf8, &segment_indices, &segment_lengths);
  DCHECK_EQ(composition_text.size(), segment_indices.size());
  DCHECK_EQ(preedit.segment_size(), segment_lengths.size());
  std::vector<mozc::renderer::win32::LineLayout> layouts;
  bool result = false;
  {
    const int offset =
        is_vertical
            ? current_pos_in_physical_coord.y - area_in_physical_coord.top
            : current_pos_in_physical_coord.x - area_in_physical_coord.left;
    const int limit = is_vertical ? area_in_physical_coord.Height()
                                  : area_in_physical_coord.Width();
    result = CalcLayoutWithTextWrapping(logfont, composition_text, limit,
                                        offset, &layouts);
  }
  if (!result) {
    LOG(ERROR) << "CalcLayoutWithTextWrapping failed.";
    return false;
  }

  if (composition_window_layouts != nullptr) {
    composition_window_layouts->clear();
  }

  int cursor_index = -1;
  if (output.has_candidates() && output.candidates().has_position()) {
    // |cursor_index| is supposed to be wide character index but
    // |output.candidates().position()| is the number of Unicode characters.
    // In case surrogate pair appears, use Util::WideCharsLen to calculate the
    // cursor position as wide character index. See b/4163234 for details.
    cursor_index = Util::WideCharsLen(
        Util::Utf8SubString(preedit_utf8, 0, output.candidates().position()));
  }

  const bool is_suggest =
      output.candidates().has_category() &&
      (output.candidates().category() == commands::SUGGESTION);

  // When this flag is true, suggest window must not hide preedit text.
  // TODO(yukawa): remove |!is_vertical| when vertical candidate window is
  //   implemented.
  const bool suggest_window_never_hides_preedit = (!is_vertical && is_suggest);

  int total_line_offset = 0;
  int total_characters = 0;
  for (size_t layout_index = 0; layout_index < layouts.size(); ++layout_index) {
    const mozc::renderer::win32::LineLayout &layout = layouts[layout_index];
    if (layout.text.size() < 0 || layout.line_length < 0 ||
        layout.character_positions.size() < 0) {
      // unexpected values found.
      return false;
    }

    if (layout.text.size() == 0 || layout.line_length == 0 ||
        layout.character_positions.size() == 0) {
      // This line is full.  Go to next line.
      total_line_offset += layout.line_width;
      total_characters += layout.text.size();
      continue;
    }

    DCHECK_GT(layout.text.size(), 0);
    DCHECK_GT(layout.line_length, 0);
    DCHECK_GT(layout.character_positions.size(), 0);

    CompositionWindowLayout window_layout;
    window_layout.text = layout.text;
    window_layout.log_font = logfont;
    CRect window_rect;
    CRect text_rect;
    CPoint base_point;
    if (is_vertical) {
      window_rect.top = area_in_physical_coord.top + layout.line_start_offset;
      window_rect.right = current_pos.x - total_line_offset;
      window_rect.left = window_rect.right - layout.line_width;
      window_rect.bottom = window_rect.top + layout.line_length;
      text_rect.SetRect(0, 0, layout.line_width, layout.line_length);
      base_point.SetPoint(layout.line_width, 0);
    } else {
      window_rect.left = area_in_physical_coord.left + layout.line_start_offset;
      window_rect.top = current_pos.y + total_line_offset;
      window_rect.right = window_rect.left + layout.line_length;
      window_rect.bottom = window_rect.top + layout.line_width;
      text_rect.SetRect(0, 0, layout.line_length, layout.line_width);
      base_point.SetPoint(0, 0);
    }
    window_layout.window_position_in_screen_coordinate = window_rect;
    window_layout.text_area = text_rect;
    window_layout.base_position = base_point;

    const int next_total_characters = total_characters + layout.text.size();

    // Calculate caret rect assuming its width is 1 pixel.
    // Note that |caret_index| is supposed to be wide character index but
    // |output.preedit().cursor()| is the number of Unicode characters.
    // In case surrogate pair appears, use Util::WideCharsLen to calculate the
    // cursor position as wide character index. See b/4163234 for details.
    // TODO(yukawa): We should use the actual caret size, which can be
    //   obtained by GetGUIThreadInfo API.
    const int caret_index = Util::WideCharsLen(
        Util::Utf8SubString(preedit_utf8, 0, output.preedit().cursor()));

    if (total_characters <= caret_index &&
        caret_index < next_total_characters) {
      // In this case, caret points existing character.  We use the left edge
      // of the pointed character.
      const int local_caret_index = caret_index - total_characters;

      const int caret_begin =
          layout.character_positions[local_caret_index].begin;
      // Add 1 because Win32 RECTs are endpoint-exclusive.
      // http://weblogs.asp.net/oldnewthing/archive/2004/02/18/75652.aspx
      // http://www.radiumsoftware.com/0402.html#040222
      const int caret_end = caret_begin + 1;
      if (is_vertical) {
        window_layout.caret_rect =
            CRect(0, caret_begin, layout.line_width, caret_end);
      } else {
        window_layout.caret_rect =
            CRect(caret_begin, 0, caret_end, layout.line_width);
      }
    } else if (((layout_index + 1) == layouts.size()) &&
               (caret_index == next_total_characters)) {
      // In this case, caret points the next to the last character.
      // The composition window should have an extra space to draw the caret if
      // the window can be extended.
      CRect extended_rect(window_layout.window_position_in_screen_coordinate);
      if (is_vertical) {
        if (extended_rect.bottom < area_in_physical_coord.bottom) {
          // Still inside of the |area| if we extend the window.
          extended_rect.InflateRect(0, 0, 0, 1);
        }
        const int caret_begin = extended_rect.Height() - 1;
        // Add 1 because Win32 RECTs are endpoint-exclusive.
        // http://weblogs.asp.net/oldnewthing/archive/2004/02/18/75652.aspx
        // http://www.radiumsoftware.com/0402.html#040222
        const int caret_end = caret_begin + 1;
        window_layout.caret_rect =
            CRect(0, caret_begin, layout.line_width, caret_end);
      } else {
        if (extended_rect.right < area_in_physical_coord.right) {
          // Still inside of the |area| if we extend the window.
          extended_rect.InflateRect(0, 0, 1, 0);
        }
        const int caret_begin = extended_rect.Width() - 1;
        // Add 1 because Win32 RECTs are endpoint-exclusive.
        // http://weblogs.asp.net/oldnewthing/archive/2004/02/18/75652.aspx
        // http://www.radiumsoftware.com/0402.html#040222
        const int caret_end = caret_begin + 1;
        window_layout.caret_rect =
            CRect(caret_begin, 0, caret_end, layout.line_width);
      }
      window_layout.window_position_in_screen_coordinate = extended_rect;
    }

    if (total_characters <= cursor_index &&
        cursor_index < next_total_characters && candidate_layout != nullptr &&
        !suggest_window_never_hides_preedit) {
      const int local_cursor_index = cursor_index - total_characters;
      CPoint cursor_pos;
      CRect exclusion_area;
      if (is_vertical) {
        cursor_pos.SetPoint(
            0, layout.character_positions[local_cursor_index].begin);
        exclusion_area.SetRect(
            cursor_pos, CPoint(window_rect.Width(), window_rect.Height()));
      } else {
        cursor_pos.SetPoint(
            layout.character_positions[local_cursor_index].begin,
            layout.line_width);
        exclusion_area.SetRect(
            CPoint(layout.character_positions[local_cursor_index].begin, 0),
            CPoint(window_rect.Width(), window_rect.Height()));
      }
      cursor_pos.Offset(window_rect.left, window_rect.top);
      exclusion_area.OffsetRect(window_rect.left, window_rect.top);
      candidate_layout->InitializeWithPositionAndExcludeRegion(cursor_pos,
                                                               exclusion_area);
    }

    const size_t min_segment_index = segment_indices[total_characters];
    const size_t max_segment_index = segment_indices[next_total_characters - 1];
    for (size_t segment_index = min_segment_index;
         segment_index <= max_segment_index; ++segment_index) {
      const commands::Preedit::Segment &segment =
          preedit.segment(segment_index);
      if ((segment.annotation() & commands::Preedit::Segment::UNDERLINE) !=
              commands::Preedit::Segment::UNDERLINE &&
          (segment.annotation() & commands::Preedit::Segment::HIGHLIGHT) !=
              commands::Preedit::Segment::HIGHLIGHT) {
        continue;
      }
      const int segment_begin =
          std::max(segment_lengths[segment_index].begin, total_characters) -
          total_characters;
      const int segment_end =
          std::min(segment_lengths[segment_index].begin +
                       segment_lengths[segment_index].length,
                   next_total_characters) -
          total_characters;
      if (segment_begin >= segment_end) {
        continue;
      }
      DCHECK_GT(segment_end, segment_begin);
      bool show_segment_gap = true;
      if ((segment_index + 1) >= preedit.segment_size()) {
        // If this segment is the last segment, we do not show the gap.
        show_segment_gap = false;
      } else if (segment_lengths[segment_index].begin +
                     segment_lengths[segment_index].length !=
                 segment_end + total_characters) {
        // If this segment continues to the next line, we do not show the
        // gap.  This behavior is different from the composition window
        // drawn by CUAS.
        show_segment_gap = false;
      }

      // As CUAS does, we make a gap in underline between segments.
      // The length of underline will be shortened 20% of the width of
      // the last character.
      const int begin_pos = layout.character_positions[segment_begin].begin;
      const int end_pos =
          layout.character_positions[segment_end - 1].begin +
          80 * layout.character_positions[segment_end - 1].length /
              (show_segment_gap ? 100 : 80);

      SegmentMarkerLayout marker;
      if ((segment.annotation() & commands::Preedit::Segment::HIGHLIGHT) ==
          commands::Preedit::Segment::HIGHLIGHT) {
        marker.highlighted = true;
      }

      if (is_vertical) {
        // |CPoint(layout.line_width, begin_pos)| is outside of the
        // window.
        marker.from = CPoint(layout.line_width - 1, begin_pos);
        marker.to = CPoint(layout.line_width - 1, end_pos);
      } else {
        // |CPoint(begin_pos, layout.line_width)| is outside of the
        // window.
        marker.from = CPoint(begin_pos, layout.line_width - 1);
        marker.to = CPoint(end_pos, layout.line_width - 1);
      }
      window_layout.marker_layouts.push_back(marker);
    }
    if (composition_window_layouts != nullptr) {
      composition_window_layouts->push_back(window_layout);
    }
    total_line_offset += layout.line_width;
    total_characters += layout.text.size();
  }

  // In this case, suggest window moves to the next line of the preedit text
  // so that suggest window never hides the preedit.
  if (suggest_window_never_hides_preedit &&
      (composition_window_layouts->size() > 0) &&
      (candidate_layout != nullptr)) {
    // Initialize the |exclusion_area| with invalid data. These values will
    // be updated to be valid at the first turn of the next for-loop.
    // For example, |exclusion_area.left| will be updated as follows.
    //   exclusion_area.left = std::min(exclusion_area.left,
    //                             std::numeric_limits<int>::max());
    CRect exclusion_area(
        std::numeric_limits<int>::max(), std::numeric_limits<int>::max(),
        std::numeric_limits<int>::min(), std::numeric_limits<int>::min());

    for (size_t i = 0; i < composition_window_layouts->size(); ++i) {
      const CompositionWindowLayout &layout = composition_window_layouts->at(i);
      CRect text_area_in_screen_coord = layout.text_area;
      text_area_in_screen_coord.OffsetRect(
          layout.window_position_in_screen_coordinate.left,
          layout.window_position_in_screen_coordinate.top);
      exclusion_area.left =
          std::min(exclusion_area.left, text_area_in_screen_coord.left);
      exclusion_area.top =
          std::min(exclusion_area.top, text_area_in_screen_coord.top);
      exclusion_area.right =
          std::max(exclusion_area.right, text_area_in_screen_coord.right);
      exclusion_area.bottom =
          std::max(exclusion_area.bottom, text_area_in_screen_coord.bottom);
    }

    CPoint cursor_pos;
    if (is_vertical) {
      cursor_pos.SetPoint(exclusion_area.left, exclusion_area.top);
    } else {
      cursor_pos.SetPoint(exclusion_area.left, exclusion_area.bottom);
    }
    candidate_layout->InitializeWithPositionAndExcludeRegion(cursor_pos,
                                                             exclusion_area);
  }
  return true;
}

bool LayoutManager::ClientPointToScreen(HWND src_window_handle,
                                        const POINT &src_point,
                                        POINT *dest_point) const {
  if (dest_point == nullptr) {
    return false;
  }

  if (!window_position_->IsWindow(src_window_handle)) {
    DLOG(ERROR) << "Invalid window handle.";
    return false;
  }

  CPoint converted = src_point;
  if (window_position_->ClientToScreen(src_window_handle, &converted) ==
      FALSE) {
    DLOG(ERROR) << "ClientToScreen failed.";
    return false;
  }

  *dest_point = converted;
  return true;
}

bool LayoutManager::ClientRectToScreen(HWND src_window_handle,
                                       const RECT &src_rect,
                                       RECT *dest_rect) const {
  if (dest_rect == nullptr) {
    return false;
  }

  if (!window_position_->IsWindow(src_window_handle)) {
    DLOG(ERROR) << "Invalid window handle.";
    return false;
  }

  CPoint top_left(src_rect.left, src_rect.top);
  if (window_position_->ClientToScreen(src_window_handle, &top_left) == FALSE) {
    DLOG(ERROR) << "ClientToScreen failed.";
    return false;
  }

  CPoint bottom_right(src_rect.right, src_rect.bottom);
  if (window_position_->ClientToScreen(src_window_handle, &bottom_right) ==
      FALSE) {
    DLOG(ERROR) << "ClientToScreen failed.";
    return false;
  }

  dest_rect->left = top_left.x;
  dest_rect->top = top_left.y;
  dest_rect->right = bottom_right.x;
  dest_rect->bottom = bottom_right.y;
  return true;
}

bool LayoutManager::LocalPointToScreen(HWND src_window_handle,
                                       const POINT &src_point,
                                       POINT *dest_point) const {
  if (dest_point == nullptr) {
    return false;
  }

  if (!window_position_->IsWindow(src_window_handle)) {
    DLOG(ERROR) << "Invalid window handle.";
    return false;
  }

  CRect window_rect;
  if (window_position_->GetWindowRect(src_window_handle, &window_rect) ==
      FALSE) {
    return false;
  }

  const CPoint offset(window_rect.TopLeft());
  dest_point->x = src_point.x + offset.x;
  dest_point->y = src_point.y + offset.y;

  return true;
}

bool LayoutManager::LocalRectToScreen(HWND src_window_handle,
                                      const RECT &src_rect,
                                      RECT *dest_rect) const {
  if (dest_rect == nullptr) {
    return false;
  }

  if (!window_position_->IsWindow(src_window_handle)) {
    DLOG(ERROR) << "Invalid window handle.";
    return false;
  }

  CRect window_rect;
  if (window_position_->GetWindowRect(src_window_handle, &window_rect) ==
      FALSE) {
    return false;
  }

  const CPoint offset(window_rect.TopLeft());
  dest_rect->left = src_rect.left + offset.x;
  dest_rect->top = src_rect.top + offset.y;
  dest_rect->right = src_rect.right + offset.x;
  dest_rect->bottom = src_rect.bottom + offset.y;

  return true;
}

bool LayoutManager::GetClientRect(HWND window_handle, RECT *client_rect) const {
  return window_position_->GetClientRect(window_handle, client_rect);
}

double LayoutManager::GetScalingFactor(HWND window_handle) const {
  const double kDefaultValue = 1.0;
  CRect window_rect_in_logical_coord;
  if (!window_position_->GetWindowRect(window_handle,
                                       &window_rect_in_logical_coord)) {
    return kDefaultValue;
  }

  CPoint top_left_in_physical_coord;
  if (!window_position_->LogicalToPhysicalPoint(
          window_handle, window_rect_in_logical_coord.TopLeft(),
          &top_left_in_physical_coord)) {
    return kDefaultValue;
  }
  CPoint bottom_right_in_physical_coord;
  if (!window_position_->LogicalToPhysicalPoint(
          window_handle, window_rect_in_logical_coord.BottomRight(),
          &bottom_right_in_physical_coord)) {
    return kDefaultValue;
  }
  const CRect window_rect_in_physical_coord(top_left_in_physical_coord,
                                            bottom_right_in_physical_coord);

  if (window_rect_in_physical_coord == window_rect_in_logical_coord) {
    // No scaling.
    return 1.0;
  }

  // use larger edge to calculate the scaling factor more accurately.
  if (window_rect_in_logical_coord.Width() >
      window_rect_in_logical_coord.Height()) {
    // Use width.
    if (window_rect_in_physical_coord.Width() <= 0 ||
        window_rect_in_logical_coord.Width() <= 0) {
      return kDefaultValue;
    }
    DCHECK_NE(0, window_rect_in_logical_coord.Width()) << "divided-by-zero";
    return (static_cast<double>(window_rect_in_physical_coord.Width()) /
            window_rect_in_logical_coord.Width());
  } else {
    // Use Height.
    if (window_rect_in_physical_coord.Height() <= 0 ||
        window_rect_in_logical_coord.Height() <= 0) {
      return kDefaultValue;
    }
    DCHECK_NE(0, window_rect_in_logical_coord.Height()) << "divided-by-zero";
    return (static_cast<double>(window_rect_in_physical_coord.Height()) /
            window_rect_in_logical_coord.Height());
  }
}

bool LayoutManager::GetDefaultGuiFont(LOGFONTW *logfont) const {
  return system_preference_->GetDefaultGuiFont(logfont);
}

LayoutManager::WritingDirection LayoutManager::GetWritingDirection(
    const commands::RendererCommand_ApplicationInfo &app_info) {
  // |escapement| is the angle between the escapement vector and the x-axis
  // of the device, in tenths of degrees.  In Windows, (Japanese) vertical
  // writing is usually implemented by setting 2700 to the |escapement|,
  // which means the escapement vector is parallel to |Rot(270 deg) * (1, 0)|
  // in the display coordinate.  Note that |escapement| and |orientation| are
  // different concept.  But we only check |escapement| here for application
  // compatibility.
  // Any |escapement| except for 2700 is treated as horizontal, on the
  // strength of IMEINFO::fdwUICaps has only UI_CAP_2700 as for Mozc.
  // See the document of LOGFONT structure and ImmGetProperty API for
  // details.
  // http://msdn.microsoft.com/en-us/library/dd145037.aspx
  // http://msdn.microsoft.com/en-us/library/dd318567.aspx
  // TODO(yukawa): Support arbitrary angle.
  if (!app_info.has_composition_font() ||
      !app_info.composition_font().has_escapement()) {
    return WRITING_DIRECTION_UNSPECIFIED;
  }

  if (app_info.composition_font().escapement() == 2700) {
    return VERTICAL_WRITING;
  }

  return HORIZONTAL_WRITING;
}

bool LayoutManager::LayoutCandidateWindowForSuggestion(
    const commands::RendererCommand::ApplicationInfo &app_info,
    CandidateWindowLayout *candidate_layout) {
  const int compatibility_mode = GetCompatibilityMode(app_info);

  CandidateWindowLayoutParams params;
  if (!ExtractParams(this, compatibility_mode, app_info, &params)) {
    return false;
  }

  const bool is_suggestion = true;
  if (LayoutCandidateWindowByCompositionTarget(
          params, compatibility_mode, is_suggestion, this, candidate_layout)) {
    DCHECK(candidate_layout->initialized());
    return true;
  }

  if ((compatibility_mode & CAN_USE_CANDIDATE_FORM_FOR_SUGGEST) ==
      CAN_USE_CANDIDATE_FORM_FOR_SUGGEST) {
    if (LayoutCandidateWindowByCandidateForm(params, compatibility_mode, this,
                                             candidate_layout)) {
      DCHECK(candidate_layout->initialized());
      return true;
    }
  }

  if (LayoutCandidateWindowByCaretInfo(params, compatibility_mode, this,
                                       candidate_layout)) {
    DCHECK(candidate_layout->initialized());
    return true;
  }

  if (LayoutCandidateWindowByCompositionForm(params, compatibility_mode, this,
                                             candidate_layout)) {
    DCHECK(candidate_layout->initialized());
    return true;
  }

  if (LayoutCandidateWindowByClientRect(params, compatibility_mode, this,
                                        candidate_layout)) {
    DCHECK(candidate_layout->initialized());
    return true;
  }

  return false;
}

bool LayoutManager::LayoutCandidateWindowForConversion(
    const commands::RendererCommand::ApplicationInfo &app_info,
    CandidateWindowLayout *candidate_layout) {
  const int compatibility_mode = GetCompatibilityMode(app_info);

  CandidateWindowLayoutParams params;
  if (!ExtractParams(this, compatibility_mode, app_info, &params)) {
    return false;
  }

  const bool is_suggestion = false;
  if (LayoutCandidateWindowByCompositionTarget(
          params, compatibility_mode, is_suggestion, this, candidate_layout)) {
    DCHECK(candidate_layout->initialized());
    return true;
  }

  if (LayoutCandidateWindowByCandidateForm(params, compatibility_mode, this,
                                           candidate_layout)) {
    DCHECK(candidate_layout->initialized());
    return true;
  }

  if (LayoutCandidateWindowByCaretInfo(params, compatibility_mode, this,
                                       candidate_layout)) {
    DCHECK(candidate_layout->initialized());
    return true;
  }

  if (LayoutCandidateWindowByCompositionForm(params, compatibility_mode, this,
                                             candidate_layout)) {
    DCHECK(candidate_layout->initialized());
    return true;
  }

  if (LayoutCandidateWindowByClientRect(params, compatibility_mode, this,
                                        candidate_layout)) {
    DCHECK(candidate_layout->initialized());
    return true;
  }

  return false;
}

int LayoutManager::GetCompatibilityMode(
    const commands::RendererCommand_ApplicationInfo &app_info) {
  if (!app_info.has_target_window_handle()) {
    return COMPATIBILITY_MODE_NONE;
  }
  const HWND target_window =
      WinUtil::DecodeWindowHandle(app_info.target_window_handle());

  if (!window_position_->IsWindow(target_window)) {
    return COMPATIBILITY_MODE_NONE;
  }

  std::wstring class_name;
  if (!window_position_->GetWindowClassName(target_window, &class_name)) {
    return COMPATIBILITY_MODE_NONE;
  }

  int mode = COMPATIBILITY_MODE_NONE;
  {{const wchar_t * kUseCandidateFormForSuggest[] = {
        L"Chrome_RenderWidgetHostHWND",
        L"JsTaroCtrl",
        L"MozillaWindowClass",
        L"OperaWindowClass",
        L"QWidget",
    };
  for (size_t i = 0; i < ARRAYSIZE(kUseCandidateFormForSuggest); ++i) {
    if (kUseCandidateFormForSuggest[i] == class_name) {
      mode |= CAN_USE_CANDIDATE_FORM_FOR_SUGGEST;
      break;
    }
  }
}
}  // namespace win32

{
  const wchar_t *kUseLocalCoord[] = {
      L"gdkWindowToplevel",
      L"SunAwtDialog",
      L"SunAwtFrame",
  };
  for (size_t i = 0; i < ARRAYSIZE(kUseLocalCoord); ++i) {
    if (kUseLocalCoord[i] == class_name) {
      mode |= USE_LOCAL_COORD_FOR_CANDIDATE_FORM;
      break;
    }
  }
}

{
  const wchar_t *kIgnoreDefaultCompositionForm[] = {
      L"SunAwtDialog",
      L"SunAwtFrame",
  };
  for (size_t i = 0; i < ARRAYSIZE(kIgnoreDefaultCompositionForm); ++i) {
    if (kIgnoreDefaultCompositionForm[i] == class_name) {
      mode |= IGNORE_DEFAULT_COMPOSITION_FORM;
      break;
    }
  }
}

{
  const wchar_t *kShowInfolistImmediately[] = {
      L"Emacs",
      L"MEADOW",
  };
  for (size_t i = 0; i < ARRAYSIZE(kShowInfolistImmediately); ++i) {
    if (kShowInfolistImmediately[i] == class_name) {
      mode |= SHOW_INFOLIST_IMMEDIATELY;
      break;
    }
  }
}

return mode;
}  // namespace renderer

bool LayoutManager::LayoutIndicatorWindow(
    const commands::RendererCommand_ApplicationInfo &app_info,
    IndicatorWindowLayout *indicator_layout) {
  if (indicator_layout == nullptr) {
    return false;
  }
  indicator_layout->Clear();

  CandidateWindowLayoutParams params;
  if (!ExtractParams(this, GetCompatibilityMode(app_info), app_info, &params)) {
    return false;
  }

  CRect target_rect;
  if (!GetTargetRectForIndicator(params, *this, &target_rect)) {
    return false;
  }

  indicator_layout->is_vertical = IsVerticalWriting(params);
  indicator_layout->window_rect = target_rect;
  return true;
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

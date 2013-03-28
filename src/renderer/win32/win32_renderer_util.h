// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_RENDERER_WIN32_WIN32_RENDERER_UTIL_H_
#define MOZC_RENDERER_WIN32_WIN32_RENDERER_UTIL_H_

#include <Windows.h>
#include <vector>
#include <string>

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "testing/base/public/gunit_prod.h"
// for FRIEND_TEST()

// TODO(yukawa): Use platform independent primitive types.
#ifdef OS_WIN
namespace mozc {

namespace commands {
class RendererCommand;
// We cannot use RendererCommand::ApplicationInfo nor
// RendererCommand::CandidateForm because C++ does not
// allow forward declaration of a nested-type.
class RendererCommand_ApplicationInfo;
class RendererCommand_CandidateForm;
}  // commands

namespace renderer {
namespace win32 {

struct CharacterRange {
  CharacterRange();
  int begin;
  int length;
};

struct LineLayout {
  wstring text;
  int line_length;
  int line_width;
  int line_start_offset;
  vector<CharacterRange> character_positions;
  LineLayout();
};

struct SegmentMarkerLayout {
  POINT from;
  POINT to;
  bool  highlighted;
  SegmentMarkerLayout();
};

struct CompositionWindowLayout {
  RECT window_position_in_screen_coordinate;
  RECT text_area;
  RECT caret_rect;
  POINT base_position;
  LOGFONT log_font;
  wstring text;
  vector<SegmentMarkerLayout> marker_layouts;
  CompositionWindowLayout();
};

// A POD-like class which represents positional information about where the
// candidate window should be displayed.
// Do not inherit from this class. This class is not designed to be
// inheritable.
class CandidateWindowLayout {
 public:
  // Initializes fields with default values keeping |initialized_| false.
  CandidateWindowLayout();

  // Destructor Stub.  Actually do nothing.
  ~CandidateWindowLayout();

  // Returns true if this object is initialized with valid parameter.
  bool initialized() const;

  // Initializes fields with given target position and sets true
  // to |initialized_|.
  void InitializeWithPosition(const POINT &position);

  // Initializes fields with given target position and exclude region and
  // sets true to |initialized_|.
  void InitializeWithPositionAndExcludeRegion(
      const POINT &position, const RECT &exclude_region);

  // Clears fields and sets false to |initialized_|.
  void Clear();

  // Returns target position in screen coordinate.
  const POINT &position() const;

  // Returns exclude region in screen coordinate.  You can call this method
  // when |has_exclude_region| returns true.
  const RECT &exclude_region() const;

  // Returns true if |exclude_region| is available.
  bool has_exclude_region() const;

 private:
  POINT position_;
  RECT exclude_region_;
  bool has_exclude_region_;
  bool initialized_;
};

struct IndicatorWindowLayout {
 public:
  IndicatorWindowLayout();
  void Clear();

  RECT window_rect;
  bool is_vertical;
};

// This interface is designed to hook API calls for unit test.
class SystemPreferenceInterface {
 public:
  virtual ~SystemPreferenceInterface() {}

  // This method can be used to retrieve some kind of default font
  // for GUI rendering.
  // Returns true if succeeds.
  virtual bool GetDefaultGuiFont(LOGFONTW *log_font) = 0;
};

// This class implements SystemPreferenceInterface for unit tests.
class SystemPreferenceFactory {
 public:
  // Returns an instance of WindowPositionEmulator. Caller must delete
  // the instance.
  static SystemPreferenceInterface *CreateMock(const LOGFONTW &gui_font);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SystemPreferenceFactory);
};

// This interface is designed to hook API calls for unit test.
class WorkingAreaInterface {
 public:
  virtual ~WorkingAreaInterface() {}

  virtual bool GetWorkingAreaFromPoint(const POINT &point,
                                       RECT *working_area) = 0;
};

class WorkingAreaFactory {
 public:
  // Returns an instance of WorkingAreaInterface. Caller must delete
  // the instance.
  static WorkingAreaInterface *Create();

  // Returns an instance of WorkingAreaInterface. Caller must delete
  // the instance.
  static WorkingAreaInterface *CreateMock(const RECT &working_area);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WorkingAreaFactory);
};

// This interface is designed to hook API calls for unit test.
class WindowPositionInterface {
 public:
  virtual ~WindowPositionInterface() {}

  // This method wraps API call of LogicalToPhysicalPoint.
  // Returns true if this method can convert the given coordinate into
  // physical space.  We do not declare this method as const method so
  // that a mock class can implement this method in non-const way.
  virtual bool LogicalToPhysicalPoint(
      HWND window_handle, const POINT &logical_coordinate,
      POINT *physical_coordinate) = 0;

  // This method wraps API call of GetWindowRect.
  virtual bool GetWindowRect(HWND window_handle, RECT *rect) = 0;

  // This method wraps API call of GetClientRect.
  virtual bool GetClientRect(HWND window_handle, RECT *rect) = 0;

  // This method wraps API call of ClientToScreen.
  virtual bool ClientToScreen(HWND window_handle, POINT *point) = 0;

  // This method wraps API call of IsWindow.
  virtual bool IsWindow(HWND window_handle) = 0;

  // This method wraps API call of GetAncestor/GA_ROOT.
  virtual HWND GetRootWindow(HWND window_handle) = 0;

  // This method wraps API call of GetClassName.
  virtual bool GetWindowClassName(HWND window_handle, wstring *class_name) = 0;
};

// This class implements WindowPositionInterface and emulates APIs
// with positional information registered by RegisterWindow method.
class WindowPositionEmulator : public WindowPositionInterface {
 public:
  // Returns an instance of WindowPositionEmulator.
  static WindowPositionEmulator *Create();

  // Returns a dummy window handle for this emulator.  You can call methods of
  // WindowPositionInterface with this dummy handle.  You need not to release
  // the returned handle.
  virtual HWND RegisterWindow(
      const wstring &class_name, const RECT &window_rect,
      const POINT &client_area_offset, const SIZE &client_area_size,
      double scale_factor) = 0;

  virtual void SetRoot(HWND child_window, HWND root_window) = 0;
};

enum CompatibilityMode {
  // This flag represents the position can be calculated with default strategy.
  COMPATIBILITY_MODE_NONE = 0,
  // Some applications always keep CandidateForm up-to-date.  When this flag
  // is specified, CandidateForm is taken into account so that the suggest
  // window can be shown at more appropriate position.  Qt-based applications
  // match this category.
  CAN_USE_CANDIDATE_FORM_FOR_SUGGEST = 1,
  // Some applications interpret the coordinate system of CandidateForm as
  // the offset from the upper-left corner of the window as opposed to most
  // of other applications which simply use local coordinate.  When this flag
  // is specified, positional fields in CandidateForm are transformed as if
  // they are the offset from the upper-left corner of the window.  GTK-based
  // applications and Java AWT-based applications match this category.
  USE_LOCAL_COORD_FOR_CANDIDATE_FORM = 2,
  // Some applications such as V2C occasionally create zero-initialized
  // CANDIDATEFORM and maintain it regardless of the actual position of the
  // composition. This compatibility flag represents that it is better to
  // ignore this phantom CANDIDATEFORM.  Currently all the Java AWT-based
  // applications match this category.
  IGNORE_DEFAULT_COMPOSITION_FORM = 4,
  // Some applications such as NTEmacs22 or Meadow3.0 automatically and
  // frequently generates WM_IME_CONTROL/IMC_SETCOMPOSITIONWINDOW messages.
  // In this case, the renderer might not be able to show the InfoList
  // because new rendering messages will come before the InfoList is
  // displayed with the default delay (~500 msec). This compatibility flag
  // represents that we should show the InfoList immediately on such
  // applications. See b/5824433 for details.
  // TODO(horo, yukawa): Perhapes we can improve the rendering algorithm
  //     when the renderer is receiving the same messages successively.
  //     If we can safely ignore such successive messages, this flag
  //     can be removed.
  SHOW_INFOLIST_IMMEDIATELY = 8,
};

class LayoutManager {
 public:
  LayoutManager();
  ~LayoutManager();

  // A special constructor for unit tests.  You can set a mock object which
  // emulates native APIs for unit test.  This class is responsible for
  // deleting the mock objects passed.
  LayoutManager(SystemPreferenceInterface *mock_system_preference,
                WindowPositionInterface *mock_window_position);

  // Calculates layout of composition windows including preferred position of
  // the candidate window and text attributes such as underline or text
  // highlighting in the composition windows.  Returns true when succeeds.
  // This method is thread-safe.
  //  command: The renderer commant to be rendered.
  //  composition_window_layouts: calculated layout for composition windows.
  //  candidate_form: calculated position for the candidate window to be
  //    displayed.
  bool LayoutCompositionWindow(
      const commands::RendererCommand &command,
      vector<CompositionWindowLayout> *composition_window_layouts,
      CandidateWindowLayout* candidate_layout) const;

  // Returns compatibility bits for given target application.
  int GetCompatibilityMode(
      const commands::RendererCommand_ApplicationInfo &app_info);

  // Determines the position where the suggest window should be placed when the
  // composition window is drawn by the application.  This function does not
  // take DPI virtualization into account.  In other words, any positional
  // field in |candidate_form| is calculated in virtualized screen coordinates
  // for the target application window.
  bool LayoutCandidateWindowForSuggestion(
      const commands::RendererCommand_ApplicationInfo &app_info,
      CandidateWindowLayout *candidate_layout);

  // Determines the position where the candidate/predict window should be
  // placed when the composition window is drawn by the application.  This
  // function does not take DPI virtualization into account.  In other words,
  // any positional field in |candidate_form| is calculated in virtualized
  // screen coordinates for the target application window.
  bool LayoutCandidateWindowForConversion(
      const commands::RendererCommand_ApplicationInfo &app_info,
      CandidateWindowLayout *candidate_layout);

  // Converts a virtualized screen coordinate for the DPI-unaware application
  // specified by |window_handle| to the universal screen coordinate for
  // DPI-aware applications.  When the LogicalToPhysicalPoint API is not
  // available in the target platform, this function simply copies |point|
  // into |result|.  If LogicalToPhysicalPoint API fails due to some
  // limitations, this function tries to emulate the result assuming the
  // scaling factor is one dimension and scaling center is (0, 0).
  // See remarks of the following document for details about the limitations.
  // http://msdn.microsoft.com/en-us/library/ms633533.aspx
  // This method is thread-safe.
  void GetPointInPhysicalCoords(
      HWND window_handle, const POINT &point, POINT *result) const;

  // RECT version of GetPointInPhysicalCoords.  This method is thread-safe.
  void GetRectInPhysicalCoords(
      HWND window_handle, const RECT &rect, RECT *result) const;

  // Converts a local coordinate into a logical screen coordinate assuming
  // |src_point| is the relative offset from the top-left of the
  // window specified by the |src_window_handle|.
  // Returns false if fails.
  bool LocalPointToScreen(HWND src_window_handle, const POINT &src_point,
                          POINT *dest_point) const;

  // Converts a local coordinate into a logical screen coordinate assuming
  // |src_point| is the relative offset from the top-left of the
  // window specified by the |src_window_handle|.
  // Returns false if fails.
  bool LocalRectToScreen(HWND src_window_handle, const RECT &src_rect,
                         RECT *dest_rect) const;

  // Converts a local coordinate into a logical screen coordinate assuming
  // |src_point| is the client coorinate in the window specified by
  // |src_window_handle|.
  // Returns false if fails.
  bool ClientRectToScreen(HWND src_window_handle, const RECT &src_rect,
                          RECT *dest_rect) const;

  // Converts a local coordinate into a logical screen coordinate assuming
  // |src_point| is the client coorinate in the window specified by
  // |src_window_handle|.
  // Returns false if fails.
  bool ClientPointToScreen(HWND src_window_handle, const POINT &src_point,
                           POINT *dest_point) const;

  // Returns true if the client rect of the target window specified by
  // |src_window_handle| is retrieved in a logical screen coordinate.
  // Returns false if fails.
  bool GetClientRect(HWND window_handle, RECT *client_rect) const;

  // Returns the scaling factor for DPI Virtualization.
  // Returns 1.0 if any error occurs.
  double GetScalingFactor(HWND window_handle) const;

  // Returns true if the default GUI font is retrieved.
  // Returns false if fails.
  bool GetDefaultGuiFont(LOGFONTW *logfong) const;

  // Represents preferred writing direction, especially for composition string.
  enum WritingDirection {
    // The writing direction is not specified.
    WRITING_DIRECTION_UNSPECIFIED = 0,
    // Horizontal writing is specified.
    HORIZONTAL_WRITING = 1,
    // Vertical writing is specified.
    VERTICAL_WRITING = 2,
  };

  // Returns writing direction.
  static WritingDirection GetWritingDirection(
      const commands::RendererCommand_ApplicationInfo &app_info);

  // Returns true when the target rect is successfully obtained.
  bool LayoutIndicatorWindow(
      const commands::RendererCommand_ApplicationInfo &app_info,
      IndicatorWindowLayout *indicator_layout);

 private:
  FRIEND_TEST(Win32RendererUtilTest, HorizontalProportional);
  FRIEND_TEST(Win32RendererUtilTest, VerticalProportional);
  FRIEND_TEST(Win32RendererUtilTest, HorizontalMonospaced);
  FRIEND_TEST(Win32RendererUtilTest, VerticalMonospaced);
  FRIEND_TEST(Win32RendererUtilTest, HorizontalProportionalCompositeGlyph);
  FRIEND_TEST(Win32RendererUtilTest, VerticalProportionalCompositeGlyph);
  FRIEND_TEST(Win32RendererUtilTest, HorizontalMonospacedCompositeGlyph);
  FRIEND_TEST(Win32RendererUtilTest, VerticalMonospacedCompositeGlyph);

  // Calculates text layout with taking text wrapping into account.  Returns
  // true when succeeds.
  //  font: The font information to be used for calculation.
  //  str: The string to be calculated.  Non-printable characters are not fully
  //    supported.  For example, this function may not work well if |str|
  //    contains CR, LF, HT, and VT.
  //  maximum_line_length: The maximum length which limits the grows of text
  //    for each line.
  //  initial_offset: Represents cursor position where the first character in
  //    the |str| starts from if there is enough space to show the first
  //    character.
  //  LineLayout: Layout information for each split line.
  static bool CalcLayoutWithTextWrapping(
      const LOGFONTW &font,
      const wstring &text,
      int maximum_line_length,
      int initial_offset,
      vector<LineLayout> *line_layouts);

  scoped_ptr<SystemPreferenceInterface> system_preference_;
  scoped_ptr<WindowPositionInterface> window_position_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManager);
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // OS_WIN
#endif  // MOZC_RENDERER_WIN32_WIN32_RENDERER_UTIL_H_

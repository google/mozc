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

#ifndef MOZC_RENDERER_WIN32_WIN32_RENDERER_UTIL_H_
#define MOZC_RENDERER_WIN32_WIN32_RENDERER_UTIL_H_

#include <windows.h>

#include <memory>
#include <string>

#include "protocol/renderer_command.pb.h"

// TODO(yukawa): Use platform independent primitive types.
#ifdef _WIN32
namespace mozc {
namespace renderer {
namespace win32 {

// A POD-like class which represents positional information about where the
// candidate window should be displayed.
// Do not inherit from this class. This class is not designed to be
// inheritable.
class CandidateWindowLayout {
 public:
  // Initializes fields with default values keeping |initialized_| false.
  CandidateWindowLayout() = default;

  // Returns true if this object is initialized with valid parameter.
  bool initialized() const;

  // Initializes fields with given target position and exclude region and
  // sets true to |initialized_|.
  void InitializeWithPositionAndExcludeRegion(const POINT& position,
                                              const RECT& exclude_region);

  // Clears fields and sets false to |initialized_|.
  void Clear();

  // Returns target position in screen coordinate.
  const POINT& position() const;

  // Returns exclude region in screen coordinate.  You can call this method
  // when |has_exclude_region| returns true.
  const RECT& exclude_region() const;

 private:
  POINT position_;
  RECT exclude_region_;
  bool initialized_ = false;
};

struct IndicatorWindowLayout {
 public:
  IndicatorWindowLayout();
  void Clear();

  RECT window_rect;
  bool is_vertical;
};

// Retrieves the working area that contains the specified |point|.
bool GetWorkingAreaFromPoint(const POINT& point, RECT* working_area);

// This interface is designed to hook API calls for unit test.
class WindowPositionInterface {
 public:
  virtual ~WindowPositionInterface() = default;

  // This method wraps API call of LogicalToPhysicalPoint.
  // Returns true if this method can convert the given coordinate into
  // physical space.  We do not declare this method as const method so
  // that a mock class can implement this method in non-const way.
  virtual bool LogicalToPhysicalPoint(HWND window_handle,
                                      const POINT& logical_coordinate,
                                      POINT* physical_coordinate) = 0;

  // This method wraps API call of GetWindowRect.
  virtual bool GetWindowRect(HWND window_handle, RECT* rect) = 0;

  // This method wraps API call of GetClientRect.
  virtual bool GetClientRect(HWND window_handle, RECT* rect) = 0;

  // This method wraps API call of ClientToScreen.
  virtual bool ClientToScreen(HWND window_handle, POINT* point) = 0;

  // This method wraps API call of IsWindow.
  virtual bool IsWindow(HWND window_handle) = 0;

  // This method wraps API call of GetAncestor/GA_ROOT.
  virtual HWND GetRootWindow(HWND window_handle) = 0;
};

// This class implements WindowPositionInterface and emulates APIs
// with positional information registered by RegisterWindow method.
class WindowPositionEmulator : public WindowPositionInterface {
 public:
  // Returns an instance of WindowPositionEmulator.
  static std::unique_ptr<WindowPositionEmulator> Create();

  // Returns a dummy window handle for this emulator.  You can call methods of
  // WindowPositionInterface with this dummy handle.  You need not to release
  // the returned handle.
  virtual HWND RegisterWindow(const RECT& window_rect,
                              const POINT& client_area_offset,
                              const SIZE& client_area_size,
                              double scale_factor) = 0;

  virtual void SetRoot(HWND child_window, HWND root_window) = 0;
};

class LayoutManager {
 public:
  LayoutManager();
  LayoutManager(const LayoutManager&) = delete;
  LayoutManager& operator=(const LayoutManager&) = delete;

  // A special constructor for unit tests.  You can set a mock object which
  // emulates native APIs for unit test.  This class is responsible for
  // deleting the mock objects passed.
  explicit LayoutManager(
      std::unique_ptr<WindowPositionInterface> mock_window_position);

  // Determines the position where the candidate/predict/suggestion window
  // should be placed.  This function does not take DPI virtualization into
  // account.  In other words, any positional field in |app_info| is stored in
  // virtualized screen coordinates for the target application window.
  bool LayoutCandidateWindow(
      const commands::RendererCommand_ApplicationInfo& app_info,
      CandidateWindowLayout* candidate_layout);

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
  void GetPointInPhysicalCoords(HWND window_handle, const POINT& point,
                                POINT* result) const;

  // RECT version of GetPointInPhysicalCoords.  This method is thread-safe.
  void GetRectInPhysicalCoords(HWND window_handle, const RECT& rect,
                               RECT* result) const;

  // Converts a local coordinate into a logical screen coordinate assuming
  // |src_point| is the client coorinate in the window specified by
  // |src_window_handle|.
  // Returns false if fails.
  bool ClientRectToScreen(HWND src_window_handle, const RECT& src_rect,
                          RECT* dest_rect) const;

  // Converts a local coordinate into a logical screen coordinate assuming
  // |src_point| is the client coorinate in the window specified by
  // |src_window_handle|.
  // Returns false if fails.
  bool ClientPointToScreen(HWND src_window_handle, const POINT& src_point,
                           POINT* dest_point) const;

  // Returns true if the client rect of the target window specified by
  // |src_window_handle| is retrieved in a logical screen coordinate.
  // Returns false if fails.
  bool GetClientRect(HWND window_handle, RECT* client_rect) const;

  // Returns the scaling factor for DPI Virtualization.
  // Returns 1.0 if any error occurs.
  double GetScalingFactor(HWND window_handle) const;

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
      const commands::RendererCommand_ApplicationInfo& app_info);

  // Returns true when the target rect is successfully obtained.
  bool LayoutIndicatorWindow(
      const commands::RendererCommand_ApplicationInfo& app_info,
      IndicatorWindowLayout* indicator_layout);

 private:
  std::unique_ptr<WindowPositionInterface> window_position_;
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // _WIN32
#endif  // MOZC_RENDERER_WIN32_WIN32_RENDERER_UTIL_H_

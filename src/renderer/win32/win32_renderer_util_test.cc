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

#include "renderer/win32/win32_renderer_util.h"

#include <atlbase.h>
#include <atltypes.h>
#include <windows.h>

#undef StrCat  // b/328804050

#include <bit>
#include <cstdint>
#include <memory>

#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "testing/gunit.h"

// Following functions should be placed in global namespace for Koenig look-up
// trick used in GTest.
void PrintTo(const POINT &point, ::std::ostream *os) {
  *os << "(" << point.x << ", " << point.y << ")";
}
void PrintTo(const RECT &rect, ::std::ostream *os) {
  *os << "(" << rect.left << ", " << rect.top << ", " << rect.right << ", "
      << rect.bottom << ")";
}

namespace mozc {
namespace renderer {
namespace win32 {
typedef mozc::commands::Output Output;
typedef mozc::commands::RendererCommand RendererCommand;
typedef mozc::commands::RendererCommand_ApplicationInfo ApplicationInfo;
typedef mozc::commands::RendererCommand_ApplicationInfo_InputFrameworkType
    InputFrameworkType;
typedef mozc::commands::RendererCommand_CharacterPosition CharacterPosition;
typedef mozc::commands::RendererCommand_Rectangle Rectangle;

namespace {

// Casts HWND to uint32_t. HWND can be 64 bits, but it's safe to downcast to
// uint32_t as 64-bit Windows still uses 32-bit handles.
// https://learn.microsoft.com/en-us/windows/win32/winprog64/interprocess-communication
inline uint32_t HwndToUint32(HWND hwnd) {
  return static_cast<uint32_t>(std::bit_cast<uintptr_t>(hwnd));
}

#define EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(                     \
    target_x, target_y, exclude_rect_left, exclude_rect_top,        \
    exclude_rect_right, exclude_rect_bottom, layout)                \
  do {                                                              \
    EXPECT_TRUE((layout).initialized());                            \
    EXPECT_EQ(CPoint((target_x), (target_y)), (layout).position()); \
    EXPECT_EQ(CRect((exclude_rect_left), (exclude_rect_top),        \
                    (exclude_rect_right), (exclude_rect_bottom)),   \
              (layout).exclude_region());                           \
  } while (false)

static CRect ToCRect(const RECT &rect) {
  return CRect(rect.left, rect.top, rect.right, rect.bottom);
}

std::unique_ptr<WindowPositionEmulator> CreateWindowEmulator(
    const RECT &window_rect, const POINT &client_area_offset,
    const SIZE &client_area_size, double scale_factor, HWND *hwnd) {
  std::unique_ptr<WindowPositionEmulator> emulator =
      WindowPositionEmulator::Create();
  *hwnd = emulator->RegisterWindow(window_rect, client_area_offset,
                                   client_area_size, scale_factor);
  return emulator;
}

class AppInfoUtil {
 public:
  AppInfoUtil() = delete;
  AppInfoUtil(const AppInfoUtil &) = delete;
  AppInfoUtil &operator=(const AppInfoUtil &) = delete;
  static void SetBasicApplicationInfo(ApplicationInfo *app_info, HWND hwnd,
                                      int visibility,
                                      InputFrameworkType framework_type) {
    app_info->set_ui_visibilities(visibility);
    app_info->set_process_id(1234);
    app_info->set_thread_id(5678);
    app_info->set_target_window_handle(HwndToUint32(hwnd));
    app_info->set_input_framework(framework_type);
  }

  static void SetCompositionTarget(ApplicationInfo *app_info, int position,
                                   int x, int y, uint32_t line_height, int left,
                                   int top, int right, int bottom) {
    CharacterPosition *char_pos = app_info->mutable_composition_target();
    char_pos->set_position(position);
    char_pos->mutable_top_left()->set_x(x);
    char_pos->mutable_top_left()->set_y(y);
    char_pos->set_line_height(line_height);
    Rectangle *area = char_pos->mutable_document_area();
    area->set_left(left);
    area->set_top(top);
    area->set_right(right);
    area->set_bottom(bottom);
  }
};

}  // namespace

TEST(Win32RendererUtilTest, GetPointInPhysicalCoordsTest) {
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(100, 200);
  const CRect kWindowRect(1000, 500, 1116, 750);

  const CPoint kInnerPoint(1100, 600);
  const CPoint kOuterPoint(10, 300);

  // Check DPI scale: 100%
  {
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateWindowEmulator(kWindowRect, kClientOffset,
                                                  kClientSize, 1.0, &hwnd));

    // Conversion from an outer point should be calculated by emulation.
    CPoint dest;
    layout_mgr.GetPointInPhysicalCoords(hwnd, kOuterPoint, &dest);

    // Should be the same position because DPI scaling is 100%.
    EXPECT_EQ(dest, kOuterPoint);

    // Conversion from an inner point should be calculated by API.
    layout_mgr.GetPointInPhysicalCoords(hwnd, kInnerPoint, &dest);

    // Should be the same position because DPI scaling is 100%.
    EXPECT_EQ(dest, kInnerPoint);
  }

  // Check DPI scale: 200%
  {
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateWindowEmulator(kWindowRect, kClientOffset,
                                                  kClientSize, 2.0, &hwnd));

    // Conversion from an outer point should be calculated by emulation.
    CPoint dest;
    layout_mgr.GetPointInPhysicalCoords(hwnd, kOuterPoint, &dest);

    // Should be doubled because DPI scaling is 200%.
    EXPECT_EQ(dest, CPoint(20, 600));

    // Conversion from an inner point should be calculated by API.
    layout_mgr.GetPointInPhysicalCoords(hwnd, kInnerPoint, &dest);

    // Should be doubled because DPI scaling is 200%.
    EXPECT_EQ(dest, CPoint(2200, 1200));
  }
}

TEST(Win32RendererUtilTest, GetRectInPhysicalCoordsTest) {
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(100, 200);
  const CRect kWindowRect(1000, 500, 1116, 750);

  const CRect kInnerRect(1100, 600, 1070, 630);
  const CRect kOuterRect(10, 300, 1110, 630);

  // Check DPI scale: 100%
  {
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateWindowEmulator(kWindowRect, kClientOffset,
                                                  kClientSize, 1.0, &hwnd));

    // Conversion from an outer rectangle should be calculated by emulation.
    CRect dest;
    layout_mgr.GetRectInPhysicalCoords(hwnd, kOuterRect, &dest);

    // Should be the same rectangle because DPI scaling is 100%.
    EXPECT_EQ(dest, kOuterRect);

    // Conversion from an inner rectangle should be calculated by API.
    layout_mgr.GetRectInPhysicalCoords(hwnd, kInnerRect, &dest);

    // Should be the same rectangle because DPI scaling is 100%.
    EXPECT_EQ(dest, kInnerRect);
  }

  // Check DPI scale: 200%
  {
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateWindowEmulator(kWindowRect, kClientOffset,
                                                  kClientSize, 2.0, &hwnd));

    // Conversion from an outer rectangle should be calculated by emulation.
    CRect dest;
    layout_mgr.GetRectInPhysicalCoords(hwnd, kOuterRect, &dest);

    // Should be doubled because DPI scaling is 200%.
    EXPECT_EQ(ToCRect(dest), CRect(20, 600, 2220, 1260));

    // Conversion from an inner rectangle should be calculated by API.
    layout_mgr.GetRectInPhysicalCoords(hwnd, kInnerRect, &dest);

    // Should be doubled because DPI scaling is 200%.
    EXPECT_EQ(ToCRect(dest), CRect(2200, 1200, 2140, 1260));
  }
}

TEST(Win32RendererUtilTest, GetScalingFactorTest) {
  constexpr double kScalingFactor = 1.5;

  {
    const CPoint kClientOffset(0, 0);
    const CSize kClientSize(100, 200);
    const CRect kWindowRect(1000, 500, 1100, 700);
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateWindowEmulator(
        kWindowRect, kClientOffset, kClientSize, kScalingFactor, &hwnd));

    ASSERT_DOUBLE_EQ(kScalingFactor, layout_mgr.GetScalingFactor(hwnd));
  }

  // Zero Width
  {
    const CPoint kClientOffset(0, 0);
    const CSize kClientSize(0, 200);
    const CRect kWindowRect(1000, 500, 1000, 700);

    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateWindowEmulator(
        kWindowRect, kClientOffset, kClientSize, kScalingFactor, &hwnd));

    ASSERT_DOUBLE_EQ(kScalingFactor, layout_mgr.GetScalingFactor(hwnd));
  }

  // Zero Height
  {
    const CPoint kClientOffset(0, 0);
    const CSize kClientSize(100, 0);
    const CRect kWindowRect(1000, 500, 1100, 500);
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateWindowEmulator(
        kWindowRect, kClientOffset, kClientSize, kScalingFactor, &hwnd));

    ASSERT_DOUBLE_EQ(kScalingFactor, layout_mgr.GetScalingFactor(hwnd));
  }

  // Zero Size
  {
    const CPoint kClientOffset(0, 0);
    const CSize kClientSize(0, 0);
    const CRect kWindowRect(1000, 500, 1000, 500);
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateWindowEmulator(
        kWindowRect, kClientOffset, kClientSize, kScalingFactor, &hwnd));

    // If the window size is zero, the result should be fallen back 1.0.
    ASSERT_DOUBLE_EQ(1.0, layout_mgr.GetScalingFactor(hwnd));
  }
}

TEST(Win32RendererUtilTest, WindowPositionEmulatorTest) {
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(100, 200);
  const CRect kWindowRect(1000, 500, 1116, 750);

  const CPoint kInnerPoint(1100, 600);
  const CPoint kOuterPoint(10, 300);
  const CRect kInnerRect(1100, 600, 1070, 630);
  const CRect kOuterRect(10, 300, 1110, 630);

  // Check DPI scale: 100%
  {
    std::unique_ptr<WindowPositionEmulator> emulator(
        WindowPositionEmulator::Create());
    const HWND hwnd =
        emulator->RegisterWindow(kWindowRect, kClientOffset, kClientSize, 1.0);

    CRect rect;
    CPoint point;

    // You cannot pass nullptr to |window_handle|.
    EXPECT_FALSE(emulator->IsWindow(nullptr));
    EXPECT_FALSE(emulator->GetWindowRect(nullptr, &rect));
    EXPECT_FALSE(emulator->GetClientRect(nullptr, &rect));
    EXPECT_FALSE(emulator->ClientToScreen(nullptr, &point));

    EXPECT_TRUE(emulator->GetWindowRect(hwnd, &rect));
    EXPECT_EQ(rect, kWindowRect);

    EXPECT_TRUE(emulator->GetClientRect(hwnd, &rect));
    EXPECT_EQ(ToCRect(rect), CRect(CPoint(0, 0), kClientSize));

    point = CPoint(0, 0);
    EXPECT_TRUE(emulator->ClientToScreen(hwnd, &point));
    EXPECT_EQ(point, kWindowRect.TopLeft() + kClientOffset);
  }

  // Interestingly, the following results are independent of DPI scaling.
  {
    std::unique_ptr<WindowPositionEmulator> emulator(
        WindowPositionEmulator::Create());
    const HWND hwnd =
        emulator->RegisterWindow(kWindowRect, kClientOffset, kClientSize, 10.0);

    CRect rect;
    CPoint point;

    // You cannot pass nullptr to |window_handle|.
    EXPECT_FALSE(emulator->IsWindow(nullptr));
    EXPECT_FALSE(emulator->GetWindowRect(nullptr, &rect));
    EXPECT_FALSE(emulator->GetClientRect(nullptr, &rect));
    EXPECT_FALSE(emulator->ClientToScreen(nullptr, &point));

    EXPECT_TRUE(emulator->GetWindowRect(hwnd, &rect));
    EXPECT_EQ(rect, kWindowRect);

    EXPECT_TRUE(emulator->GetClientRect(hwnd, &rect));
    EXPECT_EQ(ToCRect(rect), CRect(CPoint(0, 0), kClientSize));

    point = CPoint(0, 0);
    EXPECT_TRUE(emulator->ClientToScreen(hwnd, &point));
    EXPECT_EQ(point, kWindowRect.TopLeft() + kClientOffset);
  }
}

// How |LayoutManager| works for TSF Mozc isn't that complicated.
//
// TSF Mozc sends |RendererCommand::Update| only when |composition_target|
// is available, and |composition_target| is sufficient for |LayoutManager| to
// determine all the UI positions.
TEST(Win32RendererUtilTest, TSF_NormalDPI) {
  const CRect kWindowRect(507, 588, 1024, 698);
  const CPoint kClientOffset(10, 12);
  const CSize kClientSize(517, 110);
  constexpr double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateWindowEmulator(
      kWindowRect, kClientOffset, kClientSize, kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(
      &app_info, hwnd,
      ApplicationInfo::ShowCandidateWindow | ApplicationInfo::ShowSuggestWindow,
      ApplicationInfo::TSF);
  AppInfoUtil::SetCompositionTarget(&app_info, 0, 86, 122, 20, 83, 119, 109,
                                    525);

  IndicatorWindowLayout indicator_layout;
  EXPECT_TRUE(layout_mgr.LayoutIndicatorWindow(app_info, &indicator_layout));
  EXPECT_EQ(ToCRect(indicator_layout.window_rect), CRect(86, 122, 87, 142));
  EXPECT_FALSE(indicator_layout.is_vertical);

  CandidateWindowLayout candidate_window_layout;
  EXPECT_TRUE(
      layout_mgr.LayoutCandidateWindow(app_info, &candidate_window_layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(86, 142, 86, 122, 87, 142,
                                         candidate_window_layout);
}

// How |LayoutManager| works for TSF Mozc isn't that complicated.
//
// TSF Mozc sends |RendererCommand::Update| only when |composition_target|
// is available, and |composition_target| is sufficient for |LayoutManager| to
// determine all the UI positions.
TEST(Win32RendererUtilTest, TSF_HighDPI) {
  const CRect kWindowRect(507, 588, 1024, 698);
  const CPoint kClientOffset(10, 12);
  const CSize kClientSize(517, 110);
  constexpr double kScaleFactor = 2.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateWindowEmulator(
      kWindowRect, kClientOffset, kClientSize, kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(
      &app_info, hwnd,
      ApplicationInfo::ShowCandidateWindow | ApplicationInfo::ShowSuggestWindow,
      ApplicationInfo::TSF);
  AppInfoUtil::SetCompositionTarget(&app_info, 0, 86, 122, 20, 83, 119, 109,
                                    525);

  IndicatorWindowLayout indicator_layout;
  EXPECT_TRUE(layout_mgr.LayoutIndicatorWindow(app_info, &indicator_layout));
  EXPECT_EQ(ToCRect(indicator_layout.window_rect), CRect(172, 244, 174, 284));
  EXPECT_FALSE(indicator_layout.is_vertical);

  CandidateWindowLayout candidate_window_layout;
  EXPECT_TRUE(
      layout_mgr.LayoutCandidateWindow(app_info, &candidate_window_layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(172, 284, 172, 244, 174, 284,
                                         candidate_window_layout);
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

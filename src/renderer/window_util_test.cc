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

// Unit tests for WindowUtil class.

#include "renderer/window_util.h"

#include "base/coordinates.h"
#include "testing/gunit.h"

namespace mozc {
namespace renderer {

class WindowUtilTest : public testing::Test {
 public:
  WindowUtilTest()
      : working_area_(0, 0, 200, 100),
        window_size_(10, 20),
        zero_point_offset_(1, -2) {}

  void VerifyMainWindowWithPreeditRect(int preedit_left, int preedit_top,
                                       int preedit_width, int preedit_height,
                                       int expected_left, int expected_top,
                                       const char* message) {
    Rect preedit_rect(preedit_left, preedit_top, preedit_width, preedit_height);
    Rect result = WindowUtil::GetWindowRectForMainWindowFromPreeditRect(
        preedit_rect, window_size_, zero_point_offset_, working_area_);
    EXPECT_EQ(result.Left(), expected_left) << message;
    EXPECT_EQ(result.Top(), expected_top) << message;
  }

  void VerifyMainWindowWithTargetPoint(int target_point_x, int target_point_y,
                                       int expected_left, int expected_top,
                                       const char* message) {
    Point target_point(target_point_x, target_point_y);
    Rect result = WindowUtil::GetWindowRectForMainWindowFromTargetPoint(
        target_point, window_size_, zero_point_offset_, working_area_);
    EXPECT_EQ(result.Left(), expected_left) << message;
    EXPECT_EQ(result.Top(), expected_top) << message;
  }

  void VerifyMainWindowWithTargetPointAndPreeditHorizontal(
      int target_point_x, int target_point_y, int preedit_left, int preedit_top,
      int preedit_width, int preedit_height, int expected_left,
      int expected_top, const char* message) {
    Point target_point(target_point_x, target_point_y);
    Rect preedit_rect(preedit_left, preedit_top, preedit_width, preedit_height);
    Rect result =
        WindowUtil::GetWindowRectForMainWindowFromTargetPointAndPreedit(
            target_point, preedit_rect, window_size_, zero_point_offset_,
            working_area_, false);
    EXPECT_EQ(result.Left(), expected_left) << message;
    EXPECT_EQ(result.Top(), expected_top) << message;
  }

  void VerifyMainWindowWithTargetPointAndPreeditVertical(
      int target_point_x, int target_point_y, int preedit_left, int preedit_top,
      int preedit_width, int preedit_height, int expected_left,
      int expected_top, const char* message) {
    Point target_point(target_point_x, target_point_y);
    Rect preedit_rect(preedit_left, preedit_top, preedit_width, preedit_height);
    Rect result =
        WindowUtil::GetWindowRectForMainWindowFromTargetPointAndPreedit(
            target_point, preedit_rect, window_size_, zero_point_offset_,
            working_area_, true);
    EXPECT_EQ(result.Left(), expected_left) << message;
    EXPECT_EQ(result.Top(), expected_top) << message;
  }

  void VerifyCascadingWindow(int row_left, int row_top, int row_width,
                             int row_height, int expected_left,
                             int expected_top, const char* message) {
    Rect selected_row(row_left, row_top, row_width, row_height);
    Rect result = WindowUtil::GetWindowRectForCascadingWindow(
        selected_row, window_size_, zero_point_offset_, working_area_);
    EXPECT_EQ(result.Left(), expected_left) << message;
    EXPECT_EQ(result.Top(), expected_top) << message;
  }

  void VerifyInfolistWindow(int infolist_width, int infolist_height,
                            int candidate_left, int candidate_top,
                            int candidate_width, int candidate_height,
                            int expected_left, int expected_top,
                            const char* message) {
    Size window_size(infolist_width, infolist_height);
    Rect candidate_rect(candidate_left, candidate_top, candidate_width,
                        candidate_height);
    Rect result = WindowUtil::GetWindowRectForInfolistWindow(
        window_size, candidate_rect, working_area_);
    EXPECT_EQ(result.Left(), expected_left) << message;
    EXPECT_EQ(result.Top(), expected_top) << message;
  }

 private:
  Rect working_area_;
  Size window_size_;
  Point zero_point_offset_;
  Rect selected_row_;
};

TEST_F(WindowUtilTest, MainWindow) {
  VerifyMainWindowWithPreeditRect(50, 50, 20, 5, 49, 57,
                                  "Preedit is in the middle of the window");
  VerifyMainWindowWithPreeditRect(198, 50, 20, 5, 190, 57, "On the right edge");
  VerifyMainWindowWithPreeditRect(-5, 50, 20, 5, 0, 57, "On the left edge");
  // If the candidate window across the bottom edge, it appears above
  // the preedit.
  VerifyMainWindowWithPreeditRect(50, 92, 20, 5, 49, 70, "On the bottom edge");
  VerifyMainWindowWithPreeditRect(50, 110, 20, 5, 49, 80,
                                  "Under the bottom edge");
  VerifyMainWindowWithPreeditRect(50, -10, 20, 5, 49, 0, "On the top edge");

  VerifyMainWindowWithTargetPoint(50, 55, 49, 57,
                                  "Preedit is in the middle of the window");
  VerifyMainWindowWithTargetPoint(198, 55, 190, 57, "On the right edge");
  VerifyMainWindowWithTargetPoint(-5, 55, 0, 57, "On the left edge");
  // If the candidate window across the bottom edge, it appears above
  // the preedit.
  VerifyMainWindowWithTargetPoint(50, 97, 49, 80, "On the bottom edge");
  VerifyMainWindowWithTargetPoint(50, 115, 49, 80, "Under the bottom edge");
  VerifyMainWindowWithTargetPoint(50, -5, 49, 0, "On the top edge");

  VerifyMainWindowWithTargetPointAndPreeditHorizontal(
      50, 55, 50, 50, 20, 5, 49, 57, "Preedit is in the middle of the window");
  VerifyMainWindowWithTargetPointAndPreeditHorizontal(
      198, 55, 198, 50, 20, 5, 190, 57, "On the right edge");
  VerifyMainWindowWithTargetPointAndPreeditHorizontal(50, -5, 50, -10, 20, 5,
                                                      49, 0, "On the top edge");
  VerifyMainWindowWithTargetPointAndPreeditHorizontal(
      50, 55, 0, 50, 100, 5, 49, 57,
      "Preedit width is the same to client area");
  // If the candidate window across the bottom edge, it appears above
  // the preedit.
  VerifyMainWindowWithTargetPointAndPreeditHorizontal(50, 97, 50, 92, 20, 5, 49,
                                                      70, "On the bottom edge");
  VerifyMainWindowWithTargetPointAndPreeditHorizontal(
      50, 115, 50, 110, 20, 5, 49, 80, "Under the bottom edge");
  VerifyMainWindowWithTargetPointAndPreeditHorizontal(50, -5, 50, -10, 20, 5,
                                                      49, 0, "On the top edge");

  VerifyMainWindowWithTargetPointAndPreeditVertical(
      50, 55, 50, 50, 20, 5, 70, 55, "Preedit is in the middle of the window");
  VerifyMainWindowWithTargetPointAndPreeditVertical(50, 198, 50, 198, 5, 20, 55,
                                                    80, "On the bottom edge");
  VerifyMainWindowWithTargetPointAndPreeditVertical(-50, 50, -50, 50, 5, 20, 0,
                                                    50, "On the left edge");
  VerifyMainWindowWithTargetPointAndPreeditVertical(
      50, 55, 50, 0, 20, 100, 70, 55,
      "Preedit height is the same to client area");
  // If the candidate window across the right edge, it appears in the left of
  // the preedit.
  VerifyMainWindowWithTargetPointAndPreeditVertical(
      192, 50, 192, 50, 5, 20, 182, 50, "On the right edge");
  VerifyMainWindowWithTargetPointAndPreeditVertical(
      215, 50, 210, 50, 5, 20, 190, 50, "Under the right edge");
  VerifyMainWindowWithTargetPointAndPreeditVertical(-5, 50, -10, 50, 5, 20, 0,
                                                    50, "On the left edge");
}

TEST_F(WindowUtilTest, CascadingWindow) {
  VerifyCascadingWindow(50, 50, 20, 5, 69, 52,
                        "Selected row is in the middle of the window");
  // If the cascading window across the right edge, it appears the
  // left side of the main window.
  VerifyCascadingWindow(178, 50, 20, 5, 169, 52, "On the right edge");
  VerifyCascadingWindow(-30, 50, 20, 5, 0, 52, "On the left edge");
  VerifyCascadingWindow(50, 92, 20, 5, 69, 80, "On the bottom edge");
  VerifyCascadingWindow(50, -20, 20, 5, 69, 0, "On the top edge");
}

TEST_F(WindowUtilTest, InfolistWindow) {
  VerifyInfolistWindow(10, 20, 20, 30, 11, 12, 31, 30,
                       "Right of the candidate window");
  VerifyInfolistWindow(10, 10, 160, 30, 40, 12, 150, 30,
                       "Left of the candidate window");
  VerifyInfolistWindow(10, 20, 20, 85, 11, 12, 31, 80, "On the bottom edge");
}

TEST_F(WindowUtilTest, MonitorErrors) {
  // Error! monitor doesn't have width nor height.
  Rect working_area(0, 0, 0, 0);
  Size window_size(10, 20);
  Point zero_point_offset(1, -2);
  Rect preedit_rect(50, 50, 20, 5);
  Point target_point(preedit_rect.Left(), preedit_rect.Bottom());

  Rect result = WindowUtil::GetWindowRectForMainWindowFromPreeditRect(
      preedit_rect, window_size, zero_point_offset, working_area);
  // It doesn't apply edge across processing.
  EXPECT_EQ(result.Left(), 49);
  EXPECT_EQ(result.Top(), 57);

  result = WindowUtil::GetWindowRectForMainWindowFromTargetPoint(
      target_point, window_size, zero_point_offset, working_area);
  // It doesn't apply edge across processing.
  EXPECT_EQ(result.Left(), 49);
  EXPECT_EQ(result.Top(), 57);

  // Same as cascading window.
  result = WindowUtil::GetWindowRectForCascadingWindow(
      preedit_rect, window_size, zero_point_offset, working_area);
  EXPECT_EQ(result.Left(), 69);
  EXPECT_EQ(result.Top(), 52);

  // Same as infolist window.
  Rect candidate_rect(50, 32, 20, 5);
  result = WindowUtil::GetWindowRectForInfolistWindow(
      window_size, candidate_rect, working_area);
  EXPECT_EQ(result.Left(), 70);
  EXPECT_EQ(result.Top(), 32);
}

}  // namespace renderer
}  // namespace mozc

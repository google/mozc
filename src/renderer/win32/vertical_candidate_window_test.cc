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

#include <wil/resource.h>
#include <windows.h>

#include <algorithm>

#include "base/coordinates.h"
#include "client/client_interface.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "renderer/win32/candidate_window.h"
#include "renderer/win32/text_renderer.h"
#include "testing/gunit.h"

namespace mozc::renderer::win32 {
namespace {

commands::CandidateWindow MakeCandidates() {
  commands::CandidateWindow candidates;
  candidates.set_category(commands::CONVERSION);
  candidates.set_size(20);
  candidates.set_focused_index(0);
  candidates.mutable_footer()->set_index_visible(true);
  for (int i = 0; i < 5; ++i) {
    auto* candidate = candidates.add_candidate();
    candidate->set_index(i);
    candidate->set_id(i);
    candidate->set_value("日本語入力の候補");
    candidate->mutable_annotation()->set_shortcut("1");
  }
  return candidates;
}

TEST(VerticalCandidateWindowTest, ReflowsAndRestoresHorizontalLayout) {
  ASSERT_NE(TextRenderer::CreateVertical(96), nullptr);
  CandidateWindow window;
  const auto candidates = MakeCandidates();
  window.UpdateLayout(candidates);
  const Size horizontal = window.GetLayoutSize();
  const Rect horizontal_first = window.GetFirstRowInClientCord();
  const Rect horizontal_text = window.GetCandidateColumnInClientCord();

  window.UpdateLayout(candidates, true);
  const Size vertical = window.GetLayoutSize();
  const Rect first = window.GetFirstRowInClientCord();
  const Rect text = window.GetCandidateColumnInClientCord();
  // A row becomes a column on the right, with shortcut above candidate text.
  EXPECT_GT(first.Height(), first.Width());
  EXPECT_GT(first.Left(), vertical.width / 2);
  EXPECT_GT(text.Top(), first.Top());
  EXPECT_LE(first.Right(), vertical.width);
  EXPECT_LE(first.Bottom(), vertical.height);

  window.UpdateLayout(candidates, false);
  EXPECT_EQ(window.GetLayoutSize().width, horizontal.width);
  EXPECT_EQ(window.GetLayoutSize().height, horizontal.height);
  const Rect restored_first = window.GetFirstRowInClientCord();
  EXPECT_EQ(restored_first.Left(), horizontal_first.Left());
  EXPECT_EQ(restored_first.Top(), horizontal_first.Top());
  EXPECT_EQ(restored_first.Width(), horizontal_first.Width());
  EXPECT_EQ(restored_first.Height(), horizontal_first.Height());
  const Rect restored_text = window.GetCandidateColumnInClientCord();
  EXPECT_EQ(restored_text.Left(), horizontal_text.Left());
  EXPECT_EQ(restored_text.Top(), horizontal_text.Top());
  EXPECT_EQ(restored_text.Width(), horizontal_text.Width());
  EXPECT_EQ(restored_text.Height(), horizontal_text.Height());
}

TEST(VerticalCandidateWindowTest, FontMeasurementsFollowInlineDirection) {
  auto renderer = TextRenderer::CreateVertical(96);
  ASSERT_NE(renderer, nullptr);
  const Size one =
      renderer->MeasureString(TextRenderer::FONTSET_CANDIDATE, L"日");
  const Size many =
      renderer->MeasureString(TextRenderer::FONTSET_CANDIDATE, L"日日日日日日");
  EXPECT_GT(one.height, 0);
  EXPECT_GT(many.width, one.width * 4);
  EXPECT_NEAR(many.height, one.height, 1);

  renderer->OnDpiChanged(192);
  const Size high_dpi =
      renderer->MeasureString(TextRenderer::FONTSET_CANDIDATE, L"日日日日日日");
  EXPECT_GT(high_dpi.width, many.width);
  EXPECT_GT(high_dpi.height, many.height);
}

TEST(VerticalCandidateWindowTest, MouseSelectsColumnsFromRightToLeft) {
  class CommandRecorder : public client::SendCommandInterface {
   public:
    bool SendCommand(const commands::SessionCommand& value,
                     commands::Output*) override {
      command = value;
      return true;
    }
    commands::SessionCommand command;
  } recorder;
  ASSERT_NE(TextRenderer::CreateVertical(96), nullptr);
  CandidateWindow window;
  window.SetSendCommandInterface(&recorder);
  window.UpdateLayout(MakeCandidates(), true);
  const Rect first = window.GetFirstRowInClientCord();
  window.OnLButtonUp(0, CPoint(first.Left() + first.Width() / 2,
                               first.Top() + first.Height() / 2));
  EXPECT_EQ(recorder.command.type(),
            commands::SessionCommand::SELECT_CANDIDATE);
  EXPECT_EQ(recorder.command.id(), 0);
  window.OnLButtonUp(0, CPoint(first.Left() - first.Width() / 2,
                               first.Top() + first.Height() / 2));
  EXPECT_EQ(recorder.command.id(), 1);
}

TEST(VerticalCandidateWindowTest, RendersGlyphsDownward) {
  auto renderer = TextRenderer::CreateVertical(96);
  ASSERT_NE(renderer, nullptr);
  wil::unique_hdc dc(::CreateCompatibleDC(nullptr));
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = 96;
  info.bmiHeader.biHeight = -256;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* pixels = nullptr;
  wil::unique_hbitmap bitmap(
      ::CreateDIBSection(dc.get(), &info, DIB_RGB_COLORS, &pixels, nullptr, 0));
  ASSERT_TRUE(bitmap.is_valid());
  const auto old_bitmap = wil::SelectObject(dc.get(), bitmap.get());
  ::PatBlt(dc.get(), 0, 0, 96, 256, WHITENESS);
  renderer->RenderText(dc.get(), L"日本語入力", Rect(0, 0, 96, 256),
                       TextRenderer::FONTSET_CANDIDATE);
  ::GdiFlush();
  int left = 96, top = 256, right = -1, bottom = -1;
  const auto* data = static_cast<const DWORD*>(pixels);
  for (int y = 0; y < 256; ++y) {
    for (int x = 0; x < 96; ++x) {
      if ((data[y * 96 + x] & 0x00ffffff) != 0x00ffffff) {
        left = (std::min)(left, x);
        right = (std::max)(right, x);
        top = (std::min)(top, y);
        bottom = (std::max)(bottom, y);
      }
    }
  }
  ASSERT_GE(right, left);
  EXPECT_GT(bottom - top, (right - left) * 2);
}

}  // namespace
}  // namespace mozc::renderer::win32

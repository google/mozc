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

#include "renderer/renderer_style_handler.h"

#include "protocol/renderer_style.pb.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace renderer {

TEST(RendererStyleHandlerTest, SetAndGetRendererStyleTest1) {
  RendererStyle style;
  style.set_window_border(99);
  EXPECT_EQ(true, RendererStyleHandler::SetRendererStyle(style));
  RendererStyle style2;
  EXPECT_NE(99, style2.window_border());
  EXPECT_EQ(true, RendererStyleHandler::GetRendererStyle(&style2));
  EXPECT_EQ(99, style2.window_border());
}
TEST(RendererStyleHandlerTest, GetDefaultRendererStyleTest) {
  RendererStyle style;
  RendererStyleHandler::GetDefaultRendererStyle(&style);
  EXPECT_EQ(true, style.has_window_border());
}
}  // namespace renderer
}  // namespace mozc

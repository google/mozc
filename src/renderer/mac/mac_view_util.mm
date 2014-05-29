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

#include "base/coordinates.h"
#include "renderer/mac/mac_view_util.h"
#include "renderer/renderer_style.pb.h"

namespace mozc {
namespace renderer {
namespace mac {

NSPoint MacViewUtil::ToNSPoint(const mozc::Point &point) {
  return NSMakePoint(point.x, point.y);
}

mozc::Point MacViewUtil::ToPoint(const NSPoint &nspoint) {
  return mozc::Point(nspoint.x, nspoint.y);
}

NSSize MacViewUtil::ToNSSize(const mozc::Size &size) {
  return NSMakeSize(size.width, size.height);
}

mozc::Size MacViewUtil::ToSize(const NSSize &nssize) {
  return mozc::Size(nssize.width, nssize.height);
}

NSRect MacViewUtil::ToNSRect(const mozc::Rect &rect) {
  return NSMakeRect(rect.origin.x, rect.origin.y,
                    rect.size.width, rect.size.height);
}

mozc::Rect MacViewUtil::ToRect(const NSRect &nsrect) {
  return mozc::Rect(ToPoint(nsrect.origin), ToSize(nsrect.size));
}

NSSize MacViewUtil::applyTheme(const NSSize &size,
                               const RendererStyle::TextStyle &style) {
  return NSMakeSize(size.width + style.left_padding() + style.right_padding(),
                    size.height);
}

NSColor *MacViewUtil::MacViewUtil::ToNSColor(
    const mozc::renderer::RendererStyle::RGBAColor &color) {
  return [NSColor colorWithCalibratedRed:color.r() / 255.0
                                   green:color.g() / 255.0
                                    blue:color.b() / 255.0
                                   alpha:color.a()];
}

NSAttributedString *MacViewUtil::ToNSAttributedString(const string &str,
    const RendererStyle::TextStyle &style) {
  NSString *nsstr = [NSString stringWithUTF8String:str.c_str()];
  NSFont *font;
  if (style.has_font_name()) {
    font = [NSFont
          fontWithName:[NSString stringWithUTF8String:style.font_name().c_str()]
          size:style.font_size()];
  } else {
    font = [NSFont messageFontOfSize:style.font_size()];
  }
  NSDictionary *attr;
  if (style.has_foreground_color()) {
    attr = [NSDictionary dictionaryWithObjectsAndKeys:font,
            NSFontAttributeName,
            ToNSColor(style.foreground_color()),
            NSForegroundColorAttributeName,
            nil];
  } else {
    attr = [NSDictionary dictionaryWithObjectsAndKeys:font,
            NSFontAttributeName,
            nil];
  }
  return [[[NSAttributedString alloc] initWithString:nsstr attributes:attr]
          autorelease];
}
}  // namespace mozc::renderer::mac
}  // namespace mozc::renderer
}  // namespace mozc


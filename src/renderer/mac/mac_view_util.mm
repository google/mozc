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

#include "renderer/mac/mac_view_util.h"
#include <cmath>
#include "base/coordinates.h"
#include "protocol/renderer_style.pb.h"

namespace mozc {
namespace renderer {
namespace mac {

namespace {
// Keep measurement and painting on the same TextKit 1 layout path. The view
// exists only as a layout-orientation provider; it is never attached to a window.
NSTextView *VerticalTextView(NSAttributedString *text) {
  // Candidate strings are immutable. Reuse their native layouts for measuring
  // and drawing; keep the cache bounded across changing candidate pages.
  static NSCache<NSAttributedString *, NSTextView *> *cache = [] {
    NSCache<NSAttributedString *, NSTextView *> *result = [[NSCache alloc] init];
    [result setCountLimit:128];
    return result;
  }();
  NSTextView *cached = [cache objectForKey:text];
  if (cached) {
    return cached;
  }
  const NSSize containerSize = NSMakeSize(1000000, 1000000);
  NSTextView *view = [[NSTextView alloc]
      initWithFrame:NSMakeRect(0, 0, containerSize.width, containerSize.height)];
  // Accessing layoutManager selects the TextKit 1 compatibility path on newer
  // AppKit versions before configuring vertical layout.
  (void)[view layoutManager];
  [view setLayoutOrientation:NSTextLayoutOrientationVertical];
  [view setTextContainerInset:NSZeroSize];
  [view setHorizontallyResizable:NO];
  [view setVerticallyResizable:NO];
  NSTextContainer *container = [view textContainer];
  [container setWidthTracksTextView:NO];
  [container setHeightTracksTextView:NO];
  [container setContainerSize:containerSize];
  [container setLineFragmentPadding:0];
  NSMutableAttributedString *verticalText = [text mutableCopy];
  [verticalText addAttribute:NSVerticalGlyphFormAttributeName
                       value:@1
                       range:NSMakeRange(0, [verticalText length])];
  [[view textStorage] setAttributedString:verticalText];
  [[view layoutManager] ensureLayoutForTextContainer:container];
  [cache setObject:view forKey:[text copy]];
  return view;
}

NSRect VerticalTextBounds(NSTextView *view) {
  NSLayoutManager *manager = [view layoutManager];
  NSTextContainer *container = [view textContainer];
  const NSRange glyphs = [manager glyphRangeForTextContainer:container];
  return NSUnionRect([manager usedRectForTextContainer:container],
                     [manager boundingRectForGlyphRange:glyphs inTextContainer:container]);
}
}  // namespace

NSSize MacViewUtil::TextSize(NSAttributedString *text, bool vertical) {
  if (!vertical) {
    return [text size];
  }
  if ([text length] == 0) {
    return NSZeroSize;
  }
  NSTextView *view = VerticalTextView(text);
  const NSRect used = VerticalTextBounds(view);
  // TextKit lays out in container coordinates even for vertical glyph forms.
  // Its layoutOrientation contract requires the drawing coordinate system to
  // be rotated; swap the measured axes to match that transform.
  return NSMakeSize(std::ceil(used.size.height), std::ceil(used.size.width));
}

void MacViewUtil::DrawText(NSAttributedString *text, NSRect rect, bool vertical) {
  if ([text length] == 0) {
    return;
  }
  if (!vertical) {
    [text drawAtPoint:rect.origin];
    return;
  }
  NSTextView *view = VerticalTextView(text);
  NSLayoutManager *manager = [view layoutManager];
  const NSRect used = VerticalTextBounds(view);
  const NSRange glyphs = [manager glyphRangeForTextContainer:[view textContainer]];
  [NSGraphicsContext saveGraphicsState];
  NSRectClip(rect);
  NSAffineTransform *transform = [NSAffineTransform transform];
  [transform translateXBy:NSMaxX(rect) yBy:NSMinY(rect)];
  [transform rotateByDegrees:90];
  [transform concat];
  [manager drawGlyphsForGlyphRange:glyphs atPoint:NSMakePoint(-used.origin.x, -used.origin.y)];
  [NSGraphicsContext restoreGraphicsState];
}

NSPoint MacViewUtil::ToNSPoint(const mozc::Point &point) { return NSMakePoint(point.x, point.y); }

mozc::Point MacViewUtil::ToPoint(const NSPoint &nspoint) {
  return mozc::Point(nspoint.x, nspoint.y);
}

NSSize MacViewUtil::ToNSSize(const mozc::Size &size) { return NSMakeSize(size.width, size.height); }

mozc::Size MacViewUtil::ToSize(const NSSize &nssize) {
  return mozc::Size(nssize.width, nssize.height);
}

NSRect MacViewUtil::ToNSRect(const mozc::Rect &rect) {
  return NSMakeRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
}

mozc::Rect MacViewUtil::ToRect(const NSRect &nsrect) {
  return mozc::Rect(ToPoint(nsrect.origin), ToSize(nsrect.size));
}

NSSize MacViewUtil::applyTheme(const NSSize &size, const RendererStyle::TextStyle &style) {
  return NSMakeSize(size.width + style.left_padding() + style.right_padding(), size.height);
}

NSColor *MacViewUtil::MacViewUtil::ToNSColor(
    const mozc::renderer::RendererStyle::RGBAColor &color) {
  return [NSColor colorWithCalibratedRed:color.r() / 255.0
                                   green:color.g() / 255.0
                                    blue:color.b() / 255.0
                                   alpha:color.a()];
}

NSAttributedString *MacViewUtil::ToNSAttributedString(const std::string &str,
                                                      const RendererStyle::TextStyle &style) {
  NSString *nsstr = [NSString stringWithUTF8String:str.c_str()];
  NSFont *font;
  if (style.has_font_name()) {
    font = [NSFont fontWithName:[NSString stringWithUTF8String:style.font_name().c_str()]
                           size:style.font_size()];
  } else {
    font = [NSFont messageFontOfSize:style.font_size()];
  }
  NSDictionary *attr;
  if (style.has_foreground_color()) {
    attr = [NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName,
                                                      ToNSColor(style.foreground_color()),
                                                      NSForegroundColorAttributeName, nil];
  } else {
    attr = [NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, nil];
  }
  return [[NSAttributedString alloc] initWithString:nsstr attributes:attr];
}
}  // namespace mac
}  // namespace renderer
}  // namespace mozc

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

#ifndef MOZC_RENDERER_MAC_MAC_VIEW_UTIL_H_
#define MOZC_RENDERER_MAC_MAC_VIEW_UTIL_H_

#import <Cocoa/Cocoa.h>
#include "base/coordinates.h"
#include "renderer/renderer_style.pb.h"

namespace mozc {
namespace renderer {
namespace mac {

// this is pure static class
class MacViewUtil {
 public:
  static NSColor *ToNSColor(
      const mozc::renderer::RendererStyle::RGBAColor &color);

  static NSAttributedString *ToNSAttributedString(const string &str,
      const mozc::renderer::RendererStyle::TextStyle &style);

  static NSPoint ToNSPoint(const mozc::Point &point);

  static mozc::Point ToPoint(const NSPoint &nspoint);

  static NSSize ToNSSize(const mozc::Size &size);

  static mozc::Size ToSize(const NSSize &nssize);

  static NSRect ToNSRect(const mozc::Rect &rect);

  static mozc::Rect ToRect(const NSRect &nsrect);

  static NSSize applyTheme(const NSSize &size,
                           const RendererStyle::TextStyle &style);

  // Do not allow instantiation
 private:
  MacViewUtil() {}
  virtual ~MacViewUtil() {}
};

}  // namespace mozc::renderer::mac
}  // namespace mozc::renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_MAC_MAC_VIEW_UTIL_H_

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

#include <set>

#import "renderer/mac/InfolistView.h"

#include "protocol/commands.pb.h"
#include "protocol/renderer_style.pb.h"
#include "base/logging.h"
#include "client/client_interface.h"
#include "renderer/mac/mac_view_util.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/table_layout.h"

using mozc::client::SendCommandInterface;
using mozc::commands::Candidates;
using mozc::commands::Output;
using mozc::commands::SessionCommand;
using mozc::commands::Information;
using mozc::commands::InformationList;
using mozc::renderer::RendererStyle;
using mozc::renderer::RendererStyleHandler;
using mozc::renderer::mac::MacViewUtil;

// Private method declarations.
@interface InfolistView ()
// Draw the |row|-th row and return the height of it.
// If draw_flag is true, it does not draw but only calculate the size.
- (CGFloat)drawRow:(int)row ypos:(CGFloat)ypos draw_flag:(bool)draw_flag;

// Draw view and return the size of it.
// If draw_flag is true, it does not draw but only calculate the size.
- (NSSize)drawView:(bool)draw_flag;
@end

@implementation InfolistView
#pragma mark initialization

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    RendererStyle *style = new (std::nothrow) RendererStyle;
    if (style) {
      RendererStyleHandler::GetRendererStyle(style);
    }
    style_ = style;
  }

  if (!style_) {
    self = nil;
  }
  return self;
}

- (void)setCandidates:(const Candidates *)candidates {
  candidates_.CopyFrom(*candidates);
}

- (BOOL)isFlipped {
  return YES;
}

#pragma mark drawing
- (CGFloat)drawRow:(int)row ypos:(CGFloat)ypos draw_flag:(bool)draw_flag {
  const RendererStyle::InfolistStyle &infostyle = style_->infolist_style();
  const InformationList &usages = candidates_.usages();
  const RendererStyle::TextStyle &title_style = infostyle.title_style();
  const RendererStyle::TextStyle &desc_style = infostyle.description_style();
  const int title_width = infostyle.window_width() - title_style.left_padding() -
                          title_style.right_padding() - infostyle.window_border() * 2 -
                          infostyle.row_rect_padding() * 2;
  const int desc_width = infostyle.window_width() - desc_style.left_padding() -
                         desc_style.right_padding() - infostyle.window_border() * 2 -
                         infostyle.row_rect_padding() * 2;
  const NSSize title_size = NSMakeSize(title_width, 1000);
  const NSSize desc_size = NSMakeSize(desc_width, 1000);
  const Information &info = usages.information(row);

  NSAttributedString *title_string = MacViewUtil::ToNSAttributedString(info.title(), title_style);
  NSAttributedString *desc_string =
      MacViewUtil::ToNSAttributedString(info.description(), desc_style);
  NSRect title_rect = [title_string boundingRectWithSize:title_size
                                                 options:NSStringDrawingUsesLineFragmentOrigin];
  NSRect desc_rect = [desc_string boundingRectWithSize:desc_size
                                               options:NSStringDrawingUsesLineFragmentOrigin];
  CGFloat height =
      title_rect.size.height + desc_rect.size.height + infostyle.row_rect_padding() * 2;

  if (!draw_flag) {
    return height;
  }
  title_rect.origin.x =
      infostyle.window_border() + infostyle.row_rect_padding() + title_style.left_padding();
  title_rect.origin.y = ypos + infostyle.row_rect_padding();
  desc_rect.origin.x =
      infostyle.window_border() + infostyle.row_rect_padding() + desc_style.left_padding();
  desc_rect.origin.y = ypos + infostyle.row_rect_padding() + title_rect.size.height;

  if (usages.has_focused_index() && (row == usages.focused_index())) {
    NSRect focused_rect = NSMakeRect(
        infostyle.window_border(), ypos, infostyle.window_width() - infostyle.window_border() * 2,
        title_rect.size.height + desc_rect.size.height + infostyle.row_rect_padding() * 2);
    [MacViewUtil::ToNSColor(infostyle.focused_background_color()) set];
    [NSBezierPath fillRect:focused_rect];
    [MacViewUtil::ToNSColor(infostyle.focused_border_color()) set];
    // Fix the border position.  Because a line should be drawn at the
    // middle point of the pixel, origin should be shifted by 0.5 unit
    // and the size should be shrinked by 1.0 unit.
    focused_rect.origin.x += 0.5;
    focused_rect.origin.y += 0.5;
    focused_rect.size.width -= 1.0;
    focused_rect.size.height -= 1.0;
    [NSBezierPath strokeRect:focused_rect];
  } else {
    if (title_style.has_background_color()) {
      NSRect rect = NSMakeRect(infostyle.window_border(), ypos,
                               infostyle.window_width() - infostyle.window_border() * 2,
                               title_rect.size.height + infostyle.row_rect_padding());
      [MacViewUtil::ToNSColor(title_style.background_color()) set];
      [NSBezierPath fillRect:rect];
    }
    if (desc_style.has_background_color()) {
      NSRect rect = NSMakeRect(infostyle.window_border(),
                               ypos + title_rect.size.height + infostyle.row_rect_padding(),
                               infostyle.window_width() - infostyle.window_border() * 2,
                               desc_rect.size.height + infostyle.row_rect_padding());
      [MacViewUtil::ToNSColor(desc_style.background_color()) set];
      [NSBezierPath fillRect:rect];
    }
  }
  [title_string drawWithRect:title_rect options:NSStringDrawingUsesLineFragmentOrigin];
  [desc_string drawWithRect:desc_rect options:NSStringDrawingUsesLineFragmentOrigin];
  return height;
}

- (NSSize)drawView:(bool)draw_flag {
  if (!candidates_.has_usages()) {
    return NSMakeSize(0, 0);
  }

  const RendererStyle::InfolistStyle &infostyle = style_->infolist_style();
  const InformationList &usages = candidates_.usages();

  int ypos = infostyle.window_border();

  if (draw_flag && infostyle.has_caption_string()) {
    const RendererStyle::TextStyle &caption_style = infostyle.caption_style();
    const int caption_height = infostyle.caption_height();
    NSAttributedString *caption_string =
        MacViewUtil::ToNSAttributedString(infostyle.caption_string(), caption_style);
    NSRect rect =
        NSMakeRect(infostyle.window_border(), ypos,
                   infostyle.window_width() - infostyle.window_border() * 2, caption_height);
    [MacViewUtil::ToNSColor(infostyle.caption_background_color()) set];
    [NSBezierPath fillRect:rect];
    rect = NSMakeRect(
        infostyle.window_border() + infostyle.caption_padding() + caption_style.left_padding(),
        ypos + infostyle.caption_padding(),
        infostyle.window_width() - infostyle.window_border() * 2, caption_height);
    [caption_string drawWithRect:rect options:NSStringDrawingUsesLineFragmentOrigin];
  }
  ypos += infostyle.caption_height();
  for (int i = 0; i < usages.information_size(); ++i) {
    ypos += [self drawRow:i ypos:ypos draw_flag:draw_flag];
  }
  ypos += infostyle.window_border();

  if (draw_flag) {
    [MacViewUtil::ToNSColor(infostyle.border_color()) set];
    [NSBezierPath setDefaultLineWidth:infostyle.window_border()];
    [NSBezierPath setDefaultLineJoinStyle:NSLineJoinStyleMiter];
    [NSBezierPath strokeRect:NSMakeRect(0.5, 0.5, infostyle.window_width() - 1, ypos - 1)];
  }

  return NSMakeSize(infostyle.window_width(), ypos);
}

- (NSSize)updateLayout {
  return [self drawView:false];
}

- (void)drawRect:(NSRect)rect {
  [self drawView:true];
}

@end

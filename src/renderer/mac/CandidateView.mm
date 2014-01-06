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

#include <set>

#import "renderer/mac/CandidateView.h"

#include "base/logging.h"
#include "base/mutex.h"
#include "client/client_interface.h"
#include "renderer/mac/mac_view_util.h"
#include "renderer/table_layout.h"
#include "session/commands.pb.h"
#include "renderer/renderer_style.pb.h"
#include "renderer/renderer_style_handler.h"


using mozc::client::SendCommandInterface;
using mozc::commands::Candidates;
using mozc::commands::Output;
using mozc::commands::SessionCommand;
using mozc::renderer::TableLayout;
using mozc::renderer::RendererStyle;
using mozc::renderer::RendererStyleHandler;
using mozc::renderer::mac::MacViewUtil;
using mozc::once_t;
using mozc::CallOnce;

// Those constants and most rendering logic is as same as Windows
// native candidate window.
// TODO(mukai): integrate and share the code among Win and Mac.

namespace {
const NSImage *g_LogoImage = NULL;
int g_column_minimum_width = 0;
once_t g_OnceForInitializeStyle = MOZC_ONCE_INIT;

void InitializeDefaultStyle() {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  RendererStyle style;
  RendererStyleHandler::GetRendererStyle(&style);

  string logo_file_name = style.logo_file_name();
  g_LogoImage =
    [NSImage imageNamed:[NSString stringWithUTF8String:logo_file_name.c_str()]];
  if (g_LogoImage) {
    // setFlipped is deprecated at Snow Leopard, but we use this because
    // it works well with Snow Leopard and new method to deal with
    // flipped view doesn't work with Leopard.
    [g_LogoImage setFlipped:YES];

    // Fix the image size.  Sometimes the size can be smaller than the
    // actual size because of blank margin.
    NSArray *logoReps = [g_LogoImage representations];
    if (logoReps && [logoReps count] > 0) {
      NSImageRep *representation = [logoReps objectAtIndex:0];
      [g_LogoImage setSize:NSMakeSize([representation pixelsWide],
                                      [representation pixelsHigh])];
    }
  }

  NSString *nsstr =
    [NSString stringWithUTF8String:style.column_minimum_width_string().c_str()];
  NSDictionary *attr =
    [NSDictionary dictionaryWithObject:[NSFont messageFontOfSize:14]
                                forKey:NSFontAttributeName];
  NSAttributedString *defaultMessage =
    [[[NSAttributedString alloc] initWithString:nsstr attributes:attr]
     autorelease];
  g_column_minimum_width = [defaultMessage size].width;

  // default line width is specified as 1.0 *pt*, but we want to draw
  // it as 1.0 px.
  [NSBezierPath setDefaultLineWidth:1.0];
  [NSBezierPath setDefaultLineJoinStyle:NSMiterLineJoinStyle];
  [pool drain];
}
}

// Private method declarations.
@interface CandidateView ()
// Draw the |row|-th row.
- (void)drawRow:(int)row;

// Draw footer
- (void)drawFooter;

// Draw scroll bar
- (void)drawVScrollBar;
@end

@implementation CandidateView
#pragma mark initialization

- (id)initWithFrame:(NSRect)frame {
  CallOnce(&g_OnceForInitializeStyle, InitializeDefaultStyle);
  self = [super initWithFrame:frame];
  if (self) {
    tableLayout_ = new(nothrow)TableLayout;
    RendererStyle *style = new(nothrow)RendererStyle;
    if (style) {
      RendererStyleHandler::GetRendererStyle(style);
    }
    style_ = style;
    focusedRow_ = -1;
  }
  if (!tableLayout_ || !style_) {
    [self release];
    self = nil;
  }
  return self;
}

- (void)setCandidates:(const Candidates *)candidates {
  candidates_.CopyFrom(*candidates);
}

- (void)setSendCommandInterface:(SendCommandInterface *)command_sender {
  command_sender_ = command_sender;
}

- (BOOL)isFlipped {
  return YES;
}

- (void)dealloc {
  [candidateStringsCache_ release];
  delete tableLayout_;
  delete style_;
  [super dealloc];
}

- (const TableLayout *)tableLayout {
  return tableLayout_;
}

#pragma mark drawing

#define max(x, y)  (((x) > (y))? (x) : (y))
- (NSSize)updateLayout {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [candidateStringsCache_ release];
  tableLayout_->Initialize(candidates_.candidate_size(), NUMBER_OF_COLUMNS);
  tableLayout_->SetWindowBorder(style_->window_border());

  // calculating focusedRow_
  if (candidates_.has_focused_index() && candidates_.candidate_size() > 0) {
    const int focusedIndex = candidates_.focused_index();
    focusedRow_ = focusedIndex - candidates_.candidate(0).index();
  } else {
    focusedRow_ = -1;
  }

  // Reserve footer space.
  if (candidates_.has_footer()) {
    NSSize footerSize = NSZeroSize;

    const mozc::commands::Footer &footer = candidates_.footer();

    if (footer.has_label()) {
      NSAttributedString *footerLabel = MacViewUtil::ToNSAttributedString(
          footer.label(), style_->footer_style());
      NSSize footerLabelSize =
          MacViewUtil::applyTheme([footerLabel size], style_->footer_style());
      footerSize.width += footerLabelSize.width;
      footerSize.height = max(footerSize.height, footerLabelSize.height);
    }

    if (footer.has_sub_label()) {
      NSAttributedString *footerSubLabel = MacViewUtil::ToNSAttributedString(
          footer.sub_label(), style_->footer_sub_label_style());
      NSSize footerSubLabelSize =
          MacViewUtil::applyTheme([footerSubLabel size], style_->footer_sub_label_style());
      footerSize.width += footerSubLabelSize.width;
      footerSize.height = max(footerSize.height, footerSubLabelSize.height);
    }

    if (footer.logo_visible() && g_LogoImage) {
      NSSize logoSize = [g_LogoImage size];
      footerSize.width += logoSize.width;
      footerSize.height = max(footerSize.height, logoSize.height);
    }

    if (footer.index_visible()) {
      const int focusedIndex = candidates_.focused_index();
      const int totalItems = candidates_.size();
      NSString *footerIndex =
          [NSString stringWithFormat:@"%d/%d", focusedIndex + 1, totalItems];
      NSAttributedString *footerAttributedIndex =
          MacViewUtil::ToNSAttributedString([footerIndex UTF8String],
                                            style_->footer_style());
      NSSize footerIndexSize =
          MacViewUtil::applyTheme([footerAttributedIndex size],
                                  style_->footer_style());
      footerSize.width += footerIndexSize.width;
      footerSize.height = max(footerSize.height, footerIndexSize.height);
    }

    footerSize.height += style_->footer_border_colors_size();
    tableLayout_->EnsureFooterSize(MacViewUtil::ToSize(footerSize));
  }

  tableLayout_->SetRowRectPadding(style_->row_rect_padding());
  if (candidates_.candidate_size() < candidates_.size()) {
    tableLayout_->SetVScrollBar(style_->scrollbar_width());
  }

  NSAttributedString *gap1 = MacViewUtil::ToNSAttributedString(
      " ", style_->text_styles(COLUMN_GAP1));
  tableLayout_->EnsureCellSize(COLUMN_GAP1, MacViewUtil::ToSize([gap1 size]));

  NSMutableArray *newCache = [[NSMutableArray array] retain];
  for (size_t i = 0; i < candidates_.candidate_size(); ++i) {
    const Candidates::Candidate &candidate = candidates_.candidate(i);
    NSAttributedString *shortcut = MacViewUtil::ToNSAttributedString(
       candidate.annotation().shortcut(),
       style_->text_styles(COLUMN_SHORTCUT));
    string value = candidate.value();
    if (candidate.annotation().has_prefix()) {
      value = candidate.annotation().prefix() + value;
    }
    if (candidate.annotation().has_suffix()) {
      value.append(candidate.annotation().suffix());
    }
    if (!value.empty()) {
      value.append("  ");
    }

    NSAttributedString *candidateValue = MacViewUtil::ToNSAttributedString(
        value, style_->text_styles(COLUMN_CANDIDATE));
    NSAttributedString *description = MacViewUtil::ToNSAttributedString(
       candidate.annotation().description(),
       style_->text_styles(COLUMN_DESCRIPTION));
    if ([shortcut length] > 0) {
      NSSize shortcutSize = MacViewUtil::applyTheme(
          [shortcut size], style_->text_styles(COLUMN_SHORTCUT));
      tableLayout_->EnsureCellSize(COLUMN_SHORTCUT,
                                   MacViewUtil::ToSize(shortcutSize));
    }
    if ([candidateValue length] > 0) {
      NSSize valueSize = MacViewUtil::applyTheme(
          [candidateValue size], style_->text_styles(COLUMN_CANDIDATE));
      tableLayout_->EnsureCellSize(COLUMN_CANDIDATE,
                                   MacViewUtil::ToSize(valueSize));
    }
    if ([description length] > 0) {
      NSSize descriptionSize = MacViewUtil::applyTheme(
          [description size], style_->text_styles(COLUMN_DESCRIPTION));
      tableLayout_->EnsureCellSize(COLUMN_DESCRIPTION,
                                   MacViewUtil::ToSize(descriptionSize));
    }

    [newCache addObject:[NSArray arrayWithObjects:shortcut, gap1,
                                 candidateValue, description, nil]];
  }

  tableLayout_->EnsureColumnsWidth(COLUMN_CANDIDATE, COLUMN_DESCRIPTION,
                                   g_column_minimum_width);

  candidateStringsCache_ = newCache;
  tableLayout_->FreezeLayout();
  [pool drain];
  return MacViewUtil::ToNSSize(tableLayout_->GetTotalSize());
}

- (void)drawRect:(NSRect)rect {
  if (!Category_IsValid(candidates_.category())) {
    LOG(WARNING) << "Unknown candidates category: " << candidates_.category();
    return;
  }

  for (int i = 0; i < candidates_.candidate_size(); ++i) {
    [self drawRow:i];
  }

  if (candidates_.candidate_size() < candidates_.size()) {
    [self drawVScrollBar];
  }
  [self drawFooter];

  // Draw the window border at last
  [MacViewUtil::ToNSColor(style_->border_color()) set];
  mozc::Size windowSize = tableLayout_->GetTotalSize();
  [NSBezierPath strokeRect:NSMakeRect(
      0.5, 0.5, windowSize.width - 1, windowSize.height - 1)];
}

#pragma mark drawing aux methods

- (void)drawRow:(int)row {
  if (row == focusedRow_) {
    // Draw focused background
    NSRect focusedRect = MacViewUtil::ToNSRect(tableLayout_->GetRowRect(focusedRow_));
    [MacViewUtil::ToNSColor(style_->focused_background_color()) set];
    [NSBezierPath fillRect:focusedRect];
    [MacViewUtil::ToNSColor(style_->focused_border_color()) set];
    // Fix the border position.  Because a line should be drawn at the
    // middle point of the pixel, origin should be shifted by 0.5 unit
    // and the size should be shrinked by 1.0 unit.
    focusedRect.origin.x += 0.5;
    focusedRect.origin.y += 0.5;
    focusedRect.size.width -= 1.0;
    focusedRect.size.height -= 1.0;
    [NSBezierPath strokeRect:focusedRect];
  } else {
    // Draw normal background
    for (int i = COLUMN_SHORTCUT; i < NUMBER_OF_COLUMNS; ++i) {
      mozc::Rect cellRect = tableLayout_->GetCellRect(row, i);
      if (cellRect.size.width > 0 && cellRect.size.height > 0 &&
          style_->text_styles(i).has_background_color()) {
        [MacViewUtil::ToNSColor(style_->text_styles(i).background_color()) set];
        [NSBezierPath fillRect:MacViewUtil::ToNSRect(cellRect)];
      }
    }
  }

  NSArray *candidate = [candidateStringsCache_ objectAtIndex:row];
  for (int i = COLUMN_SHORTCUT; i < NUMBER_OF_COLUMNS; ++i) {
    NSAttributedString *text = [candidate objectAtIndex:i];
    NSRect cellRect = MacViewUtil::ToNSRect(tableLayout_->GetCellRect(row, i));
    NSPoint &candidatePosition = cellRect.origin;
    // Adjust the positions
    candidatePosition.x += style_->text_styles(i).left_padding();
    candidatePosition.y += (cellRect.size.height - [text size].height) / 2;
    [text drawAtPoint:candidatePosition];
  }

  if (candidates_.candidate(row).has_information_id()) {
      NSRect rect = MacViewUtil::ToNSRect(tableLayout_->GetRowRect(row));
      [MacViewUtil::ToNSColor(style_->focused_border_color()) set];
      rect.origin.x += rect.size.width - 6.0;
      rect.size.width = 4.0;
      rect.origin.y += 2.0;
      rect.size.height -= 4.0;
      [NSBezierPath fillRect:rect];
  }
}

- (void)drawFooter {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  if (candidates_.has_footer()) {
    const mozc::commands::Footer &footer = candidates_.footer();
    NSRect footerRect = MacViewUtil::ToNSRect(tableLayout_->GetFooterRect());

    // Draw footer border
    for (int i = 0; i < style_->footer_border_colors_size(); ++i) {
      [MacViewUtil::ToNSColor(style_->footer_border_colors(i)) set];
      NSPoint fromPoint = NSMakePoint(footerRect.origin.x,
                                      footerRect.origin.y + 0.5);
      NSPoint toPoint = NSMakePoint(footerRect.origin.x + footerRect.size.width,
                                    footerRect.origin.y + 0.5);
      [NSBezierPath strokeLineFromPoint:fromPoint toPoint:toPoint];
      footerRect.origin.y += 1;
    }

    // Draw Footer background and data if necessary
    NSGradient *footerBackground =
      [[[NSGradient alloc]
        initWithStartingColor:MacViewUtil::ToNSColor(style_->footer_top_color())
        endingColor:MacViewUtil::ToNSColor(style_->footer_bottom_color())]
       autorelease];
    [footerBackground drawInRect:footerRect angle:90.0];

    // Draw logo
    if (footer.logo_visible() && g_LogoImage) {
      [g_LogoImage drawAtPoint:footerRect.origin
                      fromRect:NSZeroRect /* means draw entire image */
                     operation:NSCompositeSourceOver
                      fraction:1.0 /* opacity */];
      NSSize logoSize = [g_LogoImage size];
      footerRect.origin.x += logoSize.width;
      footerRect.size.width -= logoSize.width;
    }

    // Draw label
    if (footer.has_label()) {
      NSAttributedString *footerLabel = MacViewUtil::ToNSAttributedString(
          footer.label(), style_->footer_style());
      footerRect.origin.x += style_->footer_style().left_padding();
      NSSize labelSize = [footerLabel size];
      NSPoint labelPosition = footerRect.origin;
      labelPosition.y += (footerRect.size.height - labelSize.height) / 2;
      [footerLabel drawAtPoint:labelPosition];
    }

    // Draw sub_label
    if (footer.has_sub_label()) {
      NSAttributedString *footerSubLabel = MacViewUtil::ToNSAttributedString(
          footer.sub_label(), style_->footer_sub_label_style());
      footerRect.origin.x += style_->footer_sub_label_style().left_padding();
      NSSize subLabelSize = [footerSubLabel size];
      NSPoint subLabelPosition = footerRect.origin;
      subLabelPosition.y += (footerRect.size.height - subLabelSize.height) / 2;
      [footerSubLabel drawAtPoint:subLabelPosition];
    }

    // Draw footer index (e.g. "10/120")
    if (footer.index_visible()) {
      int focusedIndex = candidates_.focused_index();
      int totalItems = candidates_.size();
      NSString *footerIndex =
          [NSString stringWithFormat:@"%d/%d", focusedIndex + 1, totalItems];
      NSAttributedString *footerAttributedIndex =
        MacViewUtil::ToNSAttributedString(
          [footerIndex UTF8String], style_->footer_style());
      NSSize footerSize = [footerAttributedIndex size];
      NSPoint footerPosition = footerRect.origin;
      footerPosition.x = footerPosition.x + footerRect.size.width -
          footerSize.width - style_->footer_style().right_padding();
      [footerAttributedIndex drawAtPoint:footerPosition];
    }
  }
  [pool drain];
}

- (void)drawVScrollBar {
  const mozc::Rect &vscrollRect = tableLayout_->GetVScrollBarRect();

  if (!vscrollRect.IsRectEmpty() && candidates_.candidate_size() > 0) {
    const int beginIndex = candidates_.candidate(0).index();
    const int candidatesTotal = candidates_.size();
    const int endIndex =
        candidates_.candidate(candidates_.candidate_size() - 1).index();

    [MacViewUtil::ToNSColor(style_->scrollbar_background_color()) set];
    [NSBezierPath fillRect:MacViewUtil::ToNSRect(vscrollRect)];

    const mozc::Rect &indicatorRect =
        tableLayout_->GetVScrollIndicatorRect(
            beginIndex, endIndex, candidatesTotal);
    [MacViewUtil::ToNSColor(style_->scrollbar_indicator_color()) set];
    [NSBezierPath fillRect:MacViewUtil::ToNSRect(indicatorRect)];
  }
}

#pragma mark event handling callbacks

const char *Inspect(id obj) {
  return [[NSString stringWithFormat:@"%@", obj] UTF8String];
}

- (void)mouseDown:(NSEvent *)event {
  mozc::Point localPos = MacViewUtil::ToPoint(
      [self convertPoint:[event locationInWindow] fromView:nil]);
  int clickedRow = -1;
  for (int i = 0; i < tableLayout_->number_of_rows(); ++i) {
    mozc::Rect rowRect = tableLayout_->GetRowRect(i);
    if (rowRect.PtrInRect(localPos)) {
      clickedRow = i;
      break;
    }
  }

  if (clickedRow >= 0 && clickedRow != focusedRow_) {
    focusedRow_ = clickedRow;
    [self setNeedsDisplay:YES];
  }
}

- (void)mouseUp:(NSEvent *)event {
  mozc::Point localPos = MacViewUtil::ToPoint(
      [self convertPoint:[event locationInWindow] fromView:nil]);
  if (command_sender_ == NULL) {
    return;
  }
  if (candidates_.candidate_size() < tableLayout_->number_of_rows()) {
    return;
  }
  for (int i = 0; i < tableLayout_->number_of_rows(); ++i) {
    mozc::Rect rowRect = tableLayout_->GetRowRect(i);
    if (rowRect.PtrInRect(localPos)) {
      SessionCommand command;
      command.set_type(SessionCommand::SELECT_CANDIDATE);
      command.set_id(candidates_.candidate(i).id());
      Output dummy_output;
      command_sender_->SendCommand(command, &dummy_output);
      break;
    }
  }
}

- (void)mouseDragged:(NSEvent *)event {
  [self mouseDown:event];
}
@end

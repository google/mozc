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

#import "renderer/mac/CandidateView.h"

#import <Foundation/Foundation.h>

#include <algorithm>
#include <set>

#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "client/client_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_style.pb.h"
#include "renderer/mac/mac_view_util.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/table_layout.h"

using mozc::client::SendCommandInterface;
using mozc::commands::CandidateWindow;
using mozc::commands::Output;
using mozc::commands::SessionCommand;
using mozc::renderer::RendererStyleHandler;
using mozc::renderer::ColumnType;
using mozc::renderer::kColumnShortcut;
using mozc::renderer::kColumnGap1;
using mozc::renderer::kColumnCandidate;
using mozc::renderer::kColumnDescription;
using mozc::renderer::kNumberOfColumns;
using mozc::renderer::TableLayout;
using mozc::renderer::mac::MacViewUtil;

// Those constants and most rendering logic is as same as Windows
// native candidate window.
// TODO(mukai): integrate and share the code among Win and Mac.

// Private method declarations.
@interface CandidateView ()
- (void)initializeDefaultStyle;

// Draw the |row|-th row.
- (void)drawRow:(int)row;

// Draw footer
- (void)drawFooter;

// Draw scroll bar
- (void)drawVScrollBar;
@end

@implementation CandidateView {
  const NSImage *logoImage_;
  int columnMinimumWidth_;

  mozc::commands::CandidateWindow candidate_window_;
  mozc::renderer::TableLayout tableLayout_;
  mozc::renderer::RendererStyle style_;

  // The row which has focused background.
  int focusedRow_;

  // Cache of attributed strings which is allocated at updateLayout.
  NSArray *candidateStringsCache_;

  // |command_sender_| holds a callback for mouse clicks.
  mozc::client::SendCommandInterface *command_sender_;
}

#pragma mark initialization

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self initializeDefaultStyle];
    focusedRow_ = -1;
  }
  return self;
}

- (void)initializeDefaultStyle {
  RendererStyleHandler::GetRendererStyle(&style_);

  const std::string &logo_file_name = style_.logo_file_name();
  logoImage_ = [NSImage imageNamed:[NSString stringWithUTF8String:logo_file_name.c_str()]];
  if (logoImage_) {
    // Fix the image size.  Sometimes the size can be smaller than the
    // actual size because of blank margin.
    const NSArray *logoReps = [logoImage_ representations];
    if (logoReps && [logoReps count] > 0) {
      const NSImageRep *representation = [logoReps objectAtIndex:0];
      [logoImage_ setSize:NSMakeSize([representation pixelsWide], [representation pixelsHigh])];
    }
  }

  NSString *nsstr = [NSString stringWithUTF8String:style_.column_minimum_width_string().c_str()];
  NSDictionary *attr = [NSDictionary dictionaryWithObject:[NSFont messageFontOfSize:14]
                                                   forKey:NSFontAttributeName];
  const NSAttributedString *defaultMessage = [[NSAttributedString alloc] initWithString:nsstr
                                                                             attributes:attr];
  columnMinimumWidth_ = [defaultMessage size].width;

  // default line width is specified as 1.0 *pt*, but we want to draw
  // it as 1.0 px.
  [NSBezierPath setDefaultLineWidth:1.0];
  [NSBezierPath setDefaultLineJoinStyle:NSLineJoinStyleMiter];
}

- (void)setCandidateWindow:(const CandidateWindow *)candidate_window {
  candidate_window_ = *candidate_window;
}

- (void)setSendCommandInterface:(SendCommandInterface *)command_sender {
  command_sender_ = command_sender;
}

// Override of NSView.
- (BOOL)isFlipped {
  return YES;
}

- (void)dealloc {
  candidateStringsCache_ = nil;
}

- (const TableLayout *)tableLayout {
  return &tableLayout_;
}

#pragma mark drawing

- (NSSize)updateLayout {
  candidateStringsCache_ = nil;
  tableLayout_.Initialize(candidate_window_.candidate_size(), kNumberOfColumns);
  tableLayout_.SetWindowBorder(style_.window_border());

  // calculating focusedRow_
  if (candidate_window_.has_focused_index() && candidate_window_.candidate_size() > 0) {
    const int focusedIndex = candidate_window_.focused_index();
    focusedRow_ = focusedIndex - candidate_window_.candidate(0).index();
  } else {
    focusedRow_ = -1;
  }

  // Reserve footer space.
  if (candidate_window_.has_footer()) {
    NSSize footerSize = NSZeroSize;

    const mozc::commands::Footer &footer = candidate_window_.footer();

    if (footer.has_label()) {
      const NSAttributedString *footerLabel =
          MacViewUtil::ToNSAttributedString(footer.label(), style_.footer_style());
      const NSSize footerLabelSize =
          MacViewUtil::applyTheme([footerLabel size], style_.footer_style());
      footerSize.width += footerLabelSize.width;
      footerSize.height = std::max(footerSize.height, footerLabelSize.height);
    }

    if (footer.has_sub_label()) {
      const NSAttributedString *footerSubLabel =
          MacViewUtil::ToNSAttributedString(footer.sub_label(), style_.footer_sub_label_style());
      const NSSize footerSubLabelSize =
          MacViewUtil::applyTheme([footerSubLabel size], style_.footer_sub_label_style());
      footerSize.width += footerSubLabelSize.width;
      footerSize.height = std::max(footerSize.height, footerSubLabelSize.height);
    }

    if (footer.logo_visible() && logoImage_) {
      const NSSize logoSize = [logoImage_ size];
      footerSize.width += logoSize.width;
      footerSize.height = std::max(footerSize.height, logoSize.height);
    }

    if (footer.index_visible()) {
      const int focusedIndex = candidate_window_.focused_index();
      const int totalItems = candidate_window_.size();
      const NSString *footerIndex =
          [NSString stringWithFormat:@"%d/%d", focusedIndex + 1, totalItems];
      const NSAttributedString *footerAttributedIndex =
          MacViewUtil::ToNSAttributedString([footerIndex UTF8String], style_.footer_style());
      const NSSize footerIndexSize =
          MacViewUtil::applyTheme([footerAttributedIndex size], style_.footer_style());
      footerSize.width += footerIndexSize.width;
      footerSize.height = std::max(footerSize.height, footerIndexSize.height);
    }

    footerSize.height += style_.footer_border_colors_size();
    tableLayout_.EnsureFooterSize(MacViewUtil::ToSize(footerSize));
  }

  tableLayout_.SetRowRectPadding(style_.row_rect_padding());
  if (candidate_window_.candidate_size() < candidate_window_.size()) {
    tableLayout_.SetVScrollBar(style_.scrollbar_width());
  }

  const NSAttributedString *gap1 =
      MacViewUtil::ToNSAttributedString(" ", style_.gap1_style());
  tableLayout_.EnsureCellSize(kColumnGap1, MacViewUtil::ToSize([gap1 size]));

  NSMutableArray *newCache = [[NSMutableArray array] init];
  for (size_t i = 0; i < candidate_window_.candidate_size(); ++i) {
    const CandidateWindow::Candidate &candidate = candidate_window_.candidate(i);
    const NSAttributedString *shortcut = MacViewUtil::ToNSAttributedString(
        candidate.annotation().shortcut(), style_.shortcut_style());
    std::string value = candidate.value();
    if (candidate.annotation().has_prefix()) {
      value.insert(0, candidate.annotation().prefix());  // Prepend the prefix() to value.
    }
    if (candidate.annotation().has_suffix()) {
      value.append(candidate.annotation().suffix());
    }
    if (!value.empty()) {
      value.append("  ");
    }

    const NSAttributedString *candidateValue =
        MacViewUtil::ToNSAttributedString(value, style_.candidate_style());
    const NSAttributedString *description = MacViewUtil::ToNSAttributedString(
        candidate.annotation().description(), style_.description_style());
    if ([shortcut length] > 0) {
      const NSSize shortcutSize =
          MacViewUtil::applyTheme([shortcut size], style_.shortcut_style());
      tableLayout_.EnsureCellSize(kColumnShortcut, MacViewUtil::ToSize(shortcutSize));
    }
    if ([candidateValue length] > 0) {
      const NSSize valueSize =
          MacViewUtil::applyTheme([candidateValue size], style_.candidate_style());
      tableLayout_.EnsureCellSize(kColumnCandidate, MacViewUtil::ToSize(valueSize));
    }
    if ([description length] > 0) {
      const NSSize descriptionSize =
          MacViewUtil::applyTheme([description size], style_.description_style());
      tableLayout_.EnsureCellSize(kColumnDescription, MacViewUtil::ToSize(descriptionSize));
    }

    [newCache
        addObject:[NSArray arrayWithObjects:shortcut, gap1, candidateValue, description, nil]];
  }

  tableLayout_.EnsureColumnsWidth(kColumnCandidate, kColumnDescription, columnMinimumWidth_);

  candidateStringsCache_ = newCache;
  tableLayout_.FreezeLayout();
  return MacViewUtil::ToNSSize(tableLayout_.GetTotalSize());
}

- (void)drawRect:(NSRect)rect {
  if (!Category_IsValid(candidate_window_.category())) {
    LOG(WARNING) << "Unknown candidates category: " << candidate_window_.category();
    return;
  }

  for (int i = 0; i < candidate_window_.candidate_size(); ++i) {
    [self drawRow:i];
  }

  if (candidate_window_.candidate_size() < candidate_window_.size()) {
    [self drawVScrollBar];
  }
  [self drawFooter];

  // Draw the window border at last
  [MacViewUtil::ToNSColor(style_.border_color()) set];
  const mozc::Size windowSize = tableLayout_.GetTotalSize();
  [NSBezierPath strokeRect:NSMakeRect(0.5, 0.5, windowSize.width - 1, windowSize.height - 1)];
}

#pragma mark drawing aux methods

- (void)drawRow:(int)row {
  if (row == focusedRow_) {
    // Draw focused background
    NSRect focusedRect = MacViewUtil::ToNSRect(tableLayout_.GetRowRect(focusedRow_));
    [MacViewUtil::ToNSColor(style_.focused_background_color()) set];
    [NSBezierPath fillRect:focusedRect];
    [MacViewUtil::ToNSColor(style_.focused_border_color()) set];
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
    auto drawBackground = [&](ColumnType type,
                              const mozc::renderer::RendererStyle::TextStyle& text_style) {
      const mozc::Rect cellRect = tableLayout_.GetCellRect(row, type);
      if (cellRect.size.width > 0 && cellRect.size.height > 0 &&
          text_style.has_background_color()) {
        [MacViewUtil::ToNSColor(text_style.background_color()) set];
        [NSBezierPath fillRect:MacViewUtil::ToNSRect(cellRect)];
      }
    };
    drawBackground(kColumnShortcut, style_.shortcut_style());
    drawBackground(kColumnGap1, style_.gap1_style());
    drawBackground(kColumnCandidate, style_.candidate_style());
    drawBackground(kColumnDescription, style_.description_style());
  }

  NSArray<NSAttributedString *> *candidate = [candidateStringsCache_ objectAtIndex:row];

  auto drawText = [&](ColumnType type,
                      const mozc::renderer::RendererStyle::TextStyle& text_style) {
    const NSAttributedString *text = [candidate objectAtIndex:type];
    NSRect cellRect = MacViewUtil::ToNSRect(tableLayout_.GetCellRect(row, type));
    NSPoint position = cellRect.origin;
    position.x += text_style.left_padding();
    position.y += (cellRect.size.height - [text size].height) / 2;
    [text drawAtPoint:position];
  };

  drawText(kColumnShortcut, style_.shortcut_style());
  drawText(kColumnGap1, style_.gap1_style());
  drawText(kColumnCandidate, style_.candidate_style());
  drawText(kColumnDescription, style_.description_style());

  if (candidate_window_.candidate(row).has_information_id()) {
    NSRect rect = MacViewUtil::ToNSRect(tableLayout_.GetRowRect(row));
    [MacViewUtil::ToNSColor(style_.focused_border_color()) set];
    rect.origin.x += rect.size.width - 6.0;
    rect.size.width = 4.0;
    rect.origin.y += 2.0;
    rect.size.height -= 4.0;
    [NSBezierPath fillRect:rect];
  }
}

- (void)drawFooter {
  if (!candidate_window_.has_footer()) {
    return;
  }
  const mozc::commands::Footer &footer = candidate_window_.footer();
  NSRect footerRect = MacViewUtil::ToNSRect(tableLayout_.GetFooterRect());

  // Draw footer border
  for (int i = 0; i < style_.footer_border_colors_size(); ++i) {
    [MacViewUtil::ToNSColor(style_.footer_border_colors(i)) set];
    const NSPoint fromPoint = NSMakePoint(footerRect.origin.x, footerRect.origin.y + 0.5);
    const NSPoint toPoint =
        NSMakePoint(footerRect.origin.x + footerRect.size.width, footerRect.origin.y + 0.5);
    [NSBezierPath strokeLineFromPoint:fromPoint toPoint:toPoint];
    footerRect.origin.y += 1;
  }

  // Draw Footer background and data if necessary
  const NSGradient *footerBackground = [[NSGradient alloc]
      initWithStartingColor:MacViewUtil::ToNSColor(style_.footer_top_color())
                endingColor:MacViewUtil::ToNSColor(style_.footer_bottom_color())];
  [footerBackground drawInRect:footerRect angle:90.0];

  // Draw logo
  if (footer.logo_visible() && logoImage_) {
    const NSPoint logoPoint = footerRect.origin;
    const NSSize logoSize = logoImage_.size;
    const NSRect logoRect = NSMakeRect(logoPoint.x, logoPoint.y, logoSize.width, logoSize.height);
    [logoImage_ drawInRect:logoRect
                    fromRect:NSZeroRect   // Draw the entire image
                  operation:NSCompositingOperationSourceOver
                    fraction:1.0  // Opacity
              respectFlipped:YES
                      hints:nil];
    footerRect.origin.x += logoSize.width;
    footerRect.size.width -= logoSize.width;
  }

  // Draw label
  if (footer.has_label()) {
    const NSAttributedString *footerLabel =
        MacViewUtil::ToNSAttributedString(footer.label(), style_.footer_style());
    footerRect.origin.x += style_.footer_style().left_padding();
    const NSSize labelSize = [footerLabel size];
    NSPoint labelPosition = footerRect.origin;
    labelPosition.y += (footerRect.size.height - labelSize.height) / 2;
    [footerLabel drawAtPoint:labelPosition];
  }

  // Draw sub_label
  if (footer.has_sub_label()) {
    const NSAttributedString *footerSubLabel =
        MacViewUtil::ToNSAttributedString(footer.sub_label(), style_.footer_sub_label_style());
    footerRect.origin.x += style_.footer_sub_label_style().left_padding();
    const NSSize subLabelSize = [footerSubLabel size];
    NSPoint subLabelPosition = footerRect.origin;
    subLabelPosition.y += (footerRect.size.height - subLabelSize.height) / 2;
    [footerSubLabel drawAtPoint:subLabelPosition];
  }

  // Draw footer index (e.g. "10/120")
  if (footer.index_visible()) {
    const std::string footerIndex =
        absl::StrFormat("%d/%d",
                        candidate_window_.focused_index() + 1,  // +1 to 1-origin from 0-origin.
                        candidate_window_.size());
    const NSAttributedString *footerAttributedIndex =
        MacViewUtil::ToNSAttributedString(footerIndex, style_.footer_style());
    const NSSize footerSize = [footerAttributedIndex size];
    NSPoint footerPosition = footerRect.origin;
    footerPosition.x = footerPosition.x + footerRect.size.width - footerSize.width -
                        style_.footer_style().right_padding();
    [footerAttributedIndex drawAtPoint:footerPosition];
  }
}

- (void)drawVScrollBar {
  const mozc::Rect vscrollRect = tableLayout_.GetVScrollBarRect();
  if (vscrollRect.IsRectEmpty() || candidate_window_.candidate_size() <= 0) {
    return;
  }

  const int beginIndex = candidate_window_.candidate(0).index();
  const int candidatesTotal = candidate_window_.size();
  const int endIndex = candidate_window_.candidate(candidate_window_.candidate_size() - 1).index();

  [MacViewUtil::ToNSColor(style_.scrollbar_background_color()) set];
  [NSBezierPath fillRect:MacViewUtil::ToNSRect(vscrollRect)];

  const mozc::Rect indicatorRect =
      tableLayout_.GetVScrollIndicatorRect(beginIndex, endIndex, candidatesTotal);
  [MacViewUtil::ToNSColor(style_.scrollbar_indicator_color()) set];
  [NSBezierPath fillRect:MacViewUtil::ToNSRect(indicatorRect)];
}

#pragma mark event handling callbacks

- (void)mouseDown:(NSEvent *)event {
  const mozc::Point localPos = MacViewUtil::ToPoint([self convertPoint:[event locationInWindow]
                                                              fromView:nil]);
  int clickedRow = -1;
  for (int i = 0; i < tableLayout_.number_of_rows(); ++i) {
    const mozc::Rect rowRect = tableLayout_.GetRowRect(i);
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
  const mozc::Point localPos = MacViewUtil::ToPoint([self convertPoint:[event locationInWindow]
                                                              fromView:nil]);
  if (command_sender_ == nullptr) {
    return;
  }
  if (candidate_window_.candidate_size() < tableLayout_.number_of_rows()) {
    return;
  }
  for (int i = 0; i < tableLayout_.number_of_rows(); ++i) {
    const mozc::Rect rowRect = tableLayout_.GetRowRect(i);
    if (rowRect.PtrInRect(localPos)) {
      SessionCommand command;
      command.set_type(SessionCommand::SELECT_CANDIDATE);
      command.set_id(candidate_window_.candidate(i).id());
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

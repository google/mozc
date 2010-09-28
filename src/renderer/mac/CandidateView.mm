// Copyright 2010, Google Inc.
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
#include "client/session_interface.h"
#include "renderer/table_layout.h"
#include "session/commands.pb.h"

using mozc::client::SendCommandInterface;
using mozc::commands::Candidates;
using mozc::commands::Output;
using mozc::commands::SessionCommand;
using mozc::renderer::TableLayout;
using mozc::once_t;
using mozc::CallOnce;

// Those constants and most rendering logic is as same as Windows
// native candidate window.
// TODO(mukai): integrate and share the code among Win and Mac.

namespace {
#pragma mark conversions between mozc-specific ones and Cocoa ones.
NSPoint ToNSPoint(const mozc::renderer::Point &point) {
  return NSMakePoint(point.x, point.y);
}

mozc::renderer::Point ToPoint(const NSPoint &nspoint) {
  return mozc::renderer::Point(nspoint.x, nspoint.y);
}

NSSize ToNSSize(const mozc::renderer::Size &size) {
  return NSMakeSize(size.width, size.height);
}

mozc::renderer::Size ToSize(const NSSize &nssize) {
  return mozc::renderer::Size(nssize.width, nssize.height);
}

NSRect ToNSRect(const mozc::renderer::Rect &rect) {
  return NSMakeRect(rect.origin.x, rect.origin.y,
                    rect.size.width, rect.size.height);
}

mozc::renderer::Rect ToRect(const NSRect &nsrect) {
  return mozc::renderer::Rect(ToPoint(nsrect.origin), ToSize(nsrect.size));
}

#pragma mark layout utilities and constants
struct TextStyle {
  NSDictionary *attributes;
  int leftPadding;
  int rightPadding;
  NSColor *background;
  TextStyle()
   : attributes(nil), leftPadding(0), rightPadding(0), background(nil) {}
  ~TextStyle() {
    [attributes release];
    [background release];
  }

  void Retain() {
    [attributes retain];
    [background retain];
  }
};

struct LayoutStyle {
  int windowBorder;
  int footerHeight;
  int rowRectPadding;
  NSColor *borderColor;
  int columnMinimumWidth;

  // Text Styles
  scoped_array<TextStyle> textStyles;
  NSArray *footerBorderColors;
  TextStyle footerStyle;
  TextStyle footerSubLabelStyle;

  // Focus colors
  NSColor *focusedBackground;
  NSColor *focusedBorder;

  // scrollbar
  NSColor *scrollBarBackground;
  NSColor *scrollBarIndicatorColor;
  int scrollBarWidth;

  // footer
  NSColor *footerTopColor;
  NSColor *footerBottomColor;
  NSImage *googleLogo;

  LayoutStyle()
   : borderColor(nil), footerBorderColors(nil), focusedBackground(nil),
    focusedBorder(nil), scrollBarBackground(nil), scrollBarIndicatorColor(nil),
    footerTopColor(nil), footerBottomColor(nil), googleLogo(nil) {
  }

  ~LayoutStyle() {
    [borderColor release];
    [footerBorderColors release];
    [focusedBackground release];
    [focusedBorder release];
    [scrollBarBackground release];
    [scrollBarIndicatorColor release];
    [footerTopColor release];
    [footerBottomColor release];
    [googleLogo release];
  }

  void Retain() {
    [borderColor retain];
    [footerBorderColors retain];
    [focusedBackground retain];
    [focusedBorder retain];
    [scrollBarBackground retain];
    [scrollBarIndicatorColor retain];
    [footerTopColor retain];
    [footerBottomColor retain];
    [googleLogo retain];
    for (int i = COLUMN_SHORTCUT; i < NUMBER_OF_COLUMNS; ++i) {
      textStyles[i].Retain();
    }
    footerStyle.Retain();
    footerSubLabelStyle.Retain();
  }
};

const LayoutStyle *kDefaultStyle = NULL;
once_t kOnceForDefaultStyle = MOZC_ONCE_INIT;

NSColor *colorWithRGB(uint8 r, uint8 g, uint8 b) {
  return [NSColor colorWithCalibratedRed:r / 255.0
                                   green:g / 255.0
                                    blue:b / 255.0
                                   alpha:1.0];
}

NSAttributedString *getAttributedString(const string &str, NSDictionary *attr) {
  NSString *nsstr = [NSString stringWithUTF8String:str.c_str()];
  return [[[NSAttributedString alloc] initWithString:nsstr attributes:attr]
           autorelease];
}

NSSize applyTheme(const NSSize &size, const TextStyle &style) {
  return NSMakeSize(size.width + style.leftPadding + style.rightPadding,
                    size.height);
}

void InitializeDefaultStyle() {
  LayoutStyle *newStyle = new(nothrow)LayoutStyle;
  if (!newStyle) {
    return;
  }

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  newStyle->windowBorder = 1;
  newStyle->scrollBarWidth = 4;
  newStyle->rowRectPadding = 0;
  newStyle->borderColor = colorWithRGB(0x96, 0x96, 0x96);

  newStyle->textStyles.reset(new TextStyle[NUMBER_OF_COLUMNS]);

  TextStyle &shortcutStyle = newStyle->textStyles[COLUMN_SHORTCUT];
  shortcutStyle.attributes =
      [NSDictionary dictionaryWithObjectsAndKeys:
                    [NSFont boldSystemFontOfSize:14],
                    NSFontAttributeName,
                    colorWithRGB(0x77, 0x77, 0x77),
                    NSForegroundColorAttributeName,
                    nil];
  shortcutStyle.leftPadding = 8;
  shortcutStyle.rightPadding = 8;
  shortcutStyle.background = colorWithRGB(0xf3, 0xf4, 0xff);

  TextStyle &gap1Style = newStyle->textStyles[COLUMN_GAP1];
  gap1Style.attributes =
      [NSDictionary dictionaryWithObject:[NSFont messageFontOfSize:14]
                                  forKey:NSFontAttributeName];

  TextStyle &candidateStyle = newStyle->textStyles[COLUMN_CANDIDATE];
  candidateStyle.attributes =
      [NSDictionary dictionaryWithObject:[NSFont messageFontOfSize:14]
                                  forKey:NSFontAttributeName];

  TextStyle &descriptionStyle = newStyle->textStyles[COLUMN_DESCRIPTION];
  descriptionStyle.attributes =
      [NSDictionary dictionaryWithObjectsAndKeys:[NSFont messageFontOfSize:12],
                    NSFontAttributeName,
                    colorWithRGB(0x88, 0x88, 0x88),
                    NSForegroundColorAttributeName,
                    nil];
  descriptionStyle.rightPadding = 8;

  // We want to ensure that the candidate window is at least wide
  // enough to render "そのほかの文字種" as a candidate.
  NSAttributedString *defaultMessage =
      getAttributedString("そのほかの文字種  ", candidateStyle.attributes);
  newStyle->columnMinimumWidth = [defaultMessage size].width;

  // Footer text style
  newStyle->footerStyle.attributes =
      [NSDictionary dictionaryWithObject:[NSFont messageFontOfSize:14]
                                  forKey:NSFontAttributeName];
  newStyle->footerStyle.leftPadding = 4;
  newStyle->footerStyle.rightPadding = 4;

  // Footer sub-label text style
  newStyle->footerSubLabelStyle.attributes =
      [NSDictionary dictionaryWithObjectsAndKeys:[NSFont messageFontOfSize:10],
                    NSFontAttributeName,
                    colorWithRGB(167, 167, 167),
                    NSForegroundColorAttributeName,
                    nil];
  newStyle->footerSubLabelStyle.leftPadding = 4;
  newStyle->footerSubLabelStyle.rightPadding = 4;

  newStyle->footerBorderColors =
      [NSArray arrayWithObjects:colorWithRGB(96, 96, 96), nil];
  newStyle->footerTopColor = colorWithRGB(0xff, 0xff, 0xff);
  newStyle->footerBottomColor = colorWithRGB(0xee, 0xee, 0xee);
  newStyle->googleLogo =
      [NSImage imageNamed:@"google_logo_dark_with_margin.png"];
  if (newStyle->googleLogo) {
    // setFlipped is deprecated at Snow Leopard, but we use this because
    // it works well with Snow Leopard and new method to deal with
    // flipped view doesn't work with Leopard.
    [newStyle->googleLogo setFlipped:YES];

    // Fix the image size.  Sometimes the size can be smaller than the
    // actual size because of blank margin.
    NSArray *logoReps = [newStyle->googleLogo representations];
    if (logoReps && [logoReps count] > 0) {
      NSImageRep *representation = [logoReps objectAtIndex:0];
      [newStyle->googleLogo setSize:NSMakeSize([representation pixelsWide],
                                               [representation pixelsHigh])];
    }
  }

  newStyle->focusedBackground = colorWithRGB(0xd1, 0xea, 0xff);
  newStyle->focusedBorder = colorWithRGB(0x7f, 0xac, 0xdd);

  newStyle->scrollBarBackground = colorWithRGB(0xe0, 0xe0, 0xe0);
  newStyle->scrollBarIndicatorColor = colorWithRGB(0x75, 0x90, 0xb8);
  newStyle->scrollBarWidth = 4;

  newStyle->Retain();
  kDefaultStyle = newStyle;

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
  CallOnce(&kOnceForDefaultStyle, InitializeDefaultStyle);
  self = [super initWithFrame:frame];
  if (self) {
    tableLayout_ = new(nothrow)TableLayout;
    focusedRow_ = -1;
  }
  if (!kDefaultStyle || !tableLayout_) {
    [self release];
    self = nil;
  }
  return self;
}

- (void)setCandidates:(const Candidates *)candidates {
  candidates_ = candidates;
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
  tableLayout_->Initialize(candidates_->candidate_size(), NUMBER_OF_COLUMNS);
  tableLayout_->SetWindowBorder(kDefaultStyle->windowBorder);

  // calculating focusedRow_
  if (candidates_->has_focused_index() && candidates_->candidate_size() > 0) {
    const int focusedIndex = candidates_->focused_index();
    focusedRow_ = focusedIndex - candidates_->candidate(0).index();
  } else {
    focusedRow_ = -1;
  }

  // Reserve footer space.
  if (candidates_->has_footer()) {
    NSSize footerSize = NSZeroSize;

    const mozc::commands::Footer &footer = candidates_->footer();

    if (footer.has_label()) {
      NSAttributedString *footerLabel = getAttributedString(
          footer.label(),
          kDefaultStyle->footerStyle.attributes);
      NSSize footerLabelSize =
          applyTheme([footerLabel size], kDefaultStyle->footerStyle);
      footerSize.width += footerLabelSize.width;
      footerSize.height = max(footerSize.height, footerLabelSize.height);
    }

    if (footer.has_sub_label()) {
      NSAttributedString *footerSubLabel = getAttributedString(
          footer.sub_label(),
          kDefaultStyle->footerSubLabelStyle.attributes);
      NSSize footerSubLabelSize =
          applyTheme([footerSubLabel size], kDefaultStyle->footerSubLabelStyle);
      footerSize.width += footerSubLabelSize.width;
      footerSize.height = max(footerSize.height, footerSubLabelSize.height);
    }

    if (footer.logo_visible() && kDefaultStyle->googleLogo) {
      NSSize logoSize = [kDefaultStyle->googleLogo size];
      footerSize.width += logoSize.width;
      footerSize.height = max(footerSize.height, logoSize.height);
    }

    if (footer.index_visible()) {
      const int focusedIndex = candidates_->focused_index();
      const int totalItems = candidates_->size();

      NSString *footerIndex =
          [NSString stringWithFormat:@"%d/%d", focusedIndex + 1, totalItems];
      NSAttributedString *footerAttributedIndex =
          [[[NSAttributedString alloc]
             initWithString:footerIndex
                 attributes:kDefaultStyle->footerStyle.attributes]
          autorelease];
      NSSize footerIndexSize =
          applyTheme([footerAttributedIndex size], kDefaultStyle->footerStyle);
      footerSize.width += footerIndexSize.width;
      footerSize.height = max(footerSize.height, footerIndexSize.height);
    }

    footerSize.height += [kDefaultStyle->footerBorderColors count];
    tableLayout_->EnsureFooterSize(ToSize(footerSize));
  }

  tableLayout_->SetRowRectPadding(kDefaultStyle->rowRectPadding);
  if (candidates_->candidate_size() < candidates_->size()) {
    tableLayout_->SetVScrollBar(kDefaultStyle->scrollBarWidth);
  }

  NSAttributedString *gap1 =
      [[[NSAttributedString alloc]
        initWithString:@" "
            attributes:kDefaultStyle->textStyles[COLUMN_GAP1].attributes]
        autorelease];
  tableLayout_->EnsureCellSize(COLUMN_GAP1, ToSize([gap1 size]));

  NSMutableArray *newCache = [[NSMutableArray array] retain];
  for (size_t i = 0; i < candidates_->candidate_size(); ++i) {
    const Candidates::Candidate &candidate = candidates_->candidate(i);
    NSAttributedString *shortcut = getAttributedString(
       candidate.annotation().shortcut(),
       kDefaultStyle->textStyles[COLUMN_SHORTCUT].attributes);
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

    NSAttributedString *candidateValue = getAttributedString(
        value, kDefaultStyle->textStyles[COLUMN_CANDIDATE].attributes);
    NSAttributedString *description = getAttributedString(
       candidate.annotation().description(),
       kDefaultStyle->textStyles[COLUMN_DESCRIPTION].attributes);
    if ([shortcut length] > 0) {
      NSSize shortcutSize = applyTheme(
          [shortcut size], kDefaultStyle->textStyles[COLUMN_SHORTCUT]);
      tableLayout_->EnsureCellSize(COLUMN_SHORTCUT, ToSize(shortcutSize));
    }
    if ([candidateValue length] > 0) {
      NSSize valueSize = applyTheme(
          [candidateValue size], kDefaultStyle->textStyles[COLUMN_CANDIDATE]);
      tableLayout_->EnsureCellSize(COLUMN_CANDIDATE, ToSize(valueSize));
    }
    if ([description length] > 0) {
      NSSize descriptionSize = applyTheme(
          [description size], kDefaultStyle->textStyles[COLUMN_DESCRIPTION]);
      tableLayout_->EnsureCellSize(COLUMN_DESCRIPTION, ToSize(descriptionSize));
    }

    [newCache addObject:[NSArray arrayWithObjects:shortcut, gap1,
                                 candidateValue, description, nil]];
  }

  tableLayout_->EnsureColumnsWidth(COLUMN_CANDIDATE, COLUMN_DESCRIPTION,
                                   kDefaultStyle->columnMinimumWidth);

  candidateStringsCache_ = newCache;
  tableLayout_->FreezeLayout();
  [pool drain];
  return ToNSSize(tableLayout_->GetTotalSize());
}

- (void)drawRect:(NSRect)rect {
  if (!candidates_) {
    return;
  }
  if (!Category_IsValid(candidates_->category())) {
    LOG(WARNING) << "Unknown candidates category: " << candidates_->category();
    return;
  }

  for (int i = 0; i < candidates_->candidate_size(); ++i) {
    [self drawRow:i];
  }

  if (candidates_->candidate_size() < candidates_->size()) {
    [self drawVScrollBar];
  }
  [self drawFooter];

  // Draw the window border at last
  [kDefaultStyle->borderColor set];
  mozc::renderer::Size windowSize = tableLayout_->GetTotalSize();
  [NSBezierPath strokeRect:NSMakeRect(
      0.5, 0.5, windowSize.width - 1, windowSize.height - 1)];
}

#pragma mark drawing aux methods

- (void)drawRow:(int)row {
  if (row == focusedRow_) {
    // Draw focused background
    NSRect focusedRect = ToNSRect(tableLayout_->GetRowRect(focusedRow_));
    [kDefaultStyle->focusedBackground set];
    [NSBezierPath fillRect:focusedRect];
    [kDefaultStyle->focusedBorder set];
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
      mozc::renderer::Rect cellRect = tableLayout_->GetCellRect(row, i);
      NSColor *backgroundColor = kDefaultStyle->textStyles[i].background;
      if (cellRect.size.width > 0 && cellRect.size.height > 0 &&
          backgroundColor) {
        [backgroundColor set];
        [NSBezierPath fillRect:ToNSRect(cellRect)];
      }
    }
  }

  NSArray *candidate = [candidateStringsCache_ objectAtIndex:row];
  for (int i = COLUMN_SHORTCUT; i < NUMBER_OF_COLUMNS; ++i) {
    NSAttributedString *text = [candidate objectAtIndex:i];
    NSRect cellRect = ToNSRect(tableLayout_->GetCellRect(row, i));
    NSPoint &candidatePosition = cellRect.origin;
    // Adjust the positions
    candidatePosition.x += kDefaultStyle->textStyles[i].leftPadding;
    candidatePosition.y += (cellRect.size.height - [text size].height) / 2;
    [text drawAtPoint:candidatePosition];
  }
}

- (void)drawFooter {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  if (candidates_->has_footer()) {
    const mozc::commands::Footer &footer = candidates_->footer();
    NSRect footerRect = ToNSRect(tableLayout_->GetFooterRect());

    // Draw footer border
    for (int i = 0; i < [kDefaultStyle->footerBorderColors count]; ++i) {
      NSColor *lineColor = [kDefaultStyle->footerBorderColors objectAtIndex:i];
      [lineColor set];
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
           initWithStartingColor:kDefaultStyle->footerTopColor
                     endingColor:kDefaultStyle->footerBottomColor]
          autorelease];
    [footerBackground drawInRect:footerRect angle:90.0];

    // Draw logo
    if (footer.logo_visible() && kDefaultStyle->googleLogo) {
      [kDefaultStyle->googleLogo
                    drawAtPoint:footerRect.origin
                       fromRect:NSZeroRect /* means draw entire image */
                      operation:NSCompositeSourceOver
                       fraction:1.0 /* opacity */];
      NSSize logoSize = [kDefaultStyle->googleLogo size];
      footerRect.origin.x += logoSize.width;
      footerRect.size.width -= logoSize.width;
    }

    // Draw label
    if (footer.has_label()) {
      NSAttributedString *footerLabel = getAttributedString(
          footer.label(),
          kDefaultStyle->footerStyle.attributes);
      footerRect.origin.x += kDefaultStyle->footerStyle.leftPadding;
      NSSize labelSize = [footerLabel size];
      NSPoint labelPosition = footerRect.origin;
      labelPosition.y += (footerRect.size.height - labelSize.height) / 2;
      [footerLabel drawAtPoint:labelPosition];
    }

    // Draw sub_label
    if (footer.has_sub_label()) {
      NSAttributedString *footerSubLabel = getAttributedString(
          footer.sub_label(),
          kDefaultStyle->footerSubLabelStyle.attributes);
      footerRect.origin.x += kDefaultStyle->footerSubLabelStyle.leftPadding;
      NSSize subLabelSize = [footerSubLabel size];
      NSPoint subLabelPosition = footerRect.origin;
      subLabelPosition.y += (footerRect.size.height - subLabelSize.height) / 2;
      [footerSubLabel drawAtPoint:subLabelPosition];
    }

    // Draw footer index (e.g. "10/120")
    if (footer.index_visible()) {
      int focusedIndex = candidates_->focused_index();
      int totalItems = candidates_->size();
      NSString *footerIndex =
          [NSString stringWithFormat:@"%d/%d", focusedIndex + 1, totalItems];
      NSAttributedString *footerAttributedIndex =
          [[[NSAttributedString alloc]
             initWithString:footerIndex
                 attributes:kDefaultStyle->footerStyle.attributes]
            autorelease];
      NSSize footerSize = [footerAttributedIndex size];
      NSPoint footerPosition = footerRect.origin;
      footerPosition.x = footerPosition.x + footerRect.size.width -
          footerSize.width - kDefaultStyle->footerStyle.rightPadding;
      [footerAttributedIndex drawAtPoint:footerPosition];
    }
  }
  [pool drain];
}

- (void)drawVScrollBar {
  const mozc::renderer::Rect &vscrollRect = tableLayout_->GetVScrollBarRect();

  if (!vscrollRect.IsRectEmpty() && candidates_->candidate_size() > 0) {
    const int beginIndex = candidates_->candidate(0).index();
    const int candidatesTotal = candidates_->size();
    const int endIndex =
        candidates_->candidate(candidates_->candidate_size() - 1).index();

    [kDefaultStyle->scrollBarBackground set];
    [NSBezierPath fillRect:ToNSRect(vscrollRect)];

    const mozc::renderer::Rect &indicatorRect =
        tableLayout_->GetVScrollIndicatorRect(
            beginIndex, endIndex, candidatesTotal);
    [kDefaultStyle->scrollBarIndicatorColor set];
    [NSBezierPath fillRect:ToNSRect(indicatorRect)];
  }
}

#pragma mark event handling callbacks

const char *Inspect(id obj) {
  return [[NSString stringWithFormat:@"%@", obj] UTF8String];
}

- (void)mouseDown:(NSEvent *)event {
  mozc::renderer::Point localPos = ToPoint(
      [self convertPoint:[event locationInWindow] fromView:nil]);
  int clickedRow = -1;
  for (int i = 0; i < tableLayout_->number_of_rows(); ++i) {
    mozc::renderer::Rect rowRect = tableLayout_->GetRowRect(i);
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
  mozc::renderer::Point localPos = ToPoint(
      [self convertPoint:[event locationInWindow] fromView:nil]);
  if (command_sender_ == NULL) {
    return;
  }

  for (int i = 0; i < tableLayout_->number_of_rows(); ++i) {
    mozc::renderer::Rect rowRect = tableLayout_->GetRowRect(i);
    if (rowRect.PtrInRect(localPos)) {
      SessionCommand command;
      command.set_type(SessionCommand::SELECT_CANDIDATE);
      command.set_id(candidates_->candidate(i).id());
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

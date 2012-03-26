// Copyright 2010-2012, Google Inc.
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

#include "renderer/unix/candidate_window.h"

#include <sstream>

#include "base/base.h"
#include "base/util.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/table_layout.h"
#include "renderer/unix/cairo_factory_interface.h"
#include "renderer/unix/const.h"
#include "renderer/unix/draw_tool.h"
#include "renderer/unix/font_spec.h"
#include "renderer/unix/text_renderer.h"

namespace mozc {
namespace renderer {
namespace gtk {

namespace {
string GetIndexGuideString(const commands::Candidates &candidates) {
  if (!candidates.has_footer() || !candidates.footer().index_visible()) {
    return "";
  }

  const int focused_index = candidates.focused_index();
  const int total_items = candidates.size();

  stringstream footer_string;
  return Util::StringPrintf("%d/%d ", focused_index + 1, total_items);
}

int GetCandidateArrayIndexByCandidateIndex(
    const commands::Candidates &candidates,
    int candidate_index) {
  for (size_t i = 0; i < candidates.candidate_size(); ++i) {
    const commands::Candidates::Candidate &candidate =
        candidates.candidate(i);
    if (candidate.index() == candidate_index) {
      return i;
    }
  }
  return candidates.candidate_size();
}
}  // namespace

CandidateWindow::CandidateWindow(
    TableLayoutInterface *table_layout,
    TextRendererInterface *text_renderer,
    DrawToolInterface *draw_tool,
    GtkWrapperInterface *gtk,
    CairoFactoryInterface *cairo_factory)
    : GtkWindowBase(gtk),
      table_layout_(table_layout),
      text_renderer_(text_renderer),
      draw_tool_(draw_tool),
      cairo_factory_(cairo_factory) {
}

bool CandidateWindow::OnPaint(GtkWidget *widget, GdkEventExpose* event) {
  draw_tool_->Reset(cairo_factory_->CreateCairoInstance(
      GetCanvasWidget()->window));

  DrawBackground();
  DrawShortcutBackground();
  DrawSelectedRect();
  DrawCells();
  DrawInformationIcon();
  DrawVScrollBar();
  DrawFooter();
  DrawFrame();
  return true;
}

void CandidateWindow::DrawBackground() {
  const Rect window_rect(Point(0, 0), GetWindowSize());
  draw_tool_->FillRect(window_rect, kDefaultBackgroundColor);
}

void CandidateWindow::DrawShortcutBackground() {
  if (table_layout_->number_of_columns() <= 0) {
    return;
  }

  const Rect first_column_rect = table_layout_->GetColumnRect(0);
  const Rect first_row_rect = table_layout_->GetRowRect(0);
  if (first_column_rect.IsRectEmpty() || first_row_rect.IsRectEmpty()) {
    return;
  }

  const Rect shortcut_background_area(first_row_rect.origin,
                                      first_column_rect.size);
  draw_tool_->FillRect(shortcut_background_area, kShortcutBackgroundColor);
}

void CandidateWindow::DrawSelectedRect() {
  if (!candidates_.has_focused_index()) {
    return;
  }

  const int selected_row_index = GetCandidateArrayIndexByCandidateIndex(
      candidates_, candidates_.focused_index());

  DCHECK_GE(selected_row_index, 0);

  if (selected_row_index >= candidates_.candidate_size()) {
    LOG(ERROR) << "focused index is invalid" << candidates_.focused_index();
    return;
  }

  const Rect selected_rect = table_layout_->GetRowRect(selected_row_index);
  draw_tool_->FillRect(selected_rect, kSelectedRowBackgroundColor);
  draw_tool_->FrameRect(selected_rect, kSelectedRowFrameColor, 1.0);
}

void CandidateWindow::DrawCells() {
  for (size_t i = 0; i < candidates_.candidate_size(); ++i) {
    const commands::Candidates::Candidate &candidate
        = candidates_.candidate(i);
    string shortcut, value, description;

    GetDisplayString(candidate, &shortcut, &value, &description);

    if (!shortcut.empty()) {
      text_renderer_->RenderText(shortcut,
                                 table_layout_->GetCellRect(i, COLUMN_SHORTCUT),
                                 FontSpec::FONTSET_SHORTCUT);
    }

    if (!value.empty()) {
      text_renderer_->RenderText(
          value,
          table_layout_->GetCellRect(i, COLUMN_CANDIDATE),
          FontSpec::FONTSET_CANDIDATE);
    }

    if (!description.empty()) {
      text_renderer_->RenderText(
          description,
          table_layout_->GetCellRect(i, COLUMN_DESCRIPTION),
          FontSpec::FONTSET_DESCRIPTION);
    }
  }
}

void CandidateWindow::DrawInformationIcon() {
  for (size_t i = 0; i < candidates_.candidate_size(); ++i) {
    if (!candidates_.candidate(i).has_information_id()) {
      continue;
    }
    const Rect row_rect = table_layout_->GetRowRect(i);
    const Rect usage_information_indicator_rect(
        row_rect.origin.x + row_rect.size.width - 6,
        row_rect.origin.y + 2,
        4,
        row_rect.size.height - 4);

    draw_tool_->FillRect(usage_information_indicator_rect, kIndicatorColor);
    draw_tool_->FrameRect(usage_information_indicator_rect, kIndicatorColor, 1);
  }
}

void CandidateWindow::DrawVScrollBar() {
  // TODO(nona): implement this function
}

void CandidateWindow::DrawFooterSeparator(Rect *footer_content_area) {
  DCHECK(footer_content_area);
  const Point dest(footer_content_area->Right(), footer_content_area->Top());
  draw_tool_->DrawLine(footer_content_area->origin, dest, kFrameColor,
                       kFooterSeparatorHeight);
  // The remaining footer content area is the one after removal of above/below
  // separation line.
  footer_content_area->origin.y += kFooterSeparatorHeight;
  footer_content_area->size.height -= kFooterSeparatorHeight;
}

void CandidateWindow::DrawFooterIndex(Rect *footer_content_rect) {
  DCHECK(footer_content_rect);
  if (!candidates_.has_footer() ||
      !candidates_.footer().index_visible() ||
      !candidates_.has_focused_index()) {
    return;
  }

  const string index_guide_string = GetIndexGuideString(candidates_);
  const Size index_guide_size = text_renderer_->GetPixelSize(
      FontSpec::FONTSET_FOOTER_INDEX, index_guide_string);
  // Render as right-aligned.
  Rect index_rect(footer_content_rect->Right() - index_guide_size.width,
                  footer_content_rect->Top(),
                  index_guide_size.width,
                  footer_content_rect->Height());
  text_renderer_->RenderText(index_guide_string,
                             index_rect,
                             FontSpec::FONTSET_FOOTER_INDEX);
  footer_content_rect->size.width -= index_guide_size.width;
}

void CandidateWindow::DrawFooterLabel(const Rect &footer_content_rect) {
  if (footer_content_rect.IsRectEmpty()) {
    return;
  }
  if (candidates_.footer().has_label()) {
    text_renderer_->RenderText(candidates_.footer().label(),
                               footer_content_rect,
                               FontSpec::FONTSET_FOOTER_LABEL);
  } else if (candidates_.footer().has_sub_label()) {
    text_renderer_->RenderText(candidates_.footer().sub_label(),
                               footer_content_rect,
                               FontSpec::FONTSET_FOOTER_SUBLABEL);
  }
}

void CandidateWindow::DrawLogo(Rect *footer_content_rect) {
  DCHECK(footer_content_rect);
  // TODO(nona): Implement this.
  // The current implementation is just a padding area.
  if (candidates_.footer().logo_visible()) {
    // The 47 pixel is same as icon width to be rendered in future.
    footer_content_rect->size.width -= 47;
    footer_content_rect->origin.x += 47;
  }
}

void CandidateWindow::DrawFooter() {
  if (!candidates_.has_footer()) {
    return;
  }

  Rect footer_content_area = table_layout_->GetFooterRect();
  if (footer_content_area.IsRectEmpty()) {
    return;
  }

  DrawFooterSeparator(&footer_content_area);
  DrawLogo(&footer_content_area);
  DrawFooterIndex(&footer_content_area);
  DrawFooterLabel(footer_content_area);
}

void CandidateWindow::DrawFrame() {
  const Rect client_rect(Point(0, 0), table_layout_->GetTotalSize());
  draw_tool_->FrameRect(client_rect, kFrameColor, 1);
}

void CandidateWindow::Initialize() {
  text_renderer_->Initialize(GetCanvasWidget()->window);
}

void CandidateWindow::UpdateScrollBarSize() {
  // TODO(nona) : Implement this.
}

void CandidateWindow::UpdateFooterSize() {
  if (!candidates_.has_footer()) {
    return;
  }

  Size footer_size(0, 0);

  if (candidates_.footer().has_label()) {
    const Size label_string_size = text_renderer_->GetPixelSize(
        FontSpec::FONTSET_FOOTER_LABEL,
        candidates_.footer().label());
    footer_size.width += label_string_size.width;
    footer_size.height = max(footer_size.height, label_string_size.height);
  } else if (candidates_.footer().has_sub_label()) {
    const Size label_string_size = text_renderer_->GetPixelSize(
        FontSpec::FONTSET_FOOTER_LABEL,
        candidates_.footer().sub_label());
    footer_size.width += label_string_size.width;
    footer_size.height = max(footer_size.height, label_string_size.height);
  }

  if (candidates_.footer().index_visible()) {
    const Size index_guide_size = text_renderer_->GetPixelSize(
      FontSpec::FONTSET_FOOTER_INDEX,
      GetIndexGuideString(candidates_));
    footer_size.width += index_guide_size.width;
    footer_size.height = max(footer_size.height, index_guide_size.height);
  }

  if (candidates_.candidate_size() < candidates_.size()) {
    const Size minimum_size = text_renderer_->GetPixelSize(
      FontSpec::FONTSET_CANDIDATE,
      kMinimumCandidateAndDescriptionWidthAsString);
    table_layout_->EnsureColumnsWidth(
      COLUMN_CANDIDATE, COLUMN_DESCRIPTION, minimum_size.width);
  }

  if (candidates_.footer().logo_visible()) {
    // TODO(nona): Implement logo sizing.
    footer_size.width += 47;
  }
  footer_size.height += kFooterSeparatorHeight;

  table_layout_->EnsureFooterSize(footer_size);
}

void CandidateWindow::UpdateGap1Size() {
  const Size gap1_size =
      text_renderer_->GetPixelSize(FontSpec::FONTSET_CANDIDATE, " ");
  table_layout_->EnsureCellSize(COLUMN_GAP1, gap1_size);
}

void CandidateWindow::UpdateCandidatesSize(bool *has_description) {
  DCHECK(has_description);
  *has_description = false;
  for (size_t i = 0; i < candidates_.candidate_size(); ++i) {
    const commands::Candidates::Candidate &candidate =
        candidates_.candidate(i);

    string shortcut, description, candidate_string;
    GetDisplayString(candidate, &shortcut, &candidate_string, &description);

    if (!shortcut.empty()) {
      string text;
      text.push_back(' ');
      text.append(shortcut);
      text.push_back(' ');
      const Size rendering_size = text_renderer_->GetPixelSize(
          FontSpec::FONTSET_SHORTCUT, text);
      table_layout_->EnsureCellSize(COLUMN_SHORTCUT, rendering_size);
    }

    if (!candidate_string.empty()) {
      const Size rendering_size = text_renderer_->GetPixelSize(
          FontSpec::FONTSET_CANDIDATE, candidate_string);
      table_layout_->EnsureCellSize(COLUMN_CANDIDATE, rendering_size);
    }

    if (!description.empty()) {
      string text;
      text.append(description);
      text.push_back(' ');
      const Size rendering_size = text_renderer_->GetPixelSize(
          FontSpec::FONTSET_DESCRIPTION, text);
      table_layout_->EnsureCellSize(COLUMN_DESCRIPTION, rendering_size);
      *has_description = true;
    }
  }
}

void CandidateWindow::UpdateGap2Size(bool has_description) {
  const char *gap2_string = (has_description ? "   " : " ");
  const Size gap2_size = text_renderer_->GetPixelSize(
      FontSpec::FONTSET_CANDIDATE, gap2_string);
  table_layout_->EnsureCellSize(COLUMN_GAP2, gap2_size);
}

Size CandidateWindow::Update(const commands::Candidates &candidates) {
  DCHECK(
      (candidates_.category()  == commands::CONVERSION) ||
      (candidates_.category()  == commands::PREDICTION) ||
      (candidates_.category()  == commands::TRANSLITERATION) ||
      (candidates_.category()  == commands::SUGGESTION) ||
      (candidates_.category()  == commands::USAGE))
      << "Unknown candidate category" << candidates_.category();

  candidates_.CopyFrom(candidates);

  table_layout_->Initialize(candidates_.candidate_size(), NUMBER_OF_COLUMNS);
  table_layout_->SetWindowBorder(kWindowBorder);
  table_layout_->SetRowRectPadding(kRowRectPadding);

  UpdateScrollBarSize();
  UpdateFooterSize();
  UpdateGap1Size();
  bool has_description;
  UpdateCandidatesSize(&has_description);
  UpdateGap2Size(has_description);

  table_layout_->FreezeLayout();
  Resize(table_layout_->GetTotalSize());
  Redraw();
  return table_layout_->GetTotalSize();
}

void CandidateWindow::GetDisplayString(
    const commands::Candidates::Candidate &candidate,
    string *shortcut,
    string *value,
    string *description) {
  DCHECK(shortcut);
  DCHECK(value);
  DCHECK(description);

  shortcut->clear();
  value->clear();
  description->clear();

  if (!candidate.has_value()) {
    return;
  }
  value->assign(candidate.value());

  if (!candidate.has_annotation()) {
    return;
  }

  const commands::Annotation &annotation = candidate.annotation();

  if (annotation.has_shortcut()) {
    shortcut->assign(annotation.shortcut());
  }

  if (annotation.has_description()) {
    description->assign(annotation.description());
  }

  if (annotation.has_prefix()) {
    value->assign(annotation.prefix());
    value->append(candidate.value());
  }

  if (annotation.has_suffix()) {
    value->append(annotation.suffix());
  }
}

Rect CandidateWindow::GetCandidateColumnInClientCord() const {
  DCHECK(table_layout_->IsLayoutFrozen()) << "Table layout is not frozen.";
  return table_layout_->GetCellRect(0, COLUMN_CANDIDATE);
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

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

#include "win32/tip/tip_ui_renderer_immersive.h"

// clang-format off
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atltypes.h>
#include <atlapp.h>
#include <atlmisc.h>
// clang-format on

#include <memory>

#include "base/util.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "renderer/table_layout.h"
#include "renderer/win32/text_renderer.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

using WTL::CBitmap;
using WTL::CBitmapHandle;
using WTL::CDC;

using ::mozc::commands::Candidates;
using ::mozc::commands::Output;
using ::mozc::renderer::TableLayout;
using ::mozc::renderer::TableLayoutInterface;
using ::mozc::renderer::win32::TextRenderer;
using ::mozc::renderer::win32::TextRenderingInfo;
typedef ::mozc::commands::Preedit_Segment::Annotation Annotation;
typedef ::mozc::commands::Candidates_Candidate Candidate;

// DPI-invariant layout size constants in pixel unit.
constexpr int kWindowBorder = 2;
constexpr int kRowRectPadding = 4;
constexpr int kIndicatorWidth = 4;

// Color scheme
const COLORREF kFrameColor = RGB(0x00, 0x00, 0x00);
const COLORREF kSelectedRowBackgroundColor = RGB(0xd1, 0xea, 0xff);
const COLORREF kDefaultBackgroundColor = RGB(0xff, 0xff, 0xff);
const COLORREF kIndicatorBackgroundColor = RGB(0xe0, 0xe0, 0xe0);
const COLORREF kIndicatorColor = RGB(0xb8, 0xb8, 0xb8);

// usage type for each column.
enum COLUMN_TYPE {
  COLUMN_GAP1,        // padding region
  COLUMN_CANDIDATE,   // show candidate string
  COLUMN_GAP2,        // padding region
  NUMBER_OF_COLUMNS,  // number of columns. (this item should be last)
};

CRect ToCRect(const Rect &rect) {
  return CRect(rect.Left(), rect.Top(), rect.Right(), rect.Bottom());
}

// Returns the smallest index of the given candidate list which satisfies
// candidates.candidate(i) == |candidate_index|.
// This function returns the size of the given candidate list when there
// aren't any candidates satisfying the above condition.
int GetCandidateArrayIndexByCandidateIndex(const Candidates &candidates,
                                           int candidate_index) {
  for (size_t i = 0; i < candidates.candidate_size(); ++i) {
    const Candidate &candidate = candidates.candidate(i);

    if (candidate.index() == candidate_index) {
      return i;
    }
  }

  return candidates.candidate_size();
}

// Returns the smallest index of the given candidate list which satisfies
// |candidates.focused_index| == |candidates.candidate(i).index()|.
// This function returns the size of the given candidate list when there
// aren't any candidates satisfying the above condition.
int GetFocusedArrayIndex(const Candidates &candidates) {
  const int invalid_index = candidates.candidate_size();

  if (!candidates.has_focused_index()) {
    return invalid_index;
  }

  const int focused_index = candidates.focused_index();

  return GetCandidateArrayIndexByCandidateIndex(candidates, focused_index);
}

void CalcLayout(const Candidates &candidates, const TextRenderer &text_renderer,
                TableLayout *table_layout,
                std::vector<std::wstring> *candidate_strings) {
  table_layout->Initialize(candidates.candidate_size(), NUMBER_OF_COLUMNS);

  table_layout->SetWindowBorder(kWindowBorder);

  // Add a positional indicator if candidate list consists of more than one
  // page.
  if (candidates.candidate_size() < candidates.size()) {
    table_layout->SetVScrollBar(kIndicatorWidth);
  }

  table_layout->SetRowRectPadding(kRowRectPadding);

  // put a padding in COLUMN_GAP1.
  // the width is determined to be equal to the width of " ".
  const Size gap1_size =
      text_renderer.MeasureString(TextRenderer::FONTSET_CANDIDATE, L" ");
  table_layout->EnsureCellSize(COLUMN_GAP1, gap1_size);

  for (size_t i = 0; i < candidates.candidate_size(); ++i) {
    std::wstring candidate_string;
    const Candidate &candidate = candidates.candidate(i);
    if (candidate.has_value()) {
      mozc::Util::Utf8ToWide(candidate.value(), &candidate_string);
    }
    if (candidate.has_annotation()) {
      const commands::Annotation &annotation = candidate.annotation();
      if (annotation.has_prefix()) {
        std::wstring annotation_prefix;
        mozc::Util::Utf8ToWide(annotation.prefix(), &annotation_prefix);
        candidate_string = annotation_prefix + candidate_string;
      }
      if (annotation.has_suffix()) {
        std::wstring annotation_suffix;
        mozc::Util::Utf8ToWide(annotation.suffix(), &annotation_suffix);
        candidate_string += annotation_suffix;
      }
    }
    candidate_strings->push_back(candidate_string);
    if (!candidate_string.empty()) {
      const Size rendering_size = text_renderer.MeasureString(
          TextRenderer::FONTSET_CANDIDATE, candidate_string);
      table_layout->EnsureCellSize(COLUMN_CANDIDATE, rendering_size);
    }
  }

  // Put a padding in COLUMN_GAP2.
  const wchar_t *gap2_string = L" ";
  const Size gap2_size =
      text_renderer.MeasureString(TextRenderer::FONTSET_CANDIDATE, gap2_string);
  table_layout->EnsureCellSize(COLUMN_GAP2, gap2_size);
  table_layout->FreezeLayout();
}

CBitmapHandle RenderImpl(const Candidates &candidates,
                         const TableLayout &table_layout,
                         const TextRenderer &text_renderer,
                         const std::vector<std::wstring> &candidate_strings) {
  const int width = table_layout.GetTotalSize().width;
  const int height = table_layout.GetTotalSize().height;

  CBitmap bitmap;
  bitmap.CreateBitmap(width, height, 1, 32, nullptr);
  CDC dc;
  dc.CreateCompatibleDC();
  CBitmapHandle old_bitmap = dc.SelectBitmap(bitmap);

  dc.SetBkMode(TRANSPARENT);
  const CRect client_crect(0, 0, width, height);

  // Background
  { dc.FillSolidRect(&client_crect, kDefaultBackgroundColor); }

  // Focused rectangle
  {
    const int focused_array_index = GetFocusedArrayIndex(candidates);
    if (0 <= focused_array_index &&
        focused_array_index < candidates.candidate_size()) {
      const commands::Candidates::Candidate &candidate =
          candidates.candidate(focused_array_index);

      const CRect selected_rect =
          ToCRect(table_layout.GetRowRect(focused_array_index));
      dc.FillSolidRect(&selected_rect, kSelectedRowBackgroundColor);
    }
  }

  // Candidate strings
  {
    const COLUMN_TYPE column_type = COLUMN_CANDIDATE;
    const TextRenderer::FONT_TYPE font_type = TextRenderer::FONTSET_CANDIDATE;

    std::vector<TextRenderingInfo> display_list;
    for (size_t i = 0; i < candidate_strings.size(); ++i) {
      const std::wstring &candidate_string = candidate_strings[i];
      const Rect &text_rect = table_layout.GetCellRect(i, column_type);
      display_list.push_back(TextRenderingInfo(candidate_string, text_rect));
    }
    text_renderer.RenderTextList(dc.m_hDC, display_list, font_type);
  }

  // Indicator
  {
    const Rect &vscroll_rect = table_layout.GetVScrollBarRect();

    if (!vscroll_rect.IsRectEmpty() && candidates.candidate_size() > 0) {
      const int begin_index = candidates.candidate(0).index();
      const int candidates_in_page = candidates.candidate_size();
      const int candidates_total = candidates.size();
      const int end_index =
          candidates.candidate(candidates_in_page - 1).index();

      const CRect background_crect = ToCRect(vscroll_rect);
      dc.FillSolidRect(&background_crect, kIndicatorBackgroundColor);

      const Rect &indicator_rect = table_layout.GetVScrollIndicatorRect(
          begin_index, end_index, candidates_total);

      const CRect indicator_crect = ToCRect(indicator_rect);
      dc.FillSolidRect(&indicator_crect, kIndicatorColor);
    }
  }

  // Edge frame
  {
    // DC brush is available in Windows 2000 and later.
    dc.SetDCBrushColor(kFrameColor);
    CRect crect = client_crect;
    for (int i = 0; i < kWindowBorder; ++i) {
      dc.FrameRect(&crect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
      crect.DeflateRect(1, 1, 1, 1);
    }
  }

  dc.SelectBitmap(old_bitmap);
  return bitmap.Detach();
}

}  // namespace

HBITMAP TipUiRendererImmersive::Render(
    const Candidates &candidates,
    const renderer::win32::TextRenderer *text_renderer,
    renderer::TableLayout *table_layout, SIZE *size, int *left_align_offset) {
  std::vector<std::wstring> candidate_strings;
  CalcLayout(candidates, *text_renderer, table_layout, &candidate_strings);

  const Size &total_size = table_layout->GetTotalSize();
  if (size != nullptr) {
    size->cx = total_size.width;
    size->cy = total_size.height;
  }
  if (left_align_offset != nullptr) {
    *left_align_offset = table_layout->GetColumnRect(COLUMN_CANDIDATE).Left();
  }
  return RenderImpl(candidates, *table_layout, *text_renderer,
                    candidate_strings);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

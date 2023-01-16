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

#include "unix/ibus/ibus_candidate_window_handler.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "base/logging.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "unix/ibus/ibus_wrapper.h"
#include "absl/strings/str_format.h"

namespace mozc {
namespace ibus {

namespace {

// Maximum Number of candidate words per page.
constexpr size_t kPageSize = 9;

// Returns a text for the candidate footer.
std::string GetFooterText(const commands::Candidates &candidates) {
  if (!candidates.has_footer()) {
    return "";
  }

  const commands::Footer &footer = candidates.footer();
  std::string text;
  if (footer.has_label()) {
    // TODO(yusukes,mozc-team): label() is not localized. Currently, it's always
    // written in Japanese (in UTF-8).
    text = footer.label();
  } else if (footer.has_sub_label()) {
    // Windows client shows sub_label() only when label() is not specified. We
    // follow the policy.
    text = footer.sub_label();
  }

  if (footer.has_index_visible() && footer.index_visible() &&
      candidates.has_focused_index()) {
    text += (text.empty() ? "" : " ") +
            absl::StrFormat("%d/%d", candidates.focused_index() + 1,
                            candidates.size());
  }
  return text;
}
}  // namespace

void IBusCandidateWindowHandler::Update(IbusEngineWrapper *engine,
                                        const commands::Output &output) {
  UpdateCandidates(engine, output);
  UpdateAuxiliaryText(engine, output);
}

void IBusCandidateWindowHandler::UpdateCursorRect(IbusEngineWrapper *engine) {
  // Nothing to do because IBus takes care of where to show its candidate
  // window.
}

void IBusCandidateWindowHandler::Hide(IbusEngineWrapper *engine) {
  engine->HideLookupTable();
  engine->HideAuxiliaryText();
}

void IBusCandidateWindowHandler::Show(IbusEngineWrapper *engine) {
  engine->ShowLookupTable();
  engine->ShowAuxiliaryText();
}

// TODO(hsumita): Writes test for this method.
bool IBusCandidateWindowHandler::UpdateCandidates(
    IbusEngineWrapper *engine, const commands::Output &output) {
  if (!output.has_candidates() || output.candidates().candidate_size() == 0) {
    engine->HideLookupTable();
    return true;
  }

  const commands::Candidates &candidates = output.candidates();
  const bool cursor_visible = candidates.has_focused_index();
  int cursor_pos = 0;
  if (candidates.has_focused_index()) {
    for (int i = 0; i < candidates.candidate_size(); ++i) {
      if (candidates.focused_index() == candidates.candidate(i).index()) {
        cursor_pos = i;
      }
    }
  }

  size_t page_size = kPageSize;
  if (candidates.has_category() &&
      candidates.category() == commands::SUGGESTION &&
      page_size > candidates.candidate_size()) {
    page_size = candidates.candidate_size();
  }
  IbusLookupTableWrapper table(page_size, cursor_pos, cursor_visible);
  if (candidates.direction() == commands::Candidates::VERTICAL) {
    table.SetOrientation(IBUS_ORIENTATION_VERTICAL);
  } else {
    table.SetOrientation(IBUS_ORIENTATION_HORIZONTAL);
  }

  for (int i = 0; i < candidates.candidate_size(); ++i) {
    const commands::Candidates::Candidate &candidate = candidates.candidate(i);
    table.AppendCandidate(candidate.value());

    const bool has_label =
        candidate.has_annotation() && candidate.annotation().has_shortcut();
    // Need to append an empty string when the candidate does not have a
    // shortcut. Otherwise the ibus lookup table shows numeric labels.
    table.AppendLabel(has_label ? candidate.annotation().shortcut() : "");
  }

  // `table` is released by UpdateLookupTable.
  engine->UpdateLookupTable(&table);
  return true;
}

// TODO(hsumita): Writes test for this method.
bool IBusCandidateWindowHandler::UpdateAuxiliaryText(
    IbusEngineWrapper *engine, const commands::Output &output) {
  if (!output.has_candidates()) {
    engine->HideAuxiliaryText();
    return true;
  }

  const std::string footer_text = GetFooterText(output.candidates());
  if (footer_text.empty()) {
    engine->HideAuxiliaryText();
  } else {
    engine->UpdateAuxiliaryText(footer_text);
  }
  return true;
}

void IBusCandidateWindowHandler::OnIBusCustomFontDescriptionChanged(
    const std::string &custom_font_description) {
  // Do nothing
  // The custom font description is managed by ibus directly.
}

void IBusCandidateWindowHandler::OnIBusUseCustomFontDescriptionChanged(
    bool use_custom_font_description) {
  // Do nothing
  // The custom font description is managed by ibus directly.
}

}  // namespace ibus
}  // namespace mozc

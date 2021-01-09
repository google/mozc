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

#include "base/logging.h"
#include "protocol/commands.pb.h"
#include "unix/ibus/mozc_engine_property.h"

namespace mozc {
namespace ibus {

namespace {

// Returns an IBusText used for showing the auxiliary text in the candidate
// window. Returns nullptr if no text has to be shown. Caller must release the
// returned IBusText object.
IBusText *ComposeAuxiliaryText(const commands::Candidates &candidates) {
  if (!candidates.has_footer()) {
    // We don't have to show the auxiliary text.
    return nullptr;
  }
  const commands::Footer &footer = candidates.footer();

  std::string auxiliary_text;
  if (footer.has_label()) {
    // TODO(yusukes,mozc-team): label() is not localized. Currently, it's always
    // written in Japanese (in UTF-8).
    auxiliary_text = footer.label();
  } else if (footer.has_sub_label()) {
    // Windows client shows sub_label() only when label() is not specified. We
    // follow the policy.
    auxiliary_text = footer.sub_label();
  }

  if (footer.has_index_visible() && footer.index_visible() &&
      candidates.has_focused_index()) {
    // Max size of candidates is 200 so 128 is sufficient size for the buffer.
    char index_buf[128] = {0};
    const int result =
        snprintf(index_buf, sizeof(index_buf) - 1, "%s%d/%d",
                 (auxiliary_text.empty() ? "" : " "),
                 candidates.focused_index() + 1, candidates.size());
    DCHECK_GE(result, 0) << "snprintf in ComposeAuxiliaryText failed";
    auxiliary_text += index_buf;
  }
  return auxiliary_text.empty()
             ? nullptr
             : ibus_text_new_from_string(auxiliary_text.c_str());
}
}  // namespace

IBusCandidateWindowHandler::IBusCandidateWindowHandler() {}

IBusCandidateWindowHandler::~IBusCandidateWindowHandler() {}

void IBusCandidateWindowHandler::Update(IBusEngine *engine,
                                        const commands::Output &output) {
  UpdateCandidates(engine, output);
  UpdateAuxiliaryText(engine, output);
}

void IBusCandidateWindowHandler::UpdateCursorRect(IBusEngine *engine) {
  // Nothing to do because IBus takes care of where to show its candidate
  // window.
}

void IBusCandidateWindowHandler::Hide(IBusEngine *engine) {
  ibus_engine_hide_lookup_table(engine);
  ibus_engine_hide_auxiliary_text(engine);
}

void IBusCandidateWindowHandler::Show(IBusEngine *engine) {
  ibus_engine_show_lookup_table(engine);
  ibus_engine_show_auxiliary_text(engine);
}

// TODO(hsumita): Writes test for this method.
bool IBusCandidateWindowHandler::UpdateCandidates(
    IBusEngine *engine, const commands::Output &output) {
  if (!output.has_candidates() || output.candidates().candidate_size() == 0) {
    ibus_engine_hide_lookup_table(engine);
    return true;
  }

  const gboolean kRound = TRUE;
  const commands::Candidates &candidates = output.candidates();
  const gboolean cursor_visible = candidates.has_focused_index() ? TRUE : FALSE;
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
  IBusLookupTable *table =
      ibus_lookup_table_new(page_size, cursor_pos, cursor_visible, kRound);
  if (candidates.direction() == commands::Candidates::VERTICAL) {
    ibus_lookup_table_set_orientation(table, IBUS_ORIENTATION_VERTICAL);
  } else {
    ibus_lookup_table_set_orientation(table, IBUS_ORIENTATION_HORIZONTAL);
  }

  for (int i = 0; i < candidates.candidate_size(); ++i) {
    const commands::Candidates::Candidate &candidate = candidates.candidate(i);
    IBusText *text = ibus_text_new_from_string(candidate.value().c_str());
    ibus_lookup_table_append_candidate(table, text);
    // |text| is released by ibus_engine_update_lookup_table along with |table|.

    const bool has_label =
        candidate.has_annotation() && candidate.annotation().has_shortcut();
    // Need to append an empty string when the candidate does not have a
    // shortcut. Otherwise the ibus lookup table shows numeric labels.
    IBusText *label = ibus_text_new_from_string(
        has_label ? candidate.annotation().shortcut().c_str() : "");
    ibus_lookup_table_append_label(table, label);
    // |label| is released by ibus_engine_update_lookup_table along with
    // |table|.
  }

  ibus_engine_update_lookup_table(engine, table, TRUE);
  // |table| is released by ibus_engine_update_lookup_table.
  return true;
}

// TODO(hsumita): Writes test for this method.
bool IBusCandidateWindowHandler::UpdateAuxiliaryText(
    IBusEngine *engine, const commands::Output &output) {
  if (!output.has_candidates()) {
    ibus_engine_hide_auxiliary_text(engine);
    return true;
  }

  IBusText *auxiliary_text = ComposeAuxiliaryText(output.candidates());
  if (auxiliary_text) {
    ibus_engine_update_auxiliary_text(engine, auxiliary_text, TRUE);
    // |auxiliary_text| is released by ibus_engine_update_auxiliary_text.
  } else {
    ibus_engine_hide_auxiliary_text(engine);
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

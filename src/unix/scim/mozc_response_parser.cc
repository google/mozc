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

#define Uses_SCIM_FRONTEND_MODULE

#include "unix/scim/mozc_response_parser.h"

#include <scim.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "session/commands.pb.h"
#include "unix/scim/mozc_lookup_table.h"
#include "unix/scim/scim_mozc.h"

namespace {

// Returns true if the candidate window contains only suggestions.
bool IsSuggestion(const mozc::commands::Candidates &candidates) {
  return !candidates.has_focused_index();
}

// Returns a position that determines a preedit cursor position _AND_ top-left
// position of a candidate window. Note that we can't set these two positions
// independently. That's a SCIM's limitation.
uint32 GetCursorPosition(const mozc::commands::Output &response) {
  if (!response.has_preedit()) {
    return 0;
  }
  if (response.preedit().has_highlighted_position()) {
    return response.preedit().highlighted_position();
  }
  return response.preedit().cursor();
}

string CreateCandidatesWindowTitle(
    const mozc_unix_scim::MozcLookupTable& candidates) {
  char buf[128];
  ::snprintf(buf, sizeof(buf),
             "%u/%u", candidates.focused(), candidates.size());
  return buf;
}

string CreateDescriptionString(const string &description) {
  return " [" + description + "]";
}

}  // namespace

namespace mozc_unix_scim {

MozcResponseParser::MozcResponseParser()
    : use_annotation_(false) {
}

MozcResponseParser::~MozcResponseParser() {
}

bool MozcResponseParser::ParseResponse(const mozc::commands::Output &response,
                                       ScimMozc *scim_mozc) const {
  DCHECK(scim_mozc);
  if (!scim_mozc) {
    return false;
  }

  // We should check the mode field first since the response for a
  // SWITCH_INPUT_MODE request only contains mode and id fields.
  if (response.has_mode()) {
    scim_mozc->SetCompositionMode(response.mode());
  }

  if (!response.consumed()) {
    // The key was not consumed by Mozc.
    return false;
  }

  if (response.has_result()) {
    const mozc::commands::Result &result = response.result();
    ParseResult(result, scim_mozc);
  }

  // First, determine the cursor position.
  if (response.has_preedit()) {
    const mozc::commands::Preedit &preedit = response.preedit();
    ParsePreedit(preedit, GetCursorPosition(response), scim_mozc);
  }

  // Then show the candidate window.
  if (response.has_candidates()) {
    const mozc::commands::Candidates &candidates = response.candidates();
    ParseCandidates(candidates, scim_mozc);
  }

  if (response.has_url()) {
    const string &url = response.url();
    scim_mozc->SetUrl(url);
  }

  return true;  // mozc consumed the key.
}

void MozcResponseParser::set_use_annotation(bool use_annotation) {
  use_annotation_ = use_annotation;
}

void MozcResponseParser::ParseResult(const mozc::commands::Result &result,
                                     ScimMozc *scim_mozc) const {
  switch (result.type()) {
    case mozc::commands::Result::NONE: {
      scim_mozc->SetAuxString("No result");  // not a fatal error.
      break;
    }
    case mozc::commands::Result::STRING: {
      const scim::WideString &result_string
          = scim::utf8_mbstowcs(result.value());
      scim_mozc->SetResultString(result_string);
      break;
    }
  }
}

void MozcResponseParser::ParseCandidates(
    const mozc::commands::Candidates &candidates, ScimMozc *scim_mozc) const {
  const char kNoShortcutLabel[] = "";
  int focused_index = -1;
  if (candidates.has_focused_index()) {
    focused_index = candidates.focused_index();
  }
  int local_index = -1;

  vector<scim::WideString> labels;
  vector<scim::WideString> values;
  vector<int32> *unique_ids = new vector<int32>;
  for (int i = 0; i < candidates.candidate_size(); ++i) {
    const uint32 index = candidates.candidate(i).index();
    if (focused_index != -1 && index == focused_index) {
      local_index = i;
    }

    string shortcut = kNoShortcutLabel;
    if (candidates.candidate(i).has_annotation() &&
        candidates.candidate(i).annotation().has_shortcut()) {
      shortcut = candidates.candidate(i).annotation().shortcut();
      // Note: scim-1.4 assumes the label always contains just one character.
      if (shortcut.size() > 1) {
        LOG(ERROR) << "Bad shortcut: " << shortcut;
        shortcut = kNoShortcutLabel;
      }
    }
    labels.push_back(scim::utf8_mbstowcs(shortcut));

    string value;
    if (use_annotation_ &&
        candidates.candidate(i).has_annotation() &&
        candidates.candidate(i).annotation().has_prefix()) {
      value = candidates.candidate(i).annotation().prefix();
    }
    value += candidates.candidate(i).value();
    if (use_annotation_ &&
        candidates.candidate(i).has_annotation() &&
        candidates.candidate(i).annotation().has_suffix()) {
      value += candidates.candidate(i).annotation().suffix();
    }
    if (use_annotation_ &&
        candidates.candidate(i).has_annotation() &&
        candidates.candidate(i).annotation().has_description()) {
      // Display descriptions ([HALF][KATAKANA], [GREEK], [Black square], etc).
      value += CreateDescriptionString(
          candidates.candidate(i).annotation().description());
    }
    values.push_back(scim::utf8_mbstowcs(value));

    if (candidates.candidate(i).has_id()) {
      const int32 id = candidates.candidate(i).id();
      DCHECK_NE(kBadCandidateId, id) << "Unexpected id is passed.";
      unique_ids->push_back(id);
    } else {
      // The parent node of the cascading window does not have an id since the
      // node does not contain a candidate word.
      unique_ids->push_back(kBadCandidateId);
    }
  }

  if (focused_index != -1 && local_index == -1) {
    LOG(ERROR) << "Bad focused_index: " << focused_index;
    local_index = 0;
  }

  MozcLookupTable *lookup_table = new MozcLookupTable(
      labels, values, unique_ids, candidates.size(), focused_index + 1);
  if (focused_index != -1) {
    lookup_table->show_cursor(true);
    lookup_table->set_cursor_pos(local_index);
  } else {
    lookup_table->show_cursor(false);
  }

  scim_mozc->SetCandidateWindow(lookup_table);
  if (IsSuggestion(candidates)) {
    scim_mozc->SetAuxString("");
  } else {
    scim_mozc->SetAuxString(CreateCandidatesWindowTitle(*lookup_table));
  }
}

void MozcResponseParser::ParsePreedit(const mozc::commands::Preedit &preedit,
                                      uint32 position,
                                      ScimMozc *scim_mozc) const {
  PreeditInfo *info = new PreeditInfo;

  for (int i = 0; i < preedit.segment_size(); ++i) {
    const mozc::commands::Preedit_Segment &segment = preedit.segment(i);
    const scim::WideString &str = scim::utf8_mbstowcs(segment.value());

    switch (segment.annotation()) {
      case mozc::commands::Preedit_Segment::NONE:
        info->attribute_list.push_back(
            scim::Attribute(info->str.size(), str.size()));
        break;
      case mozc::commands::Preedit_Segment::UNDERLINE:
        info->attribute_list.push_back(
            scim::Attribute(info->str.size(), str.size(),
                            scim::SCIM_ATTR_DECORATE,
                            scim::SCIM_ATTR_DECORATE_UNDERLINE));
        break;
      case mozc::commands::Preedit_Segment::HIGHLIGHT:
        info->attribute_list.push_back(
            scim::Attribute(info->str.size(), str.size(),
                            scim::SCIM_ATTR_DECORATE,
                            scim::SCIM_ATTR_DECORATE_HIGHLIGHT));
        break;
    }
    info->str += str;
  }
  info->cursor_pos = position;

  scim_mozc->SetPreeditInfo(info);
}

}  // namespace mozc_unix_scim

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

#include "unix/fcitx/mozc_response_parser.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "session/commands.pb.h"
#include "unix/fcitx/fcitx_mozc.h"
#include <fcitx/candidate.h>

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

string CreateDescriptionString(const string &description) {
    return " [" + description + "]";
}

}  // namespace

namespace mozc {

namespace fcitx {

MozcResponseParser::MozcResponseParser()
        : use_annotation_(false) {
}

MozcResponseParser::~MozcResponseParser() {
}

bool MozcResponseParser::ParseResponse(const mozc::commands::Output &response,
                                       FcitxMozc *fcitx_mozc) const {
    DCHECK(fcitx_mozc);
    if (!fcitx_mozc) {
        return false;
    }

    // We should check the mode field first since the response for a
    // SWITCH_INPUT_MODE request only contains mode and id fields.
    if (response.has_mode()) {
        fcitx_mozc->SetCompositionMode(response.mode());
    }

    if (!response.consumed()) {
        // The key was not consumed by Mozc.
        return false;
    }

    if (response.has_result()) {
        const mozc::commands::Result &result = response.result();
        ParseResult(result, fcitx_mozc);
    }

    // First, determine the cursor position.
    if (response.has_preedit()) {
        const mozc::commands::Preedit &preedit = response.preedit();
        ParsePreedit(preedit, GetCursorPosition(response), fcitx_mozc);
    }

    // Then show the candidate window.
    if (response.has_candidates()) {
        const mozc::commands::Candidates &candidates = response.candidates();
        ParseCandidates(candidates, fcitx_mozc);
    }

    if (response.has_url()) {
        const string &url = response.url();
        fcitx_mozc->SetUrl(url);
    }

    return true;  // mozc consumed the key.
}

void MozcResponseParser::set_use_annotation(bool use_annotation) {
    use_annotation_ = use_annotation;
}

void MozcResponseParser::ParseResult(const mozc::commands::Result &result,
                                     FcitxMozc *fcitx_mozc) const {
    switch (result.type()) {
    case mozc::commands::Result::NONE: {
        fcitx_mozc->SetAuxString("No result");  // not a fatal error.
        break;
    }
    case mozc::commands::Result::STRING: {
        fcitx_mozc->SetResultString(result.value());
        break;
    }
    }
}

void MozcResponseParser::ParseCandidates(
    const mozc::commands::Candidates &candidates, FcitxMozc *fcitx_mozc) const {
    FcitxCandidateWordReset(FcitxInputStateGetCandidateList(fcitx_mozc->GetInputState()));
    for (int i = 0; i < candidates.candidate_size(); ++i) {
        int32* id = (int32*) fcitx_utils_malloc0(sizeof(int32));
        FcitxCandidateWord candWord;
        candWord.callback = FcitxMozcGetCandidateWord;
        candWord.extraType = MSG_INPUT;
        candWord.strExtra = NULL;
        candWord.priv = id;
        candWord.strWord = NULL;
        candWord.wordType = MSG_INPUT;
        candWord.owner = fcitx_mozc;

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

        candWord.strWord = strdup(value.c_str());
        
        if (candidates.candidate(i).has_id()) {
            const int32 cid = candidates.candidate(i).id();
            DCHECK_NE(kBadCandidateId, cid) << "Unexpected id is passed.";
            *id = cid;
        } else {
            // The parent node of the cascading window does not have an id since the
            // node does not contain a candidate word.
            *id = kBadCandidateId;
        }
        FcitxCandidateWordAppend(FcitxInputStateGetCandidateList(fcitx_mozc->GetInputState()), &candWord);
    }
}

static int GetRawCursorPos(const char * str, int upos)
{
    unsigned int i;
    int pos = 0;
    for (i = 0; i < upos; i++) {
        pos += fcitx_utf8_char_len(fcitx_utf8_get_nth_char((char*)str, i));
    }
    return pos;
}


void MozcResponseParser::ParsePreedit(const mozc::commands::Preedit &preedit,
                                      uint32 position,
                                      FcitxMozc *fcitx_mozc) const {
    PreeditInfo *info = new PreeditInfo;
    std::string s;

    for (int i = 0; i < preedit.segment_size(); ++i) {
        const mozc::commands::Preedit_Segment &segment = preedit.segment(i);
        const std::string &str = segment.value();
        FcitxMessageType type;

        switch (segment.annotation()) {
        case mozc::commands::Preedit_Segment::NONE:
            type = MSG_INPUT;
            break;
        case mozc::commands::Preedit_Segment::UNDERLINE:
            type = MSG_TIPS;
            break;
        case mozc::commands::Preedit_Segment::HIGHLIGHT:
            type = MSG_CODE;

            break;
        }
        s += str;
        
        PreeditItem item;
        item.type = type;
        item.str = str;
        info->preedit.push_back(item);
    }
    info->cursor_pos = GetRawCursorPos(s.c_str(), position);

    fcitx_mozc->SetPreeditInfo(info);
}

}  // namespace fcitx

}  // namespace mozc

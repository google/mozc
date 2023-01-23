// Copyright 2010-2012, Google Inc.
// Copyright 2012~2013, Weng Xuetian <wengxt@gmail.com>
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
#include "base/util.h"
#include "protocol/commands.pb.h"
#include "unix/fcitx/fcitx_mozc.h"
#include "unix/fcitx/surrounding_text_util.h"
#include <fcitx/candidate.h>

namespace {

// Returns a position that determines a preedit cursor position _AND_ top-left
// position of a candidate window. Note that we can't set these two positions
// independently. That's a SCIM's limitation.
uint32_t GetCursorPosition(const mozc::commands::Output &response) {
    if (!response.has_preedit()) {
        return 0;
    }
    if (response.preedit().has_highlighted_position()) {
        return response.preedit().highlighted_position();
    }
    return response.preedit().cursor();
}

std::string CreateDescriptionString(const std::string &description) {
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

void MozcResponseParser::UpdateDeletionRange(const mozc::commands::Output& response, FcitxMozc* fcitx_mozc) const
{
    if (response.has_deletion_range() &&
        response.deletion_range().offset() <= 0 &&
        response.deletion_range().offset() + response.deletion_range().length() >= 0) {
        FcitxInstanceDeleteSurroundingText(fcitx_mozc->GetInstance(),
                                           FcitxInstanceGetCurrentIC(fcitx_mozc->GetInstance()),
                                           response.deletion_range().offset(),
                                           response.deletion_range().length());
    }
}

void MozcResponseParser::LaunchTool(const mozc::commands::Output& response, FcitxMozc* fcitx_mozc) const
{
    FCITX_UNUSED(fcitx_mozc);
    if (response.has_launch_tool_mode()) {
        fcitx_mozc->GetClient()->LaunchToolWithProtoBuf(response);
    }
}

void MozcResponseParser::ExecuteCallback(const mozc::commands::Output& response, FcitxMozc* fcitx_mozc) const
{
    if (!response.has_callback()) {
        return;
    }

    if (!response.callback().has_session_command()) {
        LOG(ERROR) << "callback does not have session_command";
        return;
    }

    const commands::SessionCommand &callback_command =
        response.callback().session_command();

    if (!callback_command.has_type()) {
        LOG(ERROR) << "callback_command has no type";
        return;
    }

    commands::SessionCommand session_command;
    session_command.set_type(callback_command.type());

    // TODO(nona): Make a function to handle CONVERT_REVERSE.
    // Used by CONVERT_REVERSE and/or UNDO
    // This value represents how many characters are selected as a relative
    // distance of characters. Positive value represents forward text selection
    // and negative value represents backword text selection.
    // Note that you should not allow 0x80000000 for |relative_selected_length|
    // because you cannot safely use |-relative_selected_length| nor
    // |abs(relative_selected_length)| in this case due to integer overflow.
    SurroundingTextInfo surrounding_text_info;

    switch (callback_command.type()) {
        case commands::SessionCommand::UNDO:
            break;
        case commands::SessionCommand::CONVERT_REVERSE: {

            if (!GetSurroundingText(fcitx_mozc->GetInstance(),
                                    &surrounding_text_info)) {
                return;
            }

            session_command.set_text(surrounding_text_info.selection_text);
            break;
        }
        default:
            return;
    }

    commands::Output new_output;
    if (!fcitx_mozc->SendCommand(session_command, &new_output)) {
        LOG(ERROR) << "Callback Command Failed";
        return;
    }

    if (callback_command.type() == commands::SessionCommand::CONVERT_REVERSE) {
        // We need to remove selected text as a first step of reconversion.
        commands::DeletionRange *range = new_output.mutable_deletion_range();
        // Use DeletionRange field to remove the selected text.
        // For forward selection (that is, |relative_selected_length > 0|), the
        // offset should be a negative value to delete preceding text.
        // For backward selection (that is, |relative_selected_length < 0|),
        // IBus and/or some applications seem to expect |offset == 0| somehow.
        const int32_t offset = surrounding_text_info.relative_selected_length > 0
            ? -surrounding_text_info.relative_selected_length  // forward selection
            : 0;                         // backward selection
        range->set_offset(offset);
        range->set_length(abs(surrounding_text_info.relative_selected_length));
    }

    VLOG(1) << "New output" << new_output.DebugString();

    ParseResponse(new_output, fcitx_mozc);
}

bool MozcResponseParser::ParseResponse(const mozc::commands::Output &response,
                                       FcitxMozc *fcitx_mozc) const {
    DCHECK(fcitx_mozc);
    if (!fcitx_mozc) {
        return false;
    }

    fcitx_mozc->SetUsage("", "");

    UpdateDeletionRange(response, fcitx_mozc);

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
        const std::string &url = response.url();
        fcitx_mozc->SetUrl(url);
    }
    LaunchTool(response, fcitx_mozc);
    ExecuteCallback(response, fcitx_mozc);

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

static boolean FcitxMozcPaging(void* arg, boolean prev)
{
    FcitxMozc* mozc = static_cast<FcitxMozc*>(arg);
    return mozc->paging(prev);
}

void MozcResponseParser::ParseCandidates(
    const mozc::commands::Candidates &candidates, FcitxMozc *fcitx_mozc) const {
    const commands::Footer &footer = candidates.footer();
    bool hasPrev = false;
    bool hasNext = false;
    if (candidates.has_footer()) {
        std::string auxString;
        if (footer.has_label()) {
            // TODO(yusukes,mozc-team): label() is not localized. Currently, it's always
            // written in Japanese (in UTF-8).
            auxString += footer.label();
        } else if (footer.has_sub_label()) {
            // Windows client shows sub_label() only when label() is not specified. We
            // follow the policy.
            auxString += footer.sub_label();
        }

        if (footer.has_index_visible() && footer.index_visible()) {
            // Max size of candidates is 200 so 128 is sufficient size for the buffer.
            char index_buf[128] = {0};
            const int result = snprintf(index_buf,
                                        sizeof(index_buf) - 1,
                                        "%s%d/%d",
                                        (auxString.empty() ? "" : " "),
                                        candidates.focused_index() + 1,
                                        candidates.size());
            DCHECK_GE(result, 0) << "snprintf in ComposeAuxiliaryText failed";
            auxString += index_buf;

            if (candidates.candidate_size() > 0) {

                if (candidates.candidate(0).index() > 0) {
                    hasPrev = true;
                }
                if (candidates.candidate(candidates.candidate_size() - 1).index() + 1 < candidates.size()) {
                    hasNext = true;
                }
            }
        }
        fcitx_mozc->SetAuxString(auxString);
    }

    FcitxCandidateWordList* candList = FcitxInputStateGetCandidateList(fcitx_mozc->GetInputState());
    FcitxCandidateWordReset(candList);
    FcitxCandidateWordSetPageSize(candList, 9);
    if (candidates.has_direction() &&
        candidates.direction() == commands::Candidates::HORIZONTAL) {
        FcitxCandidateWordSetLayoutHint(candList, CLH_Horizontal);
    } else {
        FcitxCandidateWordSetLayoutHint(candList, CLH_Vertical);
    }

    std::map<int32_t, std::pair<std::string, std::string> > usage_map;
    if (candidates.has_usages()) {
        const commands::InformationList& usages = candidates.usages();
        for (size_t i = 0; i < usages.information().size(); ++i) {
            const commands::Information& information = usages.information(i);
            if (!information.has_id() || !information.has_description())
                continue;
            usage_map[information.id()].first = information.title();
            usage_map[information.id()].second = information.description();
        }
    }

#define EMPTY_STR_CHOOSE "\0\0\0\0\0\0\0\0\0\0"
    std::vector<char> choose;

    int focused_index = -1;
    int local_index = -1;
    if (candidates.has_focused_index()) {
        focused_index = candidates.focused_index();
    }
    for (int i = 0; i < candidates.candidate_size(); ++i) {
        const commands::Candidates::Candidate& candidate = candidates.candidate(i);
        const uint32_t index = candidate.index();
        FcitxMessageType type;
        if (focused_index != -1 && index == focused_index) {
            local_index = i;
            type = MSG_FIRSTCAND;
        } else {
            type = MSG_OTHER;
        }
        int32_t* id = (int32_t*) fcitx_utils_malloc0(sizeof(int32_t));
        FcitxCandidateWord candWord;
        candWord.callback = FcitxMozcGetCandidateWord;
        candWord.extraType = MSG_OTHER;
        candWord.strExtra = NULL;
        candWord.priv = id;
        candWord.strWord = NULL;
        candWord.wordType = type;
        candWord.owner = fcitx_mozc;

        std::string value;
        if (use_annotation_ &&
                candidate.has_annotation() &&
                candidate.annotation().has_prefix()) {
            value = candidate.annotation().prefix();
        }
        value += candidate.value();
        if (use_annotation_ &&
                candidate.has_annotation() &&
                candidate.annotation().has_suffix()) {
            value += candidate.annotation().suffix();
        }
        if (use_annotation_ &&
                candidate.has_annotation() &&
                candidate.annotation().has_description()) {
            // Display descriptions ([HALF][KATAKANA], [GREEK], [Black square], etc).
            value += CreateDescriptionString(
                         candidate.annotation().description());
        }

        if (use_annotation_ && focused_index != -1 && index == focused_index) {
            local_index = i;
            type = MSG_FIRSTCAND;

            if (candidate.has_information_id()) {
                std::map<int32_t, std::pair<std::string, std::string> >::iterator it =
                    usage_map.find(candidate.information_id());
                if (it != usage_map.end()) {
                    fcitx_mozc->SetUsage(it->second.first, it->second.second);
                }
                value += CreateDescriptionString(_("Press Ctrl+Alt+H to show usages."));
            }
        }

        if (candidate.has_annotation() &&
            candidate.annotation().has_shortcut()) {
            choose.push_back(candidate.annotation().shortcut().c_str()[0]);
        }

        candWord.strWord = strdup(value.c_str());

        if (candidate.has_id()) {
            const int32_t cid = candidate.id();
            DCHECK_NE(kBadCandidateId, cid) << "Unexpected id is passed.";
            *id = cid;
        } else {
            // The parent node of the cascading window does not have an id since the
            // node does not contain a candidate word.
            *id = kBadCandidateId;
        }
        FcitxCandidateWordAppend(candList, &candWord);
    }

    while (choose.size() < 10) {
        choose.push_back('\0');
    }

    if (footer.has_index_visible() && footer.index_visible())
        FcitxCandidateWordSetChoose(candList, choose.data());
    else
        FcitxCandidateWordSetChoose(candList, EMPTY_STR_CHOOSE);
    FcitxCandidateWordSetFocus(candList, local_index);
    FcitxCandidateWordSetOverridePaging(candList, hasPrev, hasNext, FcitxMozcPaging, fcitx_mozc, NULL);
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
                                      uint32_t position,
                                      FcitxMozc *fcitx_mozc) const {
    PreeditInfo *info = new PreeditInfo;
    std::string s;

    for (int i = 0; i < preedit.segment_size(); ++i) {
        const mozc::commands::Preedit_Segment &segment = preedit.segment(i);
        const std::string &str = segment.value();
        FcitxMessageType type = MSG_INPUT;

        switch (segment.annotation()) {
        case mozc::commands::Preedit_Segment::NONE:
            type = (FcitxMessageType) (MSG_INPUT | MSG_NOUNDERLINE);
            break;
        case mozc::commands::Preedit_Segment::UNDERLINE:
            type = (FcitxMessageType) (MSG_TIPS);
            break;
        case mozc::commands::Preedit_Segment::HIGHLIGHT:
            type = (FcitxMessageType) (MSG_CODE | MSG_NOUNDERLINE | MSG_HIGHLIGHT);
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

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

#include "unix/fcitx/fcitx_mozc.h"

#include <string>
#include <fcitx/context.h>
#include <fcitx/candidate.h>
#include <fcitx/module.h>
#include <fcitx-config/xdg.h>

// Resolve macro naming conflict with absl.
#undef InvokeFunction

#include "base/const.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/util.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "unix/fcitx/mozc_connection.h"
#include "unix/fcitx/mozc_response_parser.h"

#define N_(x) (x)

namespace
{

static const std::string empty_string;

const struct CompositionMode
{
    const char *icon;
    const char *label;
    const char *description;
    mozc::commands::CompositionMode mode;
} kPropCompositionModes[] =
{
    {
        "mozc-direct.png",
        "A",
        N_("Direct"),
        mozc::commands::DIRECT,
    }, {
        "mozc-hiragana.png",
        "\xe3\x81\x82",  // Hiragana letter A in UTF-8.
        N_("Hiragana"),
        mozc::commands::HIRAGANA,
    }, {
        "mozc-katakana_full.png",
        "\xe3\x82\xa2",  // Katakana letter A.
        N_("Full Katakana"),
        mozc::commands::FULL_KATAKANA,
    }, {
        "mozc-alpha_half.png",
        "A",
        N_("Half ASCII"),
        mozc::commands::HALF_ASCII,
    }, {
        "mozc-alpha_full.png",
        "\xef\xbc\xa1",  // Full width ASCII letter A.
        N_("Full ASCII"),
        mozc::commands::FULL_ASCII,
    }, {
        "mozc-katakana_half.png",
        "\xef\xbd\xb1",  // Half width Katakana letter A.
        N_("Half Katakana"),
        mozc::commands::HALF_KATAKANA,
    },
};
const size_t kNumCompositionModes = FCITX_ARRAY_SIZE ( kPropCompositionModes );

// This array must correspond with the CompositionMode enum in the
// mozc/session/command.proto file.
static_assert (
    mozc::commands::NUM_OF_COMPOSITIONS == FCITX_ARRAY_SIZE ( kPropCompositionModes ),
    "number of modes must match" );

}  // namespace

INPUT_RETURN_VALUE FcitxMozcGetCandidateWord(void* arg, FcitxCandidateWord* candWord)
{
    mozc::fcitx::FcitxMozc* fcitx_mozc = (mozc::fcitx::FcitxMozc*) arg;
    fcitx_mozc->select_candidate(candWord);

    return IRV_DISPLAY_CANDWORDS;
}


namespace mozc
{

namespace fcitx
{

// For unittests.
FcitxMozc::FcitxMozc ( FcitxInstance* inst,
                       MozcConnectionInterface *connection,
                       MozcResponseParser *parser ) :
        instance(inst),
        input(FcitxInstanceGetInputState(inst)),
        connection_ ( connection ),
        parser_ ( parser ),
        composition_mode_ ( mozc::commands::HIRAGANA )
{
    // mozc::Logging::SetVerboseLevel(1);
    VLOG ( 1 ) << "FcitxMozc created.";
    const bool is_vertical = true;
    parser_->set_use_annotation ( is_vertical );
    InitializeBar();
    InitializeMenu();
    SetCompositionMode( mozc::commands::HIRAGANA );
}

FcitxMozc::~FcitxMozc()
{
    VLOG ( 1 ) << "FcitxMozc destroyed.";
}

// This function is called from SCIM framework when users press or release a
// key.
bool FcitxMozc::process_key_event (FcitxKeySym sym, uint32 keycode, uint32 state, bool layout_is_jp, bool is_key_up)
{
    std::string error;
    mozc::commands::Output raw_response;
    if ( !connection_->TrySendKeyEvent (
                GetInstance(), sym, keycode, state, composition_mode_, layout_is_jp, is_key_up, &raw_response, &error ) )
    {
        // TODO(yusukes): Show |error|.
        return false;  // not consumed.
    }

    return ParseResponse ( raw_response );
}

// This function is called from SCIM framework when users click the candidate
// window.
void FcitxMozc::select_candidate ( FcitxCandidateWord* candWord )
{
    int32 *id = (int32*) candWord->priv;

    if ( *id == kBadCandidateId )
    {
        LOG ( ERROR ) << "The clicked candidate doesn't have unique ID.";
        return;
    }
    VLOG ( 1 ) << "select_candidate, id=" << *id;

    std::string error;
    mozc::commands::Output raw_response;
    if ( !connection_->TrySendClick ( *id, &raw_response, &error ) )
    {
        LOG ( ERROR ) << "IPC failed. error=" << error;
        SetAuxString ( error );
        DrawAll();
    }
    else
    {
        ParseResponse ( raw_response );
    }
}

// This function is called from SCIM framework.
void FcitxMozc::resetim()
{
    VLOG ( 1 ) << "resetim";
    std::string error;
    mozc::commands::Output raw_response;
    if ( connection_->TrySendCommand (
                mozc::commands::SessionCommand::REVERT, &raw_response, &error ) )
    {
        parser_->ParseResponse ( raw_response, this );
    }
    ClearAll();  // just in case.
    DrawAll();

}

void FcitxMozc::reset()
{
    FcitxIM* im = FcitxInstanceGetCurrentIM(instance);
    if (!im || strcmp(im->uniqueName, "mozc") != 0) {
        FcitxUISetStatusVisable(instance, "mozc-tool", false);
        FcitxUISetStatusVisable(instance, "mozc-composition-mode", false);
    }
    else {
        FcitxUISetStatusVisable(instance, "mozc-tool", true);
        FcitxUISetStatusVisable(instance, "mozc-composition-mode", true);
        connection_->UpdatePreeditMethod();
    }
}

bool FcitxMozc::paging(bool prev)
{
    VLOG ( 1 ) << "paging";
    std::string error;
    mozc::commands::SessionCommand::CommandType command =
        prev ? mozc::commands::SessionCommand::CONVERT_PREV_PAGE
             : mozc::commands::SessionCommand::CONVERT_NEXT_PAGE;
    mozc::commands::Output raw_response;
    if ( connection_->TrySendCommand (
        command, &raw_response, &error ) )
    {
        parser_->ParseResponse ( raw_response, this );
        return true;
    }
    return false;
}

// This function is called from SCIM framework when the ic gets focus.
void FcitxMozc::init()
{
    VLOG ( 1 ) << "init";
    boolean flag = true;
    FcitxInstanceSetContext(instance, CONTEXT_DISABLE_AUTOENG, &flag);
    FcitxInstanceSetContext(instance, CONTEXT_DISABLE_FULLWIDTH, &flag);
    FcitxInstanceSetContext(instance, CONTEXT_DISABLE_QUICKPHRASE, &flag);
    FcitxInstanceSetContext(instance, CONTEXT_IM_KEYBOARD_LAYOUT, "jp");
    FcitxInstanceSetContext(instance, "CONTEXT_DISABLE_AUTO_FIRST_CANDIDATE_HIGHTLIGHT", &flag);

    connection_->UpdatePreeditMethod();
    DrawAll();
}

// This function is called when the ic loses focus.
void FcitxMozc::focus_out()
{
    VLOG ( 1 ) << "focus_out";
    std::string error;
    mozc::commands::Output raw_response;
    if ( connection_->TrySendCommand (
                mozc::commands::SessionCommand::REVERT, &raw_response, &error ) )
    {
        parser_->ParseResponse ( raw_response, this );
    }
    ClearAll();  // just in case.
    DrawAll();
    // TODO(yusukes): Call client::SyncData() like ibus-mozc.
}


bool FcitxMozc::ParseResponse ( const mozc::commands::Output &raw_response )
{
    ClearAll();
    const bool consumed = parser_->ParseResponse ( raw_response, this );
    if ( !consumed )
    {
        VLOG ( 1 ) << "The input was not consumed by Mozc.";
    }
    OpenUrl();
    DrawAll();
    return consumed;
}

void FcitxMozc::SetResultString ( const std::string &result_string )
{
    FcitxInstanceCommitString(instance, FcitxInstanceGetCurrentIC(instance), result_string.c_str());
}

void FcitxMozc::SetPreeditInfo ( const PreeditInfo *preedit_info )
{
    preedit_info_.reset ( preedit_info );
}

void FcitxMozc::SetAuxString ( const std::string &str )
{
    aux_ = str;
}

void FcitxMozc::SetCompositionMode ( mozc::commands::CompositionMode mode )
{
    composition_mode_ = mode;
    DCHECK(composition_mode_ < kNumCompositionModes);
    if (composition_mode_ < kNumCompositionModes) {
        FcitxUISetStatusString(instance,
                               "mozc-composition-mode",
                               _(kPropCompositionModes[composition_mode_].label),
                               _(kPropCompositionModes[composition_mode_].description));
    }
}

void FcitxMozc::SendCompositionMode(mozc::commands::CompositionMode mode)
{
    // Send the SWITCH_INPUT_MODE command.
    std::string error;
    mozc::commands::Output raw_response;
    if (connection_->TrySendCompositionMode(
            kPropCompositionModes[mode].mode, composition_mode_, &raw_response, &error)) {
        parser_->ParseResponse(raw_response, this);
    }
}


void FcitxMozc::SetUrl ( const std::string &url )
{
    url_ = url;
}

void FcitxMozc::ClearAll()
{
    SetPreeditInfo ( NULL );
    SetAuxString ( "" );
    FcitxCandidateWordReset(FcitxInputStateGetCandidateList(input));
    url_.clear();
}

void FcitxMozc::DrawPreeditInfo()
{
    FcitxMessages* preedit = FcitxInputStateGetPreedit(input);
    FcitxMessages* clientpreedit = FcitxInputStateGetClientPreedit(input);
    FcitxMessagesSetMessageCount(preedit, 0);
    FcitxMessagesSetMessageCount(clientpreedit, 0);
    if ( preedit_info_.get() )
    {
        VLOG ( 1 ) << "DrawPreeditInfo: cursor=" << preedit_info_->cursor_pos;

        FcitxInputContext* ic = FcitxInstanceGetCurrentIC(instance);
        boolean supportPreedit = FcitxInstanceICSupportPreedit(instance, ic);

        if (!supportPreedit)
            FcitxInputStateSetShowCursor(input, true);

        for (int i = 0; i < preedit_info_->preedit.size(); i ++) {
            if (!supportPreedit)
                FcitxMessagesAddMessageAtLast(preedit, preedit_info_->preedit[i].type, "%s", preedit_info_->preedit[i].str.c_str());
            FcitxMessagesAddMessageAtLast(clientpreedit, preedit_info_->preedit[i].type, "%s", preedit_info_->preedit[i].str.c_str());
        }
        if (!supportPreedit)
            FcitxInputStateSetCursorPos(input, preedit_info_->cursor_pos);
        FcitxInputStateSetClientCursorPos(input, preedit_info_->cursor_pos);
    }
    else {
        FcitxInputStateSetShowCursor(input, false);
    }
    if ( !aux_.empty() ) {
        FcitxMessagesAddMessageAtLast(preedit, MSG_TIPS, "%s[%s]", preedit_info_.get() ? " " : "", aux_.c_str());
    }
}

void FcitxMozc::DrawAux()
{
    FcitxMessages* auxUp = FcitxInputStateGetAuxUp(input);
    FcitxMessages* auxDown = FcitxInputStateGetAuxDown(input);
    FcitxMessagesSetMessageCount(auxUp, 0);
    FcitxMessagesSetMessageCount(auxDown, 0);
}

void FcitxMozc::DrawAll()
{
    DrawPreeditInfo();
    DrawAux();
}

void FcitxMozc::OpenUrl()
{
    if ( url_.empty() )
    {
        return;
    }
    mozc::Process::OpenBrowser ( url_ );
    url_.clear();
}

static const char* GetCompositionIconName(void* arg)
{
    FcitxMozc* mozc = (FcitxMozc*) arg;
    return mozc->GetCurrentCompositionModeIcon().c_str();
}


static const char* GetMozcToolIcon(void* arg)
{
    FcitxMozc* mozc = (FcitxMozc*) arg;
    return mozc->GetIconFile("mozc-tool.png").c_str();
}

void FcitxMozc::InitializeBar()
{
    VLOG ( 1 ) << "Registering properties";

    FcitxUIRegisterComplexStatus(instance, this,
        "mozc-composition-mode",
        _("Composition Mode"),
        _("Composition Mode"),
        NULL,
        GetCompositionIconName
    );

    if ( mozc::FileUtil::FileExists ( mozc::FileUtil::JoinPath (
                                      mozc::SystemUtil::GetServerDirectory(), mozc::kMozcTool ) ).ok() )
    {
        FcitxUIRegisterComplexStatus(instance, this,
            "mozc-tool",
            _("Tool"),
            _("Tool"),
            NULL,
            GetMozcToolIcon
        );
    }
    FcitxUISetStatusVisable(instance, "mozc-tool", false);
    FcitxUISetStatusVisable(instance, "mozc-composition-mode", false);
}

boolean CompositionMenuAction(struct _FcitxUIMenu *menu, int index)
{
    FcitxMozc* mozc = (FcitxMozc*) menu->priv;
    mozc->SendCompositionMode((mozc::commands::CompositionMode) index);
    return true;
}

void UpdateCompositionMenu(struct _FcitxUIMenu *menu)
{
    FcitxMozc* mozc = (FcitxMozc*) menu->priv;
    menu->mark = mozc->GetCompositionMode();
}

boolean ToolMenuAction(struct _FcitxUIMenu *menu, int index)
{
    std::string args;
    switch(index) {
        case 0:
            args = "--mode=config_dialog";
            break;
        case 1:
            args = "--mode=dictionary_tool";
            break;
        case 2:
            args = "--mode=word_register_dialog";
            break;
        case 3:
            args = "--mode=about_dialog";
            break;
    }
    mozc::Process::SpawnMozcProcess("mozc_tool", args);
    return true;
}

void UpdateToolMenu(struct _FcitxUIMenu *menu)
{
    return;
}

void FcitxMozc::InitializeMenu()
{
    FcitxMenuInit(&this->compositionMenu);
    compositionMenu.name = strdup(_("Composition Mode"));
    compositionMenu.candStatusBind = strdup("mozc-composition-mode");
    compositionMenu.UpdateMenu = UpdateCompositionMenu;
    compositionMenu.MenuAction = CompositionMenuAction;
    compositionMenu.priv = this;
    compositionMenu.isSubMenu = false;
    int i;
    for (i = 0; i < kNumCompositionModes; i ++)
        FcitxMenuAddMenuItem(&compositionMenu, _(kPropCompositionModes[i].description), MENUTYPE_SIMPLE, NULL);

    FcitxUIRegisterMenu(instance, &compositionMenu);

    FcitxMenuInit(&this->toolMenu);
    toolMenu.name = strdup(_("Mozc Tool"));
    toolMenu.candStatusBind = strdup("mozc-tool");
    toolMenu.UpdateMenu = UpdateToolMenu;
    toolMenu.MenuAction = ToolMenuAction;
    toolMenu.priv = this;
    toolMenu.isSubMenu = false;
    FcitxMenuAddMenuItem(&toolMenu, _("Configuration Tool"), MENUTYPE_SIMPLE, NULL);
    FcitxMenuAddMenuItem(&toolMenu, _("Dictionary Tool"), MENUTYPE_SIMPLE, NULL);
    FcitxMenuAddMenuItem(&toolMenu, _("Add Word"), MENUTYPE_SIMPLE, NULL);
    FcitxMenuAddMenuItem(&toolMenu, _("About Mozc"), MENUTYPE_SIMPLE, NULL);
    FcitxUIRegisterMenu(instance, &toolMenu);
}

bool FcitxMozc::SendCommand(const mozc::commands::SessionCommand& session_command, commands::Output* new_output)
{
    std::string error;
    return connection_->TrySendRawCommand(session_command, new_output, &error);
}


FcitxInputState* FcitxMozc::GetInputState()
{
    return input;
}

const std::string& FcitxMozc::GetIconFile(const std::string key)
{
    if (iconMap.count(key)) {
        return iconMap[key];
    }

    char* retFile;
    FILE* fp = FcitxXDGGetFileWithPrefix("mozc/icon", key.c_str(), "r", &retFile);
    if (fp)
        fclose(fp);
    if (retFile) {
        iconMap[key] = std::string(retFile);
        free(retFile);
    }
    else {
        iconMap[key] = "";
    }
    return iconMap[key];
}


const std::string& FcitxMozc::GetCurrentCompositionModeIcon() {
    DCHECK(composition_mode_ < kNumCompositionModes);
    if (composition_mode_ < kNumCompositionModes) {
        return GetIconFile(kPropCompositionModes[composition_mode_].icon);
    }
    return empty_string;
}

void FcitxMozc::SetUsage(const std::string& title_, const std::string& description_)
{
    title = title_;
    description = description_;
}

std::pair< std::string, std::string > FcitxMozc::GetUsage()
{
    return make_pair(title, description);
}

}  // namespace fcitx

}  // namespace mozc_unix_scim

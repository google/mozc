/***************************************************************************
 *   Copyright (C) 2012~2012 by CSSlayer                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#include "unix/fcitx/fcitx_mozc.h"

#include <string>
#include <fcitx/candidate.h>

#include "base/const.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/util.h"
#include "unix/fcitx/mozc_connection.h"
#include "unix/fcitx/mozc_response_parser.h"
#include "unix/fcitx/fcitx_key_translator.h"

namespace
{

const struct CompositionMode
{
    const char *icon;
    const char *label;
    const char *description;
    mozc::commands::CompositionMode mode;
} kPropCompositionModes[] =
{
    {
        "mozc-direct",
        "A",
        "Direct",
        mozc::commands::DIRECT,
    }, {
        "mozc-hiragana",
        "\xe3\x81\x82",  // Hiragana letter A in UTF-8.
        "Hiragana",
        mozc::commands::HIRAGANA,
    }, {
        "mozc-katakana_full",
        "\xe3\x82\xa2",  // Katakana letter A.
        "Full Katakana",
        mozc::commands::FULL_KATAKANA,
    }, {
        "mozc-alpha_half",
        "_A",
        "Half ASCII",
        mozc::commands::HALF_ASCII,
    }, {
        "mozc-alpha_full",
        "\xef\xbc\xa1",  // Full width ASCII letter A.
        "Full ASCII",
        mozc::commands::FULL_ASCII,
    }, {
        "mozc-katakana_half",
        "_\xef\xbd\xb1",  // Half width Katakana letter A.
        "Half Katakana",
        mozc::commands::HALF_KATAKANA,
    },
};
const size_t kNumCompositionModes = arraysize ( kPropCompositionModes );

// This array must correspond with the CompositionMode enum in the
// mozc/session/command.proto file.
COMPILE_ASSERT (
    mozc::commands::NUM_OF_COMPOSITIONS == arraysize ( kPropCompositionModes ),
    bad_number_of_modes );

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
    VLOG ( 1 ) << "FcitxMozc created.";
    const bool is_vertical
    = false;
    parser_->set_use_annotation ( is_vertical );
    InitializeBar();
}

FcitxMozc::~FcitxMozc()
{
    VLOG ( 1 ) << "FcitxMozc destroyed.";
}

// This function is called from SCIM framework when users press or release a
// key.
bool FcitxMozc::process_key_event ( FcitxKeySym sym, unsigned int state )
{

    if ( !connection_->CanSend ( sym, state ) )
    {
        VLOG ( 1 ) << "Mozc doesn't handle the key. Not consumed.";
        return false;  // not consumed.
    }

    string error;
    mozc::commands::Output raw_response;
    if ( !connection_->TrySendKeyEvent (
                sym, state, composition_mode_, &raw_response, &error ) )
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

    string error;
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
    string error;
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
    if (!im || strcmp(im->uniqueName, "fcitx-mozc") != 0) {
        FcitxUISetStatusVisable(instance, "mozc-tool", false);
        FcitxUISetStatusVisable(instance, "mozc-dictionary-tool", false);
        FcitxUISetStatusVisable(instance, "mozc-property", false);
        FcitxUISetStatusVisable(instance, "mozc-composition-mode", false);
    }
    else {
        FcitxUISetStatusVisable(instance, "mozc-tool", true);
        FcitxUISetStatusVisable(instance, "mozc-dictionary-tool", true);
        FcitxUISetStatusVisable(instance, "mozc-property", true);
        FcitxUISetStatusVisable(instance, "mozc-composition-mode", true);
    }
}


// This function is called from SCIM framework when the ic gets focus.
void FcitxMozc::init()
{
    VLOG ( 1 ) << "focus_in";
    DrawAll();
}

// This function is called when the ic loses focus.
void FcitxMozc::focus_out()
{
    VLOG ( 1 ) << "focus_out";
    string error;
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
    // Update the bar.
//    const char *icon = GetCurrentCompositionModeIcon();
    
    // TODO:
}

void FcitxMozc::SetUrl ( const string &url )
{
    url_ = url;
}

void FcitxMozc::ClearAll()
{
    SetPreeditInfo ( NULL );
    SetAuxString ( "" );
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
        
        FcitxInputStateSetShowCursor(input, true);
        
        for (int i = 0; i < preedit_info_->preedit.size(); i ++) {
            FcitxMessagesAddMessageAtLast(preedit, preedit_info_->preedit[i].type, "%s", preedit_info_->preedit[i].str.c_str());
            FcitxMessagesAddMessageAtLast(clientpreedit, preedit_info_->preedit[i].type, "%s", preedit_info_->preedit[i].str.c_str());
        }
        FcitxInputStateSetCursorPos(input, preedit_info_->cursor_pos);
        FcitxInputStateSetClientCursorPos(input, preedit_info_->cursor_pos);
    }
    else {
        FcitxInputStateSetShowCursor(input, true);
    }
}

void FcitxMozc::DrawAux()
{
    FcitxMessages* auxUp = FcitxInputStateGetAuxUp(input);
    FcitxMessages* auxDown = FcitxInputStateGetAuxDown(input);
    FcitxMessagesSetMessageCount(auxUp, 0);
    FcitxMessagesSetMessageCount(auxDown, 0);
    if ( !aux_.empty() ) {
        FcitxMessagesAddMessageAtLast(auxUp, MSG_TIPS, "%s", aux_.c_str());
    }
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

static void ToggleCompositionMode(void* arg)
{
}

static boolean GetCompositionStatus(void* arg)
{
    return true;
}

static void ToggleMozcTool(void* arg)
{
}

static void ToggleMozcDictionaryTool(void* arg)
{
}

static void ToggleMozcProperty(void* arg)
{
}

static boolean GetMozcToolStatus(void* arg)
{
    return true;
}

void FcitxMozc::InitializeBar()
{
    VLOG ( 1 ) << "Registering properties";
    // TODO(yusukes): L10N needed for "Tool", "Dictionary tool", and "Property".
    
    FcitxUIRegisterStatus(instance, this,
        "mozc-composition-mode",
        "",
        "",
        ToggleCompositionMode,
        GetCompositionStatus
    );

    if ( mozc::Util::FileExists ( mozc::Util::JoinPath (
                                      mozc::Util::GetServerDirectory(), mozc::kMozcTool ) ) )
    {
        FcitxUIRegisterStatus(instance, this,
            "mozc-tool",
            "",
            "",
            ToggleMozcTool,
            GetMozcToolStatus
        );
        
        FcitxUIRegisterStatus(instance, this,
            "mozc-dictionary-tool",
            "",
            "",
            ToggleMozcDictionaryTool,
            GetMozcToolStatus
        );
        
        FcitxUIRegisterStatus(instance, this,
            "mozc-property",
            "",
            "",
            ToggleMozcProperty,
            GetMozcToolStatus
        );
    }
    FcitxUISetStatusVisable(instance, "mozc-tool", false);
    FcitxUISetStatusVisable(instance, "mozc-dictionary-tool", false);
    FcitxUISetStatusVisable(instance, "mozc-property", false);
    FcitxUISetStatusVisable(instance, "mozc-composition-mode", false);
}


FcitxInputState* FcitxMozc::GetInputState()
{
    return input;
}


}  // namespace fcitx

}  // namespace mozc_unix_scim

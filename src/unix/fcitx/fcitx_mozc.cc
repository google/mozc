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
#include <fcitx-config/xdg.h>

#include "base/const.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/util.h"
#include "unix/fcitx/mozc_connection.h"
#include "unix/fcitx/mozc_response_parser.h"
#include "unix/fcitx/fcitx_key_translator.h"
#include <fcitx/context.h>

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
        "Direct",
        mozc::commands::DIRECT,
    }, {
        "mozc-hiragana.png",
        "\xe3\x81\x82",  // Hiragana letter A in UTF-8.
        "Hiragana",
        mozc::commands::HIRAGANA,
    }, {
        "mozc-katakana_full.png",
        "\xe3\x82\xa2",  // Katakana letter A.
        "Full Katakana",
        mozc::commands::FULL_KATAKANA,
    }, {
        "mozc-alpha_half.png",
        "_A",
        "Half ASCII",
        mozc::commands::HALF_ASCII,
    }, {
        "mozc-alpha_full.png",
        "\xef\xbc\xa1",  // Full width ASCII letter A.
        "Full ASCII",
        mozc::commands::FULL_ASCII,
    }, {
        "mozc-katakana_half.png",
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
    InitializeMenu();
    SetCompositionMode( mozc::commands::HIRAGANA );
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
    if (!im || strcmp(im->uniqueName, "mozc") != 0) {
        FcitxUISetStatusVisable(instance, "mozc-tool", false);
        FcitxUISetStatusVisable(instance, "mozc-composition-mode", false);
    }
    else {
        FcitxUISetStatusVisable(instance, "mozc-tool", true);
        FcitxUISetStatusVisable(instance, "mozc-composition-mode", true);
    }
}


// This function is called from SCIM framework when the ic gets focus.
void FcitxMozc::init()
{
    VLOG ( 1 ) << "focus_in";
    boolean flag = false;
    FcitxInstanceSetContext(instance, CONTEXT_DISABLE_AUTOENG, &flag);
    FcitxInstanceSetContext(instance, CONTEXT_DISABLE_QUICKPHRASE, &flag);
    FcitxInstanceSetContext(instance, CONTEXT_IM_KEYBOARD_LAYOUT, "jp");
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
    string error;
    mozc::commands::Output raw_response;
    if (connection_->TrySendCompositionMode(
            kPropCompositionModes[mode].mode, &raw_response, &error)) {
        parser_->ParseResponse(raw_response, this);
    }
}


void FcitxMozc::SetUrl ( const string &url )
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
    FcitxInputContext* ic = FcitxInstanceGetCurrentIC(instance);
    if ( preedit_info_.get() )
    {
        VLOG ( 1 ) << "DrawPreeditInfo: cursor=" << preedit_info_->cursor_pos;
        
        if (ic && (ic->contextCaps & CAPACITY_PREEDIT) == 0)
            FcitxInputStateSetShowCursor(input, true);
        
        for (int i = 0; i < preedit_info_->preedit.size(); i ++) {
            if (ic && (ic->contextCaps & CAPACITY_PREEDIT) == 0)
                FcitxMessagesAddMessageAtLast(preedit, preedit_info_->preedit[i].type, "%s", preedit_info_->preedit[i].str.c_str());
            FcitxMessagesAddMessageAtLast(clientpreedit, preedit_info_->preedit[i].type, "%s", preedit_info_->preedit[i].str.c_str());
        }
        if (ic && (ic->contextCaps & CAPACITY_PREEDIT) == 0)
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
        FcitxMessagesAddMessageAtLast(auxUp, MSG_TIPS, "%s ", aux_.c_str());
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
    // TODO(yusukes): L10N needed for "Tool", "Dictionary tool", and "Property".
    
    FcitxUIRegisterComplexStatus(instance, this,
        "mozc-composition-mode",
        _("Composition Mode"),
        _(""),
        NULL,
        GetCompositionIconName
    );

    if ( mozc::Util::FileExists ( mozc::Util::JoinPath (
                                      mozc::Util::GetServerDirectory(), mozc::kMozcTool ) ) )
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

FcitxMozc::FcitxMozc(const mozc::fcitx::FcitxMozc& )
{

}

boolean CompositionMenuAction(struct _FcitxUIMenu *menu, int index)
{
    FcitxMozc* mozc = (FcitxMozc*) menu->priv;
    if (index == mozc::commands::DIRECT) {
        FcitxInstanceCloseIM(mozc->GetInstance(), FcitxInstanceGetCurrentIC(mozc->GetInstance()));
    }
    else {
        mozc->SendCompositionMode((mozc::commands::CompositionMode) index);
    }
    return true;
}

void UpdateCompositionMenu(struct _FcitxUIMenu *menu)
{
    FcitxMozc* mozc = (FcitxMozc*) menu->priv;
    menu->mark = mozc->GetCompositionMode();
}

boolean ToolMenuAction(struct _FcitxUIMenu *menu, int index)
{
    string args;
    switch(index) {
        case 0:
            args = "--mode=config_dialog";
            break;
        case 1:
            args = "--mode=dictionary_tool";
            break;
        case 2:
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
        FcitxMenuAddMenuItem(&compositionMenu, kPropCompositionModes[i].description, MENUTYPE_SIMPLE, NULL);

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
    FcitxMenuAddMenuItem(&toolMenu, _("About Mozc"), MENUTYPE_SIMPLE, NULL);
    FcitxUIRegisterMenu(instance, &toolMenu);
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

}  // namespace fcitx

}  // namespace mozc_unix_scim

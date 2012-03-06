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

#include <fcitx/instance.h>
#include <fcitx/ime.h>
#include <fcitx/hook.h>
#include <languages/global_language_spec.h>
#include <languages/japanese/lang_dep_spec.h>
#include "fcitx_mozc.h"
#include "mozc_connection.h"
#include "mozc_response_parser.h"
#include <fcitx-config/xdg.h>

typedef struct _FcitxMozcState {
    mozc::japanese::LangDepSpecJapanese* language_dependency_spec_japanese;
    mozc::fcitx::FcitxMozc* mozc;
} FcitxMozcState;


static void* FcitxMozcCreate(FcitxInstance* instance);
static void FcitxMozcDestroy(void *arg);
static boolean FcitxMozcInit(void *arg); /**< FcitxMozcInit */
static void FcitxMozcResetIM(void *arg); /**< FcitxMozcResetIM */
static void FcitxMozcReset(void *arg); /**< FcitxMozcResetIM */
static INPUT_RETURN_VALUE FcitxMozcDoInput(void *arg, FcitxKeySym, unsigned int); /**< FcitxMozcDoInput */
static void FcitxMozcSave(void *arg); /**< FcitxMozcSave */
static void FcitxMozcReloadConfig(void *arg); /**< FcitxMozcReloadConfig */

extern "C" {

FCITX_EXPORT_API
FcitxIMClass ime = {
    FcitxMozcCreate,
    FcitxMozcDestroy
};
FCITX_EXPORT_API
int ABI_VERSION = FCITX_ABI_VERSION;

}

static void* FcitxMozcCreate(FcitxInstance* instance)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) fcitx_utils_malloc0(sizeof(FcitxMozcState));
    bindtextdomain("fcitx-keyboard", LOCALEDIR);
    mozcState->language_dependency_spec_japanese = new mozc::japanese::LangDepSpecJapanese;
    mozc::language::GlobalLanguageSpec::SetLanguageDependentSpec(mozcState->language_dependency_spec_japanese);
    
    mozcState->mozc = new mozc::fcitx::FcitxMozc(
        instance,
        mozc::fcitx::MozcConnection::CreateMozcConnection(),
        new mozc::fcitx::MozcResponseParser
    );
    
    mozcState->mozc->SetCompositionMode(mozc::commands::HIRAGANA);
    
    FcitxIMEventHook hk;
    hk.arg = mozcState;
    hk.func = FcitxMozcReset;
    
    FcitxInstanceRegisterResetInputHook(instance, hk);
    
    FcitxInstanceRegisterIM(instance,
        mozcState,
        "mozc",
        "Mozc",
        mozcState->mozc->GetIconFile("mozc.png").c_str(),
        FcitxMozcInit,
        FcitxMozcResetIM,
        FcitxMozcDoInput,
        NULL,
        NULL,
        FcitxMozcSave,
        FcitxMozcReloadConfig,
        NULL,
        5,
        "ja"
    );
    
    return mozcState;
}

static void FcitxMozcDestroy(void *arg)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) arg;
    delete mozcState->mozc;
    mozc::language::GlobalLanguageSpec::SetLanguageDependentSpec(NULL);
    delete mozcState->language_dependency_spec_japanese;
    free(mozcState);
} 

INPUT_RETURN_VALUE FcitxMozcDoInput(void* arg, FcitxKeySym sym, unsigned int state)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) arg;
    bool result = mozcState->mozc->process_key_event(sym, state);
    if (!result)
        return IRV_TO_PROCESS;
    else
        return IRV_DISPLAY_CANDWORDS;
}

boolean FcitxMozcInit(void* arg)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) arg;
    mozcState->mozc->init();
    return true;
}

void FcitxMozcReloadConfig(void* arg)
{

}

void FcitxMozcSave(void* arg)
{
    FCITX_UNUSED(arg);
}

void FcitxMozcResetIM(void* arg)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) arg;
    mozcState->mozc->resetim();
}

void FcitxMozcReset(void* arg)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) arg;
    mozcState->mozc->reset();

}

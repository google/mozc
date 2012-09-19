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

// workaround
#define _FCITX_LOG_H_

#include <fcitx/instance.h>
#include <fcitx/ime.h>
#include <fcitx/hook.h>
#include <fcitx/module.h>
#include <fcitx-config/xdg.h>
#include "fcitx_mozc.h"
#include "mozc_connection.h"
#include "mozc_response_parser.h"

typedef struct _FcitxMozcState {
    mozc::fcitx::FcitxMozc* mozc;
} FcitxMozcState;


static void* FcitxMozcCreate(FcitxInstance* instance);
static void FcitxMozcDestroy(void *arg);
static boolean FcitxMozcInit(void *arg); /**< FcitxMozcInit */
static void FcitxMozcResetIM(void *arg); /**< FcitxMozcResetIM */
static void FcitxMozcReset(void *arg); /**< FcitxMozcResetIM */
static INPUT_RETURN_VALUE FcitxMozcDoInput(void *arg, FcitxKeySym, unsigned int); /**< FcitxMozcDoInput */
static INPUT_RETURN_VALUE FcitxMozcDoReleaseInput(void *arg, FcitxKeySym, unsigned int); /**< FcitxMozcDoInput */
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

static inline bool CheckLayout(FcitxInstance* instance)
{
    char *layout = NULL, *variant = NULL;
    FcitxModuleFunctionArg args;
    args.args[0] = &layout;
    args.args[1] = &variant;
    bool layout_is_jp = false;
    FcitxModuleInvokeFunctionByName(instance, "fcitx-xkb", 1, args);
    if (layout && strcmp(layout, "jp") == 0)
        layout_is_jp = true;

    fcitx_utils_free(layout);
    fcitx_utils_free(variant);


    return layout_is_jp;
}

static void* FcitxMozcCreate(FcitxInstance* instance)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) fcitx_utils_malloc0(sizeof(FcitxMozcState));
    bindtextdomain("fcitx-mozc", LOCALEDIR);

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

    FcitxIMIFace iface;
    memset(&iface, 0, sizeof(FcitxIMIFace));
    iface.Init = FcitxMozcInit;
    iface.ResetIM = FcitxMozcResetIM;
    iface.DoInput = FcitxMozcDoInput;
    iface.DoReleaseInput = FcitxMozcDoReleaseInput;
    iface.ReloadConfig = FcitxMozcReloadConfig;
    iface.Save = FcitxMozcSave;


    FcitxInstanceRegisterIMv2(
        instance,
        mozcState,
        "mozc",
        "Mozc",
        mozcState->mozc->GetIconFile("mozc.png").c_str(),
        iface,
        1,
        "ja"
    );

    return mozcState;
}

static void FcitxMozcDestroy(void *arg)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) arg;
    delete mozcState->mozc;
    free(mozcState);
}

INPUT_RETURN_VALUE FcitxMozcDoInput(void* arg, FcitxKeySym _sym, unsigned int _state)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) arg;
    FcitxInstance* instance = mozcState->mozc->GetInstance();
    FcitxInputState* input = FcitxInstanceGetInputState(mozcState->mozc->GetInstance());
    FCITX_UNUSED(_sym);
    FCITX_UNUSED(_state);
    FcitxKeySym sym = (FcitxKeySym) FcitxInputStateGetKeySym(input);
    uint32 keycode = FcitxInputStateGetKeyCode(input);
    uint32 state = FcitxInputStateGetKeyState(input);
    bool result = mozcState->mozc->process_key_event(sym, keycode, state, CheckLayout(instance), false);
    if (!result)
        return IRV_TO_PROCESS;
    else
        return IRV_DISPLAY_CANDWORDS;
}

INPUT_RETURN_VALUE FcitxMozcDoReleaseInput(void* arg, FcitxKeySym _sym, unsigned int _state)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) arg;
    FcitxInstance* instance = mozcState->mozc->GetInstance();
    FcitxInputState* input = FcitxInstanceGetInputState(mozcState->mozc->GetInstance());
    FCITX_UNUSED(_sym);
    FCITX_UNUSED(_state);
    FcitxKeySym sym = (FcitxKeySym) FcitxInputStateGetKeySym(input);
    uint32 keycode = FcitxInputStateGetKeyCode(input);
    uint32 state = FcitxInputStateGetKeyState(input);
    bool result = mozcState->mozc->process_key_event(sym, keycode, state, CheckLayout(instance), true);
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

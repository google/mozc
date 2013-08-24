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

// workaround
#define _FCITX_LOG_H_

#include <fcitx/instance.h>
#include <fcitx/ime.h>
#include <fcitx/hook.h>
#include <fcitx/module.h>
#include <fcitx/module/freedesktop-notify/fcitx-freedesktop-notify.h>
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

static const FcitxHotkey MOZC_CTRL_ALT_H[2] = {
    {NULL, FcitxKey_H, FcitxKeyState_Ctrl_Alt},
    {NULL, FcitxKey_None, 0}
};

INPUT_RETURN_VALUE FcitxMozcDoInput(void* arg, FcitxKeySym _sym, unsigned int _state)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) arg;
    FcitxInstance* instance = mozcState->mozc->GetInstance();
    FcitxInputState* input = FcitxInstanceGetInputState(mozcState->mozc->GetInstance());

    if (FcitxHotkeyIsHotKey(_sym, _state, MOZC_CTRL_ALT_H)) {
        pair< string, string > usage = mozcState->mozc->GetUsage();
        if (usage.first.size() != 0 || usage.second.size() != 0) {
            FcitxFreeDesktopNotifyShow(
                instance, "fcitx-mozc-usage",
                0, mozcState->mozc->GetIconFile("mozc.png").c_str(),
                usage.first.c_str(), usage.second.c_str(),
                NULL, -1, NULL, NULL, NULL);
            return IRV_DO_NOTHING;
        }
    }

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

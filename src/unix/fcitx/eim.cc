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

#include <fcitx/instance.h>
#include <fcitx/ime.h>
#include <fcitx/hook.h>
#include <fcitx/module.h>
#include <fcitx/keys.h>
#include <fcitx-config/xdg.h>

// Resolve macro naming conflict with absl.
#undef InvokeFunction

#include "fcitx_mozc.h"
#include "mozc_connection.h"
#include "mozc_response_parser.h"
#include "base/init_mozc.h"

typedef struct _FcitxMozcState {
    mozc::fcitx::FcitxMozc* mozc;
    int inUsageState;
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
    bind_textdomain_codeset("fcitx-mozc", "UTF-8");

    int argc = 1;
    char argv0[] = "fcitx_mozc";
    char *_argv[] = {  argv0 };
    char **argv = _argv;
    mozc::InitMozc(argv[0], &argc, &argv);
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

    if (mozcState->inUsageState) {
        if (FcitxHotkeyIsHotKey(_sym, _state, FCITX_ESCAPE)) {
            mozcState->inUsageState = false;
            // send a dummy key to let server send us the candidate info back without side effect
            mozcState->mozc->process_key_event(FcitxKey_VoidSymbol, 0, 0, CheckLayout(instance), false);
            return IRV_DISPLAY_CANDWORDS;
        } else {
            return IRV_DO_NOTHING;
        }
    }

    if (FcitxHotkeyIsHotKey(_sym, _state, MOZC_CTRL_ALT_H)) {
        std::pair< std::string, std::string > usage = mozcState->mozc->GetUsage();
        if (usage.first.size() != 0 || usage.second.size() != 0) {
            mozcState->inUsageState = true;
            FcitxCandidateWordList* candList = FcitxInputStateGetCandidateList(mozcState->mozc->GetInputState());

            // clear preedit, but keep client preedit
            FcitxMessages* preedit = FcitxInputStateGetPreedit(input);
            FcitxMessagesSetMessageCount(preedit, 0);
            FcitxInputStateSetShowCursor(input, false);

            // clear aux
            FcitxMessages* auxUp = FcitxInputStateGetAuxUp(input);
            FcitxMessages* auxDown = FcitxInputStateGetAuxDown(input);
            FcitxMessagesSetMessageCount(auxUp, 0);
            FcitxMessagesSetMessageCount(auxDown, 0);

            // clear candidate table
            FcitxCandidateWordReset(candList);
            FcitxCandidateWordSetPageSize(candList, 9);
            FcitxCandidateWordSetLayoutHint(candList, CLH_Vertical);
            FcitxCandidateWordSetChoose(candList, "\0\0\0\0\0\0\0\0\0\0");
            FcitxMessagesAddMessageAtLast(preedit, MSG_TIPS, "%s [%s]", usage.first.c_str(), _("Press Escape to go back"));

            UT_array* lines = fcitx_utils_split_string(usage.second.c_str(), '\n');
            utarray_foreach(line, lines, char*) {
                FcitxCandidateWord candWord;
                candWord.callback = NULL;
                candWord.extraType = MSG_OTHER;
                candWord.strExtra = NULL;
                candWord.priv = NULL;
                candWord.strWord = strdup(*line);
                candWord.wordType = MSG_OTHER;
                candWord.owner = NULL;
                FcitxCandidateWordAppend(candList, &candWord);
            }
            utarray_free(lines);
            return IRV_DISPLAY_MESSAGE;
        }
    }

    FCITX_UNUSED(_sym);
    FCITX_UNUSED(_state);
    FcitxKeySym sym = (FcitxKeySym) FcitxInputStateGetKeySym(input);
    uint32_t keycode = FcitxInputStateGetKeyCode(input);
    uint32_t state = FcitxInputStateGetKeyState(input);
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

    if (mozcState->inUsageState) {
        return IRV_DONOT_PROCESS;
    }

    FcitxKeySym sym = (FcitxKeySym) FcitxInputStateGetKeySym(input);
    uint32_t keycode = FcitxInputStateGetKeyCode(input);
    uint32_t state = FcitxInputStateGetKeyState(input);
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
    mozcState->inUsageState = false;
    mozcState->mozc->resetim();
}

void FcitxMozcReset(void* arg)
{
    FcitxMozcState* mozcState = (FcitxMozcState*) arg;
    mozcState->mozc->reset();

}

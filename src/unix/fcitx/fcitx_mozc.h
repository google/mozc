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

#ifndef MOZC_UNIX_FCITX_FCITX_MOZC_H_
#define MOZC_UNIX_FCITX_FCITX_MOZC_H_

#include <cstdint>
#include <memory>

#include <fcitx/instance.h>
#include <fcitx/candidate.h>
#include <fcitx-config/hotkey.h>
#include <libintl.h>

#include "base/port.h"
#include "base/run_level.h"
#include "protocol/commands.pb.h"
#include "client/client_interface.h"
#include "mozc_connection.h"

#define _(x) dgettext("fcitx-mozc", (x))

INPUT_RETURN_VALUE FcitxMozcGetCandidateWord(void* arg, FcitxCandidateWord* candWord);;

namespace mozc
{

namespace fcitx
{
const int32_t kBadCandidateId = -12345;
class IMEngineFactory;
class MozcConnectionInterface;
class MozcResponseParser;
class KeyTranslator;

struct PreeditItem {
        std::string str;
        FcitxMessageType type;
};

// Preedit string and its attributes.
struct PreeditInfo
{
    uint32_t cursor_pos;
    
    std::vector<PreeditItem> preedit;
};

class FcitxMozc
{
public:
    // This constructor is used by unittests.
    FcitxMozc ( FcitxInstance* instance,
                MozcConnectionInterface *connection,
                MozcResponseParser *parser );
    FcitxMozc(const FcitxMozc &) = delete;
    virtual ~FcitxMozc();

    bool process_key_event (FcitxKeySym sym, uint32_t keycode, uint32_t state, bool layout_is_jp, bool is_key_up);
    void select_candidate ( FcitxCandidateWord* candWord );
    void resetim();
    void reset();
    void init();
    void focus_out();
    bool paging(bool prev);

    // Functions called by the MozcResponseParser class to update UI.

    // Displays a 'result' (aka 'commit string') on FCITX UI.
    void SetResultString ( const std::string &result_string );
    // Displays a 'preedit' string on FCITX UI. This function takes ownership
    // of preedit_info. If the parameter is NULL, hides the string currently
    // displayed.
    void SetPreeditInfo ( const PreeditInfo *preedit_info );
    // Displays an auxiliary message (e.g., an error message, a title of
    // candidate window). If the string is empty (""), hides the message
    // currently being displayed.
    void SetAuxString ( const std::string &str );
    // Sets a current composition mode (e.g., Hankaku Katakana).
    void SetCompositionMode ( mozc::commands::CompositionMode mode );
    
    void SendCompositionMode ( mozc::commands::CompositionMode mode );

    // Sets the url to be opened by the default browser.
    void SetUrl ( const std::string &url );

    const std::string& GetIconFile(const std::string key);
    
    const std::string& GetCurrentCompositionModeIcon();
    
    mozc::commands::CompositionMode GetCompositionMode() { return composition_mode_; }
    
    FcitxInstance* GetInstance() { return instance; }
    
    FcitxInputState* GetInputState();

    mozc::client::ClientInterface* GetClient() { return connection_->GetClient(); }

    bool SendCommand(const mozc::commands::SessionCommand& session_command, mozc::commands::Output* new_output);

    void SetUsage(const std::string& title, const std::string& description);

    std::pair<std::string, std::string> GetUsage();

    void DrawAll();

private:
    friend class FcitxMozcTest;

    // Adds Mozc-specific icons to FCITX toolbar.
    void InitializeBar();
    
    void InitializeMenu();

    // Parses the response from mozc_server. Returns whether the server consumes
    // the input or not (true means 'consumed').
    bool ParseResponse ( const mozc::commands::Output &request );

    void ClearAll();
    void DrawPreeditInfo();
    void DrawAux();

    // Open url_ with a default browser.
    void OpenUrl();

    FcitxInstance* instance;
    FcitxInputState* input;
    const std::unique_ptr<MozcConnectionInterface> connection_;
    const std::unique_ptr<MozcResponseParser> parser_;

    // Strings and a window currently displayed on FCITX UI.
    std::unique_ptr<const PreeditInfo> preedit_info_;
    std::string aux_;  // error tooltip, or candidate window title.
    std::string url_;  // URL to be opened by a browser.
    mozc::commands::CompositionMode composition_mode_;
    
    std::map<std::string, std::string> iconMap;
    
    FcitxUIMenu compositionMenu;
    FcitxUIMenu toolMenu;
    std::string description;
    std::string title;
};

}  // namespace fcitx

}  // namespace mozc

#endif  // MOZC_UNIX_FCITX_FCITX_MOZC_H_


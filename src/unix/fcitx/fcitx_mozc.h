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


#ifndef MOZC_UNIX_FCITX_FCITX_MOZC_H_
#define MOZC_UNIX_FCITX_FCITX_MOZC_H_

#include <fcitx/instance.h>
#include <fcitx/candidate.h>
#include <fcitx-config/hotkey.h>
#include <libintl.h>
#include "base/base.h"  // for DISALLOW_COPY_AND_ASSIGN.
#include "base/run_level.h"
#include "session/commands.pb.h"

#define _(x) dgettext("fcitx-mozc", (x))

INPUT_RETURN_VALUE FcitxMozcGetCandidateWord(void* arg, FcitxCandidateWord* candWord);;

namespace mozc
{

namespace fcitx
{
const int32 kBadCandidateId = -12345;
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
    uint32 cursor_pos;
    
    std::vector<PreeditItem> preedit;
};

class FcitxMozc
{
public:
    // This constructor is used by unittests.
    FcitxMozc ( FcitxInstance* instance,
                MozcConnectionInterface *connection,
                MozcResponseParser *parser );
    virtual ~FcitxMozc();

    bool process_key_event (FcitxKeySym sym, uint32 keycode, uint32 state, bool layout_is_jp, bool is_key_up);
    void select_candidate ( FcitxCandidateWord* candWord );
    void resetim();
    void reset();
    void init();
    void focus_out();

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
    void SetUrl ( const string &url );

    const std::string& GetIconFile(const std::string key);
    
    const std::string& GetCurrentCompositionModeIcon();
    
    mozc::commands::CompositionMode GetCompositionMode() { return composition_mode_; }
    
    FcitxInstance* GetInstance() { return instance; }
    
    FcitxInputState* GetInputState();

private:
    friend class FcitxMozcTest;

    // Adds Mozc-specific icons to FCITX toolbar.
    void InitializeBar();
    
    void InitializeMenu();

    // Parses the response from mozc_server. Returns whether the server consumes
    // the input or not (true means 'consumed').
    bool ParseResponse ( const mozc::commands::Output &request );

    void ClearAll();
    void DrawAll();
    void DrawPreeditInfo();
    void DrawAux();

    // Open url_ with a default browser.
    void OpenUrl();

    FcitxInstance* instance;
    FcitxInputState* input;
    const scoped_ptr<MozcConnectionInterface> connection_;
    const scoped_ptr<MozcResponseParser> parser_;

    // Strings and a window currently displayed on FCITX UI.
    scoped_ptr<const PreeditInfo> preedit_info_;
    std::string aux_;  // error tooltip, or candidate window title.
    string url_;  // URL to be opened by a browser.
    mozc::commands::CompositionMode composition_mode_;
    
    std::map<std::string, std::string> iconMap;
    
    FcitxUIMenu compositionMenu;
    FcitxUIMenu toolMenu;

    DISALLOW_COPY_AND_ASSIGN ( FcitxMozc );
};

}  // namespace fcitx

}  // namespace mozc

#endif  // MOZC_UNIX_FCITX_FCITX_MOZC_H_


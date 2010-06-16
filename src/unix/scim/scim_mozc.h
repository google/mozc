// Copyright 2010, Google Inc.
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

#ifndef MOZC_UNIX_SCIM_SCIM_MOZC_H_
#define MOZC_UNIX_SCIM_SCIM_MOZC_H_

#define Uses_SCIM_FRONTEND_MODULE

#include <scim.h>

#include "base/base.h"  // for DISALLOW_COPY_AND_ASSIGN.
#include "base/run_level.h"
#include "session/commands.pb.h"

namespace mozc_unix_scim {

class IMEngineFactory;
class MozcConnectionInterface;
class MozcLookupTable;
class MozcResponseParser;
class ScimKeyTranslator;

// Preedit string and its attributes.
struct PreeditInfo {
  uint32 cursor_pos;
  scim::WideString str;
  scim::AttributeList attribute_list;
};

// This class implements IMEngineInstanceBase interface.
// See /usr/include/scim-1.0/scim_imengine.h for details of the interface.
class ScimMozc : public scim::IMEngineInstanceBase {
 public:
  static ScimMozc *CreateScimMozc(scim::IMEngineFactoryBase *factory,
                                  const scim::String &encoding, int id,
                                  const scim::ConfigPointer *config);
  virtual ~ScimMozc();

  // Event handler functions. Overrides functions in scim::IMEngineInstanceBase.
  virtual bool process_key_event(const scim::KeyEvent &key);
  virtual void select_candidate(unsigned int index);
  virtual void reset();
  virtual void focus_in();
  virtual void trigger_property(const scim::String &property);

  // Functions called by the MozcResponseParser class to update UI.

  // Displays a 'result' (aka 'commit string') on SCIM UI.
  virtual void SetResultString(const scim::WideString &result_string);
  // Displays a 'candidate window' (aka 'lookup table') on SCIM UI. This
  // function takes ownership of candidates. If candidates is NULL, hides
  // the window currently displayed.
  virtual void SetCandidateWindow(const MozcLookupTable *candidates);
  // Displays a 'preedit' string on SCIM UI. This function takes ownership
  // of preedit_info. If the parameter is NULL, hides the string currently
  // displayed.
  virtual void SetPreeditInfo(const PreeditInfo *preedit_info);
  // Displays an auxiliary message (e.g., an error message, a title of
  // candidate window). If the string is empty (""), hides the message
  // currently being displayed.
  virtual void SetAuxString(const scim::String &str);
  // Sets a current composition mode (e.g., Hankaku Katakana).
  virtual void SetCompositionMode(mozc::commands::CompositionMode mode);

  // Sets the url to be opened by the default browser.
  virtual void SetUrl(const string &url);

 private:
  friend class ScimMozcTest;
  // This constructor is used by unittests.
  ScimMozc(scim::IMEngineFactoryBase *factory,
           const scim::String &encoding, int id,
           const scim::ConfigPointer *config,
           MozcConnectionInterface *connection,
           MozcResponseParser *parser);

  // Adds Mozc-specific icons to SCIM toolbar.
  void InitializeBar();
  const char *GetCurrentCompositionModeIcon() const;
  const char *GetCurrentCompositionModeLabel() const;

  // Parses the response from mozc_server. Returns whether the server consumes
  // the input or not (true means 'consumed').
  bool ParseResponse(const mozc::commands::Output &request);

  void ClearAll();
  void DrawAll();
  void DrawCandidateWindow();
  void DrawPreeditInfo();
  void DrawAux();

  // Open url_ with a default browser.
  void OpenUrl();

  const scoped_ptr<MozcConnectionInterface> connection_;
  const scoped_ptr<MozcResponseParser> parser_;

  // Strings and a window currently displayed on SCIM UI.
  scoped_ptr<const PreeditInfo> preedit_info_;
  scoped_ptr<const MozcLookupTable> candidates_;
  scim::String aux_;  // error tooltip, or candidate window title.
  string url_;  // URL to be opened by a browser.
  mozc::commands::CompositionMode composition_mode_;

  DISALLOW_COPY_AND_ASSIGN(ScimMozc);
};

}  // namespace mozc_unix_scim

#endif  // MOZC_UNIX_SCIM_SCIM_MOZC_H_

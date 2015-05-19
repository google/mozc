// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_LANGUAGES_HANGUL_SESSION_H_
#define MOZC_LANGUAGES_HANGUL_SESSION_H_

#include <hangul-1.0/hangul.h>
#include <deque>
#include <set>
#include <string>

#include "base/base.h"
#include "session/commands.pb.h"
#include "session/key_event_util.h"
#include "session/session_interface.h"
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace hangul {
class Session: public mozc::session::SessionInterface {
 public:
  // TODO(nona) use Mozc's commands list.
  enum CandidateOperation {
    NEXT_CANDIDATE = 0,
    PREV_CANDIDATE = 1,
    NEXT_PAGE_CANDIDATE = 2,
    PREV_PAGE_CANDIDATE = 3,
    NO_SELECT_CANDIDATE = 4,
    INITIAL_SELECTED_CANDIDATE = 5,
    DO_NOT_CHANGE_SELECTED_CANDIDATE = 6
  };

  enum InputMode {
    kHangulMode = 0,
    kHanjaLockMode = 1
  };

  Session();
  virtual ~Session();

  virtual bool SendKey(commands::Command *command);

  // Checks if the input key event will be consumed by the session.
  virtual bool TestSendKey(commands::Command *command);

  // Performs the SEND_COMMAND command defined commands.proto.
  virtual bool SendCommand(commands::Command *command);

  virtual void ReloadConfig();

  // Sets client capability for this session.  Used by unittest.
  virtual void set_client_capability(const commands::Capability &capability);

  // Sets application information for this session.
  virtual void set_application_info(
      const commands::ApplicationInfo &application_info);

  // Gets application information
  virtual const commands::ApplicationInfo &application_info() const;

  // Returns the time when this instance was created.
  virtual uint64 create_session_time() const;

  // Returns 0 (default value) if no command is executed in this session.
  virtual uint64 last_command_time() const;

#ifdef OS_CHROMEOS
  static void UpdateConfig(const config::HangulConfig &config);
#endif

 private:
#ifdef OS_CHROMEOS
  friend class HangulSessionTest;
#endif

  // Sets configurations.
  void ResetConfig();

  // Resets hanja list and current index
  bool ResetHanjaList();

  // Gets current preedit string.
  void GetPreeditString(string *result) const;

  // Gets commited string.
  void GetCommitString(string *result) const;

  // Gets current candidate string. This function stores the selected candidate
  // string into "result". If there are no selected candidates, this function
  // returns false.
  bool GetSelectedCandidate(string *result);

  // Selects candidate by shortcut id. If there are no corresponding candidates,
  // this function returns false.
  bool SelectCandidateByShortcut(const int id);

  // Sets current selected hanja candidate into result. If there are any
  // selected hanja candidates or no candidate winwow is shown, this function
  // returns false.
  bool CommitSelectedCandidate(commands::Command *command);

  // Flushes the current preedit string.
  // This function stores the remaining preedit string into "result" if exists.
  // If set result argument as NULL, this function flushes and dicards rest
  // strings. This function returns true if successfully flushed, otherwise
  // returns false.
  bool FlushPreedit(string *result);

  // Cancels the current context.
  // This function flushes the current preedit and sets this string into output
  // as a result string. Then clear current hanja candidates if exists.
  void CancelContext(commands::Output *output);

  // Cancels hanja-selection.
  void CancelHanjaSelection(commands::Output *output);

  // Looks up hanja list from hanja list with preedit string.
  bool HanjaLookup(commands::Command *command);

  // Updates candidate status. This function sets corresponding preedit string
  // into output, even if there are no candidate to show.
  void UpdateCandidate(CandidateOperation candidate_operation,
                       commands::Command *command);

  // Processes hangul mode. If key event would be consumed, this
  // function return true, otherwise returns false.
  bool ProcessHangulMode(commands::Command *command);

  // Processes selection of Hanja list. The return value means as same as
  // ProcessHangulMode(). This function returns same as ProcessKeyEvent..
  bool ProcessHanjaSelection(commands::Command *command);

  // Process Hanja-lock Mode. If key event would be consumed, this function
  // returns true, otherwise returns false.
  bool ProcessHanjaLockMode(commands::Command *command);

  // Processes ordinal key event. If key event would be processed, this function
  // returns true, otherwise returns false.
  bool ProcessKeyEvent(commands::Command *command);

  // Processes back-space key. If there is no preedit string, this function
  // returns false. The return value means as same as ProcessKey().
  bool ProcessBSKey(commands::Command *command);

  // Updates current preedit with hanja string.
  void UpdatePreeditWithHanjaString(const string &hanja,
                                    commands::Command *command);

  // Returns true if hanja selection window is shown.
  bool IsHanjaSelectionMode();

  // Processes Won key. If nothing to do with won key, this function returns
  // false, otherwise this function returns true.
  // Hangul keyboard has Won-sign key, which is actually backslash ('\', U+5C)
  // in other keyboards.  This key behaves as a backslash key nomally, but it
  // actually emits Won character (U+20A9 or U+FFE6) with Alt-modifier.  This
  // method handles such special case like this:
  //  - Alt + won key = U+20A9("₩")
  //  - Alt + Shift + won key = U+FFE6("￦")
  //  - otherwise = behave as normal backslash key
  // See http://code.google.com/p/chromium-os/issues/detail?id=15947
  // And when won key is pressed, commit preedit string or commit selected
  // candidate. This behavior same as non alphabetic key (see SendKey method
  // description comment for the detail description).
  // TODO(nona): make it customizable
  // TODO(nona): fix key translation. When Alt+Shift+'\' typed, keyevent is
  // expected as Alt+Shift+'\'(92). However, actual keyevent is
  // Alt+Shift+'|'(124).
  bool ProcessWonKey(commands::Command *command);

  // Throws the existing hangul context and create another one again
  // to clear context completely.
  void RenewContext();

  // If context has reproducible preedit, returns true, otherwise returns false.
  // The meaning of "Reproducible Preedit" is that the preedit can be back to
  // previous state with ProcessBSKey. That is, the preedit is handled by
  // libhangul.
  bool HasReproduciblePreedit();

  // Return true if the combination of the key and its modifiers is usable for
  // a shortcut.
  bool IsKeyEventForShortcut(const commands::KeyEvent &key_event);

  // Reloads symbol dictionary.
  bool ReloadSymbolDictionary(const string &symbol_dictionary_filename);

  HangulInputContext *context_;

  HanjaTable *hanja_table_;
  HanjaTable *symbol_table_;
  // hanja_list_ is the list of chinese characters for the hangul characters the
  // user has typed.
  // TODO(nona) use CandidateList.
  HanjaList *hanja_list_;
  int hanja_index_;

  // TODO(nona): pack context_, hanja_table_, hanja_list_, hanja_index_,
  // hanja_lock_preedit_ into new context object.
  deque<char32> hanja_lock_preedit_;
  InputMode current_mode_;
  commands::ApplicationInfo application_info_;
  uint64 create_session_time_;
  uint64 last_command_time_;
  uint64 last_config_updated_;
  set<KeyInformation> hanja_key_set_;
};

}  // namespace hangul
}  // namespace mozc
#endif  // MOZC_LANGUAGES_HANGUL_SESSION_H_

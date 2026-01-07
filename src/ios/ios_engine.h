// Copyright 2010-2021, Google Inc.
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

#ifndef MOZC_IOS_IOS_ENGINE_H_
#define MOZC_IOS_IOS_ENGINE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {

class SessionHandler;

namespace ios {

// Class to integrate Mozc engine and Gboard client.
// This class has not been matured yet. It will be iteratively expanded more
// or replaced with other modules.
class IosEngine {
 public:
  enum class InputMode {
    HIRAGANA,
    ALPHABET,
    DIGIT,
  };

  // Initializes the underlying engine instance with the specified data.  If it
  // fails to load, falls back to the embedded (low quality) engine.
  explicit IosEngine(const std::string& data_file_path);

  ~IosEngine();

  // The following methods are helpers to populate command proto and send it
  // to the session handler.  Input to and output from the session handler are
  // recorded in |command|.  The return value is the return value of
  // SessionHandler::EvalCommand.

  // Sets request to mobile spec.  Acceptable |keyboard_layout| are:
  // "12KEYS", "QWERTY_JA".
  bool SetMobileRequest(const std::string& keyboard_layout,
                        commands::Command* command);

  // Fills mobile settings to config.
  static void FillMobileConfig(config::Config* config);

  // Sets the config to the engine.
  bool SetConfig(const config::Config& config, commands::Command* command);

  // Creates a session.  If there's already a created session, it is deleted.
  // The created session is managed by this instance.
  bool CreateSession(commands::Command* command);

  // Deletes the session currently managed.
  bool DeleteSession(commands::Command* command);

  // Resets the current context.  If the reset is already done before,
  // nothing happens and false is returned. false is returned on error too.
  bool ResetContext(commands::Command* command);

  // Sends a special key event (e.g., backspace, arrows).
  bool SendSpecialKey(commands::KeyEvent::SpecialKey special_key,
                      commands::Command* command);

  // Sends a normal key event.  When |character| is one byte, it is sent as
  // key_code; otherwise it is sent as key_string.
  bool SendKey(const std::string& character, commands::Command* command);

  // Maybe creates a new chunk by sending STOP_KEY_TOGGLING command. This method
  // is intended to be used by a timer thread to exit toggle state in 12-key
  // toggle flick layout.  Thus, this method fails if:
  //   * The current Romaji table is not TOGGLE_FLICK_TO_HIRAGANA_INTUITIVE.
  //   * Another thread is using the underlying engine.
  // The second case is expected because this method is intended to be called by
  // a timer thread only when there's no key event for a while after a key press
  // event is occurred.  The use of underlying engine means some events have
  // occurred when the timer thread calls this method, so it's expected not to
  // send the special key to the engine.
  bool MaybeCreateNewChunk(commands::Command* command);

  // Sends a session command.
  bool SendSessionCommand(const commands::SessionCommand& session_command,
                          commands::Command* command);

  // Convenient aliases for SendSessionCommand.
  bool Submit(commands::Command* command) {
    commands::SessionCommand session_command;
    session_command.set_type(commands::SessionCommand::SUBMIT);
    return SendSessionCommand(session_command, command);
  }
  bool SubmitCandidate(int index, commands::Command* command) {
    commands::SessionCommand session_command;
    session_command.set_type(commands::SessionCommand::SUBMIT_CANDIDATE);
    session_command.set_id(index);
    return SendSessionCommand(session_command, command);
  }
  bool UndoOrRewind(commands::Command* command) {
    commands::SessionCommand session_command;
    session_command.set_type(commands::SessionCommand::UNDO_OR_REWIND);
    return SendSessionCommand(session_command, command);
  }

  // Switches input mode by configuring Mozc engine.
  bool SwitchInputMode(InputMode mode);

  // Imports a dictionary of TSV format as a user dictionary, where each line
  // should be formatted as: READING<tab>WORD<tab>POS.  If the content is empty,
  // it deletes the user dictionary.  This method is intended to be used for iOS
  // system dictionary.
  bool ImportUserDictionary(const std::string& tsv_content,
                            commands::Command* command);

  // Clear user input history of both conversion and prediction from storage.
  bool ClearUserHistory(commands::Command* command);

 private:
  // A configuration of Mozc engine, which corresponds to each of input layout.
  struct InputConfig {
    commands::Request::SpecialRomanjiTable romaji_table;
    commands::CompositionMode composition_mode;
  };

  // A set of input configurations for one keyboard set (prime, symbol, and
  // digit).
  struct InputConfigTuple {
    InputConfig hiragana_config;
    InputConfig alphabet_config;
    InputConfig digit_config;
  };

  static InputConfigTuple GetInputConfigTupleFromLayoutName(
      const std::string& layout);

  bool EvalCommandLockGuarded(commands::Command* command);
  bool SetSpecialRomajiTable(commands::Request::SpecialRomanjiTable table);
  bool Reload(commands::Command* command);

  absl::Mutex mutex_;
  std::unique_ptr<SessionHandler> session_handler_ ABSL_PT_GUARDED_BY(mutex_);
  uint64_t session_id_ = 0;
  commands::Request current_request_;
  InputConfigTuple current_config_tuple_;
  InputConfig* current_input_config_ = nullptr;

  // Command called just before. NONE is used as a n/a value.
  commands::SessionCommand::CommandType previous_command_;
};

}  // namespace ios
}  // namespace mozc
#endif  // MOZC_IOS_IOS_ENGINE_H_

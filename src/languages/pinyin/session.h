// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_LANGUAGES_PINYIN_SESSION_H_
#define MOZC_LANGUAGES_PINYIN_SESSION_H_

#include <string>

#include "base/base.h"
#include "languages/pinyin/pinyin_constant.h"
#include "session/commands.pb.h"
#include "session/session_interface.h"

namespace mozc {

#ifdef OS_CHROMEOS
namespace config {
class PinyinConfig;
}  // namespace config
#endif  // OS_CHROMEOS

namespace pinyin {
namespace keymap {
class KeymapInterface;
}  // namespace keymap

class SessionConverterInterface;
struct SessionConfig;

class Session : public mozc::session::SessionInterface {
 public:
  Session();
  virtual ~Session();

  virtual bool SendKey(commands::Command *command);
  // Checks if the input key event will be consumed by the session.
  virtual bool TestSendKey(commands::Command *command);
  virtual bool SendCommand(commands::Command *command);
  virtual void ReloadConfig();

  // Sets client capability for this session.  Used by unittest.
  // TODO(hsumita): rename this function to set_client_capability_for_unittest
  virtual void set_client_capability(const commands::Capability &capability);
  virtual void set_application_info(
      const commands::ApplicationInfo &application_info);
  virtual const commands::ApplicationInfo &application_info() const;
  virtual uint64 create_session_time() const;
  // Returns 0 (default value) if no command is executed in this session.
  virtual uint64 last_command_time() const;

#ifdef OS_CHROMEOS
  static void UpdateConfig(const config::PinyinConfig &config);
#endif

 private:
  friend class PinyinSessionTest;

  bool ProcessKeyEvent(commands::Command *command);
  bool ProcessCommand(commands::Command *command);
  void ResetContext();
  void ResetConfig();
  // Switch conversion mode. Previous context is cleared.
  // We should fill a protocol buffer before we call it if we have some data.
  void SwitchConversionMode(ConversionMode mode);
  void HandleLanguageBarCommand(
      const commands::SessionCommand &session_command);

  scoped_ptr<SessionConfig> session_config_;
  scoped_ptr<SessionConverterInterface> converter_;
  const keymap::KeymapInterface *keymap_;
  ConversionMode conversion_mode_;
  // Stores conversion mode which we should switched to at the end of SendKey()
  // or SendCommand() to fill a protocol buffer correctly.
  ConversionMode next_conversion_mode_;
  bool is_already_commited_;

  uint64 create_session_time_;
  commands::ApplicationInfo application_info_;
  uint64 last_command_time_;
  uint64 last_config_updated_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace pinyin
}  // namespace mozc
#endif  // MOZC_LANGUAGES_PINYIN_SESSION_H_

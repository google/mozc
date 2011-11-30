// Copyright 2010-2011, Google Inc.
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

// ImeContext class contains the whole internal variables representing
// a session.

#ifndef MOZC_SESSION_INTERNAL_IME_CONTEXT_H_
#define MOZC_SESSION_INTERNAL_IME_CONTEXT_H_

#include <map>
#include <string>

#include "base/base.h"
#include "session/commands.pb.h"

namespace mozc {

namespace composer {
class Composer;
}  // namespace composer

namespace session {
class SessionConverterInterface;

typedef map<string, commands::KeyEvent> TransformTable;

class ImeContext {
 public:
  ImeContext();
  virtual ~ImeContext();

  uint64 create_time() const;
  void set_create_time(uint64 create_time);
  uint64 last_command_time() const;
  void set_last_command_time(uint64 last_time);

  // Note that before using getter methods,
  // |composer_| must be set non-null value.
  const composer::Composer &composer() const;
  composer::Composer *mutable_composer();
  void set_composer(composer::Composer *composer);

  const SessionConverterInterface &converter() const;
  SessionConverterInterface *mutable_converter();
  void set_converter(SessionConverterInterface *converter);

  enum State {
    NONE = 0,
    DIRECT = 1,
    PRECOMPOSITION = 2,
    COMPOSITION = 4,
    CONVERSION = 8,
  };
  State state() const;
  void set_state(State state);

  const TransformTable &transform_table() const;
  TransformTable *mutable_transform_table();

  config::Config::SessionKeymap keymap() const;
  void set_keymap(config::Config::SessionKeymap keymap);

  const commands::Capability &client_capability() const;
  commands::Capability *mutable_client_capability();

  const commands::ApplicationInfo &application_info() const;
  commands::ApplicationInfo *mutable_application_info();

  const commands::Output &output() const;
  commands::Output *mutable_output();

  // Copy |source| context to |destination| context.  This function
  // does not perform compele copying at this moment.
  static void CopyContext(const ImeContext &src, ImeContext *dest);

 private:
  uint64 create_time_;
  uint64 last_command_time_;

  scoped_ptr<composer::Composer> composer_;

  scoped_ptr<SessionConverterInterface> converter_;

  State state_;
  TransformTable transform_table_;
  config::Config::SessionKeymap keymap_;

  commands::Capability client_capability_;

  commands::ApplicationInfo application_info_;

  // Storing the last output consisting of the last result and the
  // last performed command.
  commands::Output output_;
};

}  // namespace session
}  // namespace mozc
#endif  // MOZC_SESSION_INTERNAL_IME_CONTEXT_H_

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

#ifndef MOZC_UNIX_IBUS_CANDIDATE_WINDOW_HANDLER_H_
#define MOZC_UNIX_IBUS_CANDIDATE_WINDOW_HANDLER_H_

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/renderer_interface.h"
#include "unix/ibus/candidate_window_handler_interface.h"
#include "unix/ibus/ibus_wrapper.h"

namespace mozc {
namespace ibus {

class GsettingsObserver;

class CandidateWindowHandler : public CandidateWindowHandlerInterface {
 public:
  CandidateWindowHandler(const CandidateWindowHandler &) = delete;
  CandidateWindowHandler &operator=(const CandidateWindowHandler &) = delete;
  explicit CandidateWindowHandler(
      std::unique_ptr<renderer::RendererInterface> renderer);
  virtual ~CandidateWindowHandler();

  virtual void Update(IbusEngineWrapper *engine,
                      const commands::Output &output);
  virtual void UpdateCursorRect(IbusEngineWrapper *engine);
  virtual void Hide(IbusEngineWrapper *engine);
  virtual void Show(IbusEngineWrapper *engine);

  virtual void OnIBusCustomFontDescriptionChanged(
      const std::string &custom_font_description);

  virtual void OnIBusUseCustomFontDescriptionChanged(
      bool use_custom_font_description);

  void RegisterGSettingsObserver();

  void OnSettingsUpdated(absl::string_view key,
                         const GsettingsWrapper::Variant &value);

 protected:
  bool SendUpdateCommand(IbusEngineWrapper *engine,
                         const commands::Output &output, bool visibility);

  std::unique_ptr<renderer::RendererInterface> renderer_;
  std::unique_ptr<commands::Output> last_update_output_;

 private:
  std::string GetFontDescription() const;
  std::string custom_font_description_;
  bool use_custom_font_description_;
  std::unique_ptr<GsettingsObserver> settings_observer_;
  commands::RendererCommand::Rectangle preedit_begin_;
};

}  // namespace ibus
}  // namespace mozc
#endif  // MOZC_UNIX_IBUS_CANDIDATE_WINDOW_HANDLER_H_

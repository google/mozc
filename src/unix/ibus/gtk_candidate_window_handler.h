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

#ifndef MOZC_UNIX_IBUS_GTK_CANDIDATE_WINDOW_HANDLER_H_
#define MOZC_UNIX_IBUS_GTK_CANDIDATE_WINDOW_HANDLER_H_

#include <memory>

#include "base/port.h"
#include "unix/ibus/candidate_window_handler_interface.h"

namespace mozc {
namespace commands {
class RendererCommand;
}  // namespace commands
namespace renderer {
class RendererInterface;
}  // namespace renderer
namespace ibus {

class GSettingsObserver;

class GtkCandidateWindowHandler : public CandidateWindowHandlerInterface {
 public:
  // GtkCandidateWindowHandler takes ownership of renderer_ pointer.
  explicit GtkCandidateWindowHandler(renderer::RendererInterface *renderer);
  virtual ~GtkCandidateWindowHandler();

  virtual void Update(IBusEngine *engine, const commands::Output &output);
  virtual void UpdateCursorRect(IBusEngine *engine);
  virtual void Hide(IBusEngine *engine);
  virtual void Show(IBusEngine *engine);

  virtual void OnIBusCustomFontDescriptionChanged(
      const std::string &custom_font_description);

  virtual void OnIBusUseCustomFontDescriptionChanged(
      bool use_custom_font_description);

  void RegisterGSettingsObserver();

 protected:
  bool SendUpdateCommand(IBusEngine *engine, const commands::Output &output,
                         bool visibility) const;

  std::unique_ptr<renderer::RendererInterface> renderer_;
  std::unique_ptr<commands::Output> last_update_output_;

 private:
  std::string GetFontDescription() const;
  std::string custom_font_description_;
  bool use_custom_font_description_;
  std::unique_ptr<GSettingsObserver> settings_observer_;

  DISALLOW_COPY_AND_ASSIGN(GtkCandidateWindowHandler);
};

}  // namespace ibus
}  // namespace mozc
#endif  // MOZC_UNIX_IBUS_GTK_CANDIDATE_WINDOW_HANDLER_H_

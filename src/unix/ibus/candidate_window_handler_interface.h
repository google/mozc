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

#ifndef MOZC_UNIX_IBUS_CANDIDATE_WINDOW_HANDLER_INTERFACE_H_
#define MOZC_UNIX_IBUS_CANDIDATE_WINDOW_HANDLER_INTERFACE_H_

#include <string>

#include "unix/ibus/ibus_wrapper.h"

namespace mozc {
namespace commands {
class Output;
}  // namespace commands
namespace ibus {

class CandidateWindowHandlerInterface {
 public:
  CandidateWindowHandlerInterface() {}
  CandidateWindowHandlerInterface(const CandidateWindowHandlerInterface &) =
      delete;
  CandidateWindowHandlerInterface &operator=(
      const CandidateWindowHandlerInterface &) = delete;
  virtual ~CandidateWindowHandlerInterface() {}

  // Updates candidate state. This function also shows or hides candidate window
  // based on output argument.
  virtual void Update(IbusEngineWrapper *engine,
                      const commands::Output &output) = 0;

  // Updates candidate state. This function also shows or hides candidate window
  // based on the last |Update| call.
  virtual void UpdateCursorRect(IbusEngineWrapper *engine) = 0;

  // Hides candidate window.
  virtual void Hide(IbusEngineWrapper *engine) = 0;

  // Shows candidate window.
  virtual void Show(IbusEngineWrapper *engine) = 0;

  // Following methods handle property-changed events relevant to ibus-panel.
  // |custom_font_description| should be a string representation of
  // PangoFontDescription.
  // http://developer.gnome.org/pango/stable/pango-Fonts.html#pango-font-description-from-string
  virtual void OnIBusCustomFontDescriptionChanged(
      const std::string &custom_font_description) = 0;
  virtual void OnIBusUseCustomFontDescriptionChanged(
      bool use_custom_font_description) = 0;
};

}  // namespace ibus
}  // namespace mozc
#endif  // MOZC_UNIX_IBUS_CANDIDATE_WINDOW_HANDLER_INTERFACE_H_

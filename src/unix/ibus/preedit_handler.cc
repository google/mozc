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

#include "unix/ibus/preedit_handler.h"

#include <string>

#include "absl/log/log.h"
#include "protocol/commands.pb.h"
#include "unix/ibus/ibus_wrapper.h"

namespace mozc {
namespace ibus {

namespace {
// Returns an IBusText composed from |preedit| to render preedit text.
// Caller must release the returned IBusText object.
IbusTextWrapper ComposePreeditText(const commands::Preedit& preedit) {
  std::string data;
  for (int i = 0; i < preedit.segment_size(); ++i) {
    data.append(preedit.segment(i).value());
  }
  IbusTextWrapper text(data);

  int start = 0;
  int end = 0;
  for (int i = 0; i < preedit.segment_size(); ++i) {
    const commands::Preedit::Segment& segment = preedit.segment(i);
    IBusAttrUnderline attr = IBUS_ATTR_UNDERLINE_ERROR;
    switch (segment.annotation()) {
      case commands::Preedit::Segment::NONE:
        // No decoration
        break;
      case commands::Preedit::Segment::UNDERLINE:
        text.AppendAttribute(IBUS_ATTR_TYPE_HINT, IBUS_ATTR_PREEDIT_WHOLE,
                             start, end);
        break;
      case commands::Preedit::Segment::HIGHLIGHT:
        text.AppendAttribute(IBUS_ATTR_TYPE_HINT, IBUS_ATTR_PREEDIT_SELECTION,
                             start, end);
        break;
      default:
        LOG(ERROR) << "unknown annotation:" << segment.annotation();
        break;
    }
    start = end;
  }

  return text;
}

// Returns a cursor position used for updating preedit.
// NOTE: We do not use a cursor position obtained from Mozc when the candidate
// window is shown since ibus uses the cursor position to locate the candidate
// window and the position obtained from Mozc is not what we expect.
int CursorPos(const commands::Output& output) {
  if (!output.has_preedit()) {
    return 0;
  }
  if (output.preedit().has_highlighted_position()) {
    return output.preedit().highlighted_position();
  }
  return output.preedit().cursor();
}

}  // namespace

bool PreeditHandler::Update(IbusEngineWrapper* engine,
                            const commands::Output& output) {
  if (!output.has_preedit()) {
    engine->ClearPreeditText();
    engine->HidePreeditText();
    return true;
  }
  IbusTextWrapper text = ComposePreeditText(output.preedit());
  engine->UpdatePreeditTextWithMode(&text, CursorPos(output));
  // |text| is released by ibus_engine_update_preedit_text.
  return true;
}

}  // namespace ibus
}  // namespace mozc

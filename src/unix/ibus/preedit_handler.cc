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

#include <string>

#include "session/commands.pb.h"
#include "unix/ibus/preedit_handler.h"

namespace mozc {
namespace ibus {

namespace {
// Returns an IBusText composed from |preedit| to render preedit text.
// Caller must release the returned IBusText object.
IBusText *ComposePreeditText(const commands::Preedit &preedit) {
  string data;
  for (int i = 0; i < preedit.segment_size(); ++i) {
    data.append(preedit.segment(i).value());
  }
  IBusText *text = ibus_text_new_from_string(data.c_str());

  int start = 0;
  int end = 0;
  for (int i = 0; i < preedit.segment_size(); ++i) {
    const commands::Preedit::Segment &segment = preedit.segment(i);
    IBusAttrUnderline attr = IBUS_ATTR_UNDERLINE_ERROR;
    switch (segment.annotation()) {
      case commands::Preedit::Segment::NONE:
        attr = IBUS_ATTR_UNDERLINE_NONE;
        break;
      case commands::Preedit::Segment::UNDERLINE:
        attr = IBUS_ATTR_UNDERLINE_SINGLE;
        break;
      case commands::Preedit::Segment::HIGHLIGHT:
        attr = IBUS_ATTR_UNDERLINE_DOUBLE;
        break;
      default:
        LOG(ERROR) << "unknown annotation:" << segment.annotation();
        break;
    }
    end += segment.value_length();
    ibus_text_append_attribute(text,
                               IBUS_ATTR_TYPE_UNDERLINE,
                               attr,
                               start,
                               end);

    // Many applications show a single underline regardless of using
    // IBUS_ATTR_UNDERLINE_SINGLE or IBUS_ATTR_UNDERLINE_DOUBLE for some
    // reasons. Here we add a background color for the highlighted candidate
    // to make it easiliy distinguishable.
    if (segment.annotation() == commands::Preedit::Segment::HIGHLIGHT) {
      const guint kBackgroundColor = 0xD1EAFF;
      ibus_text_append_attribute(text,
                                 IBUS_ATTR_TYPE_BACKGROUND,
                                 kBackgroundColor,
                                 start,
                                 end);
      // IBUS_ATTR_TYPE_FOREGROUND is necessary to highlight the segment on
      // Firefox.
      const guint kForegroundColor = 0x000000;
      ibus_text_append_attribute(text,
                                 IBUS_ATTR_TYPE_FOREGROUND,
                                 kForegroundColor,
                                 start,
                                 end);
    }
    start = end;
  }

  return text;
}

// Returns a cursor position used for updating preedit.
// NOTE: We do not use a cursor position obtained from Mozc when the candidate
// window is shown since ibus uses the cursor position to locate the candidate
// window and the position obtained from Mozc is not what we expect.
int CursorPos(const commands::Output &output) {
  if (!output.has_preedit()) {
    return 0;
  }
  if (output.preedit().has_highlighted_position()) {
    return output.preedit().highlighted_position();
  }
  return output.preedit().cursor();
}

}  // namespace

PreeditHandler::PreeditHandler() {
}

PreeditHandler::~PreeditHandler() {
}

bool PreeditHandler::Update(IBusEngine *engine,
                            const commands::Output &output) {
  if (!output.has_preedit()) {
    ibus_engine_hide_preedit_text(engine);
    return true;
  }
  IBusText *text = ComposePreeditText(output.preedit());
#if IBUS_CHECK_VERSION(1, 3, 99)
  ibus_engine_update_preedit_text_with_mode(engine, text, CursorPos(output),
                                            TRUE, IBUS_ENGINE_PREEDIT_COMMIT);
#else
  ibus_engine_update_preedit_text(engine, text, CursorPos(output), TRUE);
#endif
  // |text| is released by ibus_engine_update_preedit_text.
  return true;
}

}  // namespace ibus
}  // namespace mozc

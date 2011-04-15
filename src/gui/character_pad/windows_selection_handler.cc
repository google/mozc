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

#ifdef OS_WINDOWS
#include "gui/character_pad/windows_selection_handler.h"

#include <windows.h>
#include <QtCore/QString>
#include <vector>
#include "base/base.h"
#include "base/util.h"

namespace mozc {
namespace gui {

WindowsSelectionHandler::WindowsSelectionHandler() {}
WindowsSelectionHandler::~WindowsSelectionHandler() {}

void WindowsSelectionHandler::Select(const QString &str) {
  vector<INPUT> inputs;
  for (int i = 0; i < str.size(); ++i) {
    INPUT input = { 0 };
    KEYBDINPUT kb = { 0 };
    const wchar_t ucs2 = static_cast<wchar_t>(str[i].unicode());

    // down
    kb.wScan = ucs2;
    kb.dwFlags = KEYEVENTF_UNICODE;
    input.type = INPUT_KEYBOARD;
    input.ki = kb;
    inputs.push_back(input);

    // up
    kb.wScan = ucs2;
    kb.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    input.type = INPUT_KEYBOARD;
    input.ki = kb;
    inputs.push_back(input);
  }

  if (inputs.empty()) {
    return;
  }

  if (!::SendInput(static_cast<UINT>(inputs.size()),
                   &inputs[0], sizeof(inputs[0]))) {
    LOG(ERROR)<< "SendInput failed: " << ::GetLastError();
    return;
  }
}
}  // gui
}  // mozc
#endif   // OS_WINDOWS

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

#ifndef MOZC_BASE_MAC_MAC_PROCESS_H_
#define MOZC_BASE_MAC_MAC_PROCESS_H_

#include <string>

#include "absl/strings/string_view.h"
#include "base/strings/zstring_view.h"

#ifdef __APPLE__
namespace mozc {
class MacProcess {
 public:
  // Open a browser in mac way using NSFoundation framework.
  static bool OpenBrowserForMac(absl::string_view url);

  // Open an application in mac way using NSWorkspace.
  static bool OpenApplication(absl::string_view path);

  // Open GoogleJapaneseInputTool.app as the specified tool name.
  static bool LaunchMozcTool(zstring_view tool_name);

  // Open GoogleJapaneseInputTool.app as the error message dialog.
  static bool LaunchErrorMessageDialog(zstring_view error_type);
};
}  // namespace mozc
#endif  // __APPLE__
#endif  // MOZC_BASE_MAC_MAC_PROCESS_H_

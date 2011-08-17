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

#ifndef MOZC_WIN32_BASE_OMAHA_UTIL_H_
#define MOZC_WIN32_BASE_OMAHA_UTIL_H_

// Omaha is the code name of Google Update, which is not used in
// OSS version of Mozc.
#if !defined(GOOGLE_JAPANESE_INPUT_BUILD)
#error OmahaUtil must be used with Google Japanese Input, not OSS Mozc
#endif  // !GOOGLE_JAPANESE_INPUT_BUILD

#include <windows.h>
#include <string>

#include "base/port.h"

namespace mozc {
namespace win32 {
// TODO(yukawa): Add unit test for this class.
class OmahaUtil {
 public:
  // Writes the channel name specified by |value| for Omaha.
  // Returns true if the operation completed successfully.
  static bool WriteChannel(const wstring &value);

  // Reads the channel name for Omaha.
  // Returns an empty string if there is no entry or fails to retrieve the
  // channel name.
  static wstring ReadChannel();

  // Clears the registry entry to specify error message for Omaha.
  // Returns true if the operation completed successfully.
  static bool ClearOmahaError();

  // Writes the registry entry for Omaha to show some error messages.
  // Returns true if the operation completed successfully.
  static bool WriteOmahaError(const wstring &ui_message, const wstring &header);

  // Clears the registry entry for the channel name.
  // Returns true if the operation completed successfully.
  static bool ClearChannel();

 private:
  DISALLOW_COPY_AND_ASSIGN(OmahaUtil);
};
}  // namespace win32
}  // namespace mozc
#endif  // MOZC_WIN32_BASE_OMAHA_UTIL_H_

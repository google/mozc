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

#ifndef MOZC_GUI_BASE_SETUP_UTIL_H_
#define MOZC_GUI_BASE_SETUP_UTIL_H_

#include <cstdint>
#include <memory>


namespace mozc {

class UserDictionaryStorage;

namespace gui {

class SetupUtil {
 public:
  enum SetDefaultFlags {
    NONE = 0,
    IME_DEFAULT = 1,
    DISABLE_HOTKEY = 2,
    IMPORT_MSIME_DICTIONARY = 4
  };

  SetupUtil();
  SetupUtil(const SetupUtil&) = delete;
  SetupUtil& operator=(const SetupUtil&) = delete;
  virtual ~SetupUtil();

  // locks user dictionary
  bool LockUserDictionary();

  bool IsUserDictionaryLocked() const;

  // |flags| should be assigned by SetDefaultFlags.
  // If |flags| contains
  // - IME_DEFAULT, sets this IME as the default IME
  // - DISABLE_HOTKEY, disables IME hotkey (Ctrl+Shift).
  // - IMPORT_MSIME_DICTIONARY, Imports MS-IME's user dictionary
  // this function usually is used to lock user dictionary. for example
  // SetupUtil setuputil;
  // setuputil.LockUserDictionary();
  // -- do somethings to keep userdictionary locked -----
  // setuputil.SetDafaultProperty(flags);
  void SetDefaultProperty(uint32_t flags);

 private:
  // Imports MS-IME's user dictionary to Mozc' dictionary
  bool MigrateDictionaryFromMSIME();

  std::unique_ptr<UserDictionaryStorage> storage_;

  bool is_userdictionary_locked_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_BASE_SETUP_UTIL_H_

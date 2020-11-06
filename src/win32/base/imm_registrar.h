// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_WIN32_BASE_IMM_REGISTRAR_H_
#define MOZC_WIN32_BASE_IMM_REGISTRAR_H_

#include <windows.h>

#include <string>

#include "win32/base/keyboard_layout_id.h"

namespace mozc {
namespace win32 {

// ImmRegistrar is used to register and unregister the IME in the system.
// This class can only be used in administrators account.
class ImmRegistrar {
 public:
  // Installs module to the system as an IME.
  // Returns registered HKL to hkl.
  static HRESULT Register(const std::wstring &ime_filename,
                          const std::wstring &layout_name,
                          const std::wstring &layout_display_name_resource_path,
                          int layout_display_name_resource_id, HKL *hkl);

  // Uninstalls module from the system.
  static HRESULT Unregister(const std::wstring &ime_filename);

  // Returns true if given |hkl| is an IME which consists of |ime_filename|.
  static bool IsIME(HKL hkl, const std::wstring &ime_filename);

  // Returns a file name of the IME file.
  static std::wstring GetFileNameForIME();

  // Returns a full path to the IME file.
  // Returns an empty string if it fails to compose a fullpath.
  static std::wstring GetFullPathForIME();

  // Returns a KILD for the IME file.
  static KeyboardLayoutID GetKLIDForIME();

  // Returns a layout name of the IME file.
  // Returns an empty string if it fails to compose a layout name.
  static std::wstring GetLayoutName();

  // Returns a resource ID of a layout display name.
  static int GetLayoutDisplayNameResourceId();

  // Returns a KILD for the given ime file specified by |ime_file|
  static KeyboardLayoutID GetKLIDFromFileName(const std::wstring &ime_file);

  // Add key to the preload if not exist.
  // Returns S_OK if operation completes successfully.
  static HRESULT RestorePreload(const KeyboardLayoutID &klid);

  // Removes a value equal to hkl from HKCU\Keyboard Layout\\Preload and
  // decrement value names which are more than hkl.
  static HRESULT RemoveKeyFromPreload(const KeyboardLayoutID &klid,
                                      const KeyboardLayoutID &defaultKLID);

  // Moves the value corresponding to |hkl| to the top in
  // HKCU\Keyboard Layout\Preload, which means setting |hkl| as the default
  // IME.
  // Returns S_OK if operation completes successfully.
  static HRESULT MovePreloadValueToTop(const KeyboardLayoutID &klid);

 private:
  ImmRegistrar() {}
  virtual ~ImmRegistrar() {}
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_IMM_REGISTRAR_H_

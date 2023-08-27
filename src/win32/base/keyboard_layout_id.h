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

#ifndef MOZC_WIN32_BASE_KEYBOARD_LAYOUT_ID_H_
#define MOZC_WIN32_BASE_KEYBOARD_LAYOUT_ID_H_

#include <windows.h>

#include <string>

namespace mozc {
namespace win32 {

// This class represents a keyboard layout identifier (KLID).  You can use this
// class to convert to/from both KLID text representation like "04110411" and
// KLID integer representation like 0x04110411.
// Please note that the HKY (a handle to a keyboard layout) is based on
// its entirely different principle from HKID.
// To obtain a HKY corresponding to HKID, use ::LoadKeyboardLayout.  Do not
// make a handle based on the integer representation of KLID.
// See the following article for details.
// http://blogs.msdn.com/b/michkap/archive/2005/04/17/409032.aspx
// Note that this simple wrapper accepts any KLID even if the ID is not
// registered in the registry.
class KeyboardLayoutID {
 public:
  // Initializes an instance with leaving |id_| as 'cleared'.
  KeyboardLayoutID();

  // Initializes an instance with a KLID in text form.
  // |id_| remains 'cleared' if |text| is an invalid text form.
  explicit KeyboardLayoutID(const std::wstring &text);

  // Initializes an instance with a KLID in integer form.
  explicit KeyboardLayoutID(DWORD id);

  // Returns true unless |text| does not have an invalid text form.
  // When this method returns false, it behaves as if |clear_id()| was called.
  bool Parse(const std::wstring &text);

  // Returns KLID in text form.
  // You cannot call this method when |has_id()| returns false.
  std::wstring ToString() const;

  // Returns KLID in integer form.
  // You cannot call this method when |has_id()| returns false.
  DWORD id() const;

  // Updates |id_| with the given KLID.  Note that this method never checks if
  // the given |id| is valid in terms of the existence of the corresponding
  // registry entry.
  void set_id(DWORD id);

  // Returns true unless |id_| is not 'cleared'.
  bool has_id() const;

  // Set |id_| 'cleared'.
  void clear_id();

 private:
  DWORD id_;
  bool has_id_;
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_KEYBOARD_LAYOUT_ID_H_

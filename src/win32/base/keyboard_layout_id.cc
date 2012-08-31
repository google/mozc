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

#include "win32/base/keyboard_layout_id.h"

#include <strsafe.h>

#include <iomanip>
#include <map>
#include <sstream>
#include <string>

#include "base/base.h"

namespace {
// A valid KLID consists of 8 character of hexadecimal digit in text form.
const size_t kTextLength = (KL_NAMELENGTH - 1);
const DWORD kDefaultKLID = 0;

bool IsHexadecimalDigit(wchar_t c) {
  if (L'0' <= c && c <= L'9') {
    return true;
  }
  if (L'a' <= c && c <= L'f') {
    return true;
  }
  if (L'A' <= c && c <= L'F') {
    return true;
  }
  return false;
}
}  // anonymous namespace

namespace mozc {
namespace win32 {
KeyboardLayoutID::KeyboardLayoutID()
    : id_(kDefaultKLID),
      has_id_(false) {}

KeyboardLayoutID::KeyboardLayoutID(const wstring &text)
    : id_(kDefaultKLID),
      has_id_(false) {
  Parse(text);
}

KeyboardLayoutID::KeyboardLayoutID(DWORD id)
    : id_(id),
      has_id_(true) {}

bool KeyboardLayoutID::Parse(const wstring &text) {
  clear_id();

  if (text.size() != kTextLength) {
    return false;
  }
  for (size_t i = 0; i < text.size(); ++i) {
    if (!IsHexadecimalDigit(text[i])) {
      return false;
    }
  }

  DWORD id = 0;
  wstringstream ss;
  ss << text;
  ss >> hex >> id;
  set_id(id);
  return true;
}

wstring KeyboardLayoutID::ToString() const {
  CHECK(has_id()) << "ID is not set.";
  wchar_t buffer[KL_NAMELENGTH];
  const HRESULT result =
      ::StringCchPrintf(buffer, arraysize(buffer), L"%08X", id_);
  if (FAILED(result)) {
    return L"";
  }
  return buffer;
}

DWORD KeyboardLayoutID::id() const {
  CHECK(has_id()) << "ID is not set.";
  return id_;
}

void KeyboardLayoutID::set_id(DWORD id) {
  id_ = id;
  has_id_ = true;
}

bool KeyboardLayoutID::has_id() const {
  return has_id_;
}

void KeyboardLayoutID::clear_id() {
  id_ = kDefaultKLID;
  has_id_ = false;
}

}  // namespace win32
}  // namespace mozc

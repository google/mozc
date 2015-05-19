// Copyright 2010-2013, Google Inc.
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

#include "win32/base/tsf_profile.h"

#include "win32/base/display_name_resource.h"

namespace mozc {
namespace win32 {

namespace {

#ifdef GOOGLE_JAPANESE_INPUT_BUILD

// {D5A86FD5-5308-47EA-AD16-9C4EB160EC3C}
static const GUID kGoogleJapaneseInputTextService = {
  0xd5a86fd5, 0x5308, 0x47ea, {0xad, 0x16, 0x9c, 0x4e, 0xb1, 0x60, 0xec, 0x3c}
};

// {773EB24E-CA1D-4B1B-B420-FA985BB0B80D}
static const GUID kGoogleJapaneseInputProfile = {
  0x773eb24e, 0xca1d, 0x4b1b, {0xb4, 0x20, 0xfa, 0x98, 0x5b, 0xb0, 0xb8, 0x0d}
};

#else

// {10A67BC8-22FA-4A59-90DC-2546652C56BF}
static const GUID kMozcTextService = {
  0x10a67bc8, 0x22fa, 0x4a59, {0x90, 0xdc, 0x25, 0x46, 0x65, 0x2c, 0x56, 0xbf}
};

// {186F700C-71CF-43FE-A00E-AACB1D9E6D3D}
static const GUID kMozcProfile = {
  0x186f700c, 0x71cf, 0x43fe, {0xa0, 0x0e, 0xaa, 0xcb, 0x1d, 0x9e, 0x6d, 0x3d}
};

#endif

// Represents the language ID of this text service.
const LANGID kTextServiceLanguage = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);

}  // namespace

const GUID &TsfProfile::GetTextServiceGuid() {
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  return kGoogleJapaneseInputTextService;
#else
  return kMozcTextService;
#endif
}

const GUID &TsfProfile::GetProfileGuid() {
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  return kGoogleJapaneseInputProfile;
#else
  return kMozcProfile;
#endif
}

LANGID TsfProfile::GetLangId() {
  return kTextServiceLanguage;
}

int TsfProfile::GetIconIndex() {
  return 0;
}

int TsfProfile::GetDescriptionTextIndex() {
  return IDS_TEXTSERVICE_DISPLAYNAME_SYNONYM;
}

}  // namespace win32
}  // namespace mozc


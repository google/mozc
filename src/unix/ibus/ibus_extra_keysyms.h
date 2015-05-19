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

#ifndef MOZC_UNIX_IBUS_IBUS_EXTRA_KEYSYMS_H_
#define MOZC_UNIX_IBUS_IBUS_EXTRA_KEYSYMS_H_

// for guint
#include "unix/ibus/ibus_header.h"

#ifdef OS_CHROMEOS
// Defines key symbols which are not defined on ibuskeysyms.h to handle
// some keys on ChromeOS.
const guint IBUS_Back = 0x1008FF26;
const guint IBUS_Forward = 0x1008FF27;
const guint IBUS_Reload = 0x1008FF73;
const guint IBUS_LaunchB = 0x1008FF4B;
const guint IBUS_LaunchA = 0x1008FF4A;
const guint IBUS_MonBrightnessDown = 0x1008FF03;
const guint IBUS_MonBrightnessUp = 0x1008FF02;
const guint IBUS_AudioMute = 0x1008FF12;
const guint IBUS_AudioLowerVolume = 0x1008FF11;
const guint IBUS_AudioRaiseVolume = 0x1008FF13;
#endif  // OS_CHROMEOS

#endif  // MOZC_UNIX_IBUS_IBUS_EXTRA_KEYSYMS_H_

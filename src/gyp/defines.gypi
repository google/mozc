# Copyright 2010-2021, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# defines.gypi is supposed to contain configurable options represented by
# gyp variables and define macros.
# gyp variables should be configurable by build_mozc.py.
{
  'variables': {
    # Build branding.  It should be either of 'Mozc' or 'GoogleJapaneseInput'.
    # For OSS version, 'Mozc' is only supported.
    'branding%': 'Mozc',

    # Application versions (e.g. "2.26.4200.1" / "2.26.4200").
    # The values are set by build_mozc.py
    'version%': '0.0.0.0',
    'short_version%': '0.0.0',

    # a flag whether the current build is dev-channel or not.
    'channel_dev%': '0',

    # enable ambiguous search (a.k.a. KATSUKOU-conversion).
    'enable_ambiguous_search%': '0',

    # enable typing correction.
    'enable_typing_correction%': '0',

    # use_qt is 'YES' only if you want to use GUI binaries.
    'use_qt%': 'YES',

    # Qt version to be used.
    'qt_ver%': '5',

    # use_libprotobuf represents if protobuf library is used or not.
    # This option is only for Linux.
    # You should not set this flag if you want to use "dlopen" to
    # load Mozc's modules. See
    # - https://github.com/google/mozc/issues/14
    # for the background information.
    'use_libprotobuf%': '0',
  },
  'target_defaults': {
    'defines': [
      'MOZC_GYP_BUILD',
    ],
    'conditions': [
      ['branding=="GoogleJapaneseInput"', {
        'defines': ['GOOGLE_JAPANESE_INPUT_BUILD'],
      }, {
        'defines': ['MOZC_BUILD'],
      }],
      ['channel_dev==1', {
        'defines': ['CHANNEL_DEV'],
      }],
    ]
  }
}

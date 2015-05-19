# Copyright 2010-2013, Google Inc.
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

# WARNING: This file will be copied from gyp to
# third_party/jsoncpp. Don't edit the copied version.
{
  'targets': [
    {
      # IMPORTANT: Do no directly any include file in this target nor depend
      #     on this target from your target. Include 'net/jsoncpp.h' and make
      #     depend on 'net/net.gyp:jsoncpp' instead.
      'target_name': 'jsoncpp_do_not_directly_use',
      'type': 'static_library',
      'variables': {
        'jsoncpp_defines': [
          'JSON_USE_EXCEPTION=0',  # Mozc basically disables C++ exception.
        ],
        'jsoncpp_include_dirs': [
          'include',
        ],
        'jsoncpp_srcs': [
          'src/lib_json/json_reader.cpp',
          'src/lib_json/json_value.cpp',
          'src/lib_json/json_writer.cpp',
        ],
      },
      'defines': [
        '<@(jsoncpp_defines)',
      ],
      'include_dirs': [
        '<@(jsoncpp_include_dirs)',
      ],
      'sources': [
        '<@(jsoncpp_srcs)',
      ],
      'all_dependent_settings': {
        'defines': [
          '<@(jsoncpp_defines)',
        ],
      },
    },
  ],
}

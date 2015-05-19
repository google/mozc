# Copyright 2010-2012, Google Inc.
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

# This gyp script builds NaCl Mozc modules.
#
# The targets in this gyp file are designed to be built with a NaCl toolchain.
{
  'variables': {
    'relative_dir': 'chrome/nacl',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'nacl_libraries': ['-lppapi', '-lppapi_cpp'],
  },
  'target_defaults': {
    # NaCl provides Linux-like environment.
    'defines': ['OS_LINUX'],
    'defines!': [
      'OS_WINDOWS',
      'OS_MAC',
      'OS_ANDROID',
      'OS_CHROMEOS',
    ],
    'link_settings': {
      'libraries': ['<@(nacl_libraries)'],
    },
  },
  'targets': [
    {
      'target_name': 'nacl_session.nexe',
      'type': 'executable',
      'sources': [
        'nacl_session.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../config/config.gyp:config_handler',
        '../../config/config.gyp:config_protocol',
        '../../net/net.gyp:jsoncpp',
        '../../session/session_base.gyp:key_parser',
        '../../session/session_base.gyp:request_handler',
        '../../session/session_base.gyp:session_protocol',
        '../../session/session.gyp:session',
      ],
    },
  ],
}

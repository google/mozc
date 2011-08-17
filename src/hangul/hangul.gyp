# Copyright 2010-2011, Google Inc.
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

{
  'variables': {
    'relative_dir': 'hangul',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'additional_packages': [
      'libhangul',
    ],
  },
  'targets': [
    {
      'target_name': 'hangul_session',
      'type': 'static_library',
      'sources': [
        'session.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../config/config.gyp:genproto_config',
        '../session/session_base.gyp:genproto_session',
        '../session/session_base.gyp:keymap',
        'hangul_session_server',
      ],
    },
    # This is a copy of session.gyp:session_server but does not depend
    # on session.gyp:session not to link language model/dictionary of
    # Japanese.
    {
      'target_name': 'hangul_session_server',
      'type': 'static_library',
      'sources': [
        'hangul_session_factory.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../config/config.gyp:genproto_config',
        '../session/session.gyp:session_handler',
        '../session/session_base.gyp:genproto_session',
      ],
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'targets': [
        {
          # TODO(nona):move to common.gypi
          'target_name': 'ibus_mozc_hangul_metadata',
          'type': 'static_library',
          'sources': [
            'unix/ibus/mozc_engine_property.cc',
          ],
          'dependencies': [
            '../session/session_base.gyp:genproto_session',
          ],
          'includes': [
            '../unix/ibus/ibus_libraries.gypi',
          ],
        },
        {
          'target_name': 'ibus_mozc_hangul',
          'type': 'executable',
          'sources': [
            'unix/ibus/config_updater.cc',
            'unix/ibus/main.cc',
          ],
          'dependencies': [
            '../config/config.gyp:genproto_config',
            '../session/session_base.gyp:genproto_session',
            '../unix/ibus/ibus.gyp:ibus_mozc_lib',
            'hangul_session',
            'ibus_mozc_hangul_metadata',
          ],
          'conditions': [
            ['chromeos==1', {
             'dependencies+': [
               '<@(ibus_standalone_dependencies)',
             ],
            }, {
             'dependencies+': [
               '<@(ibus_client_dependencies)',
             ],
            }],
          ],
          'includes': [
            '../unix/ibus/ibus_libraries.gypi',
          ],
        },
        {
          'target_name': 'hangul_session_test',
          'type': 'executable',
          'sources': [
            'session_test.cc',
          ],
          'dependencies': [
            '../testing/testing.gyp:gtest_main',
            'hangul_session',
          ],
          'conditions': [
            ['chromeos==1', {
             'dependencies+': [
               '<@(ibus_standalone_dependencies)',
             ],
            }, {
             'dependencies+': [
               '<@(ibus_client_dependencies)',
             ],
            }],
          ],
          'includes': [
            '../unix/ibus/ibus_libraries.gypi',
          ],
        },
      ],
    }],
  ],
}

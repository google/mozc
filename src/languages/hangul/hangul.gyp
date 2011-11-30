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
    'relative_dir': 'languages/hangul',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'hangul_session',
      'type': 'static_library',
      'sources': [
        'session.cc',
        'hangul_session_factory.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../config/config.gyp:genproto_config',
        '../../session/session_base.gyp:genproto_session',
        '../../session/session_base.gyp:keymap',
      ],
    },
    {
      'target_name': 'language_dependent_spec_hangul',
      'type': 'static_library',
      'sources': [
        'lang_dep_spec.cc',
      ],
      'dependencies': [
        '../languages.gyp:language_spec_base',
      ],
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'ibus_mozc_hangul_metadata',
          'type': 'static_library',
          'sources': [
            'unix/ibus/mozc_engine_property.cc',
          ],
          'dependencies': [
            '../../session/session_base.gyp:genproto_session',
          ],
          'includes': [
            '../../unix/ibus/ibus_libraries.gypi',
          ],
        },
        {
          'target_name': 'ibus_mozc_hangul',
          'type': 'executable',
          'sources': [
            'unix/ibus/main.cc',
          ],
          'dependencies': [
            '../../unix/ibus/ibus.gyp:ibus_mozc_lib',
            '../languages.gyp:global_language_spec',
            'ibus_mozc_hangul_metadata',
            'language_dependent_spec_hangul',
          ],
          'includes': [
            '../../unix/ibus/ibus_libraries.gypi',
          ],
          'conditions': [
            ['chromeos==1', {
             'dependencies+': [
               '../../config/config.gyp:genproto_config',
               'hangul_session',
             ],
             'sources+': [
               'unix/ibus/config_updater.cc',
             ],
             'includes': [
               'hangul_libraries.gypi',
             ]
            }],
          ],
        },
        {
          'target_name': 'mozc_server_hangul',
          'type': 'executable',
          'sources': [
            'server_main.cc',
          ],
          'dependencies': [
            '../../server/server.gyp:mozc_server_lib',
            '../languages.gyp:global_language_spec',
            'hangul_session',
            'language_dependent_spec_hangul',
          ],
          'includes': [
            'hangul_libraries.gypi',
          ],
        },
        {
          'target_name': 'hangul_session_test',
          'type': 'executable',
          'sources': [
            'session_test.cc',
          ],
          'dependencies': [
            '../../testing/testing.gyp:gtest_main',
            'hangul_session',
          ],
          'includes': [
            'hangul_libraries.gypi',
          ],
        },
      ],
    }],
  ],
}

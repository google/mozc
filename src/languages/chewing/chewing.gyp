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

{
  'variables': {
    'relative_dir': 'chewing',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'chewing_session',
      'type': 'static_library',
      'sources': [
        'session.cc',
        'chewing_session_factory.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../session/session_base.gyp:key_event_util',
        '../../session/session_base.gyp:session_protocol',
      ],
      'includes': [
        'chewing_libraries.gypi',
      ],
    },
    {
      'target_name': 'chewing_all_test',
      'type': 'none',
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'ibus_mozc_chewing',
          'type': 'executable',
          'sources': [
            'unix/ibus/main.cc',
            'unix/ibus/mozc_engine_property.cc',
          ],
          'dependencies': [
            '../../unix/ibus/ibus.gyp:ibus_mozc_lib',
          ],
          'conditions': [
            ['target_platform=="ChromeOS"', {
             'dependencies': [
               '../../config/config.gyp:config_handler',
               '../../config/config.gyp:config_protocol',
               'chewing_session',
             ],
             'sources+': [
              'unix/ibus/config_updater.cc',
             ],
             'includes': [
               'chewing_libraries.gypi',
             ],
            }],
          ],
        },
        {
          'target_name': 'mozc_server_chewing',
          'type': 'executable',
          'sources': [
            'server_main.cc',
          ],
          'dependencies': [
            '../../server/server.gyp:mozc_server_lib',
            'chewing_session',
          ],
          'includes': [
            'chewing_libraries.gypi',
          ],
        },
      ],
    }],
  ],
}

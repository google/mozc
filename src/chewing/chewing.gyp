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
    'relative_dir': 'chewing',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'additional_packages': [
      'chewing',
    ],
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
        '../base/base.gyp:base',
        '../config/config.gyp:genproto_config',
        '../session/session_base.gyp:genproto_session',
        '../session/session.gyp:session_handler',
      ],
    },
  ],
  'conditions': [
    ['chromeos==1', {
      'targets': [
        {
          'target_name': 'config_updater',
          'type': 'static_library',
          'sources': [
            'unix/ibus/config_updater.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../config/config.gyp:genproto_config',
            '../session/session_base.gyp:genproto_session',
          ],
          'includes': [
            '../unix/ibus/ibus_libraries.gypi',
          ],
        },
      ],
    }],
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'ibus_mozc_chewing_metadata',
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
          'target_name': 'ibus_mozc_chewing',
          'type': 'executable',
          'sources': [
            'unix/ibus/main.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../unix/ibus/ibus.gyp:ibus_mozc_lib',
            'chewing_session',
            'ibus_mozc_chewing_metadata',
          ],
          'conditions': [
            ['chromeos==1', {
             'dependencies+': [
               '<@(ibus_standalone_dependencies)',
               'config_updater',
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
